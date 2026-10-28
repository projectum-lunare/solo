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
    char name[16];
};

static int solo_serial_set_rts_internal(HFILE handle, int on)
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
    return (rc == 0) ? 0 : -1;
}

static int solo_serial_set_dtr_internal(HFILE handle, int on)
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
    return (rc == 0) ? 0 : -1;
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    HFILE hfile = NULLHANDLE;
    ULONG action = 0;
    APIRET rc;
    solo_serial_t *port;
    const char *p = (path && path[0] != '\0') ? path : "COM1";

    /*
     * DosOpen with:
     * - OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW: open if exists
     * - OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE: exclusive read/write access
     */
    rc = DosOpen((PCSZ)p, &hfile, &action, 0, FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                 OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE, NULL);

    if (rc != 0) {
        if (err_msg != NULL) {
            char buf[128];
            snprintf(buf, sizeof(buf), "DosOpen(%s) failed (SYS%04lu)", p, (unsigned long)rc);
            *err_msg = safe_strdup(buf);
        }
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
                dcb.usWriteTimeout = 6000; /* 60 seconds */
                dcb.usReadTimeout = 100;   /* 1 second */
                dcb.fbTimeout = MODE_READ_TIMEOUT;

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
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Out of memory allocating serial port structure");
        }
        return NULL;
    }

    port->handle = hfile;
    strncpy(port->name, p, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    return port;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
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

int solo_serial_drain(solo_serial_t *port)
{
    int attempts = 0;
    USHORT outQue = 0;
    ULONG cbData;

    if (port == NULL || port->handle == NULLHANDLE) {
        return -1;
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

    return 0;
}

int solo_serial_set_rts(solo_serial_t *port, int on)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return -1;
    }
    return solo_serial_set_rts_internal(port->handle, on);
}

int solo_serial_set_dtr(solo_serial_t *port, int on)
{
    if (port == NULL || port->handle == NULLHANDLE) {
        return -1;
    }
    return solo_serial_set_dtr_internal(port->handle, on);
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
