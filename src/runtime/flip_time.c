#define _GNU_SOURCE

#include "flip_time.h"
#include "flip_paths.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FLIP_TIME_ROOT FLIP_STATE_DATA_ROOT "/time"
#define FLIP_TIME_CONFIG FLIP_TIME_ROOT "/config"
#define FLIP_ZONEINFO_ROOT "/usr/share/zoneinfo"
#define FLIP_RTC_DEVICE "/dev/rtc0"
#define FLIP_NTPD "/usr/sbin/ntpd"

static const char *const timezones[] = {
    "Europe/London",
    "Europe/Dublin",
    "Europe/Paris",
    "Europe/Berlin",
    "Europe/Madrid",
    "Europe/Rome",
    "Europe/Amsterdam",
    "Europe/Warsaw",
    "Europe/Athens",
    "America/New_York",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Toronto",
    "America/Vancouver",
    "America/Sao_Paulo",
    "Asia/Tokyo",
    "Asia/Seoul",
    "Asia/Shanghai",
    "Asia/Hong_Kong",
    "Asia/Singapore",
    "Asia/Kolkata",
    "Australia/Sydney",
    "Australia/Perth",
    "Pacific/Auckland",
    "UTC",
};

static int ensure_dir(void)
{
    if (mkdir(FLIP_STATE_DATA_ROOT, 0755) < 0 && errno != EEXIST)
        return -1;
    if (mkdir(FLIP_TIME_ROOT, 0755) < 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int save_config(const flip_time_status_t *status)
{
    if (!status || ensure_dir() < 0)
        return -1;

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", FLIP_TIME_CONFIG);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;

    int ok = fprintf(
        f,
        "timezone=%s\nautomatic=%d\nntp_synced=%d\n",
        status->timezone,
        status->automatic ? 1 : 0,
        status->ntp_synced ? 1 : 0) > 0;

    if (ok && fflush(f) != 0)
        ok = 0;
    if (ok && fsync(fileno(f)) < 0)
        ok = 0;
    if (fclose(f) != 0)
        ok = 0;

    if (!ok || rename(tmp, FLIP_TIME_CONFIG) < 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int flip_time_load_status(flip_time_status_t *status)
{
    if (!status)
        return -1;

    memset(status, 0, sizeof(*status));
    snprintf(status->timezone, sizeof(status->timezone), "UTC");
    status->automatic = 1;

    FILE *f = fopen(FLIP_TIME_CONFIG, "r");
    if (f) {
        char line[160];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, "timezone=", 9) == 0) {
                size_t length = strlen(line + 9);
                if (length >= sizeof(status->timezone))
                    length = sizeof(status->timezone) - 1;
                memcpy(status->timezone, line + 9, length);
                status->timezone[length] = '\0';
            } else if (strncmp(line, "automatic=", 10) == 0) {
                status->automatic = atoi(line + 10) != 0;
            } else if (strncmp(line, "ntp_synced=", 11) == 0) {
                status->ntp_synced = atoi(line + 11) != 0;
            }
        }
        fclose(f);
    }

    if (status->timezone[0]) {
        (void)setenv("TZ", status->timezone, 1);
        tzset();
    }
    return 0;
}

int flip_time_set_timezone(const char *timezone)
{
    if (!timezone || !timezone[0] || strstr(timezone, "..") || timezone[0] == '/')
        return -1;

    char source[256];
    int n = snprintf(source, sizeof(source), "%s/%s", FLIP_ZONEINFO_ROOT, timezone);
    if (n < 0 || (size_t)n >= sizeof(source))
        return -1;

    struct stat st;
    if (stat(source, &st) < 0 || !S_ISREG(st.st_mode))
        return -1;

    /*
     * The stock /userdata mount is read-only, so /etc/localtime cannot be
     * replaced persistently. Keep Linux and the hardware RTC in UTC, persist
     * the user's IANA zone on the SD card, and apply TZ to FlipOS. Emulator
     * children inherit this environment and therefore see the same local time.
     */
    flip_time_status_t status;
    flip_time_load_status(&status);
    snprintf(status.timezone, sizeof(status.timezone), "%s", timezone);
    if (save_config(&status) < 0)
        return -1;

    setenv("TZ", timezone, 1);
    tzset();
    return 0;
}

static int run_process(char *const argv[]);
static int run_process_timeout(char *const argv[], unsigned timeout_ms);

static int restore_system_from_rtc(void)
{
    int fd = open(FLIP_RTC_DEVICE, O_RDONLY);
    if (fd < 0)
        return -1;

    struct rtc_time rtc;
    memset(&rtc, 0, sizeof(rtc));
    int result = ioctl(fd, RTC_RD_TIME, &rtc);
    close(fd);
    if (result < 0)
        return -1;

    struct tm utc;
    memset(&utc, 0, sizeof(utc));
    utc.tm_sec = rtc.tm_sec;
    utc.tm_min = rtc.tm_min;
    utc.tm_hour = rtc.tm_hour;
    utc.tm_mday = rtc.tm_mday;
    utc.tm_mon = rtc.tm_mon;
    utc.tm_year = rtc.tm_year;
    utc.tm_isdst = 0;

    time_t epoch = timegm(&utc);
    if (epoch == (time_t)-1)
        return -1;

    struct timespec ts = {
        .tv_sec = epoch,
        .tv_nsec = 0,
    };
    return clock_settime(CLOCK_REALTIME, &ts);
}

static int apply_ntp_service_state(int enabled)
{
    int running = access("/var/run/ntpd.pid", F_OK) == 0;
    if ((enabled && running) || (!enabled && !running))
        return 0;

    char *argv[] = {
        (char *)"/etc/init.d/S49ntp",
        (char *)(enabled ? "start" : "stop"),
        NULL
    };
    return run_process(argv);
}

int flip_time_set_automatic(int enabled)
{
    flip_time_status_t status;
    flip_time_load_status(&status);

    if (apply_ntp_service_state(enabled ? 1 : 0) < 0)
        return -1;

    /*
     * The stock init system starts ntpd before FlipOS. If Manual Time was
     * saved, ntpd may have had a brief chance to alter CLOCK_REALTIME during
     * boot. Re-apply the battery-backed UTC RTC after stopping ntpd so the
     * user's manual clock remains authoritative.
     */
    if (!enabled && restore_system_from_rtc() < 0)
        return -1;

    status.automatic = enabled ? 1 : 0;
    return save_config(&status);
}

int flip_time_write_rtc(void)
{
    time_t now = time(NULL);
    struct tm utc;
    if (now == (time_t)-1 || !gmtime_r(&now, &utc))
        return -1;

    struct rtc_time rtc;
    memset(&rtc, 0, sizeof(rtc));
    rtc.tm_sec = utc.tm_sec;
    rtc.tm_min = utc.tm_min;
    rtc.tm_hour = utc.tm_hour;
    rtc.tm_mday = utc.tm_mday;
    rtc.tm_mon = utc.tm_mon;
    rtc.tm_year = utc.tm_year;
    rtc.tm_wday = utc.tm_wday;
    rtc.tm_yday = utc.tm_yday;
    rtc.tm_isdst = 0;

    int fd = open(FLIP_RTC_DEVICE, O_RDWR);
    if (fd < 0)
        return -1;
    int result = ioctl(fd, RTC_SET_TIME, &rtc);
    close(fd);
    return result == 0 ? 0 : -1;
}

static int run_process(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }

    int status_code = 0;
    while (waitpid(pid, &status_code, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status_code))
        return -1;
    return WEXITSTATUS(status_code) == 0 ? 0 : -1;
}

