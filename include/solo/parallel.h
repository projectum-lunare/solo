/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_PARALLEL_H
#define SOLO_PARALLEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct parallel_port solo_parallel_t;

/*
 * Opens a parallel / Centronics port (IEEE 1284 / LPT) for direct
 * communication with impact printers.
 * On failure, returns NULL and populates *err_msg with a dynamically
 * allocated string (if err_msg != NULL). The caller owns the string and
 * must release it with free().
 */
solo_parallel_t *solo_parallel_open(const char *path, char **err_msg);

/*
 * Writes data to the parallel port. Returns the number of bytes written,
 * or -1 on error.
 */
int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len);

/*
 * Waits for the printer to empty its hardware buffer (hardware drain).
 */
int solo_parallel_drain(solo_parallel_t *port);

/*
 * Closes the parallel port and frees allocated resources.
 */
void solo_parallel_close(solo_parallel_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_PARALLEL_H */
