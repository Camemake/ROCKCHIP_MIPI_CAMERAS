// SPDX-License-Identifier: GPL-2.0
/*
 * The CSI1 D-PHY block comes up with its bus clock gated. The receiver
 * then accepts a stream and never sees a lane. Hold that clock on.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>

static struct clk *phy_clk;

static int __init phy1clk_init(void)
{
	struct device_node *np;
	int ret;

	np = of_find_node_by_path("/csi2-dphy1-hw@21c50000");
	if (!np)
		return -ENODEV;
	phy_clk = of_clk_get(np, 0);
	of_node_put(np);
	if (IS_ERR(phy_clk))
		return PTR_ERR(phy_clk);
	ret = clk_prepare_enable(phy_clk);
	if (ret) {
		clk_put(phy_clk);
		phy_clk = NULL;
		return ret;
	}
	pr_info("csi2-dphy1 pclk on, rate %lu\n", clk_get_rate(phy_clk));
	return 0;
}

static void __exit phy1clk_exit(void)
{
	if (!phy_clk)
		return;
	clk_disable_unprepare(phy_clk);
	clk_put(phy_clk);
}

module_init(phy1clk_init);
module_exit(phy1clk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Keep the Aura CSI1 D-PHY bus clock running");
