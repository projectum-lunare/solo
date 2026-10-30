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

#if SOLO_IS_AMIGA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/parallel.h>

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/parallel.h>
#include <proto/exec.h>

struct parallel_port {
    struct MsgPort *mp;
    struct IOExtPar *io;
    int is_open;
};

solo_parallel_t *solo_parallel_open(const char *path, char *err_buf, size_t err_cap)
{
    char dev_name[64];
    ULONG unit = 0;
    const char *colon;
    struct MsgPort *mp = NULL;
    struct IOExtPar *io = NULL;
    solo_parallel_t *port = NULL;
    BYTE open_err;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (path == NULL || path[0] == '\0') {
        strncpy(dev_name, "parallel.device", sizeof(dev_name) - 1);
        dev_name[sizeof(dev_name) - 1] = '\0';
        unit = 0;
    } else {
        colon = strchr(path, ':');
        if (colon != NULL) {
            size_t len = (size_t)(colon - path);
            if (len >= sizeof(dev_name))
                len = sizeof(dev_name) - 1;
            memcpy(dev_name, path, len);
            dev_name[len] = '\0';
            unit = (ULONG)atoi(colon + 1);
        } else {
            strncpy(dev_name, path, sizeof(dev_name) - 1);
            dev_name[sizeof(dev_name) - 1] = '\0';
            unit = 0;
        }
    }

    mp = CreateMsgPort();
    if (mp == NULL) {
        solo_set_err(err_buf, err_cap, "CreateMsgPort() failed");
        return NULL;
    }

    io = (struct IOExtPar *)CreateIORequest(mp, sizeof(struct IOExtPar));
    if (io == NULL) {
        DeleteMsgPort(mp);
        solo_set_err(err_buf, err_cap, "CreateIORequest() failed");
        return NULL;
    }

    open_err = OpenDevice((CONST_STRPTR)dev_name, unit, (struct IORequest *)io, 0);
    if (open_err != 0) {
        solo_set_err_fmt(err_buf, err_cap, "OpenDevice(%s, %lu) failed (error %d)", dev_name, unit,
                         (int)open_err);
        DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(mp);
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(mp);
        solo_set_err(err_buf, err_cap, "Memory allocation failure");
        return NULL;
    }

    port->mp = mp;
    port->io = io;
    port->is_open = 1;

    return port;
}

solo_status_t solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || port->io == NULL || buf == NULL || !port->is_open) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    port->io->IOPar.io_Command = CMD_WRITE;
    port->io->IOPar.io_Data = (APTR)buf;
    port->io->IOPar.io_Length = (ULONG)len;

    if (DoIO((struct IORequest *)port->io) != 0) {
        return SOLO_ERROR;
    }

    if (written_bytes != NULL) {
        *written_bytes = len;
    }
    return SOLO_OK;
}

solo_status_t solo_parallel_read(solo_parallel_t *port, char *buf, size_t max_len, size_t *read_bytes)
{
    (void)port;
    (void)buf;
    (void)max_len;
    if (read_bytes != NULL) *read_bytes = 0;
    return SOLO_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_get_status(solo_parallel_t *port, int *pins_out)
{
    (void)port;
    if (pins_out != NULL) *pins_out = 0;
    return SOLO_PARALLEL_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode)
{
    if (port == NULL || !port->is_open) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL || port->io == NULL || !port->is_open) {
        return -1;
    }
    return 0;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        if (port->is_open && port->io != NULL) {
            CloseDevice((struct IORequest *)port->io);
            port->is_open = 0;
        }
        if (port->io != NULL) {
            DeleteIORequest((struct IORequest *)port->io);
            port->io = NULL;
        }
        if (port->mp != NULL) {
            DeleteMsgPort(port->mp);
            port->mp = NULL;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_amiga;
#endif /* SOLO_IS_AMIGA */
