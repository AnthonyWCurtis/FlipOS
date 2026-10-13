#define _POSIX_C_SOURCE 200809L

#include "flip_wifi.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int valid_text(const char *text, size_t max_length)
{
    if (!text)
        return 0;

    size_t length = strlen(text);
    if (length == 0 || length > max_length)
        return 0;

    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 || c == 0x7f)
            return 0;
    }
    return 1;
}

static int ensure_config_dir(void)
{
    char path[512];
    size_t length = strlen(FLIP_WIFI_CONFIG_DIR);
    if (length == 0 || length >= sizeof(path))
        return -1;

    memcpy(path, FLIP_WIFI_CONFIG_DIR, length + 1);

    for (char *p = path + 1; *p; ++p) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(path, 0700) < 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }

    if (mkdir(path, 0700) < 0 && errno != EEXIST)
        return -1;

    return chmod(path, 0700);
}

static int write_escaped(FILE *f, const char *value)
{
    if (!f || !value)
        return -1;

    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == '\\' || *p == '"') {
            if (fputc('\\', f) == EOF)
                return -1;
        }
        if (fputc(*p, f) == EOF)
            return -1;
    }
    return 0;
}

static int derive_psk(
    const char *ssid,
    const char *passphrase,
    char *hex_psk,
    size_t hex_psk_size);

static int write_control_config(void)
{
    if (ensure_config_dir() < 0)
        return -1;

    int fd = open(
        FLIP_WIFI_CONFIG_FILE,
        O_WRONLY | O_CREAT | O_TRUNC,
        0600);
    if (fd < 0)
        return -1;

    static const char content[] =
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "update_config=0\n";

    ssize_t written = write(fd, content, sizeof(content) - 1);
    int ok = written == (ssize_t)(sizeof(content) - 1) && fsync(fd) == 0;
    int close_result = close(fd);

    if (!ok || close_result != 0) {
        unlink(FLIP_WIFI_CONFIG_FILE);
        return -1;
    }

    return chmod(FLIP_WIFI_CONFIG_FILE, 0600);
}

