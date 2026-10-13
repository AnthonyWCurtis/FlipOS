#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/pm_opp.h>

#define FLIP_OPP_STEP_UV 12500UL

#ifndef FLIP_OPP_SAFE_MIN_UV
#define FLIP_OPP_SAFE_MIN_UV 812500UL
#endif

struct flip_opp_adjustment {
    bool active;
    unsigned long freq_khz;
    unsigned long candidate_uv;
    unsigned long stock_target_uv;
    unsigned long stock_min_uv;
    unsigned long stock_max_uv;
};

static struct device *flip_cpu_dev;
static DEFINE_MUTEX(flip_control_lock);
static struct flip_opp_adjustment flip_adjustment;

static int flip_read_opp_target(
    unsigned long freq_khz,
    unsigned long *target_uv)
{
    struct dev_pm_opp *opp;

    if (!flip_cpu_dev || !freq_khz || !target_uv)
        return -EINVAL;

    opp = dev_pm_opp_find_freq_exact(
        flip_cpu_dev,
        freq_khz * 1000UL,
        true);
    if (IS_ERR(opp))
        return PTR_ERR(opp);

    *target_uv = dev_pm_opp_get_voltage(opp);
    dev_pm_opp_put(opp);

    return *target_uv ? 0 : -EINVAL;
}

static int flip_cpu_is_at(unsigned long freq_khz)
{
    unsigned int current_khz = cpufreq_quick_get(0);

    if (!current_khz)
        return -EIO;

    return current_khz == freq_khz ? 1 : 0;
}

static int flip_restore_locked(void)
{
    unsigned long verify_uv = 0;
    int at_target;
    int rc;

    if (!flip_adjustment.active)
        return 0;

    at_target = flip_cpu_is_at(flip_adjustment.freq_khz);
    if (at_target < 0)
        return at_target;
    if (at_target)
        return -EBUSY;

    rc = dev_pm_opp_adjust_voltage(
        flip_cpu_dev,
        flip_adjustment.freq_khz * 1000UL,
        flip_adjustment.stock_target_uv,
        flip_adjustment.stock_min_uv,
        flip_adjustment.stock_max_uv);
    if (rc)
        return rc;

    rc = flip_read_opp_target(
        flip_adjustment.freq_khz,
        &verify_uv);
    if (rc)
        return rc;

    if (verify_uv != flip_adjustment.stock_target_uv)
        return -EIO;

    pr_info(
        "flip_opp_voltage: restored freq_khz=%lu target_uv=%lu min_uv=%lu max_uv=%lu\n",
        flip_adjustment.freq_khz,
        flip_adjustment.stock_target_uv,
        flip_adjustment.stock_min_uv,
        flip_adjustment.stock_max_uv);

    memset(&flip_adjustment, 0, sizeof(flip_adjustment));
    return 0;
}

static int flip_apply_locked(
    unsigned long freq_khz,
    unsigned long candidate_uv,
    unsigned long stock_target_uv,
    unsigned long stock_min_uv,
    unsigned long stock_max_uv)
{
    unsigned long current_target_uv = 0;
    unsigned long verify_uv = 0;
    int at_target;
    int rc;

    if (flip_adjustment.active)
        return -EBUSY;

    if (!freq_khz ||
        candidate_uv < FLIP_OPP_SAFE_MIN_UV ||
        candidate_uv >= stock_target_uv ||
        stock_min_uv > stock_target_uv ||
        stock_target_uv > stock_max_uv ||
        candidate_uv % FLIP_OPP_STEP_UV ||
        stock_target_uv % FLIP_OPP_STEP_UV ||
        stock_min_uv % FLIP_OPP_STEP_UV ||
        stock_max_uv % FLIP_OPP_STEP_UV)
        return -EINVAL;

    at_target = flip_cpu_is_at(freq_khz);
    if (at_target < 0)
        return at_target;
    if (at_target)
        return -EBUSY;

    rc = flip_read_opp_target(freq_khz, &current_target_uv);
    if (rc)
        return rc;

    if (current_target_uv != stock_target_uv)
        return -ESTALE;

    rc = dev_pm_opp_adjust_voltage(
        flip_cpu_dev,
        freq_khz * 1000UL,
        candidate_uv,
        candidate_uv,
        candidate_uv);
    if (rc)
        return rc;

    rc = flip_read_opp_target(freq_khz, &verify_uv);
    if (rc)
        goto rollback;

    if (verify_uv != candidate_uv) {
        rc = -EIO;
        goto rollback;
    }

    flip_adjustment.active = true;
    flip_adjustment.freq_khz = freq_khz;
    flip_adjustment.candidate_uv = candidate_uv;
    flip_adjustment.stock_target_uv = stock_target_uv;
    flip_adjustment.stock_min_uv = stock_min_uv;
    flip_adjustment.stock_max_uv = stock_max_uv;

    pr_info(
        "flip_opp_voltage: adjusted table freq_khz=%lu stock_uv=%lu candidate_uv=%lu current_cpu_khz=%u\n",
        freq_khz,
        stock_target_uv,
        candidate_uv,
        cpufreq_quick_get(0));

    return 0;

rollback:
    dev_pm_opp_adjust_voltage(
        flip_cpu_dev,
        freq_khz * 1000UL,
        stock_target_uv,
        stock_min_uv,
        stock_max_uv);
    return rc;
}

