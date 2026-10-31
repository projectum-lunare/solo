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

#if SOLO_IS_DOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#include <bios.h>
#include <conio.h>

#include <solo/serial.h>

struct serial_port {
    int fd;                 /* DOS file handle (or -1 if using pure hardware/BIOS) */
    unsigned int bios_port; /* 0 for COM1, 1 for COM2, 2 for COM3, 3 for COM4 */
    unsigned short base_io; /* 0x3F8, 0x2F8, 0x3E8, 0x2E8 */
    int timeout_ms;         /* Read/write timeout */
    char name[8];           /* "COM1", "COM2", etc. */
};

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char *err_buf, size_t err_cap)
{
    solo_serial_t *port;
    char dos_name[8];
    unsigned int bios_port = 0;
    unsigned short base_io = 0x3F8;
    int fd;
    unsigned short init_byte = 0;
    solo_serial_opts_t default_opts;
    const char *p = (path && path[0] != '\0') ? path : "COM1";

    if ((p[0] == 'C' || p[0] == 'c') && (p[1] == 'O' || p[1] == 'o') &&
        (p[2] == 'M' || p[2] == 'm') && (p[3] >= '1' && p[3] <= '4')) {
        bios_port = (unsigned int)(p[3] - '1');
    } else if (p[0] >= '1' && p[0] <= '4' && p[1] == '\0') {
        bios_port = (unsigned int)(p[0] - '1');
    }
    base_io = (bios_port == 1)   ? 0x2F8
              : (bios_port == 2) ? 0x3E8
              : (bios_port == 3) ? 0x2E8
                                 : 0x3F8;
    sprintf(dos_name, "COM%u", bios_port + 1);

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

    /* 1. Calculate BIOS serial initialization byte */
    /* Baud rate */
    switch (opts->baud) {
    case 110:
        init_byte |= _COM_110;
        break;
    case 150:
        init_byte |= _COM_150;
        break;
    case 300:
        init_byte |= _COM_300;
        break;
    case 600:
        init_byte |= _COM_600;
        break;
    case 1200:
        init_byte |= _COM_1200;
        break;
    case 2400:
        init_byte |= _COM_2400;
        break;
    case 4800:
        init_byte |= _COM_4800;
        break;
    case 9600:
    default:
        init_byte |= _COM_9600;
        break;
    }

    /* Parity */
    if (opts->parity == 'e' || opts->parity == 'E') {
        init_byte |= _COM_EVENPARITY;
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        init_byte |= _COM_ODDPARITY;
    } else {
        init_byte |= _COM_NOPARITY;
    }

    /* Stop bits */
    if (opts->stop_bits == 2) {
        init_byte |= _COM_STOP2;
    } else {
        init_byte |= _COM_STOP1;
    }

    /* Data bits */
    if (opts->data_bits == 7) {
        init_byte |= _COM_CHR7;
    } else {
        init_byte |= _COM_CHR8;
    }

    /* 2. Initialize UART via BIOS INT 14h */
    _bios_serialcom(_COM_INIT, bios_port, init_byte);

    /* 3. For modern / high baud rates (> 9600), set UART 16550 divisor latch directly */
    if (opts->baud > 9600) {
        unsigned short divisor = (unsigned short)(115200 / opts->baud);
        if (divisor > 0) {
            unsigned char lcr = (unsigned char)inp(base_io + 3);
            outp(base_io + 3, lcr | 0x80); /* Enable DLAB */
            outp(base_io + 0, (unsigned char)(divisor & 0xFF));
            outp(base_io + 1, (unsigned char)((divisor >> 8) & 0xFF));
            outp(base_io + 3, lcr & ~0x80); /* Clear DLAB */
        }
    }

    /* 4. Open DOS device handle */
    fd = open(dos_name, O_RDWR | O_BINARY);

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        if (fd >= 0)
            close(fd);
        solo_set_err(err_buf, err_cap, "Memory allocation failure for DOS serial port");
        return NULL;
    }

    port->fd = fd;
    port->bios_port = bios_port;
    port->base_io = base_io;
    port->timeout_ms = opts->timeout_ms;
    strncpy(port->name, dos_name, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    /* 5. Set initial modem control lines (DTR, RTS, OUT2) */
    {
        unsigned char mcr = (unsigned char)inp(base_io + 4);
        mcr |= 0x08; /* OUT2: enable interrupts */
        if (opts->dtr > 0)
            mcr |= 0x01;
        else if (opts->dtr == 0)
            mcr &= ~0x01;
        if (opts->rts > 0)
            mcr |= 0x02;
        else if (opts->rts == 0)
            mcr &= ~0x02;
        outp(base_io + 4, mcr);
    }

    return port;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    unsigned char mcr;
    if (port == NULL)
        return SOLO_SERIAL_ERROR;
    mcr = (unsigned char)inp(port->base_io + 4);
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        mcr |= 0x02; /* Bit 1: RTS */
    else
        mcr &= ~0x02;
    outp(port->base_io + 4, mcr);
    return SOLO_SERIAL_OK;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    unsigned char mcr;
    if (port == NULL)
        return SOLO_SERIAL_ERROR;
    mcr = (unsigned char)inp(port->base_io + 4);
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        mcr |= 0x01; /* Bit 0: DTR */
    else
        mcr &= ~0x01;
    outp(port->base_io + 4, mcr);
    return SOLO_SERIAL_OK;
}

