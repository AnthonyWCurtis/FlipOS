#ifndef FLIP_SETUP_H
#define FLIP_SETUP_H

#include "flip_paths.h"

#ifndef FLIP_SETUP_ROOT
#define FLIP_SETUP_ROOT FLIP_STATE_DATA_ROOT "/setup"
#endif

#ifndef FLIP_SETUP_COMPLETE_FILE
#define FLIP_SETUP_COMPLETE_FILE FLIP_SETUP_ROOT "/complete"
#endif

#ifndef FLIP_SETUP_PROGRESS_FILE
#define FLIP_SETUP_PROGRESS_FILE FLIP_SETUP_ROOT "/in-progress"
#endif

int flip_setup_is_complete(void);
int flip_setup_begin(void);
int flip_setup_mark_complete(void);
int flip_setup_is_in_progress(void);

#endif
