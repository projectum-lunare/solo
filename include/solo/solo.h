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

#ifndef SOLO_NO_PLATFORM
#include <solo/platform.h>
#endif

#ifndef SOLO_NO_FILESYSTEM
#include <solo/filesystem.h>
#include <solo/fs.h>
#endif

#ifndef SOLO_NO_PARALLEL
#include <solo/parallel.h>
#endif

#ifndef SOLO_NO_SERIAL
#include <solo/serial.h>
#endif

#ifndef SOLO_NO_SOCKET
#include <solo/socket.h>
#endif

#ifndef SOLO_NO_SANDBOX
#include <solo/sandbox.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SOLO_VERSION_MAJOR 0
#define SOLO_VERSION_MINOR 2
#define SOLO_VERSION_PATCH 0
#define SOLO_VERSION_STRING "0.2.0"

/*
 * Returns the version string of the solo runtime foundation.
 */
const char *solo_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_H */
