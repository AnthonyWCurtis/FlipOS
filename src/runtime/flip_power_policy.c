#include "flip_power_policy.h"

#include <limits.h>
#include <string.h>

static long min_positive(long a, long b)
{
    if (a <= 0)
        return b;
    if (b <= 0)
        return a;
    return a < b ? a : b;
}

void flip_charge_policy_init(
    flip_charge_policy_t *policy)
{
    if (!policy)
        return;
    memset(policy, 0, sizeof(*policy));
    policy->mode = FLIP_CHARGE_MODE_IDLE;
}

static flip_charge_mode_t choose_mode(
    flip_charge_mode_t previous,
    int temp_deci_c)
{
    if (previous == FLIP_CHARGE_MODE_PAUSED_COLD) {
        if (temp_deci_c < FLIP_CHARGE_PAUSE_COLD_EXIT_DECI_C)
            return FLIP_CHARGE_MODE_PAUSED_COLD;
    } else if (temp_deci_c <= FLIP_CHARGE_PAUSE_COLD_ENTER_DECI_C) {
        return FLIP_CHARGE_MODE_PAUSED_COLD;
    }

    if (previous == FLIP_CHARGE_MODE_PAUSED_HOT) {
        if (temp_deci_c >= FLIP_CHARGE_PAUSE_HOT_EXIT_DECI_C)
            return FLIP_CHARGE_MODE_PAUSED_HOT;
    } else if (temp_deci_c >= FLIP_CHARGE_PAUSE_HOT_ENTER_DECI_C) {
        return FLIP_CHARGE_MODE_PAUSED_HOT;
    }

    if (previous == FLIP_CHARGE_MODE_HOT) {
        if (temp_deci_c >= FLIP_CHARGE_HOT_EXIT_DECI_C)
            return FLIP_CHARGE_MODE_HOT;
    } else if (temp_deci_c >= FLIP_CHARGE_HOT_ENTER_DECI_C) {
        return FLIP_CHARGE_MODE_HOT;
    }

    if (previous == FLIP_CHARGE_MODE_WARM) {
        if (temp_deci_c >= FLIP_CHARGE_WARM_EXIT_DECI_C)
            return FLIP_CHARGE_MODE_WARM;
    } else if (temp_deci_c >= FLIP_CHARGE_WARM_ENTER_DECI_C) {
        return FLIP_CHARGE_MODE_WARM;
    }

    return FLIP_CHARGE_MODE_NORMAL;
}

int flip_charge_policy_evaluate(
    flip_charge_policy_t *policy,
    const flip_battery_snapshot_t *battery,
    long stock_current_limit_ua,
    flip_charge_decision_t *decision)
{
    if (!policy || !battery || !decision)
        return -1;

    memset(decision, 0, sizeof(*decision));

    if (!battery->external_power_valid ||
        !battery->external_power_online) {
        policy->mode = FLIP_CHARGE_MODE_IDLE;
        decision->mode = FLIP_CHARGE_MODE_IDLE;
        return 0;
    }

    decision->external_power_online = 1;

    /*
     * Temperature is the safety input. If it is unavailable, preserve the
     * board's stock charger setup rather than guessing a replacement limit.
     */
    if (!battery->temp_valid) {
        policy->mode = FLIP_CHARGE_MODE_NORMAL;
        decision->mode = FLIP_CHARGE_MODE_NORMAL;
        decision->charger_enabled = 1;
        if (stock_current_limit_ua > 0) {
            decision->current_limit_ua = stock_current_limit_ua;
            decision->current_limit_valid = 1;
        }
        return 0;
    }

    flip_charge_mode_t mode =
        choose_mode(policy->mode, battery->temp_deci_c);

    policy->mode = mode;
    decision->mode = mode;

    if (mode == FLIP_CHARGE_MODE_PAUSED_COLD ||
        mode == FLIP_CHARGE_MODE_PAUSED_HOT) {
        decision->charger_enabled = 0;
        return 0;
    }

    decision->charger_enabled = 1;

    if (stock_current_limit_ua <= 0)
        return 0;

    long cap = stock_current_limit_ua;
    if (mode == FLIP_CHARGE_MODE_WARM)
        cap = min_positive(stock_current_limit_ua, FLIP_CHARGE_WARM_CAP_UA);
    else if (mode == FLIP_CHARGE_MODE_HOT)
        cap = min_positive(stock_current_limit_ua, FLIP_CHARGE_HOT_CAP_UA);

    decision->current_limit_ua = cap;
    decision->current_limit_valid = 1;
    return 0;
}

