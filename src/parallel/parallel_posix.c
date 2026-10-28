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

#if SOLO_IS_POSIX

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
    const char *target = (path && path[0] != '\0') ? path : "/dev/lp0";
    int fd;
    solo_parallel_t *port;

    fd = open(target, O_WRONLY | O_NOCTTY);
    if (fd < 0) {
        if (err_msg) {
            char buf[256];
            sprintf(buf, "Cannot open parallel device '%.128s' (%s)", target, strerror(errno));
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
            if (errno == EINTR)
                continue;
            return (total > 0) ? (int)total : -1;
        }
        if (written == 0) {
            break;
        }
        total += (size_t)written;
    }

    return (int)total;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL || port->fd < 0) {
        return -1;
    }
    return fsync(port->fd);
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
