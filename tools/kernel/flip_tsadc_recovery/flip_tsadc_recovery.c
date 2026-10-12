#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/mfd/rk808.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define TSADC_AUTO_CON 0x04
#define TSADC_INT_EN 0x08
#define TSADC_INT_PD 0x0c
#define TSADC_TSHUT_POLARITY BIT(8)
#define SHUT_GPIO_BITS (BIT(4) | BIT(5))
#define SHUT_CRU_BITS  (BIT(8) | BIT(9))

#define CRU_GLB_RST_CON 0x00dc
#define CRU_WDT_RESET_EXT BIT(7)
#define CRU_PMU_SRST_WDT_DISABLE BIT(4)

/*
 * GPIO0_A2 is the RK3566 PMIC_SLEEP / TSADC_SHUT_M1 pin. The Miyoo DT
 * connects that pin to RK817 SLEEP, which is the path Rockchip documents
 * for a full PMIC restart. PMUGRF IOMUX registers use upper-16 write masks.
 */
#define PMUGRF_GPIO0A_IOMUX_L 0x0000
#define GPIO0A2_MUX_SHIFT 8
#define GPIO0A2_MUX_MASK (0x7U << GPIO0A2_MUX_SHIFT)
#define GPIO0A2_TSADC_SHUT_M1 (0x2U << GPIO0A2_MUX_SHIFT)
#define GPIO0A2_MUX_WRITE_MASK (GPIO0A2_MUX_MASK << 16)

#define RK817_RECOVERY_MASK \
    (RK817_SLPPIN_FUNC_MSK | RK817_SLPPOL_MSK)
#define RK817_RECOVERY_VALUE \
    (SLPPIN_RST_FUN | RK817_SLPPOL_L)

static void __iomem *tsadc_regs;
static void __iomem *cru_regs;
static void __iomem *pmugrf_regs;

static struct pinctrl *tsadc_pinctrl;
static struct pinctrl_state *state_gpio;
static struct pinctrl_state *state_otpout;

static struct i2c_client *rk817_client;
static struct rk808 *rk817;

static u32 original_route_bits;
static u32 original_cru_wdt_reset_bits;
static u32 original_gpio0a2_mux;
static u32 original_rk817_recovery_bits;
static bool armed;
static bool module_ref_held;

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

static int set_gpio0a2_mux(u32 mux)
{
    u32 val;

    if (!pmugrf_regs)
        return -ENODEV;

    mux &= GPIO0A2_MUX_MASK;

    /*
     * PMUGRF IOMUX uses the Rockchip upper-half write-mask convention.
     * Write only GPIO0_A2's three mux bits and verify the live value.
     */
    writel_relaxed(
        GPIO0A2_MUX_WRITE_MASK | mux,
        pmugrf_regs + PMUGRF_GPIO0A_IOMUX_L);
    mb();

    val = readl_relaxed(
        pmugrf_regs + PMUGRF_GPIO0A_IOMUX_L);

    return (val & GPIO0A2_MUX_MASK) == mux
        ? 0
        : -EIO;
}

static int set_rk817_recovery_bits(u32 value)
{
    unsigned int live = 0;
    int ret;

    if (!rk817 || !rk817->regmap)
        return -ENODEV;

    value &= RK817_RECOVERY_MASK;

    ret = regmap_update_bits(
        rk817->regmap,
        RK817_SYS_CFG(3),
        RK817_RECOVERY_MASK,
        value);
    if (ret)
        return ret;

    ret = regmap_read(
        rk817->regmap,
        RK817_SYS_CFG(3),
        &live);
    if (ret)
        return ret;

    return (live & RK817_RECOVERY_MASK) == value
        ? 0
        : -EIO;
}

static int set_rk817_sleep_disabled(void)
{
    unsigned int live = 0;
    int ret;

    if (!rk817 || !rk817->regmap)
        return -ENODEV;

    ret = regmap_update_bits(
        rk817->regmap,
        RK817_SYS_CFG(3),
        RK817_SLPPIN_FUNC_MSK,
        SLPPIN_NULL_FUN);
    if (ret)
        return ret;

    ret = regmap_read(
        rk817->regmap,
        RK817_SYS_CFG(3),
        &live);
    if (ret)
        return ret;

    return (live & RK817_SLPPIN_FUNC_MSK) == SLPPIN_NULL_FUN
        ? 0
        : -EIO;
}

static int validate_tshut_idle_low_active(void)
{
    u32 auto_con;
    u32 pending;

    if (!tsadc_regs)
        return -ENODEV;

    auto_con = readl_relaxed(
        tsadc_regs + TSADC_AUTO_CON);
    pending = readl_relaxed(
        tsadc_regs + TSADC_INT_PD);

    /*
     * RK817 recovery is active-low. Never connect the PMIC restart input to
     * TSADC unless TSHUT is also active-low and neither source is currently
     * asserting/latching TSHUT.
     */
    if (auto_con & TSADC_TSHUT_POLARITY)
        return -EINVAL;
    if (pending & SHUT_GPIO_BITS)
        return -EBUSY;

    return 0;
}

