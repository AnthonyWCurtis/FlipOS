// SPDX-License-Identifier: GPL-2.0
/*
 * FlipOS RK817 charge-current limiter.
 *
 * Deliberately narrow by design: only PMIC_CHRG_OUT[2:0] is touched, matching
 * Rockchip's rk817_charger.c CHRG_CUR_SEL field.  Charge voltage, input limits,
 * power-path and charger-enable bits are never modified here.
 */
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define RK817_CHRG_OUT_REG 0xE4
#define RK817_CHRG_CUR_MASK 0x07

static struct i2c_client *flip_client;
static unsigned int stock_selector;
static unsigned int restore_selector;
static bool override_active;
static int stock_current_ma;
static int current_limit_ma = -1;

static int selector_to_ma(unsigned int selector)
{
    static const int current_ma[8] = {
        1000, 1500, 2000, 2500, 2750, 3000, 3500, 500
    };

    return selector < ARRAY_SIZE(current_ma)
        ? current_ma[selector]
        : -EINVAL;
}

static int ma_to_selector(int ma)
{
    switch (ma) {
    case 500:  return 7;
    case 1000: return 0;
    case 1500: return 1;
    case 2000: return 2;
    case 2500: return 3;
    case 2750: return 4;
    case 3000: return 5;
    case 3500: return 6;
    default:   return -EINVAL;
    }
}

static int apply_current_limit(int ma)
{
    int requested_selector;
    int current_value;
    int current_selector;
    int baseline_selector;
    int baseline_ma;
    int target_selector;
    int target_ma;

    if (!flip_client)
        return -ENODEV;

    current_value = i2c_smbus_read_byte_data(
        flip_client, RK817_CHRG_OUT_REG);
    if (current_value < 0)
        return current_value;

    current_selector = current_value & RK817_CHRG_CUR_MASK;

    if (ma < 0) {
        if (!override_active)
            return 0;

        current_value = (current_value & ~RK817_CHRG_CUR_MASK) |
            (restore_selector & RK817_CHRG_CUR_MASK);
        if (i2c_smbus_write_byte_data(
                flip_client,
                RK817_CHRG_OUT_REG,
                (u8)current_value) < 0)
            return -EIO;

        override_active = false;
        return 0;
    }

    requested_selector = ma_to_selector(ma);
    if (requested_selector < 0)
        return requested_selector;

    /*
     * Treat positive values as a cap, never a target.  If Rockchip's charger
     * driver is already using a lower current, leave it alone.  Once capped,
     * keep the original pre-cap selector so later thermal steps can relax
     * back toward that value without ever exceeding it.
     */
    baseline_selector = override_active
        ? (int)restore_selector
        : current_selector;
    baseline_ma = selector_to_ma((unsigned int)baseline_selector);
    if (baseline_ma < 0)
        return baseline_ma;

    target_ma = baseline_ma < ma ? baseline_ma : ma;
    target_selector = ma_to_selector(target_ma);
    if (target_selector < 0)
        return target_selector;

    if (!override_active && target_selector == current_selector)
        return 0;

    if (!override_active) {
        restore_selector = (unsigned int)current_selector;
        override_active = true;
    }

    current_value = (current_value & ~RK817_CHRG_CUR_MASK) |
        ((unsigned int)target_selector & RK817_CHRG_CUR_MASK);

    return i2c_smbus_write_byte_data(
        flip_client,
        RK817_CHRG_OUT_REG,
        (u8)current_value);
}

static int current_set(const char *value, const struct kernel_param *kp)
{
    int requested;
    int ret = kstrtoint(value, 0, &requested);

    if (ret)
        return ret;

    if (requested != -1 && ma_to_selector(requested) < 0)
        return -EINVAL;

    if (flip_client) {
        ret = apply_current_limit(requested);
        if (ret)
            return ret;
    }

    current_limit_ma = requested;
    return 0;
}

static int current_get(char *buffer, const struct kernel_param *kp)
{
    return scnprintf(buffer, PAGE_SIZE, "%d\n", current_limit_ma);
}

static const struct kernel_param_ops current_ops = {
    .set = current_set,
    .get = current_get,
};

module_param_cb(current_limit_ma, &current_ops, &current_limit_ma, 0644);
MODULE_PARM_DESC(current_limit_ma,
    "RK817 charge current limit in mA; -1 restores boot/stock selector");
module_param(stock_current_ma, int, 0444);
MODULE_PARM_DESC(stock_current_ma,
    "Charge current represented by RK817 CHRG_CUR_SEL when module loaded");

static int __init flip_rk817_charger_init(void)
{
    struct device_node *node;
    struct i2c_client *client;
    unsigned int value;
    int ret;

    node = of_find_compatible_node(NULL, NULL, "rockchip,rk817");
    if (!node)
        return -ENODEV;

    client = of_find_i2c_device_by_node(node);
    of_node_put(node);
    if (!client)
        return -EPROBE_DEFER;

    flip_client = client;

    ret = i2c_smbus_read_byte_data(
        flip_client,
        RK817_CHRG_OUT_REG);
    if (ret < 0) {
        put_device(&client->dev);
        flip_client = NULL;
        return ret;
    }
    value = (unsigned int)ret;

    stock_selector = value & RK817_CHRG_CUR_MASK;
    stock_current_ma = selector_to_ma(stock_selector);
    if (stock_current_ma < 0) {
        put_device(&client->dev);
        flip_client = NULL;
        return -EINVAL;
    }

    current_limit_ma = -1;
    override_active = false;
    restore_selector = stock_selector;
    pr_info("flip_rk817_charger: stock current=%dmA selector=%u\n",
        stock_current_ma, stock_selector);
    return 0;
}

static void __exit flip_rk817_charger_exit(void)
{
    int ret;

    if (!flip_client)
        return;

    ret = apply_current_limit(-1);
    if (ret)
        pr_err("flip_rk817_charger: failed to release charge-current cap: %d\n", ret);
    else
        pr_info("flip_rk817_charger: charge-current cap released\n");

    put_device(&flip_client->dev);
    flip_client = NULL;
}

module_init(flip_rk817_charger_init);
module_exit(flip_rk817_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FlipOS");
MODULE_DESCRIPTION("FlipOS RK817 narrow runtime charge-current limiter");
