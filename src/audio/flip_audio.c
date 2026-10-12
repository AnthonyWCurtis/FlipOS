#define _GNU_SOURCE

#include "flip_audio.h"
#include "flip_audio_buffer.h"
#include "flip_audio_queue.h"
#include "flip_audio_resampler.h"
#include "flip_audio_transport.h"
#include "flip_settings.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <sound/asound.h>
#include <pthread.h>
#include <time.h>

#define ALSA_CONTROL_DEVICE "/dev/snd/controlC0"
#define ALSA_NUMID_PLAYBACK_PATH 2
#define ALSA_NUMID_SPK_VOLUME 5
#define ALSA_NUMID_SPEAKER_SWITCH 7
#define ALSA_PLAYBACK_PATH_OFF 0
#define ALSA_PLAYBACK_PATH_SPK 2
#define AUDIO_PIPE_WRITE_TIMEOUT_MS 250
#define AUDIO_CHILD_EXIT_GRACE_MS 250
#define AUDIO_CHILD_TERM_GRACE_MS 250
#define AUDIO_CHILD_KILL_GRACE_MS 250

static atomic_int audio_fd = ATOMIC_VAR_INIT(-1);
static int mixer_fd = -1;
static pid_t audio_pid = -1;
static int speaker_volume = 30;

static flip_audio_config_t audio_config;
static flip_audio_buffer_t audio_buffer;
static flip_audio_queue_t audio_queue;
static flip_audio_resampler_t audio_resampler;

static pthread_t audio_writer_thread;
static pthread_mutex_t audio_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t audio_diag_mutex = PTHREAD_MUTEX_INITIALIZER;
static int audio_writer_started = 0;
static int audio_writer_stop = 0;
static int16_t audio_resample_buffer[
    FLIP_AUDIO_BUFFER_FRAMES *
    FLIP_AUDIO_BUFFER_CHANNELS];

static flip_audio_diag_snapshot_t audio_diag;

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
    uint64_t elapsed =
        audio_diag_elapsed_us(start_us);

    pthread_mutex_lock(
        &audio_diag_mutex);

    audio_diag.transport_write_total_us += elapsed;
    audio_diag.transport_write_count++;

    if (elapsed > audio_diag.transport_write_max_us)
        audio_diag.transport_write_max_us = elapsed;

    pthread_mutex_unlock(
        &audio_diag_mutex);
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

static void alsa_control_close(void)
{
    if (mixer_fd >= 0) {
        close(mixer_fd);
        mixer_fd = -1;
    }
}

static int alsa_control_fd(void)
{
    if (mixer_fd >= 0)
        return mixer_fd;

    mixer_fd = open(ALSA_CONTROL_DEVICE, O_RDWR | O_CLOEXEC);
    return mixer_fd;
}

static int alsa_control_write_integer(unsigned numid, long value)
{
    int fd = alsa_control_fd();
    if (fd < 0)
        return -1;

    struct snd_ctl_elem_value control;
    memset(&control, 0, sizeof(control));
    control.id.numid = numid;
    control.value.integer.value[0] = value;

    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &control) < 0) {
        /* A resume or driver reset may invalidate the cached control fd. */
        alsa_control_close();
        return -1;
    }

    return 0;
}

static int alsa_control_write_enumerated(unsigned numid, unsigned item)
{
    int fd = alsa_control_fd();
    if (fd < 0)
        return -1;

    struct snd_ctl_elem_value control;
    memset(&control, 0, sizeof(control));
    control.id.numid = numid;
    control.value.enumerated.item[0] = item;

    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &control) < 0) {
        alsa_control_close();
        return -1;
    }

    return 0;
}

static int configure_speaker(void)
{
    /*
     * Reassert the RK817 speaker route at audio-open time. Keep this path
     * process-free as well: it makes startup/resume deterministic and leaves
     * one mixer implementation for both initial setup and hotkey changes.
     */
    for (int attempt = 1; attempt <= 5; ++attempt) {
        int rc = 0;

        if (speaker_volume == 0) {
            rc |= alsa_control_write_integer(ALSA_NUMID_SPK_VOLUME, 0);
            rc |= alsa_control_write_enumerated(
                ALSA_NUMID_PLAYBACK_PATH,
                ALSA_PLAYBACK_PATH_OFF);
            rc |= alsa_control_write_integer(ALSA_NUMID_SPEAKER_SWITCH, 0);
        } else {
            rc |= alsa_control_write_enumerated(
                ALSA_NUMID_PLAYBACK_PATH,
                ALSA_PLAYBACK_PATH_SPK);
            rc |= alsa_control_write_integer(ALSA_NUMID_SPEAKER_SWITCH, 1);
            /* Set gain last so route enable can never expose full-scale gain. */
            rc |= alsa_control_write_integer(
                ALSA_NUMID_SPK_VOLUME,
                speaker_volume);
        }

        if (rc == 0) {
            fprintf(
                stderr,
                "flip_audio: speaker configured attempt=%d volume=%d\n",
                attempt,
                speaker_volume);
            return 0;
        }

        fprintf(
            stderr,
            "flip_audio: speaker configure retry=%d\n",
            attempt);

        alsa_control_close();
        usleep(100000);
    }

    return -1;
}

