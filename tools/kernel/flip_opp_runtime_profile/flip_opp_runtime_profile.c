#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/pm_opp.h>

#define FLIP_RUNTIME_MAX_OPPS 16
#define FLIP_OPP_STEP_UV 12500UL
#define FLIP_OPP_SAFE_MIN_UV 812500UL

struct flip_runtime_adjustment {
    bool active;
    unsigned long freq_khz;
    unsigned long applied_uv;
    unsigned long stock_target_uv;
    unsigned long stock_min_uv;
    unsigned long stock_max_uv;
};

static struct device *flip_cpu_dev;
static DEFINE_MUTEX(flip_control_lock);
static struct flip_runtime_adjustment flip_adjustments[FLIP_RUNTIME_MAX_OPPS];

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

static int flip_find_slot(unsigned long freq_khz)
{
    int i;

    for (i = 0; i < FLIP_RUNTIME_MAX_OPPS; ++i) {
        if (flip_adjustments[i].active &&
            flip_adjustments[i].freq_khz == freq_khz)
            return i;
    }

    return -1;
}

static int flip_find_free_slot(void)
{
    int i;

    for (i = 0; i < FLIP_RUNTIME_MAX_OPPS; ++i) {
        if (!flip_adjustments[i].active)
            return i;
    }

    return -ENOSPC;
}

static int flip_restore_one_locked(int index)
{
    struct flip_runtime_adjustment *adj;
    unsigned long verify_uv = 0;
    int at_target;
    int rc;

    if (index < 0 || index >= FLIP_RUNTIME_MAX_OPPS)
        return -EINVAL;

    adj = &flip_adjustments[index];
    if (!adj->active)
        return 0;

    at_target = flip_cpu_is_at(adj->freq_khz);
    if (at_target < 0)
        return at_target;
    if (at_target)
        return -EBUSY;

    rc = dev_pm_opp_adjust_voltage(
        flip_cpu_dev,
        adj->freq_khz * 1000UL,
        adj->stock_target_uv,
        adj->stock_min_uv,
        adj->stock_max_uv);
    if (rc)
        return rc;

    rc = flip_read_opp_target(adj->freq_khz, &verify_uv);
    if (rc)
        return rc;
    if (verify_uv != adj->stock_target_uv)
        return -EIO;

    pr_info(
        "flip_opp_runtime_profile: restored freq_khz=%lu target_uv=%lu\n",
        adj->freq_khz,
        adj->stock_target_uv);

    memset(adj, 0, sizeof(*adj));
    return 0;
}

static int flip_restore_all_locked(void)
{
    int i;
    int first_error = 0;

    for (i = FLIP_RUNTIME_MAX_OPPS - 1; i >= 0; --i) {
        int rc = flip_restore_one_locked(i);
        if (rc && !first_error)
            first_error = rc;
    }

    return first_error;
}