solo_status_t solo_serial_write(solo_serial_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    if (written_bytes != NULL) {
        *written_bytes = 0;
    }
    if (port == NULL || buf == NULL)
        return SOLO_ERROR;
    if (len == 0)
        return SOLO_OK;

    /* Primary: direct UART THR with timeout deadline */
    if (port->base_io != 0) {
        size_t i;
        clock_t start = 0;
        clock_t wait_ticks = 0;
        int has_timeout = 0;

        if (port->timeout_ms > 0) {
            has_timeout = 1;
            start = clock();
            wait_ticks = (clock_t)(((long)port->timeout_ms * CLOCKS_PER_SEC) / 1000L);
            if (wait_ticks == 0) wait_ticks = 1;
        }

        for (i = 0; i < len; i++) {
            if (has_timeout) {
                while ((inp(port->base_io + 5) & 0x20) == 0) {
                    if ((clock() - start) >= wait_ticks) {
                        if (written_bytes != NULL) *written_bytes = i;
                        return (i > 0) ? SOLO_OK : SOLO_AGAIN;
                    }
                }
            } else if (port->timeout_ms == 0) {
                /* Non-blocking: check if transmitter ready */
                if ((inp(port->base_io + 5) & 0x20) == 0) {
                    if (written_bytes != NULL) *written_bytes = i;
                    return (i > 0) ? SOLO_OK : SOLO_AGAIN;
                }
            } else {
                /* Blocking indefinitely */
                while ((inp(port->base_io + 5) & 0x20) == 0) {
                    /* wait */
                }
            }
            outp(port->base_io + 0, (unsigned char)buf[i]);
        }
        if (written_bytes != NULL) *written_bytes = len;
        return SOLO_OK;
    }

    /* Fallback: write via DOS handle */
    if (port->fd >= 0) {
        int n = write(port->fd, buf, len);
        if (n >= 0) {
            if (written_bytes != NULL) *written_bytes = (size_t)n;
            return SOLO_OK;
        }
        return SOLO_ERROR;
    }

    return SOLO_ERROR;
}

solo_status_t solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, size_t *read_bytes, int timeout_ms)
{
    int eff_timeout;
    if (read_bytes != NULL) {
        *read_bytes = 0;
    }
    if (port == NULL || buf == NULL || max_len == 0)
        return SOLO_ERROR;

    eff_timeout = (timeout_ms >= 0) ? timeout_ms : port->timeout_ms;

    if (port->base_io != 0) {
        if (eff_timeout > 0) {
            clock_t start = clock();
            clock_t wait_ticks = (clock_t)(((long)eff_timeout * CLOCKS_PER_SEC) / 1000L);
            if (wait_ticks == 0) wait_ticks = 1;
            while ((inp(port->base_io + 5) & 0x01) == 0) {
                if ((clock() - start) >= wait_ticks) {
                    return SOLO_AGAIN; /* Timeout */
                }
            }
        } else if (eff_timeout == 0) {
            if ((inp(port->base_io + 5) & 0x01) == 0) {
                return SOLO_AGAIN; /* Would block */
            }
        } else {
            while ((inp(port->base_io + 5) & 0x01) == 0) {
                /* wait indefinitely */
            }
        }
        buf[0] = (char)inp(port->base_io + 0);
        if (read_bytes != NULL) *read_bytes = 1;
        return SOLO_OK;
    }

    if (port->fd >= 0) {
        int n = read(port->fd, buf, max_len);
        if (n >= 0) {
            if (read_bytes != NULL) *read_bytes = (size_t)n;
            return SOLO_OK;
        }
        return SOLO_ERROR;
    }
    return SOLO_ERROR;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL)
        return SOLO_SERIAL_ERROR;
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

solo_line_state_t solo_serial_get_cts(solo_serial_t *port)
{
    if (port == NULL) return SOLO_LINE_ERROR;
    return (inp(port->base_io + 6) & 0x10) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
}

solo_line_state_t solo_serial_get_dsr(solo_serial_t *port)
{
    if (port == NULL) return SOLO_LINE_ERROR;
    return (inp(port->base_io + 6) & 0x20) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
}

solo_line_state_t solo_serial_get_ri(solo_serial_t *port)
{
    if (port == NULL) return SOLO_LINE_ERROR;
    return (inp(port->base_io + 6) & 0x40) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
}

solo_line_state_t solo_serial_get_cd(solo_serial_t *port)
{
    if (port == NULL) return SOLO_LINE_ERROR;
    return (inp(port->base_io + 6) & 0x80) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
}

solo_serial_status_t solo_serial_drain(solo_serial_t *port)
{
    int timeout = 100000;
    if (port == NULL)
        return SOLO_SERIAL_ERROR;

    if (port->fd >= 0) {
        _commit(port->fd);
    }

    /* Poll LSR bit 6: Transmitter Shift Register Empty (TEMT - completely empty line) */
    while ((inp(port->base_io + 5) & 0x40) == 0 && --timeout > 0) {
        /* spin wait */
    }
    return (timeout > 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        if (port->fd >= 0) {
            close(port->fd);
            port->fd = -1;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_dos;
#endif /* SOLO_IS_DOS */
