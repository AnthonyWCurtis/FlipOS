#include "flip_session.h"
#include "flip_session_manager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLIP_SESSION_MANAGER_ROOT
#define FLIP_SESSION_MANAGER_ROOT \
    "/tmp/flipos-session-manager-recovery-test"
#endif

static flip_session_state_t fake_current;
static int recover_calls = 0;
static int read_calls = 0;
static int fake_recover_result = 0;
static int fake_read_result = 1;

static void reset_root(void)
{
    char command[768];

    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(command));
    assert(system(command) == 0);
}

int flip_session_recover_current(void)
{
    recover_calls++;

    if (fake_recover_result != 0)
        return fake_recover_result;

    if (strcmp(
            fake_current.state,
            "running") == 0) {

        snprintf(
            fake_current.state,
            sizeof(fake_current.state),
            "%s",
            "interrupted");

        return 1;
    }

    return 0;
}

int flip_session_read_current(
    flip_session_state_t *state)
{
    read_calls++;

    assert(state != NULL);

    *state = fake_current;

    return fake_read_result;
}

static void test_stale_running_becomes_retained_interrupted(void)
{
    const char *rom =
        "/tmp/flipos-session-manager-recovery.gb";

    reset_root();

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "running");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        rom);

    FILE *rom_file =
        fopen(
            rom,
            "wb");

    assert(rom_file != NULL);
    assert(fclose(rom_file) == 0);

    recover_calls = 0;
    read_calls = 0;
    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    assert(recover_calls == 1);
    assert(read_calls == 1);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 1);

    assert(
        strcmp(
            list.items[0].system,
            "GB") == 0);

    assert(
        strcmp(
            list.items[0].rom,
            rom) == 0);

    assert(
        list.items[0].status ==
        FLIP_RETAINED_SESSION_INTERRUPTED);

    flip_session_manager_list_free(
        &list);

    remove(rom);
    reset_root();
}


static void test_malformed_current_does_not_fabricate_session(void)
{
    reset_root();

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    recover_calls = 0;
    read_calls = 0;

    /*
     * Existing current-session recovery quarantines malformed
     * metadata and reports that it handled the stale record.
     * A subsequent read therefore has no valid current session.
     */
    fake_recover_result = 1;
    fake_read_result = -1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 0);

    flip_session_manager_list_free(
        &list);

    reset_root();
}


static void test_missing_rom_does_not_create_retained_session(void)
{
    const char *missing_rom =
        "/tmp/flipos-session-manager-missing.gb";

    reset_root();

    remove(missing_rom);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        missing_rom);

    recover_calls = 0;
    read_calls = 0;
    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 0);

    flip_session_manager_list_free(
        &list);

    reset_root();
}

static void test_unsupported_rom_does_not_create_retained_session(void)
{
    const char *unsupported_rom =
        "/tmp/flipos-session-manager-recovery.txt";

    reset_root();

    FILE *rom_file =
        fopen(
            unsupported_rom,
            "wb");

    assert(rom_file != NULL);
    assert(fclose(rom_file) == 0);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        unsupported_rom);

    recover_calls = 0;
    read_calls = 0;
    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 0);

    flip_session_manager_list_free(
        &list);

    remove(unsupported_rom);
    reset_root();
}


static void test_interrupted_without_resume_stays_visible(void)
{
    const char *rom =
        "/tmp/flipos-session-manager-no-resume.gb";

    reset_root();

    FILE *rom_file =
        fopen(
            rom,
            "wb");

    assert(rom_file != NULL);
    assert(fclose(rom_file) == 0);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        rom);

    recover_calls = 0;
    read_calls = 0;
    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_refresh(
            &list) == 0);

    assert(list.count == 1);

    assert(
        list.items[0].status ==
        FLIP_RETAINED_SESSION_INTERRUPTED);

    assert(
        list.items[0].resume_available ==
        0);

    flip_session_manager_list_free(
        &list);

    remove(rom);
    reset_root();
}


