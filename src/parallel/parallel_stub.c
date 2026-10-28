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

#if !defined(SOLO_OS_HAS_NATIVE_DRIVERS)

#include <stdlib.h>
#include <string.h>

#include <solo/parallel.h>

struct parallel_port {
    int dummy;
};

solo_parallel_t *solo_parallel_open(const char *path, char **err_msg)
{
    (void)path;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("Parallel ports are not supported on this platform");
    }
    return NULL;
}

int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len)
{
    (void)port;
    (void)buf;
    (void)len;
    return -1;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    (void)port;
    return -1;
}

void solo_parallel_close(solo_parallel_t *port)
{
    (void)port;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_stub;
#endif /* !SOLO_OS_HAS_NATIVE_DRIVERS */
