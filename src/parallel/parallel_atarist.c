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

#if SOLO_IS_ATARIST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osbind.h>

#include <solo/parallel.h>

struct parallel_port {
    short dev_id; /* BIOS device 0 = PRT: */
    char name[16];
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    solo_parallel_t *port;
    (void)path;
    (void)err_msg;

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Out of memory allocating parallel port handle");
        }
        return NULL;
    }

    port->dev_id = 0; /* BIOS Device 0: Printer (PRT:) */
    strncpy(port->name, (path && path[0] != '\0') ? path : "PRT:", sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    return port;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    size_t i;

    if (port == NULL || buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        /*
         * Poll Bcostat(0) until printer is ready to accept a byte.
         * The Atari ST sends parallel data via the Yamaha YM2149 sound chip
         * and monitors BUSY via the MFP 68901.
         * If the printer is powered off or offline, timeout after a delay.
         */
        long timeout = 2000000L;
        while (Bcostat(port->dev_id) == 0) {
            if (--timeout <= 0) {
                return (i > 0) ? (int)i : -1;
            }
        }

        Bconout(port->dev_id, (unsigned char)buf[i]);
    }

    return (int)len;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    long timeout = 2000000L;

    if (port == NULL) {
        return -1;
    }

    /* Wait for hardware BUSY line to clear */
    while (Bcostat(port->dev_id) == 0) {
        if (--timeout <= 0) {
            return -1;
        }
    }

    return 0;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_atarist;
#endif /* SOLO_IS_ATARIST */
