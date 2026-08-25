#define _GNU_SOURCE

#include "flip_audio.h"
#include "flip_settings.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static int audio_fd = -1;
static pid_t audio_pid = -1;
static int speaker_volume = 30;

/*
 * Miyoo Flip stock RK817 deep-resume workaround.
 *
 * Confirmed on real hardware:
 *
 * Audible playback:
 *   0x17 = 0x40
 *   0x38 = 0x10
 *   0x40 = 0xa5
 *
 * Broken post-deep-resume playback:
 *   0x17 = 0x06
 *   0x38 = 0x00
 *   0x40 = 0x69
 *
 * Only repair the exact confirmed broken signature.
 */
static int rk817_codec_resume_recover(void)
{
    int fd =
        open(
            "/dev/i2c-0",
            O_RDWR);

    if (fd < 0)
        return -1;

    if (ioctl(
            fd,
            I2C_SLAVE_FORCE,
            0x20) < 0) {

        close(fd);
        return -1;
    }

    const unsigned char regs[3] = {
        0x17,
        0x38,
        0x40
    };

    unsigned char values[3];

    for (int i = 0;
         i < 3;
         ++i) {

        unsigned char reg =
            regs[i];

        if (write(
                fd,
                &reg,
                1) != 1 ||
            read(
                fd,
                &values[i],
                1) != 1) {

            close(fd);
            return -1;
        }
    }

    fprintf(
        stderr,
        "flip_audio: RK817 state "
        "17=%02x 38=%02x 40=%02x\n",
        values[0],
        values[1],
        values[2]);

    /*
     * Do nothing unless this is the exact state observed
     * during silent playback following RK3566 deep suspend.
     */
    if (values[0] != 0x06 ||
        values[1] != 0x00 ||
        values[2] != 0x69) {

        close(fd);
        return 0;
    }

    fprintf(
        stderr,
        "flip_audio: RK817 deep-resume "
        "recovery required\n");

    const unsigned char targets[3] = {
        0x40,
        0x10,
        0xa5
    };

    for (int i = 0;
         i < 3;
         ++i) {

        unsigned char data[2] = {
            regs[i],
            targets[i]
        };

        if (write(
                fd,
                data,
                sizeof(data)) !=
            sizeof(data)) {

            close(fd);
            return -1;
        }

        usleep(10000);
    }

    close(fd);

    fprintf(
        stderr,
        "flip_audio: RK817 deep-resume "
        "recovery applied\n");

    return 1;
}

static int configure_speaker(void)
{
    /*
     * RK817 can require a short settling period after the
     * RK3566 resumes from deep suspend.
     *
     * Reassert the complete speaker route and retry before
     * opening PCM.
     */
    for (int attempt = 1;
         attempt <= 5;
         ++attempt) {

        int rc = 0;

        rc |= system(
            "/usr/bin/amixer sset 'Speaker' on "
            ">/dev/null 2>&1");

        rc |= system(
            "/usr/bin/amixer sset 'Playback Path' SPK "
            ">/dev/null 2>&1");

        char command[128];

        snprintf(
            command,
            sizeof(command),
            "/usr/bin/amixer sset 'SPK' %d%% "
            ">/dev/null 2>&1",
            speaker_volume);

        rc |= system(command);

        if (rc == 0) {

            fprintf(
                stderr,
                "flip_audio: speaker configured "
                "attempt=%d volume=%d\n",
                attempt,
                speaker_volume);

            return 0;
        }

        fprintf(
            stderr,
            "flip_audio: speaker configure retry=%d\n",
            attempt);

        usleep(100000);
    }

    return -1;
}

