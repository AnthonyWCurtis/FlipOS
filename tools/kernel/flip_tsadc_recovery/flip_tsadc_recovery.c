#include <linux/bitops.h>
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

#define CRU_GLB_RST_CON 0x00dc
#define CRU_WDT_RESET_EXT BIT(7)
#define CRU_PMU_SRST_WDT_DISABLE BIT(4)

static void __iomem *tsadc_regs;
static void __iomem *cru_regs;

static struct pinctrl *tsadc_pinctrl;
static struct pinctrl_state *state_gpio;
static struct pinctrl_state *state_otpout;

static u32 original_route_bits;
static u32 original_cru_wdt_reset_bits;
static bool armed;

static int recovery_param_set(const char *val, const struct kernel_param *kp);
static int recovery_param_get(char *buf, const struct kernel_param *kp);

static const struct kernel_param_ops recovery_param_ops = {
    .set = recovery_param_set,
    .get = recovery_param_get,
};

module_param_cb(recovery, &recovery_param_ops, NULL, 0600);
MODULE_PARM_DESC(
    recovery,
    "0=stock reset routing, 1=TSADC->PMIC + WDT PMU reset recovery");

static int set_cru_wdt_recovery(bool enable)
{
    u32 val;

    if (!cru_regs)
        return -ENODEV;

    /*
     * CRU_GLB_RST_CON bit 7 extends watchdog reset and bit 4 controls
     * whether watchdog reset reaches the PMU reset domain. Bit 4 is
     * active-low. Deep-undervolt recovery needs the broadest watchdog reset
     * route available, while normal operation must get the exact boot-time
     * register state back. Preserve every unrelated bit.
     */
    val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);
    if (enable) {
        val |= CRU_WDT_RESET_EXT;
        val &= ~CRU_PMU_SRST_WDT_DISABLE;
    } else {
        val &= ~(CRU_WDT_RESET_EXT | CRU_PMU_SRST_WDT_DISABLE);
        val |= original_cru_wdt_reset_bits;
    }

    writel_relaxed(val, cru_regs + CRU_GLB_RST_CON);
    mb();

    val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);
    if (enable) {
        if (!(val & CRU_WDT_RESET_EXT) ||
            (val & CRU_PMU_SRST_WDT_DISABLE))
            return -EIO;
    } else if ((val &
                (CRU_WDT_RESET_EXT | CRU_PMU_SRST_WDT_DISABLE)) !=
               original_cru_wdt_reset_bits) {
        return -EIO;
    }

    return 0;
}

static int restore_cru_wdt_recovery(void)
{
    return set_cru_wdt_recovery(false);
}

static int set_route(bool to_pmic)
{
    u32 val;
    int ret;

    if (!tsadc_regs || !cru_regs || !tsadc_pinctrl)
        return -ENODEV;

    if (to_pmic) {
        ret = set_cru_wdt_recovery(true);
        if (ret)
            return ret;

        ret = pinctrl_select_state(
            tsadc_pinctrl,
            state_otpout);
        if (ret) {
            restore_cru_wdt_recovery();
            return ret;
        }

        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        val &= ~SHUT_CRU_BITS;
        val |= SHUT_GPIO_BITS;
        writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
        mb();

        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        if ((val & (SHUT_GPIO_BITS | SHUT_CRU_BITS)) !=
            SHUT_GPIO_BITS) {
            (void)pinctrl_select_state(
                tsadc_pinctrl,
                state_gpio);
            (void)restore_cru_wdt_recovery();
            return -EIO;
        }

        armed = true;
    } else {
        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);
        val |= original_route_bits;
        writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
        mb();

        val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
        if ((val & (SHUT_GPIO_BITS | SHUT_CRU_BITS)) !=
            original_route_bits)
            return -EIO;

        ret = pinctrl_select_state(
            tsadc_pinctrl,
            state_gpio);
        if (ret)
            return ret;

        ret = restore_cru_wdt_recovery();
        if (ret)
            return ret;

        armed = false;
    }

    return 0;
}

static int recovery_param_set(
    const char *val,
    const struct kernel_param *kp)
{
    int requested;
    int ret;

    (void)kp;

    ret = kstrtoint(val, 0, &requested);
    if (ret)
        return ret;
    if (requested != 0 && requested != 1)
        return -EINVAL;
    if (!tsadc_regs || !cru_regs || !tsadc_pinctrl)
        return -EAGAIN;

    return set_route(requested == 1);
}

