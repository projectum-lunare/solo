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

#include <solo/serial.h>

struct serial_port {
    HFILE handle;
    int timeout_ms;
    char name[16];
};

static solo_serial_status_t solo_serial_set_rts_internal(HFILE handle, int on)
{
    MODEMSTATUS ms;
    ULONG cbParam = sizeof(ms);
    APIRET rc;

    memset(&ms, 0, sizeof(ms));
    if (on) {
        ms.fbModemOn = RTS_ON;
        ms.fbModemOff = 0xFF;
    } else {
        ms.fbModemOn = 0x00;
        ms.fbModemOff = RTS_OFF;
    }

    rc = DosDevIOCtl(handle, IOCTL_ASYNC, ASYNC_SETMODEMCTRL, &ms, sizeof(ms), &cbParam, NULL, 0,
                     NULL);
    return (rc == 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

static solo_serial_status_t solo_serial_set_dtr_internal(HFILE handle, int on)
{
    MODEMSTATUS ms;
    ULONG cbParam = sizeof(ms);
    APIRET rc;

    memset(&ms, 0, sizeof(ms));
    if (on) {
        ms.fbModemOn = DTR_ON;
        ms.fbModemOff = 0xFF;
    } else {
        ms.fbModemOn = 0x00;
        ms.fbModemOff = DTR_OFF;
    }

    rc = DosDevIOCtl(handle, IOCTL_ASYNC, ASYNC_SETMODEMCTRL, &ms, sizeof(ms), &cbParam, NULL, 0,
                     NULL);
    return (rc == 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char *err_buf, size_t err_cap)
{
    HFILE hfile = NULLHANDLE;
    ULONG action = 0;
    APIRET rc;
    solo_serial_t *port;
    solo_serial_opts_t default_opts;
    const char *p = (path && path[0] != '\0') ? path : "COM1";

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
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
    }

    /*
     * DosOpen with:
     * - OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW: open if exists
     * - OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE: exclusive read/write access
     */
    rc = DosOpen((PCSZ)p, &hfile, &action, 0, FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                 OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE, NULL);

    if (rc != 0) {
        solo_set_err_fmt(err_buf, err_cap, "DosOpen(%s) failed (SYS%04lu)", p, (unsigned long)rc);
        return NULL;
    }

    if (opts != NULL) {
        ULONG cbParam;
        ULONG cbData;

        /* 1. Baud Rate */
        if (opts->baud > 19200) {
            ULONG ulBaud = (ULONG)opts->baud;
            cbParam = sizeof(ulBaud);
            rc = DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE, &ulBaud, sizeof(ulBaud),
                             &cbParam, NULL, 0, NULL);
            if (rc != 0) {
                /* Fallback to standard 16-bit baud call */
                USHORT usBaud = (USHORT)opts->baud;
                cbParam = sizeof(usBaud);
                DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_SETBAUDRATE, &usBaud, sizeof(usBaud),
                            &cbParam, NULL, 0, NULL);
            }
        } else if (opts->baud > 0) {
            USHORT usBaud = (USHORT)opts->baud;
            cbParam = sizeof(usBaud);
            DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_SETBAUDRATE, &usBaud, sizeof(usBaud), &cbParam,
                        NULL, 0, NULL);
        }

        /* 2. Line Control (Data Bits, Parity, Stop Bits) */
        {
            LINECONTROL lc;
            memset(&lc, 0, sizeof(lc));
            lc.bDataBits =
                (BYTE)((opts->data_bits >= 5 && opts->data_bits <= 8) ? opts->data_bits : 8);

            if (opts->parity == 'e' || opts->parity == 'E') {
                lc.bParity = 2; /* Even */
            } else if (opts->parity == 'o' || opts->parity == 'O') {
                lc.bParity = 1; /* Odd */
            } else {
                lc.bParity = 0; /* None */
            }

            lc.bStopBits = (BYTE)(opts->stop_bits == 2 ? 2 : 0); /* 0: 1 stop bit, 2: 2 stop bits */
            lc.fTransBreak = 0;

            cbParam = sizeof(lc);
            DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_SETLINECTRL, &lc, sizeof(lc), &cbParam, NULL, 0,
                        NULL);
        }

        /* 3. DCB Configuration & Flow Control */
        {
            DCBINFO dcb;
            memset(&dcb, 0, sizeof(dcb));
            cbData = sizeof(dcb);
            if (DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_GETDCBINFO, NULL, 0, NULL, &dcb, sizeof(dcb),
                            &cbData) == 0) {
                int t = (opts != NULL) ? opts->timeout_ms : -1;
                if (t > 0) {
                    USHORT cs = (USHORT)(t / 10);
                    if (cs == 0) cs = 1;
                    dcb.usReadTimeout = cs;
                    dcb.usWriteTimeout = cs;
                    dcb.fbTimeout = MODE_READ_TIMEOUT;
                } else if (t == 0) {
                    dcb.usReadTimeout = 0;
                    dcb.usWriteTimeout = 1;
                    dcb.fbTimeout = MODE_NOWAIT_READ_TIMEOUT;
                } else {
                    dcb.usReadTimeout = 0xFFFF;
                    dcb.usWriteTimeout = 0xFFFF;
                    dcb.fbTimeout = MODE_NO_WRITE_TIMEOUT;
                }

                if (opts->flow_control == 1) {
                    /* Hardware Flow Control (RTS/CTS) */
                    dcb.fbCtlHndShake |= MODE_CTS_HANDSHAKE;
                    dcb.fbFlowReplace =
                        (dcb.fbFlowReplace & ~MODE_TRANSMIT_TOGGLE) | MODE_RTS_HANDSHAKE;
                    dcb.fbFlowReplace &= ~(MODE_AUTO_TRANSMIT | MODE_AUTO_RECEIVE);
                } else if (opts->flow_control == 2) {
                    /* Software Flow Control (XON/XOFF) */
                    dcb.fbCtlHndShake &= ~MODE_CTS_HANDSHAKE;
                    dcb.fbFlowReplace =
                        (dcb.fbFlowReplace & ~MODE_TRANSMIT_TOGGLE) | MODE_RTS_CONTROL;
                    dcb.fbFlowReplace |= (MODE_AUTO_TRANSMIT | MODE_AUTO_RECEIVE);
                    dcb.bXONChar = 0x11;
                    dcb.bXOFFChar = 0x13;
                } else {
                    /* No Flow Control */
                    dcb.fbCtlHndShake &= ~MODE_CTS_HANDSHAKE;
                    dcb.fbFlowReplace =
                        (dcb.fbFlowReplace & ~MODE_TRANSMIT_TOGGLE) | MODE_RTS_CONTROL;
                    dcb.fbFlowReplace &= ~(MODE_AUTO_TRANSMIT | MODE_AUTO_RECEIVE);
                }

                cbParam = sizeof(dcb);
                DosDevIOCtl(hfile, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, sizeof(dcb), &cbParam, NULL,
                            0, NULL);
            }
        }

        /* 4. Initial Modem Lines (RTS / DTR) */
        if (opts->rts != -1) {
            solo_serial_set_rts_internal(hfile, opts->rts);
        }
        if (opts->dtr != -1) {
            solo_serial_set_dtr_internal(hfile, opts->dtr);
        }
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        DosClose(hfile);
        solo_set_err(err_buf, err_cap, "Out of memory allocating serial port structure");
        return NULL;
    }

    port->handle = hfile;
    port->timeout_ms = (opts != NULL) ? opts->timeout_ms : -1;
    strncpy(port->name, p, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    return port;
}

