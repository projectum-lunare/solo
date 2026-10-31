/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <solo/platform.h>

#if SOLO_IS_POSIX

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#if defined(__has_include)
#if __has_include(<linux/lp.h>)
#include <linux/lp.h>
#endif
#endif
#ifndef LPGETSTATUS
#define LPGETSTATUS 0x060b
#define LP_PBUSY    0x80
#define LP_PACK     0x40
#define LP_POUTPA   0x20
#define LP_PSELECD  0x10
#define LP_PERRORP  0x08
#endif
#endif

#include <solo/parallel.h>

struct parallel_port {
    int fd;
    int is_rdwr;
};

solo_parallel_t *solo_parallel_open(const char *path, char *err_buf, size_t err_cap)
{
    const char *target = (path && path[0] != '\0') ? path : "/dev/lp0";
    int fd;
    int is_rdwr = 1;
    solo_parallel_t *port;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    /* Try bidirectional open first; fall back to write-only */
    fd = open(target, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        is_rdwr = 0;
        fd = open(target, O_WRONLY | O_NOCTTY);
    }

    if (fd < 0) {
        solo_set_err_fmt(err_buf, err_cap, "Cannot open parallel device '%.128s': %s", target, strerror(errno));
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        close(fd);
        solo_set_err(err_buf, err_cap, "Memory allocation failure");
        return NULL;
    }

    port->fd = fd;
    port->is_rdwr = is_rdwr;
    return port;
}

solo_status_t solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    ssize_t written;
    size_t total = 0;

    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || port->fd < 0 || buf == NULL) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    while (total < len) {
        written = write(port->fd, buf + total, len - total);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            if (written_bytes != NULL) *written_bytes = total;
            return (total > 0) ? SOLO_OK : SOLO_ERROR;
        }
        if (written == 0) {
            break;
        }
        total += (size_t)written;
    }

    if (written_bytes != NULL) {
        *written_bytes = total;
    }
    return SOLO_OK;
}

solo_status_t solo_parallel_read(solo_parallel_t *port, char *buf, size_t max_len, size_t *read_bytes)
{
    ssize_t n;

    if (read_bytes != NULL) {
        *read_bytes = 0;
    }

    if (port == NULL || port->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_ERROR;
    }

    do {
        n = read(port->fd, buf, max_len);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        return SOLO_ERROR;
    }
    if (n == 0) {
        return SOLO_EOF;
    }

    if (read_bytes != NULL) {
        *read_bytes = (size_t)n;
    }
    return SOLO_OK;
}

solo_parallel_status_t solo_parallel_get_status(solo_parallel_t *port, int *pins_out)
{
    if (port == NULL || port->fd < 0 || pins_out == NULL) {
        return SOLO_PARALLEL_ERROR;
    }

#if defined(__linux__) && defined(LPGETSTATUS)
    {
        int st = 0;
        if (ioctl(port->fd, LPGETSTATUS, &st) == 0) {
            int pins = 0;
            /*
             * Bit 7 (LP_PBUSY = 0x80) is inverted hardware input:
             * 0 = printer is BUSY
             * 1 = printer is NOT busy (idle / ready)
             */
            if (!(st & LP_PBUSY)) pins |= SOLO_PARALLEL_PIN_BUSY;
            if (!(st & LP_PACK)) pins |= SOLO_PARALLEL_PIN_ACK;
            if (st & LP_POUTPA) pins |= SOLO_PARALLEL_PIN_PE;
            if (st & LP_PSELECD) pins |= SOLO_PARALLEL_PIN_SLCT;
            if (!(st & LP_PERRORP)) pins |= SOLO_PARALLEL_PIN_FAULT;
            *pins_out = pins;
            return SOLO_PARALLEL_OK;
        }
    }
#endif

    *pins_out = 0;
    return SOLO_PARALLEL_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode)
{
    if (port == NULL || port->fd < 0) {
        return SOLO_PARALLEL_ERROR;
    }
    if (mode == SOLO_PARALLEL_MODE_SPP) {
        return SOLO_PARALLEL_OK;
    }
    return SOLO_PARALLEL_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_drain(solo_parallel_t *port)
{
    int pins = 0;
    int timeout_count = 1000;

    if (port == NULL || port->fd < 0) {
        return SOLO_PARALLEL_ERROR;
    }

    fsync(port->fd);

    /* Poll hardware status pin until printer is no longer busy */
    while (solo_parallel_get_status(port, &pins) == SOLO_PARALLEL_OK &&
           (pins & SOLO_PARALLEL_PIN_BUSY)) {
        if (--timeout_count <= 0) {
            return SOLO_PARALLEL_ERROR; /* Printer still busy after timeout */
        }
        usleep(1000); /* 1 ms */
    }

    return SOLO_PARALLEL_OK;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        if (port->fd >= 0) {
            fsync(port->fd);
            close(port->fd);
            port->fd = -1;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_posix;
#endif /* SOLO_IS_POSIX */