static int recovery_param_get(
    char *buf,
    const struct kernel_param *kp)
{
    u32 tsadc = 0;
    u32 cru = 0;

    (void)kp;

    if (tsadc_regs)
        tsadc = readl_relaxed(
            tsadc_regs + TSADC_INT_EN);
    if (cru_regs)
        cru = readl_relaxed(
            cru_regs + CRU_GLB_RST_CON);

    return scnprintf(
        buf,
        PAGE_SIZE,
        "armed=%u int_en=0x%08x original_route=0x%08x "
        "cru_glb_rst_con=0x%08x wdt_ext=%u pmu_wdt_reset=%u "
        "original_cru_wdt_bits=0x%08x\n",
        armed ? 1U : 0U,
        tsadc,
        original_route_bits,
        cru,
        (cru & CRU_WDT_RESET_EXT) ? 1U : 0U,
        (cru & CRU_PMU_SRST_WDT_DISABLE) ? 0U : 1U,
        original_cru_wdt_reset_bits);
}

static void __iomem *map_compatible(
    const char *compatible)
{
    struct device_node *np;
    void __iomem *regs;

    np = of_find_compatible_node(
        NULL,
        NULL,
        compatible);
    if (!np)
        return NULL;

    regs = of_iomap(np, 0);
    of_node_put(np);
    return regs;
}

static int __init flip_tsadc_recovery_init(void)
{
    struct device_node *np;
    struct platform_device *pdev;
    int ret;

    np = of_find_compatible_node(
        NULL,
        NULL,
        "rockchip,rk3568-tsadc");
    if (!np)
        return -ENODEV;

    pdev = of_find_device_by_node(np);
    if (!pdev) {
        of_node_put(np);
        return -ENODEV;
    }

    tsadc_regs = of_iomap(np, 0);
    of_node_put(np);
    if (!tsadc_regs) {
        put_device(&pdev->dev);
        return -ENOMEM;
    }

    cru_regs = map_compatible(
        "rockchip,rk3568-cru");
    if (!cru_regs) {
        ret = -ENODEV;
        put_device(&pdev->dev);
        goto fail_maps;
    }

    tsadc_pinctrl = pinctrl_get(&pdev->dev);
    put_device(&pdev->dev);
    if (IS_ERR(tsadc_pinctrl)) {
        ret = PTR_ERR(tsadc_pinctrl);
        tsadc_pinctrl = NULL;
        goto fail_maps;
    }

    state_gpio = pinctrl_lookup_state(
        tsadc_pinctrl,
        "gpio");
    if (IS_ERR(state_gpio)) {
        ret = PTR_ERR(state_gpio);
        goto fail;
    }

    state_otpout = pinctrl_lookup_state(
        tsadc_pinctrl,
        "otpout");
    if (IS_ERR(state_otpout)) {
        ret = PTR_ERR(state_otpout);
        goto fail;
    }

    original_route_bits =
        readl_relaxed(tsadc_regs + TSADC_INT_EN) &
        (SHUT_GPIO_BITS | SHUT_CRU_BITS);

    original_cru_wdt_reset_bits =
        readl_relaxed(
            cru_regs + CRU_GLB_RST_CON) &
        (CRU_WDT_RESET_EXT | CRU_PMU_SRST_WDT_DISABLE);

    pr_info(
        "flip_tsadc_recovery: loaded route=0x%08x "
        "wdt_ext=%u pmu_wdt_reset=%u armed=0\n",
        original_route_bits,
        (original_cru_wdt_reset_bits &
         CRU_WDT_RESET_EXT) ? 1U : 0U,
        (original_cru_wdt_reset_bits &
         CRU_PMU_SRST_WDT_DISABLE) ? 0U : 1U);

    return 0;

fail:
    pinctrl_put(tsadc_pinctrl);
    tsadc_pinctrl = NULL;

fail_maps:
    if (cru_regs) {
        iounmap(cru_regs);
        cru_regs = NULL;
    }
    if (tsadc_regs) {
        iounmap(tsadc_regs);
        tsadc_regs = NULL;
    }

    return ret;
}

static void __exit flip_tsadc_recovery_exit(void)
{
    if (armed)
        set_route(false);

    if (tsadc_pinctrl)
        pinctrl_put(tsadc_pinctrl);
    if (cru_regs)
        iounmap(cru_regs);
    if (tsadc_regs)
        iounmap(tsadc_regs);

    pr_info("flip_tsadc_recovery: unloaded\n");
}

module_init(flip_tsadc_recovery_init);
module_exit(flip_tsadc_recovery_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(
    "FlipOS RK3566 TSADC + watchdog PMU reset recovery helper");
