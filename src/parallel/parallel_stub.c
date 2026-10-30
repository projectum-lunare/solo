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

solo_parallel_t *solo_parallel_open(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "Parallel ports are not supported on this platform");
    return NULL;
}

solo_status_t solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    (void)port;
    (void)buf;
    (void)len;
    if (written_bytes != NULL) *written_bytes = 0;
    return SOLO_UNSUPPORTED;
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
    (void)port;
    (void)mode;
    return SOLO_PARALLEL_UNSUPPORTED;
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
