// SPDX-License-Identifier: GPL-2.0
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#define TSADC_INT_EN 0x08
#define SHUT_GPIO_BITS (BIT(4) | BIT(5))
#define SHUT_CRU_BITS  (BIT(8) | BIT(9))

static void __iomem *tsadc_regs;
static struct pinctrl *tsadc_pinctrl;
static struct pinctrl_state *state_gpio;
static struct pinctrl_state *state_otpout;
static u32 original_route_bits;
static bool armed;

static int recovery_param_set(const char *val, const struct kernel_param *kp);
static int recovery_param_get(char *buf, const struct kernel_param *kp);

static const struct kernel_param_ops recovery_param_ops = {
    .set = recovery_param_set,
    .get = recovery_param_get,
};

module_param_cb(recovery, &recovery_param_ops, NULL, 0600);
MODULE_PARM_DESC(recovery, "0=stock CRU route, 1=temporary PMIC/GPIO route");

static int set_route(bool to_pmic)
{
    u32 val;
    int ret;

    if (!tsadc_regs || !tsadc_pinctrl)
        return -ENODEV;

    if (to_pmic) {
        ret = pinctrl_select_state(tsadc_pinctrl, state_otpout);
        if (ret)
            return ret;

        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        val &= ~SHUT_CRU_BITS;
        val |= SHUT_GPIO_BITS;
        writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
        mb();
        armed = true;
    } else {
        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);
        val |= original_route_bits;
        writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
        mb();

        ret = pinctrl_select_state(tsadc_pinctrl, state_gpio);
        if (ret)
            return ret;

        armed = false;
    }

    return 0;
}

static int recovery_param_set(const char *val, const struct kernel_param *kp)
{
    int requested;
    int ret;

    (void)kp;

    ret = kstrtoint(val, 0, &requested);
    if (ret)
        return ret;
    if (requested != 0 && requested != 1)
        return -EINVAL;
    if (!tsadc_regs || !tsadc_pinctrl)
        return -EAGAIN;

    return set_route(requested == 1);
}

static int recovery_param_get(char *buf, const struct kernel_param *kp)
{
    u32 val = 0;

    (void)kp;

    if (tsadc_regs)
        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);

    return scnprintf(buf, PAGE_SIZE,
                     "%u int_en=0x%08x original=0x%08x\n",
                     armed ? 1U : 0U,
                     val,
                     original_route_bits);
}

static int __init flip_tsadc_recovery_init(void)
{
    struct device_node *np;
    struct platform_device *pdev;
    int ret;

    np = of_find_compatible_node(NULL, NULL, "rockchip,rk3568-tsadc");
    if (!np)
        return -ENODEV;

    pdev = of_find_device_by_node(np);
    if (!pdev) {
        of_node_put(np);
        return -ENODEV;
    }

    tsadc_regs = of_iomap(np, 0);
    of_node_put(np);
    if (!tsadc_regs)
        return -ENOMEM;

    tsadc_pinctrl = pinctrl_get(&pdev->dev);
    put_device(&pdev->dev);
    if (IS_ERR(tsadc_pinctrl)) {
        ret = PTR_ERR(tsadc_pinctrl);
        tsadc_pinctrl = NULL;
        iounmap(tsadc_regs);
        tsadc_regs = NULL;
        return ret;
    }

    state_gpio = pinctrl_lookup_state(tsadc_pinctrl, "gpio");
    if (IS_ERR(state_gpio)) {
        ret = PTR_ERR(state_gpio);
        goto fail;
    }

    state_otpout = pinctrl_lookup_state(tsadc_pinctrl, "otpout");
    if (IS_ERR(state_otpout)) {
        ret = PTR_ERR(state_otpout);
        goto fail;
    }

    original_route_bits =
        readl_relaxed(tsadc_regs + TSADC_INT_EN) &
        (SHUT_GPIO_BITS | SHUT_CRU_BITS);

    pr_info("flip_tsadc_recovery: loaded route_bits=0x%08x armed=0\n",
            original_route_bits);
    return 0;

fail:
    pinctrl_put(tsadc_pinctrl);
    tsadc_pinctrl = NULL;
    iounmap(tsadc_regs);
    tsadc_regs = NULL;
    return ret;
}

static void __exit flip_tsadc_recovery_exit(void)
{
    if (armed)
        set_route(false);

    if (tsadc_pinctrl)
        pinctrl_put(tsadc_pinctrl);
    if (tsadc_regs)
        iounmap(tsadc_regs);

    pr_info("flip_tsadc_recovery: unloaded\n");
}

module_init(flip_tsadc_recovery_init);
module_exit(flip_tsadc_recovery_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FlipOS lab-only RK3566 TSADC recovery route helper");