static int audio_pipe_write(
    const int16_t *samples,
    size_t frames,
    void *userdata)
{
    (void)userdata;

    int fd =
        atomic_load_explicit(
            &audio_fd,
            memory_order_acquire);

    if (fd < 0 ||
        !samples ||
        frames == 0)
        return -1;

    size_t bytes =
        frames *
        FLIP_AUDIO_BUFFER_CHANNELS *
        sizeof(int16_t);

    uint64_t transport_start_us =
        audio_diag_monotonic_us();

    if (flip_audio_transport_write_all(
            fd,
            samples,
            bytes,
            AUDIO_PIPE_WRITE_TIMEOUT_MS) < 0) {

        int write_errno = errno;

        if (write_errno == ETIMEDOUT) {
            fprintf(
                stderr,
                "flip_audio: aplay transport stalled for %d ms; disabling audio\n",
                AUDIO_PIPE_WRITE_TIMEOUT_MS);
        } else if (write_errno == EPIPE) {
            fprintf(
                stderr,
                "flip_audio: aplay pipe closed\n");
        } else {
            fprintf(
                stderr,
                "flip_audio: write failed: %s\n",
                strerror(write_errno));
        }

        int expected_fd = fd;
        if (atomic_compare_exchange_strong_explicit(
                &audio_fd,
                &expected_fd,
                -1,
                memory_order_acq_rel,
                memory_order_acquire)) {
            close(fd);
        }

        audio_diag_record_transport_write(
            transport_start_us);

        errno = write_errno;
        return -1;
    }

    audio_diag_record_transport_write(
        transport_start_us);

    return 0;
}

static int audio_queue_sink(
    const int16_t *samples,
    size_t frames,
    void *userdata)
{
    (void)userdata;

    if (!samples ||
        frames == 0)
        return -1;

    int16_t silent[
        FLIP_AUDIO_BUFFER_FRAMES *
        FLIP_AUDIO_BUFFER_CHANNELS];

    const int16_t *queued_samples =
        samples;

    if (speaker_volume == 0) {
        size_t sample_count =
            frames *
            FLIP_AUDIO_BUFFER_CHANNELS;

        if (sample_count >
            FLIP_AUDIO_BUFFER_FRAMES *
            FLIP_AUDIO_BUFFER_CHANNELS) {

            return -1;
        }

        memset(
            silent,
            0,
            sample_count *
                sizeof(int16_t));

        queued_samples =
            silent;
    }

    pthread_mutex_lock(
        &audio_queue_mutex);

    size_t accepted =
        flip_audio_queue_write(
            &audio_queue,
            queued_samples,
            frames);

    size_t peak_frames =
        audio_queue.peak_frames;

    pthread_cond_signal(
        &audio_queue_cond);

    pthread_mutex_unlock(
        &audio_queue_mutex);

    pthread_mutex_lock(
        &audio_diag_mutex);

    if (accepted < frames) {
        audio_diag.queue_dropped_frames +=
            frames - accepted;
    }

    if (peak_frames >
        audio_diag.queue_peak_frames) {

        audio_diag.queue_peak_frames =
            peak_frames;
    }

    pthread_mutex_unlock(
        &audio_diag_mutex);

    /*
     * A full queue intentionally drops the newest samples.
     * Output transport must never stall the emulation clock.
     */
    return 0;
}

static void *audio_writer_main(
    void *userdata)
{
    (void)userdata;

    int16_t chunk[
        FLIP_AUDIO_BUFFER_FRAMES *
        FLIP_AUDIO_BUFFER_CHANNELS];

    for (;;) {
        pthread_mutex_lock(
            &audio_queue_mutex);

        while (flip_audio_queue_frames(
                   &audio_queue) == 0 &&
               !audio_writer_stop) {

            pthread_cond_wait(
                &audio_queue_cond,
                &audio_queue_mutex);
        }

        if (flip_audio_queue_frames(
                &audio_queue) == 0 &&
            audio_writer_stop) {

            pthread_mutex_unlock(
                &audio_queue_mutex);
            break;
        }

        size_t frames =
            flip_audio_queue_read(
                &audio_queue,
                chunk,
                FLIP_AUDIO_BUFFER_FRAMES);

        pthread_mutex_unlock(
            &audio_queue_mutex);

        if (frames == 0)
            continue;

        if (audio_pipe_write(
                chunk,
                frames,
                NULL) < 0) {

            break;
        }
    }

    return NULL;
}

