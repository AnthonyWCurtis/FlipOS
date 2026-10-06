#ifndef FLIP_OPTIMIZER_CALIBRATION_SESSION_H
#define FLIP_OPTIMIZER_CALIBRATION_SESSION_H

#include "flip_optimizer_calibration.h"

enum {
    FLIP_OPTIMIZER_RECONCILE_NONE = 0,
    FLIP_OPTIMIZER_RECONCILE_CONFIRMED = 1,
    FLIP_OPTIMIZER_RECONCILE_ROLLED_BACK = 2,
    FLIP_OPTIMIZER_RECONCILE_ORPHANED = 3
};

int flip_optimizer_calibration_session_prepare(
    flip_optimizer_calibration_t *calibration,
    size_t *opp_index,
    unsigned *candidate_uv);

int flip_optimizer_calibration_session_ready_for_apply(
    const flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_session_confirm_stable(
    flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_session_cancel_unapplied(
    flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_session_reconcile(
    flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_session_clear_stock_only(
    const flip_optimizer_calibration_t *calibration);

#endif