int flip_audio_open(void)
{
    if (audio_fd >= 0)
        return 0;

    flip_settings_t settings;

    if (flip_settings_load(
            &settings) >= 0) {

        speaker_volume =
            settings.volume;
    }

    fprintf(
        stderr,
        "flip_audio: global volume=%d\n",
        speaker_volume);

    /*
     * Confirmed on the real Miyoo Flip:
     *
     *   ALSA hw:0,0
     *   RK817 internal codec
     *   S16_LE
     *   stereo
     *   32768 Hz
     */
    if (configure_speaker() < 0) {
        fprintf(
            stderr,
            "flip_audio: speaker mixer setup failed\n");
    }

    int pipefd[2];

    if (pipe(pipefd) < 0) {
        fprintf(
            stderr,
            "flip_audio: pipe failed: %s\n",
            strerror(errno));
        return -1;
    }

    /*
     * Do not let a dead aplay process terminate the emulator
     * with SIGPIPE.
     */
    signal(SIGPIPE, SIG_IGN);

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(
            stderr,
            "flip_audio: fork failed: %s\n",
            strerror(errno));

        close(pipefd[0]);
        close(pipefd[1]);

        return -1;
    }

    if (pid == 0) {

        close(pipefd[1]);

        if (dup2(
                pipefd[0],
                STDIN_FILENO) < 0) {
            _exit(126);
        }

        close(pipefd[0]);

        execl(
            "/usr/bin/aplay",
            "aplay",
            "-q",
            "-D",
            "hw:0,0",
            "-t",
            "raw",
            "-f",
            "S16_LE",
            "-c",
            "2",
            "-r",
            "32768",
            "-B",
            "50000",
            "-F",
            "10000",
            (char *)NULL);

        _exit(127);
    }

    close(pipefd[0]);

    audio_fd = pipefd[1];
    audio_pid = pid;

    /*
     * Give RK817/ASoC time to establish the playback path,
     * then repair the confirmed deep-resume codec state if
     * necessary.
     */
    usleep(100000);

    int recovery =
        rk817_codec_resume_recover();

    if (recovery < 0) {

        fprintf(
            stderr,
            "flip_audio: RK817 recovery check failed\n");

    } else if (recovery > 0) {

        fprintf(
            stderr,
            "flip_audio: RK817 audio recovered\n");
    }

    fprintf(
        stderr,
        "flip_audio: OPEN_OK pid=%d "
        "device=hw:0,0 format=S16_LE "
        "channels=2 rate=32768\n",
        (int)audio_pid);

    return 0;
}

size_t flip_audio_write(
    const int16_t *samples,
    size_t frames)
{
    if (audio_fd < 0 ||
        !samples ||
        frames == 0)
        return 0;

    /*
     * Stereo S16_LE:
     * 2 samples per frame, 2 bytes per sample.
     */
    const unsigned char *ptr =
        (const unsigned char *)samples;

    size_t total =
        frames * 2 * sizeof(int16_t);

    size_t remaining =
        total;

    while (remaining > 0) {

        ssize_t written =
            write(
                audio_fd,
                ptr,
                remaining);

        if (written > 0) {
            ptr += written;
            remaining -= (size_t)written;
            continue;
        }

        if (written < 0 &&
            errno == EINTR)
            continue;

        if (written < 0 &&
            errno == EPIPE) {

            fprintf(
                stderr,
                "flip_audio: aplay pipe closed\n");

            close(audio_fd);
            audio_fd = -1;

            return 0;
        }

        fprintf(
            stderr,
            "flip_audio: write failed: %s\n",
            strerror(errno));

        return 0;
    }

    return frames;
}


int flip_audio_set_volume(int percent)
{
    if (percent < 0)
        percent = 0;

    if (percent > 100)
        percent = 100;

    char command[128];

    snprintf(
        command,
        sizeof(command),
        "/usr/bin/amixer sset 'SPK' %d%% "
        ">/dev/null 2>&1",
        percent);

    if (system(command) != 0) {
        fprintf(
            stderr,
            "flip_audio: volume set failed\n");

        return -1;
    }

    speaker_volume = percent;

    if (flip_settings_set_volume(
            speaker_volume) < 0) {

        fprintf(
            stderr,
            "flip_audio: volume persistence failed\n");
    }

    fprintf(
        stderr,
        "flip_audio: volume=%d\n",
        speaker_volume);

    return 0;
}

int flip_audio_get_volume(void)
{
    return speaker_volume;
}

void flip_audio_close(void)
{
    if (audio_fd >= 0) {
        close(audio_fd);
        audio_fd = -1;
    }

    if (audio_pid > 0) {

        int status = 0;

        while (waitpid(
                   audio_pid,
                   &status,
                   0) < 0) {

            if (errno == EINTR)
                continue;

            break;
        }

        if (WIFEXITED(status)) {
            fprintf(
                stderr,
                "flip_audio: aplay exit=%d\n",
                WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(
                stderr,
                "flip_audio: aplay signal=%d\n",
                WTERMSIG(status));
        }

        audio_pid = -1;
    }
}
