#include "flip_frontend_present.h"

#include "flip_osd.h"
#include "flip_frontend_idle.h"
#include "flip_frontend_screensaver.h"

static flip_frontend_idle_t *frontend_idle = NULL;

static double present_time = 0.0;

void flip_frontend_present_set_idle(
    flip_frontend_idle_t *idle)
{
    frontend_idle = idle;
}

int flip_frontend_present(
    flip_drm_t *drm)
{
    if (!drm)
        return -1;

    present_time += 0.03;

    if (frontend_idle &&
        flip_frontend_idle_active(
            frontend_idle)) {

        if (flip_frontend_screensaver_draw(
                drm,
                present_time) < 0)
            return -1;

    } else {

        /*
         * Global FlipOS OSD is always composited last over
         * frontend-owned UI before presentation.
         */
        if (flip_osd_draw(drm) < 0)
            return -1;
    }

    flip_drm_present(drm);
    return 0;
}
