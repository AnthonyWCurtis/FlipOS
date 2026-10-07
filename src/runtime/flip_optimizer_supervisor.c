#include "flip_optimizer_supervisor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *flip_optimizer_supervisor_reset_name(
    flip_optimizer_reset_cause_t cause)
{
    switch (cause) {
    case FLIP_OPTIMIZER_RESET_COLDBOOT:
        return "coldboot";
    case FLIP_OPTIMIZER_RESET_NORMAL:
        return "normal";
    case FLIP_OPTIMIZER_RESET_PANIC:
        return "panic";
    case FLIP_OPTIMIZER_RESET_WATCHDOG:
        return "watchdog";
    case FLIP_OPTIMIZER_RESET_OTHER:
        return "other";
    case FLIP_OPTIMIZER_RESET_UNKNOWN:
    default:
        return "unknown";
    }
}

static flip_optimizer_reset_cause_t read_boot_mode(
    char *boot_mode,
    size_t boot_mode_size)
{
    if (!boot_mode || boot_mode_size < 2)
        return FLIP_OPTIMIZER_RESET_UNKNOWN;

    boot_mode[0] = '\0';

    FILE *file = fopen(FLIP_OPTIMIZER_BOOT_MODE_PATH, "r");
    if (!file)
        return FLIP_OPTIMIZER_RESET_UNKNOWN;

    if (!fgets(boot_mode, (int)boot_mode_size, file)) {
        fclose(file);
        boot_mode[0] = '\0';
        return FLIP_OPTIMIZER_RESET_UNKNOWN;
    }

    fclose(file);

    char *end = strpbrk(boot_mode, "\r\n");
    if (end)
        *end = '\0';

    if (!boot_mode[0])
        return FLIP_OPTIMIZER_RESET_UNKNOWN;
    if (strcmp(boot_mode, "coldboot") == 0)
        return FLIP_OPTIMIZER_RESET_COLDBOOT;
    if (strcmp(boot_mode, "normal") == 0)
        return FLIP_OPTIMIZER_RESET_NORMAL;
    if (strcmp(boot_mode, "panic") == 0)
        return FLIP_OPTIMIZER_RESET_PANIC;
    if (strcmp(boot_mode, "watchdog") == 0)
        return FLIP_OPTIMIZER_RESET_WATCHDOG;

    return FLIP_OPTIMIZER_RESET_OTHER;
}

const char *flip_optimizer_supervisor_reason_name(
    flip_optimizer_supervisor_reason_t reason)
{
    switch (reason) {
    case FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN:
        return "clean";
    case FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED:
        return "unconfirmed";
    case FLIP_OPTIMIZER_SUPERVISOR_REASON_CORRUPT:
        return "corrupt";
    case FLIP_OPTIMIZER_SUPERVISOR_REASON_UNREADABLE:
        return "unreadable";
    }

    return "unreadable";
}

int flip_optimizer_supervisor_boot_decide(
    flip_optimizer_supervisor_boot_decision_t *decision)
{
    if (!decision)
        return -1;

    memset(decision, 0, sizeof(*decision));

    decision->reset_cause = read_boot_mode(
        decision->boot_mode,
        sizeof(decision->boot_mode));

    flip_optimizer_guard_boot_inspection_t inspection;
    if (flip_optimizer_guard_boot_inspect(&inspection) < 0) {
        decision->stock_safe_required = 1;
        decision->durable_settlement_required = 1;
        decision->reason =
            FLIP_OPTIMIZER_SUPERVISOR_REASON_UNREADABLE;
        return 0;
    }

    switch (inspection.state) {
    case FLIP_OPTIMIZER_GUARD_BOOT_CLEAN:
        decision->reason =
            FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN;
        return 0;

    case FLIP_OPTIMIZER_GUARD_BOOT_UNCONFIRMED:
        decision->stock_safe_required = 1;
        decision->durable_settlement_required = 1;
        decision->reason =
            FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED;
        strcpy(decision->trial_id, inspection.trial_id);
        return 0;

    case FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT:
        decision->stock_safe_required = 1;
        decision->durable_settlement_required = 1;
        decision->reason =
            FLIP_OPTIMIZER_SUPERVISOR_REASON_CORRUPT;
        return 0;

    case FLIP_OPTIMIZER_GUARD_BOOT_IO_ERROR:
    default:
        decision->stock_safe_required = 1;
        decision->durable_settlement_required = 1;
        decision->reason =
            FLIP_OPTIMIZER_SUPERVISOR_REASON_UNREADABLE;
        return 0;
    }
}


static int set_runtime_stock_safe(int enabled)
{
    if (!enabled) {
        if (unlink(FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH) == 0 ||
            errno == ENOENT)
            return 0;
        return -1;
    }

    int fd = open(
        FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH,
        O_WRONLY | O_CREAT | O_TRUNC,
        0600);
    if (fd < 0)
        return -1;

    const char contents[] = "stock-safe\n";
    size_t length = sizeof(contents) - 1;
    ssize_t written = write(fd, contents, length);
    int saved_errno = errno;
    int close_rc = close(fd);
    errno = saved_errno;

    return written == (ssize_t)length && close_rc == 0
        ? 0
        : -1;
}

int flip_optimizer_supervisor_boot_enforce(
    flip_optimizer_supervisor_boot_decision_t *decision)
{
    flip_optimizer_supervisor_boot_decision_t local;
    if (!decision)
        decision = &local;

    if (flip_optimizer_supervisor_boot_decide(decision) < 0)
        return -1;

    /*
     * The enforcement latch lives in RAM. It remains available even when
     * persistent optimizer state is read-only or damaged.
     */
    return set_runtime_stock_safe(decision->stock_safe_required);
}

int flip_optimizer_supervisor_runtime_stock_safe(void)
{
    return access(
        FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH,
        F_OK) == 0;
}
