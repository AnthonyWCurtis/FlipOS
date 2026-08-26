#define _GNU_SOURCE

#include "flip_audio.h"
#include "flip_audio_buffer.h"
#include "flip_audio_resampler.h"
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
#include <time.h>

static int audio_fd = -1;
static pid_t audio_pid = -1;
static int speaker_volume = 30;

static flip_audio_config_t audio_config;
static flip_audio_buffer_t audio_buffer;
static flip_audio_resampler_t audio_resampler;
static int16_t audio_resample_buffer[
    FLIP_AUDIO_BUFFER_FRAMES *
    FLIP_AUDIO_BUFFER_CHANNELS];

static flip_audio_diag_snapshot_t audio_diag;
static int audio_diag_submit_active = 0;

static uint64_t audio_diag_monotonic_us(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;

    return
        (uint64_t)now.tv_sec * 1000000ULL +
        (uint64_t)now.tv_nsec / 1000ULL;
}

static uint64_t audio_diag_elapsed_us(uint64_t start_us)
{
    uint64_t now_us = audio_diag_monotonic_us();

    if (now_us < start_us)
        return 0;

    return now_us - start_us;
}

static void audio_diag_record_transport_write(
    uint64_t start_us)
{
    if (!audio_diag_submit_active)
        return;

    uint64_t elapsed =
        audio_diag_elapsed_us(start_us);

    audio_diag.transport_write_total_us += elapsed;
    audio_diag.transport_write_count++;

    if (elapsed > audio_diag.transport_write_max_us)
        audio_diag.transport_write_max_us = elapsed;
}

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

static int audio_transport_write(
    const int16_t *samples,
    size_t frames,
    void *userdata)
{
    (void)userdata;

    if (audio_fd < 0 ||
        !samples ||
        frames == 0)
        return -1;

    /*
     * Stereo S16_LE:
     * 2 samples per frame, 2 bytes per sample.
     */
    const unsigned char *ptr =
        (const unsigned char *)samples;

    size_t remaining =
        frames *
        FLIP_AUDIO_BUFFER_CHANNELS *
        sizeof(int16_t);

    uint64_t transport_start_us =
        audio_diag_monotonic_us();

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

            audio_diag_record_transport_write(
                transport_start_us);

            return -1;
        }

        fprintf(
            stderr,
            "flip_audio: write failed: %s\n",
            strerror(errno));

        close(audio_fd);
        audio_fd = -1;

        audio_diag_record_transport_write(
            transport_start_us);

        return -1;
    }

    audio_diag_record_transport_write(
        transport_start_us);

    return 0;
}

int flip_audio_open(
    const flip_audio_config_t *config)
{
    if (!config ||
        config->source_sample_rate == 0 ||
        config->output_sample_rate != 32000 ||
        config->channels != 2) {

        fprintf(
            stderr,
            "flip_audio: unsupported config "
            "source_rate=%u output_rate=%u channels=%u\n",
            config
                ? config->source_sample_rate
                : 0,
            config
                ? config->output_sample_rate
                : 0,
            config
                ? config->channels
                : 0);

        return -1;
    }

    if (audio_fd >= 0) {

        if (audio_config.source_sample_rate ==
                config->source_sample_rate &&
            audio_config.output_sample_rate ==
                config->output_sample_rate &&
            audio_config.channels ==
                config->channels) {

            return 0;
        }

        fprintf(
            stderr,
            "flip_audio: already open with "
            "different configuration\n");

        return -1;
    }

    if (flip_audio_resampler_init(
            &audio_resampler,
            config->source_sample_rate,
            config->output_sample_rate,
            config->channels) < 0) {

        fprintf(
            stderr,
            "flip_audio: resampler initialization failed\n");

        return -1;
    }

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
     *   32000 Hz physical PCM rate
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
        flip_audio_resampler_reset(
            &audio_resampler);
        return -1;
    }

    /*
     * Do not let a dead aplay process terminate the emulator
     * with SIGPIPE.
     */
    signal(SIGPIPE, SIG_IGN);

    char output_rate_text[16];

    snprintf(
        output_rate_text,
        sizeof(output_rate_text),
        "%u",
        config->output_sample_rate);

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(
            stderr,
            "flip_audio: fork failed: %s\n",
            strerror(errno));

        close(pipefd[0]);
        close(pipefd[1]);
        flip_audio_resampler_reset(
            &audio_resampler);

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
            output_rate_text,
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
    audio_config = *config;

    flip_audio_buffer_init(
        &audio_buffer);

    flip_audio_diag_reset();

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
        "channels=%u source_rate=%u output_rate=%u\n",
        (int)audio_pid,
        audio_config.channels,
        audio_config.source_sample_rate,
        audio_config.output_sample_rate);

    return 0;
}

