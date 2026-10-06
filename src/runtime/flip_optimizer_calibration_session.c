#include "flip_optimizer_calibration_session.h"

#include "flip_optimizer_guard.h"

#include <string.h>

static int pending_matches_guard(
    const flip_optimizer_calibration_t *calibration,
    const flip_optimizer_guard_status_t *guard)
{
    return calibration &&
        guard &&
        calibration->pending &&
        guard->active &&
        strcmp(
            calibration->pending_trial_id,
            guard->trial_id) == 0;
}

int flip_optimizer_calibration_session_prepare(
    flip_optimizer_calibration_t *calibration,
    size_t *opp_index,
    unsigned *candidate_uv)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        calibration->pending)
        return -1;

    flip_optimizer_guard_status_t guard;

    if (flip_optimizer_guard_status(&guard) < 0 ||
        guard.active ||
        guard.stock_only)
        return -1;

    flip_optimizer_calibration_t original = *calibration;

    int rc = flip_optimizer_calibration_prepare_next(
        calibration,
        opp_index,
        candidate_uv);

    if (rc <= 0)
        return rc;

    if (flip_optimizer_calibration_save(calibration) < 0) {
        *calibration = original;
        return -1;
    }

    if (flip_optimizer_guard_arm(
            calibration->pending_trial_id,
            0) < 0) {
        *calibration = original;
        if (flip_optimizer_calibration_save(
                calibration) < 0)
            return -1;
        return -1;
    }

    return 1;
}

int flip_optimizer_calibration_session_ready_for_apply(
    const flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        !calibration->pending)
        return 0;

    flip_optimizer_guard_status_t guard;

    if (flip_optimizer_guard_status(&guard) < 0)
        return 0;

    return !guard.stock_only &&
        pending_matches_guard(calibration, &guard);
}

int flip_optimizer_calibration_session_confirm_stable(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        !flip_optimizer_calibration_session_ready_for_apply(
            calibration))
        return -1;

    char trial_id[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];

    strcpy(
        trial_id,
        calibration->pending_trial_id);

    /*
     * Confirm the guard first. If power is lost before calibration state is
     * committed, reconcile() can recover the confirmed event on next boot.
     */
    if (flip_optimizer_guard_confirm(
            trial_id,
            0) < 0)
        return -1;

    if (flip_optimizer_calibration_record_pending(
            calibration,
            1) < 0)
        return -1;

    return flip_optimizer_calibration_save(calibration);
}

int flip_optimizer_calibration_session_cancel_unapplied(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        !flip_optimizer_calibration_session_ready_for_apply(
            calibration))
        return -1;

    char trial_id[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];

    strcpy(
        trial_id,
        calibration->pending_trial_id);

    if (flip_optimizer_guard_cancel(
            trial_id,
            0) < 0)
        return -1;

    if (flip_optimizer_calibration_cancel_pending(
            calibration) < 0)
        return -1;

    return flip_optimizer_calibration_save(calibration);
}

int flip_optimizer_calibration_session_reconcile(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0)
        return -1;

    if (!calibration->pending)
        return FLIP_OPTIMIZER_RECONCILE_NONE;

    flip_optimizer_guard_status_t guard;

    if (flip_optimizer_guard_status(&guard) < 0)
        return -1;

    if (guard.active) {
        if (!pending_matches_guard(
                calibration,
                &guard))
            return -1;

        return FLIP_OPTIMIZER_RECONCILE_NONE;
    }

    if (guard.last_trial_id[0] &&
        strcmp(
            calibration->pending_trial_id,
            guard.last_trial_id) == 0) {

        if (strcmp(
                guard.last_action,
                "confirmed") == 0) {
            if (flip_optimizer_calibration_record_pending(
                    calibration,
                    1) < 0 ||
                flip_optimizer_calibration_save(
                    calibration) < 0)
                return -1;

            return FLIP_OPTIMIZER_RECONCILE_CONFIRMED;
        }

        if (strcmp(
                guard.last_action,
                "rolled_back") == 0 ||
            strcmp(
                guard.last_action,
                "rollback_corrupt") == 0) {
            if (flip_optimizer_calibration_record_rollback(
                    calibration,
                    calibration->pending_trial_id) < 0 ||
                flip_optimizer_calibration_save(
                    calibration) < 0)
                return -1;

            return FLIP_OPTIMIZER_RECONCILE_ROLLED_BACK;
        }
    }

    /*
     * No guard was ever armed for this persisted candidate, so no compliant
     * voltage backend could have applied it. Drop the orphan and retry later.
     */
    if (flip_optimizer_calibration_cancel_pending(
            calibration) < 0 ||
        flip_optimizer_calibration_save(
            calibration) < 0)
        return -1;

    return FLIP_OPTIMIZER_RECONCILE_ORPHANED;
}

int flip_optimizer_calibration_session_clear_stock_only(
    const flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        calibration->pending)
        return -1;

    return flip_optimizer_guard_clear_stock_only();
}
