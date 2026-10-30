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

#include <solo/parallel.h>

#include <fcntl.h>
#include <unistd.h>

struct parallel_port {
    int fd;
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    const char *target_path = (path && path[0] != '\0') ? path : "/dev/lpt";
    int fd;
    solo_parallel_t *port;

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    fd = open(target_path, O_WRONLY);
    if (fd < 0 && strncmp(target_path, "/dev/lpt", 8) == 0) {
        /* Fallback to device space #p/lpt if #p is not bound to /dev */
        char alt_path[128];
        snprintf(alt_path, sizeof(alt_path), "#p/%s", target_path + 5);
        fd = open(alt_path, O_WRONLY);
        if (fd >= 0) {
            target_path = alt_path;
        }
    }

    if (fd < 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cannot open Plan 9 parallel device '%s' (%s)", target_path,
                     strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        close(fd);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }

    port->fd = fd;
    return port;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    ssize_t written;
    size_t total = 0;

    if (port == NULL || port->fd < 0 || buf == NULL) {
        return -1;
    }

    while (total < len) {
        written = write(port->fd, buf + total, len - total);
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

int solo_parallel_read(solo_parallel_t *port, char *buf, size_t max_len)
{
    (void)port;
    (void)buf;
    (void)max_len;
    return -1;
}

solo_parallel_status_t solo_parallel_get_status(solo_parallel_t *port, int *pins_out)
{
    (void)port;
    if (pins_out != NULL) *pins_out = 0;
    return SOLO_PARALLEL_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode)
{
    if (port == NULL || port->fd < 0) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL || port->fd < 0)
        return -1;
    fsync(port->fd);
    return 0;
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
typedef int make_iso_compilers_happy_parallel_plan9;
#endif /* SOLO_IS_PLAN9 */
