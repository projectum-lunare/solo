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

#if SOLO_IS_WINDOWS_CE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/parallel.h>

#include <windows.h>

struct parallel_port {
    HANDLE handle;
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    HANDLE h;
    solo_parallel_t *port;
    WCHAR target_path[64];
    char buf[64];
    size_t len;
    const char *p = (path && path[0] != '\0') ? path : "LPT1:";

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    len = strlen(p);
    if (len >= sizeof(buf) - 2)
        len = sizeof(buf) - 3;
    memcpy(buf, p, len);
    buf[len] = '\0';
    if (len > 0 && buf[len - 1] != ':') {
        buf[len] = ':';
        buf[len + 1] = '\0';
    }

    MultiByteToWideChar(CP_ACP, 0, buf, -1, target_path, sizeof(target_path) / sizeof(WCHAR));
    h = CreateFileW(target_path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "CreateFileW(%s) failed (error %lu)",
                     path ? path : "LPT1:", (unsigned long)GetLastError());
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        CloseHandle(h);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }

    port->handle = h;
    return port;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    DWORD written = 0;
    size_t total = 0;

    if (port == NULL || port->handle == INVALID_HANDLE_VALUE || buf == NULL) {
        return -1;
    }

    while (total < len) {
        DWORD chunk = (DWORD)(len - total);
        if (!WriteFile(port->handle, buf + total, chunk, &written, NULL)) {
            return (total > 0) ? (int)total : -1;
        }
        if (written == 0) {
            break;
        }
        total += written;
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
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    return FlushFileBuffers(port->handle) ? 0 : -1;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        if (port->handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(port->handle);
            CloseHandle(port->handle);
            port->handle = INVALID_HANDLE_VALUE;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_windows_ce;
#endif /* SOLO_IS_WINDOWS_CE */
