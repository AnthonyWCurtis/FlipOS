#include "flip_power_policy.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static flip_battery_snapshot_t battery_at(int temp_deci_c)
{
    flip_battery_snapshot_t battery;
    memset(&battery, 0, sizeof(battery));
    battery.external_power_valid = 1;
    battery.external_power_online = 1;
    battery.temp_valid = 1;
    battery.temp_deci_c = temp_deci_c;
    return battery;
}

static void test_charge_policy_never_raises_stock(void)
{
    flip_charge_policy_t policy;
    flip_charge_decision_t decision;
    flip_charge_policy_init(&policy);

    flip_battery_snapshot_t battery = battery_at(250);
    assert(flip_charge_policy_evaluate(
        &policy, &battery, 1200000, &decision) == 0);
    assert(decision.mode == FLIP_CHARGE_MODE_NORMAL);
    assert(decision.charger_enabled);
    assert(decision.current_limit_valid);
    assert(decision.current_limit_ua == 1200000);

    battery.temp_deci_c = 410;
    assert(flip_charge_policy_evaluate(
        &policy, &battery, 1200000, &decision) == 0);
    assert(decision.mode == FLIP_CHARGE_MODE_WARM);
    assert(decision.current_limit_ua == 1200000);
}

static void test_charge_thermal_steps_and_hysteresis(void)
{
    flip_charge_policy_t policy;
    flip_charge_decision_t decision;
    flip_charge_policy_init(&policy);

    flip_battery_snapshot_t battery = battery_at(401);
    assert(flip_charge_policy_evaluate(
        &policy, &battery, 2000000, &decision) == 0);
    assert(decision.mode == FLIP_CHARGE_MODE_WARM);
    assert(decision.current_limit_ua == 1500000);

    battery.temp_deci_c = 390;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_WARM);

    battery.temp_deci_c = 379;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_NORMAL);

    battery.temp_deci_c = 451;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_HOT);
    assert(decision.current_limit_ua == 1000000);

    battery.temp_deci_c = 440;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_HOT);

    battery.temp_deci_c = 429;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_WARM);

    battery.temp_deci_c = 501;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_PAUSED_HOT);
    assert(!decision.charger_enabled);

    battery.temp_deci_c = 480;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_PAUSED_HOT);

    battery.temp_deci_c = 469;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_HOT);
}

static void test_charge_offline_and_missing_temperature(void)
{
    flip_charge_policy_t policy;
    flip_charge_decision_t decision;
    flip_charge_policy_init(&policy);

    flip_battery_snapshot_t battery = battery_at(250);
    battery.external_power_online = 0;
    flip_charge_policy_evaluate(&policy, &battery, 2000000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_IDLE);
    assert(!decision.charger_enabled);

    battery.external_power_online = 1;
    battery.temp_valid = 0;
    flip_charge_policy_evaluate(&policy, &battery, 1800000, &decision);
    assert(decision.mode == FLIP_CHARGE_MODE_NORMAL);
    assert(decision.charger_enabled);
    assert(decision.current_limit_ua == 1800000);
}

static void test_low_battery_guard(void)
{
    flip_low_battery_guard_t guard;
    flip_low_battery_guard_init(&guard);

    flip_battery_snapshot_t battery;
    memset(&battery, 0, sizeof(battery));
    battery.percent_valid = 1;
    battery.voltage_valid = 1;
    battery.percent = 0;
    battery.voltage_uv = 3450000;

    /* Gauge 0% by itself must never force shutdown. */
    for (int i = 0; i < 30; ++i)
        assert(flip_low_battery_guard_update(
            &guard, &battery, 1000) == FLIP_LOW_BATTERY_WARN);

    battery.voltage_uv = 3290000;
    for (int i = 0; i < 7; ++i)
        assert(flip_low_battery_guard_update(
            &guard, &battery, 1000) == FLIP_LOW_BATTERY_WARN);
    assert(flip_low_battery_guard_update(
        &guard, &battery, 1000) == FLIP_LOW_BATTERY_SHUTDOWN);
}

static void test_low_battery_load_dip_and_charger_cancel(void)
{
    flip_low_battery_guard_t guard;
    flip_low_battery_guard_init(&guard);

    flip_battery_snapshot_t battery;
    memset(&battery, 0, sizeof(battery));
    battery.percent_valid = 1;
    battery.voltage_valid = 1;
    battery.percent = 1;
    battery.voltage_uv = 3290000;

    for (int i = 0; i < 4; ++i)
        assert(flip_low_battery_guard_update(
            &guard, &battery, 1000) != FLIP_LOW_BATTERY_SHUTDOWN);

    battery.voltage_uv = 3600000;
    assert(flip_low_battery_guard_update(
        &guard, &battery, 1000) == FLIP_LOW_BATTERY_WARN);

    battery.voltage_uv = 3100000;
    assert(flip_low_battery_guard_update(
        &guard, &battery, 1000) == FLIP_LOW_BATTERY_WARN);
    assert(flip_low_battery_guard_update(
        &guard, &battery, 1000) == FLIP_LOW_BATTERY_SHUTDOWN);

    battery.external_power_valid = 1;
    battery.external_power_online = 1;
    assert(flip_low_battery_guard_update(
        &guard, &battery, 1000) == FLIP_LOW_BATTERY_OK);
    assert(guard.critical_ms == 0);
    assert(guard.emergency_ms == 0);
}

int main(void)
{
    test_charge_policy_never_raises_stock();
    test_charge_thermal_steps_and_hysteresis();
    test_charge_offline_and_missing_temperature();
    test_low_battery_guard();
    test_low_battery_load_dip_and_charger_cancel();
    puts("flip_power_policy tests passed");
    return EXIT_SUCCESS;
}
