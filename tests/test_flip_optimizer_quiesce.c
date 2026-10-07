#include "flip_optimizer_quiesce.h"

#include <assert.h>
#include <stdio.h>

static void test_writer_matching(void)
{
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-frontend") == 1);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-gb /rom/game.gb") == 1);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-gba /rom/game.gba") == 1);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-gpu-service") == 1);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-perf-capture playtest 60") == 1);

    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-perf-capture snapshot") == 0);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/flipos-dev/bin/flip-optimizer-candidate live-once 20") == 0);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/usr/miyoo/bin/runmiyoo.sh") == 0);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/mnt/SDCARD/.tmp_update/updater") == 0);
    assert(flip_optimizer_quiesce_cmdline_is_writer(
               "/bin/sh") == 0);
}

int main(void)
{
    test_writer_matching();
    printf("flip_optimizer_quiesce tests passed\n");
    return 0;
}