static solo_serial_status_t solo_serial_apply_dcb_timeout(HFILE handle, int timeout_ms)
{
    DCBINFO dcb;
    ULONG cbData;
    ULONG cbParam;

    memset(&dcb, 0, sizeof(dcb));
    cbData = sizeof(dcb);
    if (DosDevIOCtl(handle, IOCTL_ASYNC, ASYNC_GETDCBINFO, NULL, 0, NULL, &dcb, sizeof(dcb), &cbData) != 0) {
        return SOLO_SERIAL_ERROR;
    }

    if (timeout_ms > 0) {
        USHORT cs = (USHORT)(timeout_ms / 10);
        if (cs == 0) cs = 1;
        dcb.usReadTimeout = cs;
        dcb.usWriteTimeout = cs;
        dcb.fbTimeout = MODE_READ_TIMEOUT;
    } else if (timeout_ms == 0) {
        dcb.usReadTimeout = 0;
        dcb.usWriteTimeout = 1;
        dcb.fbTimeout = MODE_NOWAIT_READ_TIMEOUT;
    } else {
        dcb.usReadTimeout = 0xFFFF;
        dcb.usWriteTimeout = 0xFFFF;
        dcb.fbTimeout = MODE_NO_WRITE_TIMEOUT;
    }

    cbParam = sizeof(dcb);
    if (DosDevIOCtl(handle, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, sizeof(dcb), &cbParam, NULL, 0, NULL) != 0) {
        return SOLO_SERIAL_ERROR;
    }

    return SOLO_SERIAL_OK;
}

