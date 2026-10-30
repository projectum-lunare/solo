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

#include <solo/serial.h>

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/serial.h>
#include <proto/exec.h>

struct serial_port {
    struct MsgPort *mp;
    struct IOExtSer *io;
    int is_open;
    int timeout_ms;
};

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    char dev_name[64];
    ULONG unit = 0;
    const char *colon;
    struct MsgPort *mp = NULL;
    struct IOExtSer *io = NULL;
    solo_serial_t *port = NULL;
    solo_serial_opts_t default_opts;
    BYTE open_err;

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
    } else if (opts->timeout_ms > 0) {
        if (err_msg) {
            *err_msg = safe_strdup("Arbitrary millisecond timeouts are unsupported on Amiga serial.device");
        }
        return NULL;
    }

    if (path == NULL || path[0] == '\0') {
        strncpy(dev_name, "serial.device", sizeof(dev_name) - 1);
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
        if (err_msg)
            *err_msg = safe_strdup("CreateMsgPort() failed");
        return NULL;
    }

    io = (struct IOExtSer *)CreateIORequest(mp, sizeof(struct IOExtSer));
    if (io == NULL) {
        DeleteMsgPort(mp);
        if (err_msg)
            *err_msg = safe_strdup("CreateIORequest() failed");
        return NULL;
    }

    io->io_SerFlags = SERF_SHARED;
    open_err = OpenDevice((CONST_STRPTR)dev_name, unit, (struct IORequest *)io, 0);
    if (open_err != 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "OpenDevice(%s, %lu) failed (error %d)", dev_name, unit,
                     (int)open_err);
            *err_msg = safe_strdup(buf);
        }
        DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(mp);
        return NULL;
    }

    /* Configure communication parameters via SDCMD_SETPARAMS */
    io->io_Baud = (ULONG)((opts->baud > 0) ? opts->baud : 9600);
    io->io_ReadLen = (UBYTE)((opts->data_bits >= 5 && opts->data_bits <= 8) ? opts->data_bits : 8);
    io->io_WriteLen = io->io_ReadLen;
    io->io_StopBits = (UBYTE)((opts->stop_bits == 2) ? 2 : 1);

    /* Parity */
    if (opts->parity == 'e' || opts->parity == 'E') {
        io->io_SerFlags |= SERF_PARTY_ON;
        io->io_SerFlags &= ~SERF_PARTY_ODD;
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        io->io_SerFlags |= (SERF_PARTY_ON | SERF_PARTY_ODD);
    } else {
        io->io_SerFlags &= ~(SERF_PARTY_ON | SERF_PARTY_ODD);
    }

    /* Flow Control (RTS/CTS 7-wire vs XON/XOFF) */
    if (opts->flow_control == 1) {
        io->io_ExtFlags |= SEXTF_MSPON; /* Enable 7-wire hardware handshake */
        io->io_SerFlags |= SERF_XDISABLED;
    } else if (opts->flow_control == 2) {
        io->io_ExtFlags &= ~SEXTF_MSPON;
        io->io_SerFlags &= ~SERF_XDISABLED; /* Software XON/XOFF */
    } else {
        io->io_ExtFlags &= ~SEXTF_MSPON;
        io->io_SerFlags |= SERF_XDISABLED;
    }

    /* Enable high-speed processing */
    io->io_SerFlags |= SERF_RAD_BOOGIE;

    io->IOSer.io_Command = SDCMD_SETPARAMS;
    if (DoIO((struct IORequest *)io) != 0) {
        if (err_msg)
            *err_msg = safe_strdup("SDCMD_SETPARAMS failed on serial.device");
        CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(mp);
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(mp);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }

    port->mp = mp;
    port->io = io;
    port->is_open = 1;
    port->timeout_ms = (opts != NULL) ? opts->timeout_ms : -1;

    return port;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
{
    if (port == NULL || port->io == NULL || buf == NULL || !port->is_open) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (port->timeout_ms > 0) {
        return SOLO_SERIAL_UNSUPPORTED;
    }

    port->io->IOSer.io_Command = CMD_WRITE;
    port->io->IOSer.io_Data = (APTR)buf;
    port->io->IOSer.io_Length = (ULONG)len;

    if (port->timeout_ms == 0) {
        SendIO((struct IORequest *)port->io);
        if (!CheckIO((struct IORequest *)port->io)) {
            AbortIO((struct IORequest *)port->io);
            WaitIO((struct IORequest *)port->io);
            return 0; /* Timeout: 0 bytes written */
        }
        WaitIO((struct IORequest *)port->io);
        if (port->io->IOSer.io_Error != 0) {
            return -1;
        }
        return (int)len;
    }

    if (DoIO((struct IORequest *)port->io) != 0) {
        return -1;
    }

    return (int)len;
}

int solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, int timeout_ms)
{
    int effective_timeout;

    if (port == NULL || port->io == NULL || buf == NULL || !port->is_open || max_len == 0) {
        return -1;
    }

    effective_timeout = (timeout_ms < 0) ? port->timeout_ms : timeout_ms;
    if (effective_timeout > 0) {
        /* Amiga serial.device does not implement arbitrary millisecond timeouts without timer.device */
        return SOLO_SERIAL_UNSUPPORTED;
    }
    if (effective_timeout == 0) {
        /* Non-blocking check via SDCMD_QUERY */
        port->io->IOSer.io_Command = SDCMD_QUERY;
        if (DoIO((struct IORequest *)port->io) != 0) {
            return -1;
        }
        if (port->io->IOSer.io_Actual == 0) {
            return 0; /* No data ready */
        }
        if (max_len > port->io->IOSer.io_Actual) {
            max_len = port->io->IOSer.io_Actual;
        }
    }

    port->io->IOSer.io_Command = CMD_READ;
    port->io->IOSer.io_Data = (APTR)buf;
    port->io->IOSer.io_Length = (ULONG)max_len;
    if (DoIO((struct IORequest *)port->io) != 0) {
        return -1;
    }
    return (int)port->io->IOSer.io_Actual;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || !port->is_open) {
        return SOLO_SERIAL_ERROR;
    }
    if (timeout_ms > 0) {
        /* Amiga serial.device does not implement arbitrary millisecond timeouts without timer.device */
        return SOLO_SERIAL_UNSUPPORTED;
    }
    port->timeout_ms = timeout_ms;
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
    if (port == NULL || port->io == NULL || !port->is_open) {
        return -1;
    }
    /* DoIO CMD_WRITE is synchronous in Exec serial.device */
    return 0;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->io == NULL || !port->is_open) {
        return SOLO_SERIAL_ERROR;
    }
    (void)state;
    return SOLO_SERIAL_OK;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->io == NULL || !port->is_open) {
        return SOLO_SERIAL_ERROR;
    }
    (void)state;
    return SOLO_SERIAL_OK;
}

void solo_serial_close(solo_serial_t *port)
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
typedef int make_iso_compilers_happy_serial_amiga;
#endif /* SOLO_IS_AMIGA */
