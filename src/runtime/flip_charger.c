#include "flip_charger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_int(const char *path, int *value)
{
    if (!path || !value)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[64];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    errno = 0;
    char *end = NULL;
    long parsed = strtol(line, &end, 10);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        ++end;

    if (errno || end == line || (end && *end) || parsed < -1 || parsed > 10000)
        return -1;

    *value = (int)parsed;
    return 0;
}

static int write_current(int ma)
{
    FILE *f = fopen(FLIP_CHARGER_CURRENT_LIMIT_PATH, "w");
    if (!f)
        return -1;

    int ok = fprintf(f, "%d\n", ma) >= 0 && fflush(f) == 0;
    int close_result = fclose(f);
    return ok && close_result == 0 ? 0 : -1;
}

void flip_charger_init(flip_charger_t *charger)
{
    if (!charger)
        return;

    memset(charger, 0, sizeof(*charger));
    flip_charge_policy_init(&charger->policy);
    charger->applied_current_ma = -2;
    charger->mode = FLIP_CHARGE_MODE_IDLE;

    int stock_ma = 0;
    if (access(FLIP_CHARGER_CURRENT_LIMIT_PATH, W_OK) == 0 &&
        read_int(FLIP_CHARGER_STOCK_CURRENT_PATH, &stock_ma) == 0 &&
        stock_ma >= 500 && stock_ma <= 3500) {
        charger->helper_available = 1;
        charger->stock_current_ma = stock_ma;
    }
}

int flip_charger_update(
    flip_charger_t *charger,
    const flip_battery_snapshot_t *battery)
{
    if (!charger || !battery)
        return -1;

    if (!charger->helper_available)
        return 0;

    flip_charge_decision_t decision;
    if (flip_charge_policy_evaluate(
            &charger->policy,
            battery,
            (long)charger->stock_current_ma * 1000L,
            &decision) < 0)
        return -1;

    charger->mode = decision.mode;

    int target_ma = -1;
    if (decision.mode == FLIP_CHARGE_MODE_WARM)
        target_ma = (int)(FLIP_CHARGE_WARM_CAP_UA / 1000L);
    else if (decision.mode == FLIP_CHARGE_MODE_HOT)
        target_ma = (int)(FLIP_CHARGE_HOT_CAP_UA / 1000L);
    else if (decision.mode == FLIP_CHARGE_MODE_PAUSED_COLD ||
             decision.mode == FLIP_CHARGE_MODE_PAUSED_HOT) {
        /*
         * We intentionally do not touch RK817 charger-enable/power-path bits.
         * At extreme temperature request the chip's minimum programmable
         * current; RK817's own thermistor protection remains authoritative.
         */
        target_ma = 500;
    }

    /*
     * Positive caps are reasserted every sample because the vendor charger
     * driver can rewrite CHRG_CUR_SEL after USB/charger-type events.  -1 means
     * release control completely and is only written on a state transition.
     */
    if (target_ma < 0 && charger->applied_current_ma == -1)
        return 0;

    if (write_current(target_ma) < 0)
        return -1;

    charger->applied_current_ma = target_ma;
    return 1;
}

int flip_charger_restore(flip_charger_t *charger)
{
    if (!charger)
        return -1;

    if (!charger->helper_available)
        return 0;

    if (write_current(-1) < 0)
        return -1;

    charger->applied_current_ma = -1;
    charger->mode = FLIP_CHARGE_MODE_IDLE;
    flip_charge_policy_init(&charger->policy);
    return 0;
}
