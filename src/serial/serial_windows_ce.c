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

#include <solo/serial.h>

#include <windows.h>

struct serial_port {
    HANDLE handle;
    DCB orig_dcb;
    int timeout_ms;
};

static solo_serial_status_t solo_serial_apply_comm_timeouts(HANDLE handle, int timeout_ms)
{
    COMMTIMEOUTS to;
    memset(&to, 0, sizeof(to));
    if (timeout_ms > 0) {
        /* Timed: wait up to timeout_ms for first byte, return immediately if already buffered */
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = MAXDWORD;
        to.ReadTotalTimeoutConstant = (DWORD)timeout_ms;
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = (DWORD)timeout_ms;
    } else if (timeout_ms == 0) {
        /* Non-blocking: return immediately with whatever is in buffer (0 if empty) */
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = 0;
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = 1; /* minimal timeout to avoid infinite blocking */
    } else {
        /* Indefinite blocking: wait until data arrives */
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = MAXDWORD;
        to.ReadTotalTimeoutConstant = MAXDWORD - 1;
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = 0; /* 0 means infinite wait in Comm API */
    }
    return SetCommTimeouts(handle, &to) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    HANDLE h;
    solo_serial_t *port;
    DCB dcb;
    solo_serial_opts_t default_opts;
    WCHAR target_path[64];
    char buf[64];
    size_t len;
    const char *p = (path && path[0] != '\0') ? path : "COM1:";

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
    h = CreateFileW(target_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "CreateFileW(%s) failed (error %lu)", path,
                     (unsigned long)GetLastError());
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        CloseHandle(h);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }
    port->handle = h;
    port->timeout_ms = (opts != NULL) ? opts->timeout_ms : -1;

    memset(&port->orig_dcb, 0, sizeof(DCB));
    port->orig_dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &port->orig_dcb)) {
        if (err_msg)
            *err_msg = safe_strdup("GetCommState failed");
        CloseHandle(h);
        free(port);
        return NULL;
    }

    dcb = port->orig_dcb;
    dcb.BaudRate = (DWORD)opts->baud;
    dcb.ByteSize = (BYTE)opts->data_bits;

    /* Parity */
    if (opts->parity == 'e' || opts->parity == 'E') {
        dcb.fParity = TRUE;
        dcb.Parity = EVENPARITY;
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        dcb.fParity = TRUE;
        dcb.Parity = ODDPARITY;
    } else {
        dcb.fParity = FALSE;
        dcb.Parity = NOPARITY;
    }

    /* Stop bits */
    if (opts->stop_bits == 2) {
        dcb.StopBits = TWOSTOPBITS;
    } else {
        dcb.StopBits = ONESTOPBIT;
    }

    /* Flow Control */
    if (opts->flow_control == 1) { /* hardware: RTS/CTS */
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
    } else if (opts->flow_control == 2) { /* software: XON/XOFF */
        dcb.fOutxCtsFlow = FALSE;
        dcb.fRtsControl = (opts->rts) ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
    } else { /* none / off */
        dcb.fOutxCtsFlow = FALSE;
        dcb.fRtsControl = (opts->rts) ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
    }

    /* Initial DTR line state */
    dcb.fDtrControl = (opts->dtr) ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;

    if (!SetCommState(h, &dcb)) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "SetCommState failed (error %lu)",
                     (unsigned long)GetLastError());
            *err_msg = safe_strdup(buf);
        }
        CloseHandle(h);
        free(port);
        return NULL;
    }

    /* Configure communication timeouts */
    solo_serial_apply_comm_timeouts(h, port->timeout_ms);

    /* Assert or clear RTS/DTR lines if specified */
    if (opts->rts >= 0) {
        EscapeCommFunction(h, opts->rts ? SETRTS : CLRRTS);
    }
    if (opts->dtr >= 0) {
        EscapeCommFunction(h, opts->dtr ? SETDTR : CLRDTR);
    }

    return port;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return SOLO_SERIAL_ERROR;
    }
    return EscapeCommFunction(port->handle, (state == SOLO_SERIAL_LINE_ON || (int)state == 1) ? SETRTS : CLRRTS) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return SOLO_SERIAL_ERROR;
    }
    return EscapeCommFunction(port->handle, (state == SOLO_SERIAL_LINE_ON || (int)state == 1) ? SETDTR : CLRDTR) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
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

int solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, int timeout_ms)
{
    DWORD read_bytes = 0;
    int need_restore = 0;

    if (port == NULL || port->handle == INVALID_HANDLE_VALUE || buf == NULL || max_len == 0) {
        return -1;
    }

    if (timeout_ms >= 0 && timeout_ms != port->timeout_ms) {
        if (solo_serial_apply_comm_timeouts(port->handle, timeout_ms) == SOLO_SERIAL_OK) {
            need_restore = 1;
        }
    }

    if (!ReadFile(port->handle, buf, (DWORD)max_len, &read_bytes, NULL)) {
        if (need_restore) {
            solo_serial_apply_comm_timeouts(port->handle, port->timeout_ms);
        }
        return -1;
    }

    if (need_restore) {
        solo_serial_apply_comm_timeouts(port->handle, port->timeout_ms);
    }

    return (int)read_bytes;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return SOLO_SERIAL_ERROR;
    }
    if (solo_serial_apply_comm_timeouts(port->handle, timeout_ms) != SOLO_SERIAL_OK) {
        return SOLO_SERIAL_ERROR;
    }
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

int solo_serial_get_cts(solo_serial_t *port)
{
    DWORD modem_stat = 0;
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) return -1;
    if (!GetCommModemStatus(port->handle, &modem_stat)) return -1;
    return (modem_stat & MS_CTS_ON) ? 1 : 0;
}

int solo_serial_get_dsr(solo_serial_t *port)
{
    DWORD modem_stat = 0;
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) return -1;
    if (!GetCommModemStatus(port->handle, &modem_stat)) return -1;
    return (modem_stat & MS_DSR_ON) ? 1 : 0;
}

int solo_serial_get_cd(solo_serial_t *port)
{
    DWORD modem_stat = 0;
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) return -1;
    if (!GetCommModemStatus(port->handle, &modem_stat)) return -1;
    return (modem_stat & MS_RLSD_ON) ? 1 : 0;
}

int solo_serial_get_ri(solo_serial_t *port)
{
    DWORD modem_stat = 0;
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) return -1;
    if (!GetCommModemStatus(port->handle, &modem_stat)) return -1;
    return (modem_stat & MS_RING_ON) ? 1 : 0;
}

int solo_serial_drain(solo_serial_t *port)
{
    if (port == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    return FlushFileBuffers(port->handle) ? 0 : -1;
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        if (port->handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(port->handle);
            SetCommState(port->handle, &port->orig_dcb);
            CloseHandle(port->handle);
            port->handle = INVALID_HANDLE_VALUE;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_windows_ce;
#endif /* SOLO_IS_WINDOWS_CE */
