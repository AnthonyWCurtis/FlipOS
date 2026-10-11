#ifndef FLIP_POWER_POLICY_H
#define FLIP_POWER_POLICY_H

#include "flip_battery.h"

#define FLIP_CHARGE_WARM_ENTER_DECI_C 400
#define FLIP_CHARGE_WARM_EXIT_DECI_C 380
#define FLIP_CHARGE_HOT_ENTER_DECI_C 450
#define FLIP_CHARGE_HOT_EXIT_DECI_C 430
#define FLIP_CHARGE_PAUSE_HOT_ENTER_DECI_C 500
#define FLIP_CHARGE_PAUSE_HOT_EXIT_DECI_C 470
#define FLIP_CHARGE_PAUSE_COLD_ENTER_DECI_C 0
#define FLIP_CHARGE_PAUSE_COLD_EXIT_DECI_C 50

#define FLIP_CHARGE_WARM_CAP_UA 1500000L
#define FLIP_CHARGE_HOT_CAP_UA 1000000L

#define FLIP_LOW_BATTERY_WARN_PERCENT 10
#define FLIP_LOW_BATTERY_WARN_UV 3500000L
#define FLIP_LOW_BATTERY_CRITICAL_PERCENT 1
#define FLIP_LOW_BATTERY_CRITICAL_UV 3300000L
#define FLIP_LOW_BATTERY_EMERGENCY_UV 3150000L
#define FLIP_LOW_BATTERY_CRITICAL_HOLD_MS 8000U
#define FLIP_LOW_BATTERY_EMERGENCY_HOLD_MS 2000U

typedef enum {
    FLIP_CHARGE_MODE_IDLE = 0,
    FLIP_CHARGE_MODE_NORMAL,
    FLIP_CHARGE_MODE_WARM,
    FLIP_CHARGE_MODE_HOT,
    FLIP_CHARGE_MODE_PAUSED_COLD,
    FLIP_CHARGE_MODE_PAUSED_HOT
} flip_charge_mode_t;

typedef struct {
    flip_charge_mode_t mode;
} flip_charge_policy_t;

typedef struct {
    flip_charge_mode_t mode;
    int external_power_online;
    int charger_enabled;
    long current_limit_ua;
    int current_limit_valid;
} flip_charge_decision_t;

typedef enum {
    FLIP_LOW_BATTERY_OK = 0,
    FLIP_LOW_BATTERY_WARN,
    FLIP_LOW_BATTERY_SHUTDOWN
} flip_low_battery_action_t;

typedef struct {
    unsigned critical_ms;
    unsigned emergency_ms;
} flip_low_battery_guard_t;

void flip_charge_policy_init(
    flip_charge_policy_t *policy);

int flip_charge_policy_evaluate(
    flip_charge_policy_t *policy,
    const flip_battery_snapshot_t *battery,
    long stock_current_limit_ua,
    flip_charge_decision_t *decision);

const char *flip_charge_mode_name(
    flip_charge_mode_t mode);

void flip_low_battery_guard_init(
    flip_low_battery_guard_t *guard);

flip_low_battery_action_t
flip_low_battery_guard_update(
    flip_low_battery_guard_t *guard,
    const flip_battery_snapshot_t *battery,
    unsigned elapsed_ms);

#endif
