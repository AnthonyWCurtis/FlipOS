#include "flip_frontend_screensaver.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    flip_frontend_screensaver_motion_t motion;

    flip_frontend_screensaver_motion_init(
        &motion,
        100,
        100,
        1,
        1);

    /*
     * Normal movement.
     */
    flip_frontend_screensaver_motion_step(
        &motion,
        640,
        480,
        180,
        100);

    assert(motion.x == 101);
    assert(motion.y == 101);
    assert(motion.dx == 1);
    assert(motion.dy == 1);

    /*
     * Right-edge bounce.
     */
    motion.x = 640 - 180;
    motion.dx = 1;

    flip_frontend_screensaver_motion_step(
        &motion,
        640,
        480,
        180,
        100);

    assert(motion.dx == -1);

    /*
     * Bottom-edge bounce.
     */
    motion.y = 480 - 100;
    motion.dy = 1;

    flip_frontend_screensaver_motion_step(
        &motion,
        640,
        480,
        180,
        100);

    assert(motion.dy == -1);

    /*
     * Left/top bounce.
     */
    motion.x = 0;
    motion.y = 0;
    motion.dx = -1;
    motion.dy = -1;

    flip_frontend_screensaver_motion_step(
        &motion,
        640,
        480,
        180,
        100);

    assert(motion.dx == 1);
    assert(motion.dy == 1);

    puts("frontend screensaver motion tests passed");
    return 0;
}
