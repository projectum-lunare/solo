#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>
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
#include <solo/sandbox.h>

#if SOLO_IS_LINUX || defined(__linux__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef __NR_landlock_create_ruleset
#if defined(__x86_64__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule       445
#define __NR_landlock_restrict_self  446
#elif defined(__i386__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule       445
#define __NR_landlock_restrict_self  446
#elif defined(__aarch64__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule       445
#define __NR_landlock_restrict_self  446
#elif defined(__arm__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule       445
#define __NR_landlock_restrict_self  446
#elif defined(__riscv)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule       445
#define __NR_landlock_restrict_self  446
#endif
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef O_PATH
#define O_PATH 010000000
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif

#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#define LANDLOCK_RULE_PATH_BENEATH      1

struct landlock_ruleset_attr {
#if defined(__GNUC__)
    __extension__ unsigned long long handled_access_fs;
#else
    unsigned long handled_access_fs;
#endif
};

struct landlock_path_beneath_attr {
#if defined(__GNUC__)
    __extension__ unsigned long long allowed_access;
#else
    unsigned long allowed_access;
#endif
    int parent_fd;
};

static int s_ruleset_fd = -1;
static int s_cached_abi = -2;

static int probe_landlock_abi(void)
{
#if defined(__NR_landlock_create_ruleset)
    long res;
    if (s_cached_abi != -2) {
        return s_cached_abi;
    }
    res = syscall(__NR_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
    if (res > 0) {
        s_cached_abi = (int)res;
    } else {
        s_cached_abi = -1;
    }
    return s_cached_abi;
#else
    s_cached_abi = -1;
    return -1;
#endif
}

static unsigned long get_fs_mask(int abi)
{
    if (abi >= 5) {
        return 0xffffUL;
    } else if (abi >= 3) {
        return 0x7fffUL;
    } else if (abi >= 2) {
        return 0x3fffUL;
    } else {
        return 0x1fffUL;
    }
}

solo_sandbox_backend_t solo_sandbox_backend(int *version_out)
{
    int abi = probe_landlock_abi();
    if (abi > 0) {
        if (version_out != NULL) {
            *version_out = abi;
        }
        return SOLO_SANDBOX_BACKEND_LANDLOCK;
    }
    if (version_out != NULL) {
        *version_out = 0;
    }
    return SOLO_SANDBOX_BACKEND_NONE;
}

const char *solo_sandbox_backend_name(int *version_out)
{
    solo_sandbox_backend_t b = solo_sandbox_backend(version_out);
    return (b == SOLO_SANDBOX_BACKEND_LANDLOCK) ? "landlock" : "none";
}

solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char **err_msg)
{
    int abi;
    int fd;
    struct stat st;
    struct landlock_path_beneath_attr pb;
    unsigned long access_rights = 0;

    if (path == NULL || path[0] == '\0') {
        return SOLO_SANDBOX_OK;
    }

    abi = probe_landlock_abi();
    if (abi <= 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Landlock is not supported by the running Linux kernel");
        }
        return SOLO_SANDBOX_UNSUPPORTED;
    }

#if defined(__NR_landlock_create_ruleset)
    if (s_ruleset_fd < 0) {
        struct landlock_ruleset_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.handled_access_fs = get_fs_mask(abi);
        s_ruleset_fd = (int)syscall(__NR_landlock_create_ruleset, &attr, sizeof(attr), 0);
        if (s_ruleset_fd < 0) {
            if (err_msg != NULL) {
                char buf[128];
                sprintf(buf, "landlock_create_ruleset failed (%s)", strerror(errno));
                *err_msg = safe_strdup(buf);
            }
            return SOLO_SANDBOX_ERROR;
        }
    }

    fd = open(path, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        fd = open(path, O_RDONLY);
    }
    if (fd < 0) {
        if (err_msg != NULL) {
            char buf[256];
            sprintf(buf, "Cannot open path '%.128s' for sandbox: %s", path, strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return SOLO_SANDBOX_ERROR;
    }

    if (fstat(fd, &st) != 0) {
        close(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("fstat failed on sandbox target");
        }
        return SOLO_SANDBOX_ERROR;
    }

    if (S_ISDIR(st.st_mode)) {
        if (permissions & SOLO_SANDBOX_PERM_READ) {
            access_rights |= (1UL << 0) | (1UL << 2) | (1UL << 3); /* EXEC | READ_FILE | READ_DIR */
        }
        if (permissions & SOLO_SANDBOX_PERM_WRITE) {
            access_rights |= (1UL << 1) | (1UL << 4) | (1UL << 5) | (1UL << 7) | (1UL << 8);
            if (abi >= 3)
                access_rights |= (1UL << 14); /* TRUNCATE */
        }
    } else {
        if (permissions & SOLO_SANDBOX_PERM_READ) {
            access_rights |= (1UL << 0) | (1UL << 2); /* EXEC | READ_FILE */
        }
        if (permissions & SOLO_SANDBOX_PERM_WRITE) {
            access_rights |= (1UL << 1); /* WRITE_FILE */
            if (abi >= 3)
                access_rights |= (1UL << 14); /* TRUNCATE */
            if (abi >= 5)
                access_rights |= (1UL << 15); /* IOCTL_DEV */
        }
    }

    access_rights &= get_fs_mask(abi);

    memset(&pb, 0, sizeof(pb));
    pb.allowed_access = access_rights;
    pb.parent_fd = fd;

    if (syscall(__NR_landlock_add_rule, s_ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0) != 0) {
        close(fd);
        if (err_msg != NULL) {
            char buf[256];
            sprintf(buf, "landlock_add_rule failed on '%.128s': %s", path, strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return SOLO_SANDBOX_ERROR;
    }

    close(fd);
    return SOLO_SANDBOX_OK;
#else
    (void)path;
    (void)permissions;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("Landlock is not supported in this build");
    }
    return SOLO_SANDBOX_UNSUPPORTED;
#endif
}

solo_sandbox_status_t solo_sandbox_lock(char **err_msg)
{
#if defined(__NR_landlock_restrict_self)
    if (s_ruleset_fd >= 0) {
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("prctl(PR_SET_NO_NEW_PRIVS) failed");
            }
            return SOLO_SANDBOX_ERROR;
        }
        if (syscall(__NR_landlock_restrict_self, s_ruleset_fd, 0) != 0) {
            if (err_msg != NULL) {
                char buf[128];
                sprintf(buf, "landlock_restrict_self failed (%s)", strerror(errno));
                *err_msg = safe_strdup(buf);
            }
            close(s_ruleset_fd);
            s_ruleset_fd = -1;
            return SOLO_SANDBOX_ERROR;
        }
        close(s_ruleset_fd);
        s_ruleset_fd = -1;
        return SOLO_SANDBOX_OK;
    }
    if (probe_landlock_abi() <= 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Landlock is not supported by the running Linux kernel");
        }
        return SOLO_SANDBOX_UNSUPPORTED;
    }
    return SOLO_SANDBOX_OK;
#else
    if (err_msg != NULL) {
        *err_msg = safe_strdup("Landlock is not supported in this build");
    }
    return SOLO_SANDBOX_UNSUPPORTED;
#endif
}

solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char **err_msg)
{
    (void)promises;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("pledge is not supported on Linux (Landlock only supports filesystem restrictions)");
    }
    return SOLO_SANDBOX_UNSUPPORTED;
}

#endif /* SOLO_IS_LINUX || defined(__linux__) */
