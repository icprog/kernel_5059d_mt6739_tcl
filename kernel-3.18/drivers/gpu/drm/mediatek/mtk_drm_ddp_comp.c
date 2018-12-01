
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <drm/drmP.h>
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_ddp_comp.h"

#define DISP_REG_BLS_EN				0x0000
#define DISP_REG_BLS_SRC_SIZE			0x0018
#define DISP_REG_BLS_PWM_DUTY			0x00a0
#define DISP_REG_BLS_PWM_CON			0x00a8

#define DISP_OD_EN				0x0000
#define DISP_OD_INTEN				0x0008
#define DISP_OD_INTSTA				0x000c
#define DISP_OD_CFG				0x0020
#define DISP_OD_SIZE				0x0030

#define DISP_REG_UFO_START			0x0000

#define DISP_COLOR_CFG_MAIN			0x0400
#define DISP_COLOR_START			0x0000
#define DISP_COLOR_WIDTH			(DISP_COLOR_START + 0x50)
#define DISP_COLOR_HEIGHT			(DISP_COLOR_START + 0x54)

#define	OD_RELAY_MODE		BIT(0)

#define	UFO_BYPASS		BIT(2)

#define	COLOR_BYPASS_ALL	BIT(7)
#define	COLOR_SEQ_SEL		BIT(13)

#define BLS_PWM_CLKDIV		BIT(16)
#define BLS_PWM_EN		BIT(16)

static void mtk_bls_config(struct mtk_ddp_comp *comp, unsigned int w,
		unsigned int h, unsigned int vrefresh)
{
	writel(h << 16 | w, comp->regs + DISP_REG_BLS_SRC_SIZE);
	writel(0, comp->regs + DISP_REG_BLS_PWM_DUTY);
	writel(BLS_PWM_CLKDIV, comp->regs + DISP_REG_BLS_PWM_CON);
	writel(BLS_PWM_EN, comp->regs + DISP_REG_BLS_EN);
}

static void mtk_color_config(struct mtk_ddp_comp *comp, unsigned int w,
			     unsigned int h, unsigned int vrefresh)
{
	writel(w, comp->regs + comp->data->color_offset + DISP_COLOR_WIDTH);
	writel(h, comp->regs + comp->data->color_offset + DISP_COLOR_HEIGHT);
}

static void mtk_color_start(struct mtk_ddp_comp *comp)
{
	writel(COLOR_BYPASS_ALL | COLOR_SEQ_SEL,
	       comp->regs + DISP_COLOR_CFG_MAIN);
	writel(0x1, comp->regs + comp->data->color_offset + DISP_COLOR_START);
}

static void mtk_od_config(struct mtk_ddp_comp *comp, unsigned int w,
			  unsigned int h, unsigned int vrefresh)
{
	writel(w << 16 | h, comp->regs + DISP_OD_SIZE);
}

static void mtk_od_start(struct mtk_ddp_comp *comp)
{
	writel(OD_RELAY_MODE, comp->regs + DISP_OD_CFG);
	writel(1, comp->regs + DISP_OD_EN);
}

static void mtk_ufoe_start(struct mtk_ddp_comp *comp)
{
	writel(UFO_BYPASS, comp->regs + DISP_REG_UFO_START);
}

static const struct mtk_ddp_comp_funcs ddp_bls = {
	.config = mtk_bls_config,
};

static const struct mtk_ddp_comp_funcs ddp_color = {
	.config = mtk_color_config,
	.start = mtk_color_start,
};

static const struct mtk_ddp_comp_funcs ddp_od = {
	.config = mtk_od_config,
	.start = mtk_od_start,
};

static const struct mtk_ddp_comp_funcs ddp_ufoe = {
	.start = mtk_ufoe_start,
};

static const char * const mtk_ddp_comp_stem[MTK_DDP_COMP_TYPE_MAX] = {
	[MTK_DISP_OVL] = "ovl",
	[MTK_DISP_RDMA] = "rdma",
	[MTK_DISP_WDMA] = "wdma",
	[MTK_DISP_COLOR] = "color",
	[MTK_DISP_AAL] = "aal",
	[MTK_DISP_GAMMA] = "gamma",
	[MTK_DISP_UFOE] = "ufoe",
	[MTK_DSI] = "dsi",
	[MTK_DPI] = "dpi",
	[MTK_DISP_PWM] = "pwm",
	[MTK_DISP_MUTEX] = "mutex",
	[MTK_DISP_OD] = "od",
	[MTK_DISP_BLS] = "bls",
};

struct mtk_ddp_comp_match {
	enum mtk_ddp_comp_type type;
	int alias_id;
	const struct mtk_ddp_comp_funcs *funcs;
};