static int hold_module_while_armed(void)
{
    if (module_ref_held)
        return 0;

    if (!try_module_get(THIS_MODULE))
        return -ENODEV;

    module_ref_held = true;
    return 0;
}

static void release_module_after_restore(void)
{
    if (!module_ref_held)
        return;

    module_ref_held = false;
    module_put(THIS_MODULE);
}

static int restore_tsadc_route(void)
{
    u32 val;

    val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
    val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);
    val |= original_route_bits;
    writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
    mb();

    val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
    return (val & (SHUT_GPIO_BITS | SHUT_CRU_BITS)) ==
           original_route_bits
        ? 0
        : -EIO;
}

static int restore_recovery_path(void)
{
    int first_error = 0;
    int ret;

    /*
     * Disable the RK817 SLEEP action before changing any SoC mux. That makes
     * restoration safe even if a mux transition briefly drives the line low.
     */
    ret = set_rk817_sleep_disabled();
    if (ret && !first_error)
        first_error = ret;

    ret = set_gpio0a2_mux(original_gpio0a2_mux);
    if (ret && !first_error)
        first_error = ret;

    ret = pinctrl_select_state(
        tsadc_pinctrl,
        state_gpio);
    if (ret && !first_error)
        first_error = ret;

    ret = restore_tsadc_route();
    if (ret && !first_error)
        first_error = ret;

    ret = restore_cru_wdt_recovery();
    if (ret && !first_error)
        first_error = ret;

    ret = set_rk817_recovery_bits(
        original_rk817_recovery_bits);
    if (ret && !first_error)
        first_error = ret;

    if (!first_error) {
        armed = false;
        release_module_after_restore();
    }

    return first_error;
}

static int set_route(bool to_pmic)
{
    u32 val;
    int ret;

    if (!tsadc_regs ||
        !cru_regs ||
        !pmugrf_regs ||
        !tsadc_pinctrl ||
        !rk817 ||
        !rk817->regmap)
        return -ENODEV;

    if (to_pmic && armed)
        return 0;
    if (!to_pmic && !armed)
        return 0;

    if (!to_pmic)
        return restore_recovery_path();

    ret = validate_tshut_idle_low_active();
    if (ret)
        return ret;

    /*
     * First make the RK817 SLEEP input inert. The stock PMIC_SLEEP signal is
     * low while the system is awake, whereas the recovery function is
     * active-low; changing function before changing the SoC mux could
     * therefore restart the PMIC immediately.
     */
    ret = set_rk817_sleep_disabled();
    if (ret)
        return ret;

    ret = set_cru_wdt_recovery(true);
    if (ret)
        goto fail_restore;

    /*
     * Crucial ordering: TSADC_INT_EN documents that a disabled TSHUT GPIO
     * output is held LOW. Enable the route while GPIO0_A2 is still disconnected
     * from RK817, then mux the pin only after the signal is known idle.
     */
    val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
    val &= ~SHUT_CRU_BITS;
    val |= SHUT_GPIO_BITS;
    writel_relaxed(val, tsadc_regs + TSADC_INT_EN);
    mb();

    val = readl_relaxed(tsadc_regs + TSADC_INT_EN);
    if ((val & (SHUT_GPIO_BITS | SHUT_CRU_BITS)) !=
        SHUT_GPIO_BITS) {
        ret = -EIO;
        goto fail_restore;
    }

    ret = pinctrl_select_state(
        tsadc_pinctrl,
        state_otpout);
    if (ret)
        goto fail_restore;

    ret = set_gpio0a2_mux(
        GPIO0A2_TSADC_SHUT_M1);
    if (ret)
        goto fail_restore;

    /*
     * Re-check after the mux switch and before the PMIC input becomes live.
     * If a TSHUT condition appeared while arming, fail closed instead of
     * turning it into an unexpected PMIC restart.
     */
    ret = validate_tshut_idle_low_active();
    if (ret)
        goto fail_restore;

    ret = set_rk817_recovery_bits(
        RK817_RECOVERY_VALUE);
    if (ret)
        goto fail_restore;

    ret = hold_module_while_armed();
    if (ret)
        goto fail_restore;

    armed = true;
    return 0;

fail_restore:
    {
        int restore_ret = restore_recovery_path();
        if (restore_ret) {
            /*
             * A partial arm is a hazard state. Keep armed=true so unload or an
             * explicit recovery=0 retries restoration instead of pretending
             * that the hardware is back at its original routing.
             */
            armed = true;
            (void)hold_module_while_armed();
            return restore_ret;
        }
    }
    return ret;
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
        !cru_regs ||
        !pmugrf_regs ||
        !tsadc_pinctrl ||
        !rk817 ||
        !rk817->regmap)
        return -EAGAIN;

    return set_route(requested == 1);
}

