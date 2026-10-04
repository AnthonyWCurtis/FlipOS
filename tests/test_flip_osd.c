#include "flip_osd.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    flip_osd_snapshot_t snapshot;

    flip_osd_init();

    assert(flip_osd_snapshot(&snapshot) == 0);
    assert(snapshot.type == FLIP_OSD_NONE);

    flip_osd_show_volume(150);
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.type == FLIP_OSD_VOLUME);
    assert(snapshot.percent == 100);
    assert(snapshot.poweroff_active == 0);

    flip_osd_show_brightness(10);
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.type == FLIP_OSD_BRIGHTNESS);
    assert(snapshot.percent == 0);

    flip_osd_show_brightness(255);
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.percent == 100);

    flip_osd_show_status("STATE SAVED");
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.type == FLIP_OSD_STATUS);
    assert(strcmp(snapshot.status, "STATE SAVED") == 0);

    flip_osd_set_poweroff_progress(0.5);
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.poweroff_active == 1);
    assert(snapshot.poweroff_percent == 50);

    flip_osd_set_poweroff_progress(0.0);
    assert(flip_osd_snapshot(&snapshot) == 1);
    assert(snapshot.poweroff_active == 0);
    assert(snapshot.type == FLIP_OSD_STATUS);

    flip_osd_reset();
    assert(flip_osd_snapshot(&snapshot) == 0);
    assert(snapshot.type == FLIP_OSD_NONE);
    assert(snapshot.poweroff_active == 0);

    flip_osd_show_volume(25);

    struct timespec wait = {
        .tv_sec = 1,
        .tv_nsec = 50000000L
    };
    assert(nanosleep(&wait, NULL) == 0);

    assert(flip_osd_snapshot(&snapshot) == 0);
    assert(snapshot.type == FLIP_OSD_NONE);

    puts("flip_osd service tests passed");
    return 0;
}