int flip_wifi_write_config(
    const char *ssid,
    const char *passphrase)
{
    if (!valid_text(ssid, FLIP_WIFI_SSID_MAX - 1))
        return -1;

    int open_network = !passphrase || !passphrase[0];
    if (!open_network) {
        size_t length = strlen(passphrase);
        if (length < 8 || length > 63 ||
            !valid_text(passphrase, 63))
            return -1;
    }

    char hex_psk[65] = "";
    if (!open_network &&
        derive_psk(ssid, passphrase, hex_psk, sizeof(hex_psk)) < 0)
        return -1;

    if (ensure_config_dir() < 0)
        return -1;

    char tmp[640];
    int written = snprintf(
        tmp, sizeof(tmp), "%s.tmp", FLIP_WIFI_CONFIG_FILE);
    if (written < 0 || (size_t)written >= sizeof(tmp))
        return -1;

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;

    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(tmp);
        return -1;
    }

    int ok = 1;
    ok &= fprintf(f,
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "update_config=0\n"
        "network={\n"
        "    ssid=\"") >= 0;
    ok &= write_escaped(f, ssid) == 0;
    ok &= fprintf(f, "\"\n") >= 0;

    if (open_network) {
        ok &= fprintf(f, "    key_mgmt=NONE\n") >= 0;
    } else {
        /* Store only WPA's derived 256-bit PSK; never the plaintext secret. */
        ok &= fprintf(f, "    psk=%s\n", hex_psk) >= 0;
    }

    ok &= fprintf(f, "}\n") >= 0;

    if (!ok || fflush(f) != 0 || fsync(fd) != 0 || fclose(f) != 0) {
        unlink(tmp);
        return -1;
    }

    if (chmod(tmp, 0600) < 0 ||
        rename(tmp, FLIP_WIFI_CONFIG_FILE) < 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int run_capture(
    char *const argv[],
    char *output,
    size_t output_size)
{
    int pipefd[2];
    if (!argv || !argv[0] || pipe(pipefd) < 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execv(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    size_t used = 0;
    if (output && output_size > 0)
        output[0] = '\0';

    char buffer[512];
    ssize_t got;
    while ((got = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        if (output && output_size > 1 && used < output_size - 1) {
            size_t copy = (size_t)got;
            if (copy > output_size - 1 - used)
                copy = output_size - 1 - used;
            memcpy(output + used, buffer, copy);
            used += copy;
            output[used] = '\0';
        }
    }
    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int derive_psk(
    const char *ssid,
    const char *passphrase,
    char *hex_psk,
    size_t hex_psk_size)
{
    if (!ssid || !passphrase || !hex_psk || hex_psk_size < 65)
        return -1;

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) < 0)
        return -1;
    if (pipe(out_pipe) < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(out_pipe[1]);
        char *argv[] = {
            (char *)FLIP_WIFI_WPA_PASSPHRASE,
            (char *)ssid,
            NULL
        };
        execv(argv[0], argv);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    size_t pass_len = strlen(passphrase);
    ssize_t written = write(in_pipe[1], passphrase, pass_len);
    if (written == (ssize_t)pass_len)
        if (write(in_pipe[1], "\n", 1) != 1)
            written = -1;
    close(in_pipe[1]);

    char output[4096];
    size_t used = 0;
    ssize_t got;
    while ((got = read(out_pipe[0], output + used,
                       sizeof(output) - 1 - used)) > 0) {
        used += (size_t)got;
        if (used >= sizeof(output) - 1)
            break;
    }
    output[used] = '\0';
    close(out_pipe[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;

    const char *line = output;
    while (line && *line) {
        while (*line == ' ' || *line == '\t')
            ++line;
        if (strncmp(line, "psk=", 4) == 0 &&
            line[4] != '"') {
            const char *value = line + 4;
            const char *end = strpbrk(value, "\r\n");
            size_t length = end ? (size_t)(end - value) : strlen(value);
            if (length != 64)
                return -1;
            for (size_t i = 0; i < length; ++i) {
                if (!isxdigit((unsigned char)value[i]))
                    return -1;
            }
            memcpy(hex_psk, value, 64);
            hex_psk[64] = '\0';
            return 0;
        }
        line = strchr(line, '\n');
        if (line)
            ++line;
    }

    return -1;
}

static int interface_present(void)
{
    char path[256];
    int n = snprintf(path, sizeof(path),
        "/sys/class/net/%s", FLIP_WIFI_INTERFACE);
    return n > 0 && (size_t)n < sizeof(path) && access(path, F_OK) == 0;
}

static int interface_up(void)
{
    char *argv[] = {
        (char *)FLIP_WIFI_IP,
        "link", "set", (char *)FLIP_WIFI_INTERFACE, "up", NULL
    };
    return run_capture(argv, NULL, 0) == 0 ? 0 : -1;
}

static int wpa_status_text(char *output, size_t output_size)
{
    char *argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "status", NULL
    };
    return run_capture(argv, output, output_size);
}

static int ensure_supplicant(void)
{
    if (!interface_present() || interface_up() < 0)
        return -1;

    char status[512];
    if (wpa_status_text(status, sizeof(status)) == 0)
        return 0;

    if (access(FLIP_WIFI_CONFIG_FILE, R_OK) != 0) {
        if (write_control_config() < 0)
            return -1;
    }

    char *argv[] = {
        (char *)FLIP_WIFI_WPA_SUPPLICANT,
        "-B",
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "-c", (char *)FLIP_WIFI_CONFIG_FILE,
        NULL
    };
    if (run_capture(argv, NULL, 0) != 0)
        return -1;

    struct timespec wait = { .tv_sec = 0, .tv_nsec = 200000000L };
    nanosleep(&wait, NULL);
    return 0;
}

static int hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F')
        return (int)(c - 'A') + 10;
    return -1;
}

/*
 * wpa_cli scan_results escapes non-ASCII / non-printable SSID bytes as
 * literal \xNN sequences. Keep the actual SSID bytes in FlipOS rather than
 * the printable control-interface representation, otherwise WPA PSK
 * derivation and the generated network block use the wrong SSID.
 */
static int decode_scan_ssid(
    const char *escaped,
    char *decoded,
    size_t decoded_size)
{
    if (!escaped || !decoded || decoded_size == 0)
        return -1;

    size_t out = 0;

    for (size_t i = 0; escaped[i] != '\0'; ++i) {
        unsigned char value = (unsigned char)escaped[i];

        if (value == '\\' &&
            escaped[i + 1] == 'x' &&
            escaped[i + 2] != '\0' &&
            escaped[i + 3] != '\0') {

            int hi = hex_value((unsigned char)escaped[i + 2]);
            int lo = hex_value((unsigned char)escaped[i + 3]);

            if (hi >= 0 && lo >= 0) {
                value = (unsigned char)((hi << 4) | lo);
                i += 3;
            }
        } else if (
            value == '\\' &&
            escaped[i + 1] == '\\') {

            value = '\\';
            i += 1;
        }

        if (value == '\0' || out + 1 >= decoded_size)
            return -1;

        decoded[out++] = (char)value;
    }

    decoded[out] = '\0';
    return 0;
}

static int parse_tab_line(
    char *line,
    flip_wifi_network_t *network)
{
    char *fields[5] = {0};
    int count = 0;
    char *cursor = line;

    while (count < 5) {
        fields[count++] = cursor;
        char *tab = strchr(cursor, '\t');
        if (!tab)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }

    if (count < 5 || !fields[4][0])
        return 0;

    memset(network, 0, sizeof(*network));

    if (decode_scan_ssid(
            fields[4],
            network->ssid,
            sizeof(network->ssid)) < 0)
        return 0;

    network->signal_dbm = atoi(fields[2]);
    snprintf(network->flags, sizeof(network->flags), "%s", fields[3]);
    network->secure =
        strstr(fields[3], "WPA") != NULL ||
        strstr(fields[3], "WEP") != NULL;
    return 1;
}

int flip_wifi_parse_scan_results(
    const char *text,
    flip_wifi_network_t *networks,
    int max_networks)
{
    if (!text || !networks || max_networks <= 0)
        return -1;

    char *copy = strdup(text);
    if (!copy)
        return -1;

    int count = 0;
    char *save = NULL;
    char *line = strtok_r(copy, "\r\n", &save);
    int first = 1;

    while (line && count < max_networks) {
        if (first && strstr(line, "bssid") && strstr(line, "ssid")) {
            first = 0;
            line = strtok_r(NULL, "\r\n", &save);
            continue;
        }
        first = 0;

        flip_wifi_network_t candidate;
        if (parse_tab_line(line, &candidate)) {
            int duplicate = -1;
            for (int i = 0; i < count; ++i) {
                if (strcmp(networks[i].ssid, candidate.ssid) == 0) {
                    duplicate = i;
                    break;
                }
            }
            if (duplicate >= 0) {
                if (candidate.signal_dbm > networks[duplicate].signal_dbm)
                    networks[duplicate] = candidate;
            } else {
                networks[count++] = candidate;
            }
        }

        line = strtok_r(NULL, "\r\n", &save);
    }

    free(copy);

    /* Strongest networks first. */
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (networks[j].signal_dbm > networks[i].signal_dbm) {
                flip_wifi_network_t tmp = networks[i];
                networks[i] = networks[j];
                networks[j] = tmp;
            }
        }
    }

    return count;
}

int flip_wifi_scan(
    flip_wifi_network_t *networks,
    int max_networks)
{
    if (!networks || max_networks <= 0 || ensure_supplicant() < 0)
        return -1;

    char *scan_argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "scan", NULL
    };
    if (run_capture(scan_argv, NULL, 0) != 0)
        return -1;

    char output[16384];
    char *results_argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "scan_results", NULL
    };

    /*
     * The Flip's RTL8733BU commonly needs about five seconds to complete a
     * full 2.4/5 GHz survey. Poll rather than assuming desktop-class timing.
     */
    for (int attempt = 0; attempt < 24; ++attempt) {
        struct timespec wait = { .tv_sec = 0, .tv_nsec = 250000000L };
        nanosleep(&wait, NULL);

        if (run_capture(results_argv, output, sizeof(output)) != 0)
            continue;

        int count = flip_wifi_parse_scan_results(
            output, networks, max_networks);
        if (count > 0)
            return count;
    }

    /* A successful scan can legitimately find no broadcast SSIDs. */
    return 0;
}