static int run_process_timeout(char *const argv[], unsigned timeout_ms)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }

    const unsigned poll_ms = 100;
    unsigned waited_ms = 0;
    int status_code = 0;

    while (waited_ms < timeout_ms) {
        pid_t result = waitpid(pid, &status_code, WNOHANG);
        if (result == pid) {
            if (!WIFEXITED(status_code))
                return -1;
            return WEXITSTATUS(status_code) == 0 ? 0 : -1;
        }
        if (result < 0 && errno != EINTR)
            return -1;

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = (long)poll_ms * 1000000L,
        };
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        }
        waited_ms += poll_ms;
    }

    (void)kill(pid, SIGTERM);
    for (unsigned i = 0; i < 5; ++i) {
        pid_t result = waitpid(pid, &status_code, WNOHANG);
        if (result == pid)
            return -1;
        if (result < 0 && errno != EINTR)
            return -1;

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 100000000L,
        };
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        }
    }

    (void)kill(pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
    return -1;
}

int flip_time_sync_ntp(void)
{
    /*
     * The stock image already runs ntpd continuously. A second one-shot
     * instance cannot bind UDP/123 while that daemon is active, so pause the
     * service, perform a single authoritative sync using /etc/ntp.conf, then
     * restore the background daemon only when Automatic Time is enabled.
     */
    flip_time_status_t status;
    flip_time_load_status(&status);

    if (apply_ntp_service_state(0) < 0)
        return -1;

    char *sync_argv[] = {
        (char *)FLIP_NTPD,
        (char *)"-q",
        (char *)"-g",
        (char *)"-n",
        NULL
    };

    /*
     * A one-shot sync is a user-facing action. NTP can otherwise wait for
     * multiple polling cycles while Wi-Fi is settling, leaving the frontend
     * apparently frozen. Bound the explicit sync attempt; the normal
     * background ntpd service is restored below and can continue converging.
     */
    int sync_result = run_process_timeout(sync_argv, 12000);
    int restore_result =
        status.automatic
            ? apply_ntp_service_state(1)
            : 0;

    if (sync_result < 0 || restore_result < 0)
        return -1;

    if (flip_time_write_rtc() < 0)
        return -1;

    status.ntp_synced = 1;
    return save_config(&status);
}

int flip_time_set_manual(const struct tm *local_time)
{
    if (!local_time)
        return -1;

    struct tm value = *local_time;
    value.tm_isdst = -1;
    time_t epoch = mktime(&value);
    if (epoch == (time_t)-1)
        return -1;

    struct timespec ts = {
        .tv_sec = epoch,
        .tv_nsec = 0,
    };
    if (clock_settime(CLOCK_REALTIME, &ts) < 0)
        return -1;

    if (flip_time_write_rtc() < 0)
        return -1;

    flip_time_status_t status;
    flip_time_load_status(&status);
    status.ntp_synced = 0;
    return save_config(&status);
}

int flip_time_format_local(char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return -1;

    time_t now = time(NULL);
    struct tm local;
    if (now == (time_t)-1 || !localtime_r(&now, &local))
        return -1;

    if (strftime(out, out_size, "%Y-%m-%d  %H:%M", &local) == 0)
        return -1;
    return 0;
}

const char *flip_time_timezone_at(int index)
{
    int count = (int)(sizeof(timezones) / sizeof(timezones[0]));
    if (index < 0 || index >= count)
        return NULL;
    return timezones[index];
}

int flip_time_timezone_count(void)
{
    return (int)(sizeof(timezones) / sizeof(timezones[0]));
}
