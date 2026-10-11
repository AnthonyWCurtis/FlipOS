#ifndef FLIP_TIME_H
#define FLIP_TIME_H

#include <stddef.h>
#include <time.h>

#define FLIP_TIMEZONE_MAX 64

typedef struct {
    char timezone[FLIP_TIMEZONE_MAX];
    int automatic;
    int ntp_synced;
} flip_time_status_t;

int flip_time_load_status(flip_time_status_t *status);
int flip_time_set_timezone(const char *timezone);
int flip_time_set_automatic(int enabled);
int flip_time_sync_ntp(void);
int flip_time_set_manual(const struct tm *local_time);
int flip_time_write_rtc(void);
int flip_time_format_local(char *out, size_t out_size);
const char *flip_time_timezone_at(int index);
int flip_time_timezone_count(void);

#endif
