#define _GNU_SOURCE

#include "flip_input.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DIGITAL_DEVICE "/dev/input/event0"
#define HALL_DEVICE    "/dev/input/event1"
#define HALL_VALUE     "/sys/devices/platform/hall-mh248/hallvalue"
#define DEV_LID_OVERRIDE "/tmp/flipos-dev-lid-override"
#define DEV_BUTTON_INJECT "/tmp/flipos-dev-button"
#define UART_DEVICE    "/dev/ttyS1"

#define UART_START 0xff
#define UART_END   0xfe

#define CALIBRATION_FRAMES 60

static int digital_fd = -1;
static int hall_fd = -1;
static int uart_fd = -1;

static uint8_t uart_buffer[6];
static int uart_pos = 0;
static uint64_t dev_button_release_mask = 0;

static long cal_lx;
static long cal_ly;
static long cal_rx;
static long cal_ry;
static int cal_frames;

static int code_to_button(unsigned code)
{
    switch (code) {
        case 103: return FLIP_BTN_UP;
        case 108: return FLIP_BTN_DOWN;
        case 105: return FLIP_BTN_LEFT;
        case 106: return FLIP_BTN_RIGHT;

        case 57:  return FLIP_BTN_A;
        case 29:  return FLIP_BTN_B;
        case 42:  return FLIP_BTN_X;
        case 56:  return FLIP_BTN_Y;

        case 15:  return FLIP_BTN_L1;
        case 14:  return FLIP_BTN_R1;
        case 104: return FLIP_BTN_L2;
        case 109: return FLIP_BTN_R2;

        case 28:  return FLIP_BTN_START;
        case 97:  return FLIP_BTN_SELECT;
        case 1:   return FLIP_BTN_MENU;

        /*
         * Linux KEY_POWER on the digital keypad device. The hall sensor also
         * reports code 116, but it is read from HALL_DEVICE separately and
         * never passes through this digital mapping.
         */
        case 116: return FLIP_BTN_POWER;

        case 100: return FLIP_BTN_L3;
        case 54:  return FLIP_BTN_R3;

        case 115: return FLIP_BTN_VOL_UP;
        case 114: return FLIP_BTN_VOL_DOWN;

        default:
            return -1;
    }
}

