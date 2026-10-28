/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_FILESYSTEM_H
#define SOLO_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Checks whether a file, device node, or directory exists at path.
 * Returns 1 if exists, 0 otherwise.
 */
int solo_fs_exists(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_FILESYSTEM_H */
