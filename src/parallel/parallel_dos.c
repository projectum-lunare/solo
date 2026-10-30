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
#include <fcntl.h>
#include <io.h>
#include <bios.h>

#include <solo/parallel.h>

struct parallel_port {
    int fd;                 /* DOS file handle (or -1 if using pure BIOS) */
    unsigned int bios_port; /* 0 for LPT1, 1 for LPT2, 2 for LPT3 */
    char name[8];           /* "LPT1", "LPT2", "LPT3", or "PRN" */
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    solo_parallel_t *port;
    const char *dos_name = (path && path[0] != '\0') ? path : "LPT1";
    unsigned int bios_port = 0;
    int fd;

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    if ((dos_name[0] == 'L' || dos_name[0] == 'l') && (dos_name[1] == 'P' || dos_name[1] == 'p') &&
        (dos_name[2] == 'T' || dos_name[2] == 't') && dos_name[3] >= '1' && dos_name[3] <= '3') {
        bios_port = (unsigned int)(dos_name[3] - '1');
    }

    /*
     * Attempt to open DOS character device file ("LPT1", "PRN", etc.) in binary write mode.
     * DOS character devices support direct binary block writes.
     */
    fd = open(dos_name, O_WRONLY | O_BINARY);

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        if (fd >= 0)
            close(fd);
        if (err_msg != NULL)
            *err_msg = safe_strdup("Memory allocation failure for DOS parallel port");
        return NULL;
    }

    port->fd = fd;
    port->bios_port = bios_port;
    strncpy(port->name, dos_name, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    /* Initialize printer port via BIOS INT 17h */
    _bios_printer(_PRINTER_INIT, bios_port, 0);

    return port;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    if (port == NULL || buf == NULL)
        return -1;
    if (len == 0)
        return 0;

    /* Primary: fast block write via DOS handle */
    if (port->fd >= 0) {
        int n = write(port->fd, buf, len);
        if (n >= 0)
            return n;
    }

    /* Fallback: byte-by-byte write via BIOS INT 17h */
    {
        size_t i;
        for (i = 0; i < len; i++) {
            unsigned short status =
                _bios_printer(_PRINTER_WRITE, port->bios_port, (unsigned char)buf[i]);
            /* Check error bits: timeout (0x01) or I/O error (0x08) */
            if ((status & 0x09) != 0) {
                return (i > 0) ? (int)i : -1;
            }
        }
        return (int)len;
    }
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
    unsigned short st;
    int pins = 0;
    if (port == NULL || pins_out == NULL) return SOLO_PARALLEL_ERROR;
    st = _bios_printer(_PRINTER_STATUS, (unsigned short)port->bios_port, 0);
    if (!(st & 0x80)) pins |= SOLO_PARALLEL_PIN_BUSY;
    if (st & 0x40)    pins |= SOLO_PARALLEL_PIN_ACK;
    if (st & 0x20)    pins |= SOLO_PARALLEL_PIN_PE;
    if (st & 0x10)    pins |= SOLO_PARALLEL_PIN_SLCT;
    if (st & 0x08)    pins |= SOLO_PARALLEL_PIN_FAULT;
    *pins_out = pins;
    return SOLO_PARALLEL_OK;
}

solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode)
{
    if (port == NULL) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    if (port == NULL)
        return -1;

    /* Flush DOS file buffers if using file handle */
    if (port->fd >= 0) {
        _commit(port->fd);
    }

    /* Poll BIOS INT 17h printer status until not BUSY (bit 7 = 0x80) */
    {
        int attempts;
        for (attempts = 0; attempts < 1000; attempts++) {
            unsigned short status = _bios_printer(_PRINTER_STATUS, port->bios_port, 0);
            /* Bit 7 (0x80) is inverted BUSY line: 1 means NOT busy */
            if (status & 0x80) {
                return 0;
            }
        }
        return -1; /* Timeout: printer still busy */
    }
}

void solo_parallel_close(solo_parallel_t *port)
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
typedef int make_iso_compilers_happy_parallel_dos;
#endif /* SOLO_IS_DOS */
