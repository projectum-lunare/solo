/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stddef.h>

#include <solo/filesystem.h>
#include <solo/platform.h>

/* =============================================================================
 * Platform-specific filesystem implementation
 * ============================================================================= */

#if SOLO_IS_WINDOWS

#include <windows.h>

int solo_fs_exists(const char *path)
{
    if (path == NULL) {
        return 0;
    }
#if SOLO_IS_WINDOWS_CE
    {
        WCHAR wpath[MAX_PATH];
        DWORD attr;
        MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
        attr = GetFileAttributesW(wpath);
        return (attr != 0xFFFFFFFFU /* INVALID_FILE_ATTRIBUTES */);
    }
#else
    {
        DWORD attr = GetFileAttributesA(path);
        return (attr != 0xFFFFFFFFU /* INVALID_FILE_ATTRIBUTES */);
    }
#endif
}

#elif SOLO_IS_AMIGA

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>

int solo_fs_exists(const char *path)
{
    BPTR lock;
    if (path == NULL) {
        return 0;
    }
    lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock != 0) {
        UnLock(lock);
        return 1;
    }
    return 0;
}

#elif SOLO_IS_ATARIST

#include <osbind.h>

int solo_fs_exists(const char *path)
{
    if (path == NULL) {
        return 0;
    }
    return (Fsfirst(path, 0) == 0);
}

#elif SOLO_IS_DOS || SOLO_IS_OS2 || SOLO_CC_WATCOM

#include <io.h>

int solo_fs_exists(const char *path)
{
    if (path == NULL) {
        return 0;
    }
    return (access(path, 0) == 0);
}

#elif SOLO_IS_POSIX || SOLO_IS_PLAN9

#include <unistd.h>

int solo_fs_exists(const char *path)
{
    if (path == NULL) {
        return 0;
    }
    return (access(path, 0) == 0);
}

#else

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

/*
 * Generic ISO C fallback:
 * Uses stat() to check for node existence without opening device nodes or FIFOs.
 */
int solo_fs_exists(const char *path)
{
    struct stat st;
    if (path == NULL) {
        return 0;
    }
    if (stat(path, &st) == 0) {
        return 1;
    }
    return (errno != ENOENT && errno != ENOTDIR);
}

#endif
