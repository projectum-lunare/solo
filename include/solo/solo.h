/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_H
#define SOLO_H

#include <solo/platform.h>
#include <solo/filesystem.h>
#include <solo/fs.h>
#include <solo/parallel.h>
#include <solo/serial.h>
#include <solo/socket.h>
#include <solo/sandbox.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOLO_VERSION_MAJOR 0
#define SOLO_VERSION_MINOR 1
#define SOLO_VERSION_PATCH 0
#define SOLO_VERSION_STRING "0.1.0"

/*
 * Returns the version string of the solo runtime foundation.
 */
const char *solo_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_H */