const char *flip_charge_mode_name(
    flip_charge_mode_t mode)
{
    switch (mode) {
    case FLIP_CHARGE_MODE_IDLE:
        return "IDLE";
    case FLIP_CHARGE_MODE_NORMAL:
        return "NORMAL";
    case FLIP_CHARGE_MODE_WARM:
        return "WARM";
    case FLIP_CHARGE_MODE_HOT:
        return "HOT";
    case FLIP_CHARGE_MODE_PAUSED_COLD:
        return "PAUSED_COLD";
    case FLIP_CHARGE_MODE_PAUSED_HOT:
        return "PAUSED_HOT";
    default:
        return "UNKNOWN";
    }
}

void flip_low_battery_guard_init(
    flip_low_battery_guard_t *guard)
{
    if (!guard)
        return;
    memset(guard, 0, sizeof(*guard));
}

static unsigned saturating_add(
    unsigned value,
    unsigned delta)
{
    if (UINT_MAX - value < delta)
        return UINT_MAX;
    return value + delta;
}

flip_low_battery_action_t
flip_low_battery_guard_update(
    flip_low_battery_guard_t *guard,
    const flip_battery_snapshot_t *battery,
    unsigned elapsed_ms)
{
    if (!guard || !battery)
        return FLIP_LOW_BATTERY_OK;

    if (battery->external_power_valid &&
        battery->external_power_online) {
        guard->critical_ms = 0;
        guard->emergency_ms = 0;
        return FLIP_LOW_BATTERY_OK;
    }

    int warning = 0;
    if (battery->capacity_level_valid &&
        (battery->capacity_level == FLIP_BATTERY_CAPACITY_LOW ||
         battery->capacity_level == FLIP_BATTERY_CAPACITY_CRITICAL)) {
        warning = 1;
    }
    if (battery->percent_valid &&
        battery->percent <= FLIP_LOW_BATTERY_WARN_PERCENT)
        warning = 1;
    if (battery->voltage_valid &&
        battery->voltage_uv <= FLIP_LOW_BATTERY_WARN_UV)
        warning = 1;

    /*
     * Percentage alone is deliberately not enough to shut down. The Flip's
     * RK817 gauge can report 0% well before the cell is exhausted. Require a
     * low cell voltage as corroboration, then debounce it to reject load dips.
     */
    int critical =
        battery->voltage_valid &&
        battery->voltage_uv <= FLIP_LOW_BATTERY_CRITICAL_UV &&
        ((battery->percent_valid &&
          battery->percent <= FLIP_LOW_BATTERY_CRITICAL_PERCENT) ||
         (battery->capacity_level_valid &&
          battery->capacity_level == FLIP_BATTERY_CAPACITY_CRITICAL));

    int emergency =
        battery->voltage_valid &&
        battery->voltage_uv <= FLIP_LOW_BATTERY_EMERGENCY_UV;

    guard->critical_ms = critical
        ? saturating_add(guard->critical_ms, elapsed_ms)
        : 0;
    guard->emergency_ms = emergency
        ? saturating_add(guard->emergency_ms, elapsed_ms)
        : 0;

    if (guard->emergency_ms >= FLIP_LOW_BATTERY_EMERGENCY_HOLD_MS ||
        guard->critical_ms >= FLIP_LOW_BATTERY_CRITICAL_HOLD_MS)
        return FLIP_LOW_BATTERY_SHUTDOWN;

    return warning
        ? FLIP_LOW_BATTERY_WARN
        : FLIP_LOW_BATTERY_OK;
}