static void stop_udhcpc(void)
{
    FILE *f = fopen(FLIP_WIFI_UDHCPC_PIDFILE, "r");
    if (f) {
        long pid_value = 0;
        if (fscanf(f, "%ld", &pid_value) == 1 && pid_value > 1) {
            (void)kill((pid_t)pid_value, SIGTERM);
            for (int attempt = 0; attempt < 10; ++attempt) {
                if (kill((pid_t)pid_value, 0) < 0 && errno == ESRCH)
                    break;
                struct timespec wait = { .tv_sec = 0, .tv_nsec = 100000000L };
                nanosleep(&wait, NULL);
            }
        }
        fclose(f);
    }
    (void)unlink(FLIP_WIFI_UDHCPC_PIDFILE);
}

static void request_dhcpcd(void)
{
    char *argv[] = {
        (char *)FLIP_WIFI_DHCPCD,
        "-n",
        (char *)FLIP_WIFI_INTERFACE,
        NULL
    };
    (void)run_capture(argv, NULL, 0);
}

static int usable_ipv4_address(const char *address)
{
    if (!address || !address[0])
        return 0;

    unsigned a, b, c, d;
    char extra;
    if (sscanf(address, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4)
        return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return 0;

    /* 169.254/16 is IPv4 link-local fallback, not a DHCP lease. */
    if (a == 169 && b == 254)
        return 0;
    if (a == 0 || a == 127 || a >= 224)
        return 0;

    return 1;
}

static int status_usable_ip(const char *status)
{
    if (!status)
        return 0;

    const char *p = strstr(status, "ip_address=");
    if (!p)
        return 0;
    p += strlen("ip_address=");

    char address[64];
    size_t length = strcspn(p, "\r\n");
    if (length >= sizeof(address))
        length = sizeof(address) - 1;
    memcpy(address, p, length);
    address[length] = '\0';

    return usable_ipv4_address(address);
}

int flip_wifi_connect(
    const char *ssid,
    const char *passphrase)
{
    if (flip_wifi_write_config(ssid, passphrase) < 0 ||
        !interface_present() || interface_up() < 0)
        return -1;

    char *terminate_argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "terminate", NULL
    };
    (void)run_capture(terminate_argv, NULL, 0);

    char *supplicant_argv[] = {
        (char *)FLIP_WIFI_WPA_SUPPLICANT,
        "-B",
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "-c", (char *)FLIP_WIFI_CONFIG_FILE,
        NULL
    };
    if (run_capture(supplicant_argv, NULL, 0) != 0)
        return -1;

    /* Wait for WPA association instead of assuming it finishes in 2 seconds. */
    int associated = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        struct timespec wait = { .tv_sec = 0, .tv_nsec = 500000000L };
        nanosleep(&wait, NULL);

        char status[2048];
        if (wpa_status_text(status, sizeof(status)) == 0 &&
            strstr(status, "wpa_state=COMPLETED") != NULL) {
            associated = 1;
            break;
        }
    }

    if (!associated)
        return -1;

    /*
     * dhcpcd starts before Wi-Fi association in the stock image and may settle
     * on IPv4 link-local.  Re-submit wlan0 after WPA completes so the already
     * running dhcpcd manager immediately performs DHCP on the live link.
     */
    stop_udhcpc();
    request_dhcpcd();

    for (int attempt = 0; attempt < 30; ++attempt) {
        char status[2048];
        if (wpa_status_text(status, sizeof(status)) == 0) {
            if (status_usable_ip(status))
                return FLIP_WIFI_CONNECT_OK;
            if (strstr(status, "wpa_state=COMPLETED") == NULL)
                return FLIP_WIFI_CONNECT_ERROR;
        }

        struct timespec wait = { .tv_sec = 0, .tv_nsec = 500000000L };
        nanosleep(&wait, NULL);
    }

    return FLIP_WIFI_CONNECT_AUTH_NO_IP;
}