static int dev_button_name_to_button(const char *name)
{
    if (!name)
        return -1;
    static const struct { const char *name; int button; } map[] = {
        {"UP", FLIP_BTN_UP}, {"DOWN", FLIP_BTN_DOWN},
        {"LEFT", FLIP_BTN_LEFT}, {"RIGHT", FLIP_BTN_RIGHT},
        {"A", FLIP_BTN_A}, {"B", FLIP_BTN_B}, {"X", FLIP_BTN_X}, {"Y", FLIP_BTN_Y},
        {"L1", FLIP_BTN_L1}, {"R1", FLIP_BTN_R1}, {"L2", FLIP_BTN_L2}, {"R2", FLIP_BTN_R2},
        {"START", FLIP_BTN_START}, {"SELECT", FLIP_BTN_SELECT}, {"MENU", FLIP_BTN_MENU},
        {"POWER", FLIP_BTN_POWER},
        {"L3", FLIP_BTN_L3}, {"R3", FLIP_BTN_R3},
        {"VOL+", FLIP_BTN_VOL_UP}, {"VOL-", FLIP_BTN_VOL_DOWN},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
        if (strcmp(name, map[i].name) == 0)
            return map[i].button;
    return -1;
}

static int apply_dev_button_injection(flip_input_state_t *state)
{
    if (!state)
        return 0;

    int changed = 0;

    /* Release every button injected on the previous poll. */
    for (int button = 0; button < FLIP_BTN_COUNT; ++button) {
        if (!(dev_button_release_mask & (1ULL << button)))
            continue;
        if (state->down[button]) {
            state->down[button] = 0;
            changed = 1;
        }
    }
    dev_button_release_mask = 0;

    FILE *f = fopen(DEV_BUTTON_INJECT, "r");
    if (!f)
        return changed;

    /*
     * One or more whitespace-separated names may be supplied. Supporting a
     * chord here is useful for development-only hotkeys such as MENU VOL- for
     * brightness while keeping the production input path untouched.
     */
    char value[128] = {0};
    if (fgets(value, sizeof(value), f)) {
        char *save = NULL;
        for (char *name = strtok_r(value, " \t\r\n", &save);
             name;
             name = strtok_r(NULL, " \t\r\n", &save)) {
            int button = dev_button_name_to_button(name);
            if (button < 0 || button >= FLIP_BTN_COUNT || button >= 64)
                continue;
            state->down[button] = 1;
            dev_button_release_mask |= 1ULL << button;
            changed = 1;
        }
    }

    fclose(f);
    (void)unlink(DEV_BUTTON_INJECT);
    return changed;
}

const char *flip_button_name(flip_button_t button)
{
    static const char *names[FLIP_BTN_COUNT] = {
        "UP",
        "DOWN",
        "LEFT",
        "RIGHT",

        "A",
        "B",
        "X",
        "Y",

        "L1",
        "R1",
        "L2",
        "R2",

        "START",
        "SELECT",
        "MENU",
        "POWER",

        "L3",
        "R3",

        "VOL+",
        "VOL-"
    };

    if (button < 0 || button >= FLIP_BTN_COUNT)
        return "UNKNOWN";

    return names[button];
}

static int configure_uart(int fd)
{
    struct termios t;

    if (tcgetattr(fd, &t) < 0)
        return -1;

    cfmakeraw(&t);

    cfsetispeed(&t, B9600);
    cfsetospeed(&t, B9600);

    t.c_cflag &= ~CSIZE;
    t.c_cflag |= CS8 | CLOCAL | CREAD;

    t.c_cflag &= ~PARENB;
    t.c_cflag &= ~CSTOPB;

#ifdef CRTSCTS
    t.c_cflag &= ~CRTSCTS;
#endif

    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;

    tcflush(fd, TCIFLUSH);

    return tcsetattr(fd, TCSANOW, &t);
}

int flip_input_open(void)
{
    digital_fd =
        open(DIGITAL_DEVICE, O_RDONLY | O_NONBLOCK);

    if (digital_fd < 0) {
        fprintf(stderr,
                "flip_input: cannot open %s: %s\n",
                DIGITAL_DEVICE,
                strerror(errno));
        return -1;
    }

    hall_fd =
        open(HALL_DEVICE, O_RDONLY | O_NONBLOCK);

    if (hall_fd < 0) {
        fprintf(stderr,
                "flip_input: cannot open %s: %s\n",
                HALL_DEVICE,
                strerror(errno));

        close(digital_fd);
        digital_fd = -1;

        return -1;
    }

    uart_fd =
        open(UART_DEVICE,
             O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (uart_fd < 0) {
        fprintf(stderr,
                "flip_input: cannot open %s: %s\n",
                UART_DEVICE,
                strerror(errno));

        flip_input_close();
        return -1;
    }

    if (configure_uart(uart_fd) < 0) {
        fprintf(stderr,
                "flip_input: cannot configure %s: %s\n",
                UART_DEVICE,
                strerror(errno));

        flip_input_close();
        return -1;
    }

    uart_pos = 0;

    cal_lx = 0;
    cal_ly = 0;
    cal_rx = 0;
    cal_ry = 0;
    cal_frames = 0;

    return 0;
}

void flip_input_close(void)
{
    if (digital_fd >= 0) {
        close(digital_fd);
        digital_fd = -1;
    }

    if (hall_fd >= 0) {
        close(hall_fd);
        hall_fd = -1;
    }

    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
}

static int process_frame(
    flip_input_state_t *state,
    const uint8_t frame[6])
{
    /*
     * Confirmed UART format:
     *
     * FF YL XL YR XR FE
     */
    int ly = frame[1];
    int lx = frame[2];
    int ry = frame[3];
    int rx = frame[4];

    if (!state->analog_ready) {
        cal_ly += ly;
        cal_lx += lx;
        cal_ry += ry;
        cal_rx += rx;

        cal_frames++;

        if (cal_frames >= CALIBRATION_FRAMES) {
            state->centre_ly =
                (int)(cal_ly / cal_frames);

            state->centre_lx =
                (int)(cal_lx / cal_frames);

            state->centre_ry =
                (int)(cal_ry / cal_frames);

            state->centre_rx =
                (int)(cal_rx / cal_frames);

            state->raw_ly = ly;
            state->raw_lx = lx;
            state->raw_ry = ry;
            state->raw_rx = rx;

            state->analog_ready = 1;

            return 1;
        }

        return 0;
    }

    if (state->raw_ly == ly &&
        state->raw_lx == lx &&
        state->raw_ry == ry &&
        state->raw_rx == rx)
        return 0;

    state->raw_ly = ly;
    state->raw_lx = lx;
    state->raw_ry = ry;
    state->raw_rx = rx;

    return 1;
}

static int feed_uart(
    flip_input_state_t *state,
    uint8_t value)
{
    if (uart_pos == 0) {
        if (value == UART_START) {
            uart_buffer[0] = value;
            uart_pos = 1;
        }

        return 0;
    }

    uart_buffer[uart_pos++] = value;

    if (uart_pos < 6)
        return 0;

    if (uart_buffer[0] == UART_START &&
        uart_buffer[5] == UART_END) {

        uart_pos = 0;

        return process_frame(
            state,
            uart_buffer);
    }

    /*
     * Resynchronise without assuming reads are frame-aligned.
     */
    if (value == UART_START) {
        uart_buffer[0] = UART_START;
        uart_pos = 1;
    } else {
        uart_pos = 0;
    }

    return 0;
}

static int read_lid_state(
    flip_input_state_t *state)
{
    if (!state)
        return -1;

    int closed = -1;

    /*
     * Development-only lid override. /tmp is volatile and the hook is
     * completely inert unless a developer explicitly creates the file.
     * Accepted values are exactly "open" or "closed". Any malformed
     * override is ignored and the real hall sensor remains authoritative.
     */
    FILE *override = fopen(DEV_LID_OVERRIDE, "r");
    if (override) {
        char value[16] = {0};
        if (fgets(value, sizeof(value), override)) {
            char *end = strpbrk(value, "\r\n");
            if (end)
                *end = '\0';
            if (strcmp(value, "open") == 0)
                closed = 0;
            else if (strcmp(value, "closed") == 0)
                closed = 1;
        }
        fclose(override);
    }

    if (closed < 0) {
        FILE *f = fopen(HALL_VALUE, "r");
        if (!f)
            return -1;

        int raw = -1;
        int ok = fscanf(f, "%d", &raw);
        fclose(f);
        if (ok != 1)
            return -1;

        /*
         * Confirmed on Miyoo Flip:
         * hallvalue=1 -> lid OPEN
         * hallvalue=0 -> lid CLOSED
         */
        closed = raw == 0 ? 1 : 0;
    }

    int changed =
        !state->lid_ready ||
        state->lid_closed != closed;

    state->lid_closed =
        closed;

    state->lid_ready = 1;

    return changed;
}

int flip_input_poll(
    flip_input_state_t *state,
    int timeout_ms)
{
    struct pollfd fds[3];

    fds[0].fd = digital_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = hall_fd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    fds[2].fd = uart_fd;
    fds[2].events = POLLIN;
    fds[2].revents = 0;

    int result = poll(fds, 3, timeout_ms);

    if (result < 0) {
        if (errno == EINTR)
            return 0;

        return -1;
    }

    int changed = 0;

    /*
     * hallvalue is the authoritative persistent lid state.
     *
     * /dev/input/event1 is only a momentary hall/wake pulse
     * and is not guaranteed to notify both physical
     * transitions. Sample hallvalue independently on every
     * host input poll.
     */
    int lid_changed =
        read_lid_state(
            state);

    if (lid_changed > 0)
        changed = 1;

    if (fds[0].revents & POLLIN) {
        struct input_event ev;

        while (read(
                   digital_fd,
                   &ev,
                   sizeof(ev))
               == sizeof(ev)) {

            if (ev.type != EV_KEY)
                continue;

            int button =
                code_to_button(ev.code);

            if (button < 0)
                continue;

            uint8_t new_state =
                ev.value ? 1 : 0;

            if (state->down[button] != new_state) {
                state->down[button] = new_state;
                changed = 1;
            }
        }
    }

    if (fds[1].revents & POLLIN) {
        struct input_event ev;
        int hall_event = 0;

        while (read(
                   hall_fd,
                   &ev,
                   sizeof(ev))
               == sizeof(ev)) {

            if (ev.type != EV_KEY)
                continue;

            if (ev.code != 116)
                continue;

            /*
             * event1 is a momentary hall wake pulse,
             * not the persistent lid state.
             */
            hall_event = 1;
        }

        if (hall_event) {

            int lid_changed =
                read_lid_state(
                    state);

            if (lid_changed > 0)
                changed = 1;
        }
    }

    if (fds[2].revents & POLLIN) {
        uint8_t data[128];
        ssize_t count;

        while ((count =
                    read(uart_fd,
                         data,
                         sizeof(data))) > 0) {

            for (ssize_t i = 0;
                 i < count;
                 ++i) {

                if (feed_uart(
                        state,
                        data[i]))
                    changed = 1;
            }
        }
    }

    if (apply_dev_button_injection(state))
        changed = 1;

    return changed;
}