static int audio_writer_start(void)
{
    pthread_mutex_lock(
        &audio_queue_mutex);

    flip_audio_queue_init(
        &audio_queue);

    audio_writer_stop = 0;

    pthread_mutex_unlock(
        &audio_queue_mutex);

    int rc = pthread_create(
        &audio_writer_thread,
        NULL,
        audio_writer_main,
        NULL);

    if (rc != 0) {
        fprintf(
            stderr,
            "flip_audio: writer thread create failed: %s\n",
            strerror(rc));

        return -1;
    }

    audio_writer_started = 1;

    fprintf(
        stderr,
        "flip_audio: async writer started queue_frames=%d\n",
        FLIP_AUDIO_QUEUE_FRAMES);

    return 0;
}

static void audio_writer_stop_and_join(void)
{
    if (!audio_writer_started)
        return;

    pthread_mutex_lock(
        &audio_queue_mutex);

    audio_writer_stop = 1;

    pthread_cond_signal(
        &audio_queue_cond);

    pthread_mutex_unlock(
        &audio_queue_mutex);

    (void)pthread_join(
        audio_writer_thread,
        NULL);

    audio_writer_started = 0;
    audio_writer_stop = 0;

    pthread_mutex_lock(
        &audio_queue_mutex);

    flip_audio_queue_reset(
        &audio_queue);

    pthread_mutex_unlock(
        &audio_queue_mutex);
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

    if (atomic_load_explicit(&audio_fd, memory_order_acquire) >= 0) {

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

    speaker_volume =
        flip_settings_get_volume();

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

    if (flip_audio_transport_set_nonblocking(pipefd[1]) < 0) {
        fprintf(
            stderr,
            "flip_audio: could not make aplay pipe non-blocking: %s\n",
            strerror(errno));

        close(pipefd[1]);

        int status = 0;
        (void)flip_audio_transport_shutdown_child(
            pid,
            0,
            AUDIO_CHILD_TERM_GRACE_MS,
            AUDIO_CHILD_KILL_GRACE_MS,
            &status);

        flip_audio_resampler_reset(
            &audio_resampler);

        return -1;
    }

    atomic_store_explicit(
        &audio_fd,
        pipefd[1],
        memory_order_release);
    audio_pid = pid;
    audio_config = *config;

    flip_audio_buffer_init(
        &audio_buffer);

    flip_audio_diag_reset();

    if (audio_writer_start() < 0) {
        int fd =
            atomic_exchange_explicit(
                &audio_fd,
                -1,
                memory_order_acq_rel);
        if (fd >= 0)
            close(fd);

        int status = 0;
        (void)flip_audio_transport_shutdown_child(
            audio_pid,
            0,
            AUDIO_CHILD_TERM_GRACE_MS,
            AUDIO_CHILD_KILL_GRACE_MS,
            &status);

        audio_pid = -1;

        flip_audio_resampler_reset(
            &audio_resampler);

        return -1;
    }

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
    if (atomic_load_explicit(&audio_fd, memory_order_acquire) < 0 ||
        !samples ||
        frames == 0)
        return 0;

    int sink_failed = 0;
    size_t source_accepted = 0;

    uint64_t submit_start_us =
        audio_diag_monotonic_us();

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
                    audio_queue_sink,
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

    uint64_t submit_us =
        audio_diag_elapsed_us(submit_start_us);

    pthread_mutex_lock(
        &audio_diag_mutex);

    audio_diag.submit_total_us += submit_us;
    audio_diag.submit_count++;

    if (submit_us > audio_diag.submit_max_us)
        audio_diag.submit_max_us = submit_us;

    pthread_mutex_unlock(
        &audio_diag_mutex);

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

int flip_audio_set_source_rate(
    unsigned source_sample_rate)
{
    if (source_sample_rate == 0)
        return -1;

    /*
     * Libretro cores can report timing changes while a game is running.
     * If audio is not open yet, Core Host will use the latest rate from
     * the bridge when it creates the Audio Host.
     */
    if (atomic_load_explicit(&audio_fd, memory_order_acquire) < 0)
        return 0;

    if (audio_config.source_sample_rate ==
        source_sample_rate) {

        return 0;
    }

    unsigned previous_rate =
        audio_config.source_sample_rate;

    if (flip_audio_resampler_init(
            &audio_resampler,
            source_sample_rate,
            audio_config.output_sample_rate,
            audio_config.channels) < 0) {

        return -1;
    }

    flip_audio_buffer_reset(
        &audio_buffer);

    audio_config.source_sample_rate =
        source_sample_rate;

    fprintf(
        stderr,
        "flip_audio: source_rate changed %u->%u\n",
        previous_rate,
        source_sample_rate);

    return 0;
}

int flip_audio_end_frame(void)
{
    if (atomic_load_explicit(&audio_fd, memory_order_acquire) < 0) {
        flip_audio_buffer_reset(
            &audio_buffer);
        flip_audio_resampler_reset(
            &audio_resampler);
        return 0;
    }

    if (flip_audio_buffer_flush(
            &audio_buffer,
            audio_queue_sink,
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

    pthread_mutex_lock(
        &audio_diag_mutex);

    *snapshot = audio_diag;

    pthread_mutex_unlock(
        &audio_diag_mutex);
}

void flip_audio_diag_reset(void)
{
    pthread_mutex_lock(
        &audio_diag_mutex);

    memset(
        &audio_diag,
        0,
        sizeof(audio_diag));

    pthread_mutex_unlock(
        &audio_diag_mutex);
}

int flip_audio_set_volume(int percent)
{
    if (percent < 0)
        percent = 0;

    if (percent > 100)
        percent = 100;

    int old_volume = speaker_volume;
    int rc = 0;

    /*
     * Volume hotkeys run on the emulation thread. Do not spawn amixer or any
     * other process here: fork/exec and mixer route reconstruction can stall a
     * frame and sound like a game slowdown. The RK817 controls are simple ALSA
     * controls, so write them directly through /dev/snd/controlC0.
     */
    if (percent > 0 && old_volume > 0) {
        rc = alsa_control_write_integer(ALSA_NUMID_SPK_VOLUME, percent);
    } else if (percent == 0 && old_volume > 0) {
        pthread_mutex_lock(&audio_queue_mutex);
        flip_audio_queue_reset(&audio_queue);
        pthread_mutex_unlock(&audio_queue_mutex);
        flip_audio_buffer_reset(&audio_buffer);

        rc |= alsa_control_write_integer(ALSA_NUMID_SPK_VOLUME, 0);
        rc |= alsa_control_write_enumerated(
            ALSA_NUMID_PLAYBACK_PATH,
            ALSA_PLAYBACK_PATH_OFF);
        rc |= alsa_control_write_integer(ALSA_NUMID_SPEAKER_SWITCH, 0);
    } else if (percent > 0 && old_volume == 0) {
        /* Keep digital output silent until the physical speaker path is ready. */
        rc |= alsa_control_write_enumerated(
            ALSA_NUMID_PLAYBACK_PATH,
            ALSA_PLAYBACK_PATH_SPK);
        rc |= alsa_control_write_integer(ALSA_NUMID_SPEAKER_SWITCH, 1);
        rc |= alsa_control_write_integer(ALSA_NUMID_SPK_VOLUME, percent);
    }

    if (rc != 0) {
        fprintf(
            stderr,
            "flip_audio: direct volume control failed\n");
        return -1;
    }

    speaker_volume = percent;

    if (flip_settings_set_volume(speaker_volume) < 0) {
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
    if (atomic_load_explicit(&audio_fd, memory_order_acquire) >= 0) {

        if (flip_audio_buffer_flush(
                &audio_buffer,
                audio_queue_sink,
                NULL) < 0) {

            fprintf(
                stderr,
                "flip_audio: final queue flush failed\n");
        }
    }

    /*
     * Drain queued PCM while the transport remains healthy. The PCM pipe is
     * non-blocking and each transport write has a hard deadline, so a wedged
     * aplay/ALSA path cannot hold pthread_join() indefinitely.
     */
    audio_writer_stop_and_join();

    int fd =
        atomic_exchange_explicit(
            &audio_fd,
            -1,
            memory_order_acq_rel);
    if (fd >= 0)
        close(fd);

    flip_audio_buffer_reset(
        &audio_buffer);
    flip_audio_resampler_reset(
        &audio_resampler);

    if (audio_pid > 0) {

        int status = 0;

        if (flip_audio_transport_shutdown_child(
                audio_pid,
                AUDIO_CHILD_EXIT_GRACE_MS,
                AUDIO_CHILD_TERM_GRACE_MS,
                AUDIO_CHILD_KILL_GRACE_MS,
                &status) < 0) {

            fprintf(
                stderr,
                "flip_audio: aplay could not be reaped within shutdown deadline: %s\n",
                strerror(errno));

        } else if (WIFEXITED(status)) {
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

        /*
         * Never block emulator shutdown forever for a child stuck in an
         * uninterruptible kernel state. If the bounded SIGKILL wait failed,
         * init will reap the orphan when this emulator process exits.
         */
        audio_pid = -1;
    }

    alsa_control_close();

    memset(
        &audio_config,
        0,
        sizeof(audio_config));
}