static void test_repeated_recovery_does_not_duplicate_session(void)
{
    const char *rom =
        "/tmp/flipos-session-manager-idempotent.gb";

    reset_root();

    FILE *rom_file =
        fopen(
            rom,
            "wb");

    assert(rom_file != NULL);
    assert(fclose(rom_file) == 0);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        rom);

    recover_calls = 0;
    read_calls = 0;
    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(
        &list,
        0,
        sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 1);

    assert(
        strcmp(
            list.items[0].rom,
            rom) == 0);

    assert(
        list.items[0].status ==
        FLIP_RETAINED_SESSION_INTERRUPTED);

    flip_session_manager_list_free(
        &list);

    remove(rom);
    reset_root();
}


static void test_unsupported_system_does_not_create_retained_session(void)
{
    const char *rom =
        "/tmp/flipos-session-manager-unknown.gb";

    reset_root();

    FILE *rom_file =
        fopen(
            rom,
            "wb");

    assert(rom_file != NULL);
    assert(fclose(rom_file) == 0);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "UNKNOWN");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        rom);

    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 0);

    flip_session_manager_list_free(
        &list);

    remove(rom);
    reset_root();
}

static void test_recovery_preserves_existing_game_data(void)
{
    const char *rom =
        "/tmp/flipos-session-manager-preserve.gb";

    const char *save =
        "/tmp/flipos-session-manager-preserve.sav";

    const char *state_file =
        "/tmp/flipos-session-manager-preserve.state";

    static const char save_bytes[] =
        "SAVE-SENTINEL";

    static const char state_bytes[] =
        "STATE-SENTINEL";

    reset_root();

    FILE *f = fopen(rom, "wb");
    assert(f != NULL);
    assert(fclose(f) == 0);

    f = fopen(save, "wb");
    assert(f != NULL);
    assert(
        fwrite(
            save_bytes,
            1,
            sizeof(save_bytes),
            f) == sizeof(save_bytes));
    assert(fclose(f) == 0);

    f = fopen(state_file, "wb");
    assert(f != NULL);
    assert(
        fwrite(
            state_bytes,
            1,
            sizeof(state_bytes),
            f) == sizeof(state_bytes));
    assert(fclose(f) == 0);

    memset(
        &fake_current,
        0,
        sizeof(fake_current));

    fake_current.valid = 1;

    snprintf(
        fake_current.state,
        sizeof(fake_current.state),
        "%s",
        "interrupted");

    snprintf(
        fake_current.system,
        sizeof(fake_current.system),
        "%s",
        "GB");

    snprintf(
        fake_current.rom,
        sizeof(fake_current.rom),
        "%s",
        rom);

    fake_recover_result = 0;
    fake_read_result = 1;

    assert(
        flip_session_manager_recover_interrupted()
        == 0);

    char buffer[64];

    f = fopen(save, "rb");
    assert(f != NULL);

    size_t count =
        fread(
            buffer,
            1,
            sizeof(save_bytes),
            f);

    assert(fclose(f) == 0);
    assert(count == sizeof(save_bytes));

    assert(
        memcmp(
            buffer,
            save_bytes,
            sizeof(save_bytes)) == 0);

    f = fopen(state_file, "rb");
    assert(f != NULL);

    count =
        fread(
            buffer,
            1,
            sizeof(state_bytes),
            f);

    assert(fclose(f) == 0);
    assert(count == sizeof(state_bytes));

    assert(
        memcmp(
            buffer,
            state_bytes,
            sizeof(state_bytes)) == 0);

    remove(rom);
    remove(save);
    remove(state_file);
    reset_root();
}

int main(void)
{
    test_stale_running_becomes_retained_interrupted();
    test_malformed_current_does_not_fabricate_session();
    test_missing_rom_does_not_create_retained_session();
    test_unsupported_rom_does_not_create_retained_session();
    test_interrupted_without_resume_stays_visible();
    test_repeated_recovery_does_not_duplicate_session();
    test_unsupported_system_does_not_create_retained_session();
    test_recovery_preserves_existing_game_data();
    return 0;
}