static const struct mtk_ddp_comp_match mtk_ddp_matches[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL]	= { MTK_DISP_AAL,	0, NULL },
	[DDP_COMPONENT_BLS]	= { MTK_DISP_BLS,	0, &ddp_bls },
	[DDP_COMPONENT_COLOR0]	= { MTK_DISP_COLOR,	0, &ddp_color },
	[DDP_COMPONENT_COLOR1]	= { MTK_DISP_COLOR,	1, &ddp_color },
	[DDP_COMPONENT_DPI0]	= { MTK_DPI,		0, NULL },
	[DDP_COMPONENT_DSI0]	= { MTK_DSI,		0, NULL },
	[DDP_COMPONENT_DSI1]	= { MTK_DSI,		1, NULL },
	[DDP_COMPONENT_GAMMA]	= { MTK_DISP_GAMMA,	0, NULL },
	[DDP_COMPONENT_OD]	= { MTK_DISP_OD,	0, &ddp_od },
	[DDP_COMPONENT_OVL0]	= { MTK_DISP_OVL,	0, NULL },
	[DDP_COMPONENT_OVL1]	= { MTK_DISP_OVL,	1, NULL },
	[DDP_COMPONENT_PWM0]	= { MTK_DISP_PWM,	0, NULL },
	[DDP_COMPONENT_RDMA0]	= { MTK_DISP_RDMA,	0, NULL },
	[DDP_COMPONENT_RDMA1]	= { MTK_DISP_RDMA,	1, NULL },
	[DDP_COMPONENT_RDMA2]	= { MTK_DISP_RDMA,	2, NULL },
	[DDP_COMPONENT_UFOE]	= { MTK_DISP_UFOE,	0, &ddp_ufoe },
	[DDP_COMPONENT_WDMA0]	= { MTK_DISP_WDMA,	0, NULL },
	[DDP_COMPONENT_WDMA1]	= { MTK_DISP_WDMA,	1, NULL },
};

static struct mtk_ddp_comp_driver_data mt2701_color_driver_data = {
	.color_offset = 0x0f00,
};

static struct mtk_ddp_comp_driver_data mt8173_color_driver_data = {
	.color_offset = 0x0c00,
};

static const struct of_device_id mtk_disp_color_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-color",
	  .data = &mt2701_color_driver_data},
	{ .compatible = "mediatek,mt8173-disp-cp;pr",
	  .data = &mt8173_color_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_color_driver_dt_match);

static inline struct mtk_ddp_comp_driver_data *mtk_color_get_driver_data(
	struct device_node *node)
{
	const struct of_device_id *of_id =
		of_match_node(mtk_disp_color_driver_dt_match, node);

	return (struct mtk_ddp_comp_driver_data *)of_id->data;
}

int mtk_ddp_comp_get_id(struct device_node *node,
			enum mtk_ddp_comp_type comp_type)
{
	int id = of_alias_get_id(node, mtk_ddp_comp_stem[comp_type]);
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_ddp_matches); i++) {
		if (comp_type == mtk_ddp_matches[i].type &&
		    (id < 0 || id == mtk_ddp_matches[i].alias_id))
			return i;
	}

	return -EINVAL;
}

int mtk_ddp_comp_init(struct device *dev, struct device_node *node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs)
{
	enum mtk_ddp_comp_type type;
	struct device_node *larb_node;
	struct platform_device *larb_pdev;

	if (comp_id < 0 || comp_id >= DDP_COMPONENT_ID_MAX)
		return -EINVAL;

	comp->id = comp_id;
	comp->funcs = funcs ?: mtk_ddp_matches[comp_id].funcs;

	if (comp_id == DDP_COMPONENT_DPI0 ||
	    comp_id == DDP_COMPONENT_DSI0 ||
	    comp_id == DDP_COMPONENT_PWM0) {
		comp->regs = NULL;
		comp->clk = NULL;
		comp->irq = 0;
		return 0;
	}

	comp->regs = of_iomap(node, 0);
	comp->irq = of_irq_get(node, 0);
	comp->clk = of_clk_get(node, 0);
	if (IS_ERR(comp->clk))
		comp->clk = NULL;

	type = mtk_ddp_matches[comp_id].type;

	if (type == MTK_DISP_COLOR)
		comp->data = mtk_color_get_driver_data(node);

	/* Only DMA capable components need the LARB property */
	comp->larb_dev = NULL;
	if (type != MTK_DISP_OVL &&
	    type != MTK_DISP_RDMA &&
	    type != MTK_DISP_WDMA)
		return 0;

	larb_node = of_parse_phandle(node, "mediatek,larb", 0);
	if (!larb_node) {
		dev_err(dev,
			"Missing mediadek,larb phandle in %s node\n",
			node->full_name);
		return -EINVAL;
	}

	larb_pdev = of_find_device_by_node(larb_node);
	if (!larb_pdev) {
		dev_warn(dev, "Waiting for larb device %s\n",
			 larb_node->full_name);
		of_node_put(larb_node);
		return -EPROBE_DEFER;
	}
	of_node_put(larb_node);

	comp->larb_dev = &larb_pdev->dev;

	return 0;
}

int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (private->ddp_comp[comp->id])
		return -EBUSY;

	private->ddp_comp[comp->id] = comp;
	return 0;
}

void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	private->ddp_comp[comp->id] = NULL;
}