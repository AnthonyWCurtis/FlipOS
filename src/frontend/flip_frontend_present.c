#include "flip_frontend_present.h"

#include "flip_osd.h"

int flip_frontend_present(
    flip_drm_t *drm)
{
    if (!drm)
        return -1;

    /*
     * Global FlipOS OSD is always composited last over
     * frontend-owned UI before the framebuffer is presented.
     */
    if (flip_osd_draw(drm) < 0)
        return -1;

    flip_drm_present(drm);
    return 0;
}
