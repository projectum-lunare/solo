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

#if SOLO_IS_OS2

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSDEVIOCTL
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <solo/parallel.h>

struct parallel_port {
    HFILE handle;
    char name[16];
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    const char *dev_name = (path && path[0] != '\0') ? path : "LPT1";
    HFILE hfile = NULLHANDLE;
    ULONG action = 0;
    APIRET rc;
    solo_parallel_t *port;

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    /*
     * DosOpen with:
     * - OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW: open device if it exists
     * - OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY: exclusive write access
     */
    rc = DosOpen((PCSZ)dev_name, &hfile, &action, 0, FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                 OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY, NULL);

    if (rc != 0) {
        if (err_msg != NULL) {
            char buf[128];
            snprintf(buf, sizeof(buf), "DosOpen(%s) failed (SYS%04lu)", dev_name,
                     (unsigned long)rc);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        DosClose(hfile);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Out of memory allocating parallel port structure");
        }
        return NULL;
    }

    port->handle = hfile;
    strncpy(port->name, dev_name, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    return port;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    ULONG written = 0;
    APIRET rc;

    if (port == NULL || port->handle == NULLHANDLE || buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    rc = DosWrite(port->handle, (PVOID)buf, (ULONG)len, &written);
    if (rc != 0) {
        return -1;
    }

    return (int)written;
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
    if (port == NULL || port->handle == NULLHANDLE) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return -1;
    }

    /*
     * DosResetBuffer flushes all internal file/device buffers to the
     * attached parallel printer device.
     */
    DosResetBuffer(port->handle);
    return 0;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        if (port->handle != NULLHANDLE) {
            DosClose(port->handle);
            port->handle = NULLHANDLE;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_os2;
#endif /* SOLO_IS_OS2 */
