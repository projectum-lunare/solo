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

/*
 * Sandbox backend identifiers.
 */
typedef enum {
    SOLO_SANDBOX_BACKEND_NONE     = 0,
    SOLO_SANDBOX_BACKEND_LANDLOCK = 1,
    SOLO_SANDBOX_BACKEND_OPENBSD  = 2,
    SOLO_SANDBOX_BACKEND_CAPSICUM = 3
} solo_sandbox_backend_t;

/*
 * Sandbox filesystem unveil access permissions.
 */
typedef enum {
    SOLO_SANDBOX_PERM_NONE  = 0,
    SOLO_SANDBOX_PERM_READ  = (1 << 0),
    SOLO_SANDBOX_PERM_WRITE = (1 << 1),
    SOLO_SANDBOX_PERM_EXEC  = (1 << 2)
} solo_sandbox_perm_t;

#define SOLO_SANDBOX_READ  SOLO_SANDBOX_PERM_READ
#define SOLO_SANDBOX_WRITE SOLO_SANDBOX_PERM_WRITE
#define SOLO_SANDBOX_EXEC  SOLO_SANDBOX_PERM_EXEC

/*
 * Sandbox operation status codes.
 */
typedef enum {
    SOLO_SANDBOX_OK          =  0, /* Operation succeeded and restriction was applied */
    SOLO_SANDBOX_ERROR       = -1, /* Operational error (e.g. invalid path, permission denied) */
    SOLO_SANDBOX_UNSUPPORTED = -2  /* Sandbox or requested feature not supported on platform/kernel */
} solo_sandbox_status_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the active sandboxing backend type enum:
 * SOLO_SANDBOX_BACKEND_NONE, SOLO_SANDBOX_BACKEND_LANDLOCK,
 * SOLO_SANDBOX_BACKEND_OPENBSD, or SOLO_SANDBOX_BACKEND_CAPSICUM.
 * If version_out is non-NULL, stores ABI version or 0.
 */
solo_sandbox_backend_t solo_sandbox_backend(int *version_out);

/*
 * Returns the name of the active sandboxing backend:
 * "landlock", "openbsd", "capsicum", or "none".
 * If version_out is non-NULL, stores ABI version or 0.
 */
const char *solo_sandbox_backend_name(int *version_out);

/*
 * Unveils a filesystem path with given permissions (read/write/exec).
 *
 * NOTE: Sandboxing operations are process-global.
 * Under Linux (Landlock), calls to unveil() accumulate path rules into a
 * process-wide ruleset until solo_sandbox_lock() is called to enforce them.
 * Under FreeBSD (Capsicum), path-based unveil is not supported and returns
 * SOLO_SANDBOX_UNSUPPORTED.
 *
 * Returns SOLO_SANDBOX_OK on success,
 * SOLO_SANDBOX_UNSUPPORTED if sandboxing is not supported on this platform,
 * or SOLO_SANDBOX_ERROR on failure (with *err_msg dynamically allocated
 * if err_msg != NULL; the caller owns this string and must release it with free()).
 */
solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char **err_msg);

/*
 * Locks the sandbox, enforcing the restrictions on the current process.
 * If called without prior unveil(), enforces an empty ruleset that prevents
 * opening any newly restricted filesystem paths.
 * Returns SOLO_SANDBOX_OK on success,
 * SOLO_SANDBOX_UNSUPPORTED if sandboxing is not supported on this platform,
 * or SOLO_SANDBOX_ERROR on failure (with *err_msg dynamically allocated
 * if err_msg != NULL; the caller owns this string and must release it with free()).
 */
solo_sandbox_status_t solo_sandbox_lock(char **err_msg);

/*
 * Pledges a set of system call promises (e.g. "stdio rpath wpath").
 * On platforms supporting pledge (OpenBSD), enforces the promises.
 * On unsupported platforms, returns SOLO_SANDBOX_UNSUPPORTED (-2) and populates
 * *err_msg dynamically (if err_msg != NULL; caller must release it with free()).
 * Returns SOLO_SANDBOX_OK on success, or SOLO_SANDBOX_ERROR on failure.
 */
solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char **err_msg);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SANDBOX_H */