static int recovery_param_get(
    char *buf,
    const struct kernel_param *kp)
{
    u32 tsadc = 0;
    u32 auto_con = 0;
    u32 pending = 0;
    u32 cru = 0;
    u32 gpio0a2_mux = 0;
    unsigned int rk817_cfg3 = 0;

    (void)kp;

    if (tsadc_regs) {
        tsadc = readl_relaxed(
            tsadc_regs + TSADC_INT_EN);
        auto_con = readl_relaxed(
            tsadc_regs + TSADC_AUTO_CON);
        pending = readl_relaxed(
            tsadc_regs + TSADC_INT_PD);
    }
    if (cru_regs)
        cru = readl_relaxed(
            cru_regs + CRU_GLB_RST_CON);
    if (pmugrf_regs)
        gpio0a2_mux = readl_relaxed(
            pmugrf_regs + PMUGRF_GPIO0A_IOMUX_L) &
            GPIO0A2_MUX_MASK;
    if (rk817 && rk817->regmap)
        (void)regmap_read(
            rk817->regmap,
            RK817_SYS_CFG(3),
            &rk817_cfg3);

    return scnprintf(
        buf,
        PAGE_SIZE,
        "armed=%u int_en=0x%08x tshut_low_active=%u "
        "tshut_pending=0x%08x original_route=0x%08x "
        "gpio0a2_mux=0x%03x original_gpio0a2_mux=0x%03x "
        "rk817_sys_cfg3=0x%02x rk817_restart_active_low=%u "
        "original_rk817_recovery=0x%02x "
        "cru_glb_rst_con=0x%08x wdt_ext=%u pmu_wdt_reset=%u "
        "original_cru_wdt_bits=0x%08x\n",
        armed ? 1U : 0U,
        tsadc,
        (auto_con & TSADC_TSHUT_POLARITY) ? 0U : 1U,
        (u32)(pending & SHUT_GPIO_BITS),
        original_route_bits,
        gpio0a2_mux,
        original_gpio0a2_mux,
        rk817_cfg3,
        (rk817_cfg3 & RK817_RECOVERY_MASK) ==
            RK817_RECOVERY_VALUE ? 1U : 0U,
        original_rk817_recovery_bits,
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
    unsigned int rk817_cfg3 = 0;
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

    pmugrf_regs = map_compatible(
        "rockchip,rk3568-pmugrf");
    if (!pmugrf_regs) {
        ret = -ENODEV;
        put_device(&pdev->dev);
        goto fail_maps;
    }

    np = of_find_compatible_node(
        NULL,
        NULL,
        "rockchip,rk817");
    if (!np) {
        ret = -ENODEV;
        put_device(&pdev->dev);
        goto fail_maps;
    }

    rk817_client = of_find_i2c_device_by_node(np);
    of_node_put(np);
    if (!rk817_client) {
        ret = -EPROBE_DEFER;
        put_device(&pdev->dev);
        goto fail_maps;
    }

    rk817 = i2c_get_clientdata(rk817_client);
    if (!rk817 ||
        rk817->variant != RK817_ID ||
        !rk817->regmap) {
        ret = -EPROBE_DEFER;
        put_device(&pdev->dev);
        goto fail_client;
    }

    tsadc_pinctrl = pinctrl_get(&pdev->dev);
    put_device(&pdev->dev);
    if (IS_ERR(tsadc_pinctrl)) {
        ret = PTR_ERR(tsadc_pinctrl);
        tsadc_pinctrl = NULL;
        goto fail_client;
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

    original_gpio0a2_mux =
        readl_relaxed(
            pmugrf_regs + PMUGRF_GPIO0A_IOMUX_L) &
        GPIO0A2_MUX_MASK;

    ret = regmap_read(
        rk817->regmap,
        RK817_SYS_CFG(3),
        &rk817_cfg3);
    if (ret)
        goto fail;

    original_rk817_recovery_bits =
        rk817_cfg3 & RK817_RECOVERY_MASK;

    pr_info(
        "flip_tsadc_recovery: loaded route=0x%08x "
        "gpio0a2_mux=0x%03x rk817_recovery=0x%02x "
        "wdt_ext=%u pmu_wdt_reset=%u armed=0\n",
        original_route_bits,
        original_gpio0a2_mux,
        original_rk817_recovery_bits,
        (original_cru_wdt_reset_bits &
         CRU_WDT_RESET_EXT) ? 1U : 0U,
        (original_cru_wdt_reset_bits &
         CRU_PMU_SRST_WDT_DISABLE) ? 0U : 1U);

    return 0;

fail:
    pinctrl_put(tsadc_pinctrl);
    tsadc_pinctrl = NULL;

fail_client:
    if (rk817_client) {
        put_device(&rk817_client->dev);
        rk817_client = NULL;
        rk817 = NULL;
    }

fail_maps:
    if (pmugrf_regs) {
        iounmap(pmugrf_regs);
        pmugrf_regs = NULL;
    }
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
        (void)set_route(false);

    if (tsadc_pinctrl)
        pinctrl_put(tsadc_pinctrl);
    if (rk817_client)
        put_device(&rk817_client->dev);
    if (pmugrf_regs)
        iounmap(pmugrf_regs);
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
    "FlipOS RK3566 watchdog -> TSADC_SHUT_M1 -> RK817 restart recovery");
