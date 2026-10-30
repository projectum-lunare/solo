/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <solo/platform.h>

#if SOLO_IS_PLAN9

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/serial.h>

#include <fcntl.h>
#include <unistd.h>

struct serial_port {
    int data_fd;
    int ctl_fd;
};

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    char data_path[128];
    char ctl_path[128];
    int ctl_fd;
    int data_fd;
    char cmd[256];
    size_t cmd_len = 0;
    solo_serial_t *port;
    solo_serial_opts_t default_opts;
    int baud, bits, stop;
    const char *target = (path && path[0] != '\0') ? path : "/dev/eia0";

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    if (opts == NULL) {
        default_opts.baud = 9600;
        default_opts.data_bits = 8;
        default_opts.parity = 'n';
        default_opts.stop_bits = 1;
        default_opts.flow_control = 0; /* safe default: none */
        default_opts.rts = 1;
        default_opts.dtr = 1;
        default_opts.timeout_ms = -1; /* default: blocking indefinitely */
        opts = &default_opts;
    } else if (opts->timeout_ms >= 0) {
        if (err_msg) {
            *err_msg = safe_strdup("Timed/non-blocking serial I/O is unsupported on Plan 9");
        }
        return NULL;
    }

    snprintf(data_path, sizeof(data_path), "%s", target);
    snprintf(ctl_path, sizeof(ctl_path), "%sctl", target);

    /* Try opening control file */
    ctl_fd = open(ctl_path, O_RDWR);
    if (ctl_fd < 0 && strncmp(ctl_path, "/dev/eia", 8) == 0) {
        /* Fallback to device space #t/eiaXctl if #t is not bound to /dev */
        char alt_ctl[128];
        char alt_data[128];
        snprintf(alt_ctl, sizeof(alt_ctl), "#t/%s", ctl_path + 5);
        snprintf(alt_data, sizeof(alt_data), "#t/%s", data_path + 5);
        ctl_fd = open(alt_ctl, O_RDWR);
        if (ctl_fd >= 0) {
            strncpy(ctl_path, alt_ctl, sizeof(ctl_path) - 1);
            strncpy(data_path, alt_data, sizeof(data_path) - 1);
            ctl_path[sizeof(ctl_path) - 1] = '\0';
            data_path[sizeof(data_path) - 1] = '\0';
        }
    }

    if (ctl_fd < 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cannot open Plan 9 serial control file '%s' (%s)", ctl_path,
                     strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    /* Format Plan 9 uart(3) control commands */
    baud = (opts->baud > 0) ? opts->baud : 9600;
    cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "b%d\n", baud);

    bits = (opts->data_bits >= 5 && opts->data_bits <= 8) ? opts->data_bits : 8;
    cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "l%d\n", bits);

    stop = (opts->stop_bits == 2) ? 2 : 1;
    cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "s%d\n", stop);

    if (opts->parity == 'e' || opts->parity == 'E') {
        cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "pe\n");
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "po\n");
    } else {
        cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "pn\n");
    }

    /* Hardware Flow Control (m1 = obey CTS, m0 = ignore) */
    if (opts->flow_control == 1) {
        cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "m1\n");
    } else {
        cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "m0\n");
    }

    /* Modem Lines (r1/r0 = RTS, d1/d0 = DTR) */
    if (opts->rts >= 0) {
        cmd_len +=
            (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "%s\n", opts->rts ? "r1" : "r0");
    }
    if (opts->dtr >= 0) {
        cmd_len +=
            (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "%s\n", opts->dtr ? "d1" : "d0");
    }

    /* Enable FIFOs */
    cmd_len += (size_t)snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, "i1\n");

    /* Send configuration to control file */
    if (write(ctl_fd, cmd, cmd_len) < 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Failed to write configuration to '%s' (%s)", ctl_path,
                     strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        close(ctl_fd);
        return NULL;
    }

    /* Open serial data file */
    data_fd = open(data_path, O_RDWR);
    if (data_fd < 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cannot open Plan 9 serial data file '%s' (%s)", data_path,
                     strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        close(ctl_fd);
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        close(data_fd);
        close(ctl_fd);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }

    port->data_fd = data_fd;
    port->ctl_fd = ctl_fd;

    return port;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    int on;
    if (port == NULL || port->ctl_fd < 0)
        return SOLO_SERIAL_ERROR;
    on = (state == SOLO_SERIAL_LINE_ON || (int)state == 1);
    return (write(port->ctl_fd, on ? "r1\n" : "r0\n", 3) == 3) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    int on;
    if (port == NULL || port->ctl_fd < 0)
        return SOLO_SERIAL_ERROR;
    on = (state == SOLO_SERIAL_LINE_ON || (int)state == 1);
    return (write(port->ctl_fd, on ? "d1\n" : "d0\n", 3) == 3) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
{
    ssize_t written;
    size_t total = 0;

    if (port == NULL || port->data_fd < 0 || buf == NULL) {
        return -1;
    }

    while (total < len) {
        written = write(port->data_fd, buf + total, len - total);
        if (written < 0) {
            return (total > 0) ? (int)total : -1;
        }
        if (written == 0) {
            break;
        }
        total += (size_t)written;
    }

    return (int)total;
}

int solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, int timeout_ms)
{
    if (port == NULL || port->data_fd < 0 || buf == NULL || max_len == 0) {
        return -1;
    }
    if (timeout_ms >= 0) {
        /* Plan 9 uart(3) does not support timed or non-blocking read */
        return SOLO_SERIAL_UNSUPPORTED;
    }
    return (int)read(port->data_fd, buf, max_len);
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || port->data_fd < 0) {
        return SOLO_SERIAL_ERROR;
    }
    if (timeout_ms >= 0) {
        /* Plan 9 uart(3) does not implement arbitrary millisecond or non-blocking timeouts */
        return SOLO_SERIAL_UNSUPPORTED;
    }
    return SOLO_SERIAL_OK;
}

int solo_serial_get_cts(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_dsr(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_cd(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_ri(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_drain(solo_serial_t *port)
{
    if (port == NULL || port->data_fd < 0)
        return -1;
    fsync(port->data_fd);
    return 0;
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        if (port->data_fd >= 0) {
            fsync(port->data_fd);
            close(port->data_fd);
            port->data_fd = -1;
        }
        if (port->ctl_fd >= 0) {
            close(port->ctl_fd);
            port->ctl_fd = -1;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_plan9;
#endif /* SOLO_IS_PLAN9 */
