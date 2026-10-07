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

#define PMUGRF_SOC_CON3 0x010c
#define PMUGRF_WDT_FAILSAFE BIT(1)
#define PMUGRF_WDT_FAILSAFE_WE BIT(17)

#define CRU_GLB_RST_CON 0x00dc
#define CRU_WDT_RESET_EXT BIT(7)

static void __iomem *tsadc_regs;
static void __iomem *pmugrf_regs;
static void __iomem *cru_regs;

static struct pinctrl *tsadc_pinctrl;
static struct pinctrl_state *state_gpio;
static struct pinctrl_state *state_otpout;

static u32 original_route_bits;
static u32 original_pmu_wdt_failsafe;
static u32 original_cru_wdt_reset_ext;
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
    "0=stock reset routing, 1=TSADC->PMIC + PMU failsafe WDT + extended WDT reset");

static int set_pmu_wdt_failsafe(bool enable)
{
    u32 write;

    if (!pmugrf_regs)
        return -ENODEV;

    /*
     * PMU_GRF uses write-enable bits in the upper half-word. Touch only
     * SOC_CON3[1] and leave every unrelated control bit unchanged.
     */
    write = PMUGRF_WDT_FAILSAFE_WE;
    if (enable)
        write |= PMUGRF_WDT_FAILSAFE;

    writel_relaxed(write, pmugrf_regs + PMUGRF_SOC_CON3);
    mb();

    return 0;
}

static int set_cru_wdt_reset_extension(bool enable)
{
    u32 val;

    if (!cru_regs)
        return -ENODEV;

    /*
     * CRU_GLB_RST_CON is a normal R/W register on this RK3566 build, not a
     * PMUGRF-style write-mask register. Preserve every unrelated bit.
     */
    val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);
    if (enable)
        val |= CRU_WDT_RESET_EXT;
    else
        val &= ~CRU_WDT_RESET_EXT;

    writel_relaxed(val, cru_regs + CRU_GLB_RST_CON);
    mb();

    return 0;
}

static int restore_cru_wdt_reset_extension(void)
{
    u32 val;

    if (!cru_regs)
        return -ENODEV;

    val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);
    val &= ~CRU_WDT_RESET_EXT;
    val |= original_cru_wdt_reset_ext;
    writel_relaxed(val, cru_regs + CRU_GLB_RST_CON);
    mb();

    return 0;
}

static int restore_pmu_wdt_failsafe(void)
{
    return set_pmu_wdt_failsafe(
        original_pmu_wdt_failsafe != 0);
}

static int set_route(bool to_pmic)
{
    u32 val;
    int ret;

    if (!tsadc_regs ||
        !pmugrf_regs ||
        !cru_regs ||
        !tsadc_pinctrl)
        return -ENODEV;

    if (to_pmic) {
        ret = set_pmu_wdt_failsafe(true);
        if (ret)
            return ret;

        ret = set_cru_wdt_reset_extension(true);
        if (ret) {
            restore_pmu_wdt_failsafe();
            return ret;
        }

        ret = pinctrl_select_state(
            tsadc_pinctrl,
            state_otpout);
        if (ret) {
            restore_cru_wdt_reset_extension();
            restore_pmu_wdt_failsafe();
            return ret;
        }

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

        ret = pinctrl_select_state(
            tsadc_pinctrl,
            state_gpio);
        if (ret)
            return ret;

        ret = restore_cru_wdt_reset_extension();
        if (ret)
            return ret;

        ret = restore_pmu_wdt_failsafe();
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
    if (!tsadc_regs ||
        !pmugrf_regs ||
        !cru_regs ||
        !tsadc_pinctrl)
        return -EAGAIN;

    return set_route(requested == 1);
}

static int recovery_param_get(
    char *buf,
    const struct kernel_param *kp)
{
    u32 tsadc = 0;
    u32 pmu = 0;
    u32 cru = 0;

    (void)kp;

    if (tsadc_regs)
        tsadc = readl_relaxed(
            tsadc_regs + TSADC_INT_EN);
    if (pmugrf_regs)
        pmu = readl_relaxed(
            pmugrf_regs + PMUGRF_SOC_CON3);
    if (cru_regs)
        cru = readl_relaxed(
            cru_regs + CRU_GLB_RST_CON);

    return scnprintf(
        buf,
        PAGE_SIZE,
        "%u int_en=0x%08x original_route=0x%08x "
        "pmu_soc_con3=0x%08x original_wdt_failsafe=0x%08x "
        "cru_glb_rst_con=0x%08x original_wdt_ext=0x%08x\n",
        armed ? 1U : 0U,
        tsadc,
        original_route_bits,
        pmu,
        original_pmu_wdt_failsafe,
        cru,
        original_cru_wdt_reset_ext);
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

    pmugrf_regs = map_compatible(
        "rockchip,rk3568-pmugrf");
    if (!pmugrf_regs) {
        ret = -ENODEV;
        put_device(&pdev->dev);
        goto fail_maps;
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

    original_pmu_wdt_failsafe =
        readl_relaxed(
            pmugrf_regs + PMUGRF_SOC_CON3) &
        PMUGRF_WDT_FAILSAFE;

    original_cru_wdt_reset_ext =
        readl_relaxed(
            cru_regs + CRU_GLB_RST_CON) &
        CRU_WDT_RESET_EXT;

    pr_info(
        "flip_tsadc_recovery: loaded route=0x%08x "
        "wdt_failsafe=%u wdt_ext=%u armed=0\n",
        original_route_bits,
        original_pmu_wdt_failsafe ? 1U : 0U,
        original_cru_wdt_reset_ext ? 1U : 0U);

    return 0;

fail:
    pinctrl_put(tsadc_pinctrl);
    tsadc_pinctrl = NULL;

fail_maps:
    if (cru_regs) {
        iounmap(cru_regs);
        cru_regs = NULL;
    }
    if (pmugrf_regs) {
        iounmap(pmugrf_regs);
        pmugrf_regs = NULL;
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
    if (pmugrf_regs)
        iounmap(pmugrf_regs);
    if (tsadc_regs)
        iounmap(tsadc_regs);

    pr_info("flip_tsadc_recovery: unloaded\n");
}

module_init(flip_tsadc_recovery_init);
module_exit(flip_tsadc_recovery_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(
    "FlipOS RK3566 TSADC + PMU failsafe watchdog recovery helper");