static int flip_control_set(
    const char *value,
    const struct kernel_param *kp)
{
    unsigned long freq_khz = 0;
    unsigned long candidate_uv = 0;
    unsigned long stock_target_uv = 0;
    unsigned long stock_min_uv = 0;
    unsigned long stock_max_uv = 0;
    int rc;

    (void)kp;

    mutex_lock(&flip_control_lock);

    if (sysfs_streq(value, "restore")) {
        rc = flip_restore_locked();
        mutex_unlock(&flip_control_lock);
        return rc;
    }

    if (sscanf(
            value,
            "apply,%lu,%lu,%lu,%lu,%lu",
            &freq_khz,
            &candidate_uv,
            &stock_target_uv,
            &stock_min_uv,
            &stock_max_uv) != 5) {
        mutex_unlock(&flip_control_lock);
        return -EINVAL;
    }

    rc = flip_apply_locked(
        freq_khz,
        candidate_uv,
        stock_target_uv,
        stock_min_uv,
        stock_max_uv);

    mutex_unlock(&flip_control_lock);
    return rc;
}

static int flip_control_get(
    char *buffer,
    const struct kernel_param *kp)
{
    unsigned int current_khz;
    unsigned long target_uv = 0;
    int n;

    (void)kp;

    mutex_lock(&flip_control_lock);

    current_khz = cpufreq_quick_get(0);

    if (flip_adjustment.active)
        flip_read_opp_target(
            flip_adjustment.freq_khz,
            &target_uv);

    n = scnprintf(
        buffer,
        PAGE_SIZE,
        "active=%d freq_khz=%lu candidate_uv=%lu "
        "stock_target_uv=%lu stock_min_uv=%lu stock_max_uv=%lu "
        "current_cpu_khz=%u table_target_uv=%lu\n",
        flip_adjustment.active ? 1 : 0,
        flip_adjustment.freq_khz,
        flip_adjustment.candidate_uv,
        flip_adjustment.stock_target_uv,
        flip_adjustment.stock_min_uv,
        flip_adjustment.stock_max_uv,
        current_khz,
        target_uv);

    mutex_unlock(&flip_control_lock);
    return n;
}

static const struct kernel_param_ops flip_control_ops = {
    .set = flip_control_set,
    .get = flip_control_get,
};

module_param_cb(control, &flip_control_ops, NULL, 0600);
MODULE_PARM_DESC(
    control,
    "apply,FREQ_KHZ,CANDIDATE_UV,STOCK_TARGET_UV,STOCK_MIN_UV,STOCK_MAX_UV or restore");

static int __init flip_opp_voltage_init(void)
{
    flip_cpu_dev = get_cpu_device(0);
    if (!flip_cpu_dev)
        return -ENODEV;

    memset(&flip_adjustment, 0, sizeof(flip_adjustment));

    pr_info(
        "flip_opp_voltage: loaded read/write table backend; no voltage adjusted\n");
    return 0;
}

static void __exit flip_opp_voltage_exit(void)
{
    int rc = 0;

    mutex_lock(&flip_control_lock);

    if (flip_adjustment.active) {
        rc = flip_restore_locked();
        if (rc)
            pr_err(
                "flip_opp_voltage: unload restore failed rc=%d; reboot required for stock OPP table\n",
                rc);
    }

    mutex_unlock(&flip_control_lock);

    pr_info("flip_opp_voltage: unloaded rc=%d\n", rc);
}

module_init(flip_opp_voltage_init);
module_exit(flip_opp_voltage_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FlipOS");
MODULE_DESCRIPTION("FlipOS runtime CPU OPP voltage-table backend");