static int flip_apply_one_locked(
    unsigned long freq_khz,
    unsigned long optimized_uv,
    unsigned long stock_target_uv,
    unsigned long stock_min_uv,
    unsigned long stock_max_uv)
{
    unsigned long current_target_uv = 0;
    unsigned long verify_uv = 0;
    int at_target;
    int slot;
    int rc;

    if (!freq_khz ||
        optimized_uv < FLIP_OPP_SAFE_MIN_UV ||
        optimized_uv >= stock_target_uv ||
        stock_min_uv > stock_target_uv ||
        stock_target_uv > stock_max_uv ||
        optimized_uv % FLIP_OPP_STEP_UV ||
        stock_target_uv % FLIP_OPP_STEP_UV ||
        stock_min_uv % FLIP_OPP_STEP_UV ||
        stock_max_uv % FLIP_OPP_STEP_UV)
        return -EINVAL;

    if (flip_find_slot(freq_khz) >= 0)
        return -EEXIST;

    slot = flip_find_free_slot();
    if (slot < 0)
        return slot;

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
        optimized_uv,
        optimized_uv,
        optimized_uv);
    if (rc)
        return rc;

    rc = flip_read_opp_target(freq_khz, &verify_uv);
    if (rc)
        goto rollback;
    if (verify_uv != optimized_uv) {
        rc = -EIO;
        goto rollback;
    }

    flip_adjustments[slot].active = true;
    flip_adjustments[slot].freq_khz = freq_khz;
    flip_adjustments[slot].applied_uv = optimized_uv;
    flip_adjustments[slot].stock_target_uv = stock_target_uv;
    flip_adjustments[slot].stock_min_uv = stock_min_uv;
    flip_adjustments[slot].stock_max_uv = stock_max_uv;

    pr_info(
        "flip_opp_runtime_profile: applied freq_khz=%lu stock_uv=%lu optimized_uv=%lu current_cpu_khz=%u\n",
        freq_khz,
        stock_target_uv,
        optimized_uv,
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
    unsigned long optimized_uv = 0;
    unsigned long stock_target_uv = 0;
    unsigned long stock_min_uv = 0;
    unsigned long stock_max_uv = 0;
    int rc;

    (void)kp;

    mutex_lock(&flip_control_lock);

    if (sysfs_streq(value, "restore-all")) {
        rc = flip_restore_all_locked();
        mutex_unlock(&flip_control_lock);
        return rc;
    }

    if (sscanf(
            value,
            "apply,%lu,%lu,%lu,%lu,%lu",
            &freq_khz,
            &optimized_uv,
            &stock_target_uv,
            &stock_min_uv,
            &stock_max_uv) != 5) {
        mutex_unlock(&flip_control_lock);
        return -EINVAL;
    }

    rc = flip_apply_one_locked(
        freq_khz,
        optimized_uv,
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
    int i;
    int active = 0;
    int n = 0;

    (void)kp;

    mutex_lock(&flip_control_lock);

    for (i = 0; i < FLIP_RUNTIME_MAX_OPPS; ++i)
        active += flip_adjustments[i].active ? 1 : 0;

    n += scnprintf(
        buffer + n,
        PAGE_SIZE - n,
        "active_count=%d current_cpu_khz=%u",
        active,
        cpufreq_quick_get(0));

    for (i = 0; i < FLIP_RUNTIME_MAX_OPPS && n < PAGE_SIZE - 80; ++i) {
        if (!flip_adjustments[i].active)
            continue;

        n += scnprintf(
            buffer + n,
            PAGE_SIZE - n,
            " opp%d=%lu:%lu",
            i,
            flip_adjustments[i].freq_khz,
            flip_adjustments[i].applied_uv);
    }

    n += scnprintf(buffer + n, PAGE_SIZE - n, "\n");

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
    "apply,FREQ_KHZ,OPTIMIZED_UV,STOCK_TARGET_UV,STOCK_MIN_UV,STOCK_MAX_UV or restore-all");

static int __init flip_runtime_profile_init(void)
{
    flip_cpu_dev = get_cpu_device(0);
    if (!flip_cpu_dev)
        return -ENODEV;

    memset(flip_adjustments, 0, sizeof(flip_adjustments));

    pr_info(
        "flip_opp_runtime_profile: loaded; stock OPP table unchanged\n");
    return 0;
}

static void __exit flip_runtime_profile_exit(void)
{
    int rc;

    mutex_lock(&flip_control_lock);
    rc = flip_restore_all_locked();
    mutex_unlock(&flip_control_lock);

    if (rc)
        pr_err(
            "flip_opp_runtime_profile: unload restore failed rc=%d; reboot restores stock OPP table\n",
            rc);

    pr_info("flip_opp_runtime_profile: unloaded rc=%d\n", rc);
}

module_init(flip_runtime_profile_init);
module_exit(flip_runtime_profile_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FlipOS");
MODULE_DESCRIPTION("FlipOS published CPU OPP runtime profile backend");