size_t flip_audio_submit(
    const int16_t *samples,
    size_t frames)
{
    if (audio_fd < 0 ||
        !samples ||
        frames == 0)
        return 0;

    int sink_failed = 0;
    size_t source_accepted = 0;

    uint64_t submit_start_us =
        audio_diag_monotonic_us();

    audio_diag_submit_active = 1;

    while (source_accepted < frames &&
           !sink_failed) {

        size_t consumed = 0;

        size_t produced =
            flip_audio_resampler_process(
                &audio_resampler,
                samples +
                    source_accepted *
                    FLIP_AUDIO_BUFFER_CHANNELS,
                frames - source_accepted,
                &consumed,
                audio_resample_buffer,
                FLIP_AUDIO_BUFFER_FRAMES);

        if (produced > 0) {
            size_t output_accepted =
                flip_audio_buffer_submit(
                    &audio_buffer,
                    audio_resample_buffer,
                    produced,
                    audio_transport_write,
                    NULL,
                    &sink_failed);

            if (output_accepted != produced)
                sink_failed = 1;
        }

        source_accepted += consumed;

        if (consumed == 0 &&
            produced == 0) {

            fprintf(
                stderr,
                "flip_audio: resampler made no progress\n");
            sink_failed = 1;
        }
    }

    audio_diag_submit_active = 0;

    uint64_t submit_us =
        audio_diag_elapsed_us(submit_start_us);

    audio_diag.submit_total_us += submit_us;
    audio_diag.submit_count++;

    if (submit_us > audio_diag.submit_max_us)
        audio_diag.submit_max_us = submit_us;

    if (sink_failed) {

        fprintf(
            stderr,
            "flip_audio: transport failed; "
            "continuing silent\n");

        flip_audio_buffer_reset(
            &audio_buffer);
        flip_audio_resampler_reset(
            &audio_resampler);
    }

    return source_accepted;
}

int flip_audio_end_frame(void)
{
    if (audio_fd < 0) {
        flip_audio_buffer_reset(
            &audio_buffer);
        flip_audio_resampler_reset(
            &audio_resampler);
        return 0;
    }

    if (flip_audio_buffer_flush(
            &audio_buffer,
            audio_transport_write,
            NULL) < 0) {

        flip_audio_buffer_reset(
            &audio_buffer);
        flip_audio_resampler_reset(
            &audio_resampler);
        return -1;
    }

    return 0;
}

void flip_audio_diag_snapshot(
    flip_audio_diag_snapshot_t *snapshot)
{
    if (!snapshot)
        return;

    *snapshot = audio_diag;
}

void flip_audio_diag_reset(void)
{
    memset(
        &audio_diag,
        0,
        sizeof(audio_diag));

    audio_diag_submit_active = 0;
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

        if (flip_audio_buffer_flush(
                &audio_buffer,
                audio_transport_write,
                NULL) < 0) {

            fprintf(
                stderr,
                "flip_audio: final flush failed\n");
        }

        if (audio_fd >= 0) {
            close(audio_fd);
            audio_fd = -1;
        }
    }

    flip_audio_buffer_reset(
        &audio_buffer);
    flip_audio_resampler_reset(
        &audio_resampler);

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

    memset(
        &audio_config,
        0,
        sizeof(audio_config));
}
