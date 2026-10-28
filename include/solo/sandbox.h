/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_SANDBOX_H
#define SOLO_SANDBOX_H

#include <stddef.h>

#define SOLO_SANDBOX_READ  (1 << 0)
#define SOLO_SANDBOX_WRITE (1 << 1)
#define SOLO_SANDBOX_EXEC  (1 << 2)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the name of the active sandboxing backend:
 * "landlock", "openbsd", "capsicum", or "none".
 * If version_out is non-NULL, stores ABI version or 0.
 */
const char *solo_sandbox_backend_name(int *version_out);

/*
 * Unveils a filesystem path with given permissions (read/write/exec).
 * Returns 0 on success, or -1 on error (with err_msg populated).
 */
int solo_sandbox_unveil(const char *path, int permissions, char **err_msg);

/*
 * Locks the sandbox, enforcing the restrictions on the current process.
 * After locking, no further paths can be unveiled.
 * Returns 0 on success, or -1 on error (with err_msg populated).
 */
int solo_sandbox_lock(char **err_msg);

/*
 * Pledges a set of system call promises (e.g. "stdio rpath wpath").
 * On platforms supporting pledge (OpenBSD), enforces the promises.
 * On other platforms, returns 0.
 */
int solo_sandbox_pledge(const char *promises, char **err_msg);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SANDBOX_H */