solo_status_t solo_serial_write(solo_serial_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    ULONG written = 0;
    APIRET rc;

    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || port->handle == NULLHANDLE || buf == NULL) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    rc = DosWrite(port->handle, (PVOID)buf, (ULONG)len, &written);
    if (rc != 0) {
        if (rc == ERROR_TIMEOUT) {
            if (written_bytes != NULL) *written_bytes = (size_t)written;
            return (written > 0) ? SOLO_OK : SOLO_AGAIN;
        }
        if (written_bytes != NULL) *written_bytes = (size_t)written;
        return (written > 0) ? SOLO_OK : SOLO_ERROR;
    }

    if (written_bytes != NULL) {
        *written_bytes = (size_t)written;
    }
    return SOLO_OK;
}

solo_status_t solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, size_t *read_bytes, int timeout_ms)
{
    ULONG read_len = 0;
    APIRET rc;
    int need_restore = 0;

    if (read_bytes != NULL) {
        *read_bytes = 0;
    }

    if (port == NULL || port->handle == NULLHANDLE || buf == NULL || max_len == 0) {
        return SOLO_ERROR;
    }

    if (timeout_ms >= 0 && timeout_ms != port->timeout_ms) {
        if (solo_serial_apply_dcb_timeout(port->handle, timeout_ms) == SOLO_SERIAL_OK) {
            need_restore = 1;
        }
    }

    rc = DosRead(port->handle, (PVOID)buf, (ULONG)max_len, &read_len);

    if (need_restore) {
        solo_serial_apply_dcb_timeout(port->handle, port->timeout_ms);
    }

    if (rc != 0) {
        if (rc == ERROR_TIMEOUT) {
            if (read_bytes != NULL) *read_bytes = (size_t)read_len;
            return (read_len > 0) ? SOLO_OK : SOLO_AGAIN;
        }
        return SOLO_ERROR;
    }

    if (read_len == 0) {
        return SOLO_AGAIN;
    }

    if (read_bytes != NULL) {
        *read_bytes = (size_t)read_len;
    }
    return SOLO_OK;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return SOLO_SERIAL_ERROR;
    }
    if (solo_serial_apply_dcb_timeout(port->handle, timeout_ms) != SOLO_SERIAL_OK) {
        return SOLO_SERIAL_ERROR;
    }
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

solo_line_state_t solo_serial_get_cts(solo_serial_t *port)
{
    (void)port;
    return SOLO_LINE_ERROR;
}

solo_line_state_t solo_serial_get_dsr(solo_serial_t *port)
{
    (void)port;
    return SOLO_LINE_ERROR;
}

solo_line_state_t solo_serial_get_cd(solo_serial_t *port)
{
    (void)port;
    return SOLO_LINE_ERROR;
}

solo_line_state_t solo_serial_get_ri(solo_serial_t *port)
{
    (void)port;
    return SOLO_LINE_ERROR;
}

solo_serial_status_t solo_serial_drain(solo_serial_t *port)
{
    int attempts = 0;
    USHORT outQue = 0;
    ULONG cbData;

    if (port == NULL || port->handle == NULLHANDLE) {
        return SOLO_SERIAL_ERROR;
    }

    /* Flush OS/2 device write buffers */
    DosResetBuffer(port->handle);

    /* Wait for transmitter queue to drain */
    while (attempts++ < 100) {
        cbData = sizeof(outQue);
        if (DosDevIOCtl(port->handle, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT, NULL, 0, NULL, &outQue,
                        sizeof(outQue), &cbData) == 0) {
            if (outQue == 0) {
                break;
            }
        } else {
            break;
        }
        DosSleep(10); /* 10 ms */
    }

    return SOLO_SERIAL_OK;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return SOLO_SERIAL_ERROR;
    }
    return solo_serial_set_rts_internal(port->handle, (state == SOLO_SERIAL_LINE_ON || (int)state == 1) ? 1 : 0);
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return SOLO_SERIAL_ERROR;
    }
    return solo_serial_set_dtr_internal(port->handle, (state == SOLO_SERIAL_LINE_ON || (int)state == 1) ? 1 : 0);
}

void solo_serial_close(solo_serial_t *port)
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
typedef int make_iso_compilers_happy_serial_os2;
#endif /* SOLO_IS_OS2 */