int flip_wifi_disconnect(void)
{
    stop_udhcpc();

    char *argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "disconnect", NULL
    };
    return run_capture(argv, NULL, 0) == 0 ? 0 : -1;
}

static void copy_status_value(
    const char *text,
    const char *key,
    char *out,
    size_t out_size)
{
    if (!text || !key || !out || out_size == 0)
        return;

    size_t key_length = strlen(key);
    const char *line = text;
    while (line && *line) {
        if (strncmp(line, key, key_length) == 0 && line[key_length] == '=') {
            const char *value = line + key_length + 1;
            const char *end = strpbrk(value, "\r\n");
            size_t length = end ? (size_t)(end - value) : strlen(value);
            if (length >= out_size)
                length = out_size - 1;
            memcpy(out, value, length);
            out[length] = '\0';
            return;
        }
        line = strchr(line, '\n');
        if (line)
            ++line;
    }
}

int flip_wifi_forget(void)
{
    stop_udhcpc();

    char *terminate_argv[] = {
        (char *)FLIP_WIFI_WPA_CLI,
        "-i", (char *)FLIP_WIFI_INTERFACE,
        "terminate", NULL
    };
    (void)run_capture(terminate_argv, NULL, 0);

    int result = 0;
    if (unlink(FLIP_WIFI_CONFIG_FILE) < 0 && errno != ENOENT)
        result = -1;

    char tmp[640];
    int written = snprintf(tmp, sizeof(tmp), "%s.tmp", FLIP_WIFI_CONFIG_FILE);
    if (written > 0 && (size_t)written < sizeof(tmp))
        (void)unlink(tmp);

    return result;
}

int flip_wifi_get_status(
    flip_wifi_status_t *status)
{
    if (!status)
        return -1;

    memset(status, 0, sizeof(*status));
    status->interface_present = interface_present();
    if (!status->interface_present)
        return 0;

    char output[2048];
    if (wpa_status_text(output, sizeof(output)) != 0)
        return 0;

    status->enabled = 1;
    char state[64] = "";
    copy_status_value(output, "wpa_state", state, sizeof(state));
    copy_status_value(output, "ssid", status->ssid, sizeof(status->ssid));
    copy_status_value(output, "ip_address", status->ip_address, sizeof(status->ip_address));
    status->connected = strcmp(state, "COMPLETED") == 0;
    status->usable_ip = usable_ipv4_address(status->ip_address);
    return 0;
}
