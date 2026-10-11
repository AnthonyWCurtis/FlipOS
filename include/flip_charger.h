#ifndef FLIP_CHARGER_H
#define FLIP_CHARGER_H

#include "flip_battery.h"
#include "flip_power_policy.h"

#ifndef FLIP_CHARGER_CURRENT_LIMIT_PATH
#define FLIP_CHARGER_CURRENT_LIMIT_PATH \
    "/sys/module/flip_rk817_charger/parameters/current_limit_ma"
#endif

#ifndef FLIP_CHARGER_STOCK_CURRENT_PATH
#define FLIP_CHARGER_STOCK_CURRENT_PATH \
    "/sys/module/flip_rk817_charger/parameters/stock_current_ma"
#endif

typedef struct {
    flip_charge_policy_t policy;
    int helper_available;
    int stock_current_ma;
    int applied_current_ma;
    flip_charge_mode_t mode;
} flip_charger_t;

void flip_charger_init(flip_charger_t *charger);

int flip_charger_update(
    flip_charger_t *charger,
    const flip_battery_snapshot_t *battery);

int flip_charger_restore(flip_charger_t *charger);

#endif
