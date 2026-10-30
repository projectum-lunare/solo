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

#if SOLO_IS_AMIGA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>

#include "fs_internal.h"

struct solo_fs_dir {
    BPTR lock;
    struct FileInfoBlock fib;
    int first;
    int finished;
    char path[SOLO_PATH_MAX];
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap)
{
    BPTR lock;
    struct FileInfoBlock fib;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));
    lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock == 0) {
        solo_set_err(err_buf, err_cap, "Lock failed (file or directory not found)");
        return SOLO_ERROR;
    }

    if (!Examine(lock, &fib)) {
        UnLock(lock);
        solo_set_err(err_buf, err_cap, "Examine failed on Amiga lock");
        return SOLO_ERROR;
    }

    UnLock(lock);

    st->size = (solo_fsize_t)fib.fib_Size;
    st->mtime = 0;
    st->atime = 0;
    st->ctime = 0;
    st->mode = (int)fib.fib_Protection;
    st->is_readonly = (fib.fib_Protection & FIBF_WRITE) ? 1 : 0;

    if (fib.fib_DirEntryType > 0) {
        st->type = SOLO_FS_TYPE_DIR;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    BPTR lock;
    (void)mode;
    lock = CreateDir((CONST_STRPTR)path);
    if (lock == 0) {
        solo_set_err(err_buf, err_cap, "CreateDir failed on AmigaDOS");
        return SOLO_ERROR;
    }
    UnLock(lock);
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    if (!DeleteFile((CONST_STRPTR)path)) {
        solo_set_err(err_buf, err_cap, "DeleteFile failed on AmigaDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    if (!DeleteFile((CONST_STRPTR)path)) {
        solo_set_err(err_buf, err_cap, "DeleteFile failed on AmigaDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    if (!Rename((CONST_STRPTR)oldpath, (CONST_STRPTR)newpath)) {
        solo_set_err(err_buf, err_cap, "Rename failed on AmigaDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    if (!SetProtection((CONST_STRPTR)path, (LONG)mode)) {
        solo_set_err(err_buf, err_cap, "SetProtection failed on AmigaDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    solo_fs_dir_t *dir;
    BPTR lock;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock == 0) {
        solo_set_err(err_buf, err_cap, "Lock failed on directory");
        return NULL;
    }

    dir = (solo_fs_dir_t *)malloc(sizeof(solo_fs_dir_t));
    if (dir == NULL) {
        UnLock(lock);
        solo_set_err(err_buf, err_cap, "out of memory allocating directory handle");
        return NULL;
    }

    memset(dir, 0, sizeof(*dir));
    dir->lock = lock;
    strncpy(dir->path, path, sizeof(dir->path) - 1);

    if (!Examine(lock, &dir->fib)) {
        UnLock(lock);
        free(dir);
        solo_set_err(err_buf, err_cap, "Examine failed on directory");
        return NULL;
    }

    dir->first = 1;
    dir->finished = 0;
    return dir;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    if (dir == NULL || ent == NULL || dir->finished) {
        return SOLO_EOF;
    }

    if (!ExNext(dir->lock, &dir->fib)) {
        dir->finished = 1;
        return SOLO_EOF;
    }

    strncpy(ent->name, (const char *)dir->fib.fib_FileName, sizeof(ent->name) - 1);
    ent->name[sizeof(ent->name) - 1] = '\0';

    if (dir->fib.fib_DirEntryType > 0) {
        ent->type = SOLO_FS_TYPE_DIR;
    } else {
        ent->type = SOLO_FS_TYPE_FILE;
    }
    return SOLO_OK;
}

void solo_pal_fs_dir_close(solo_fs_dir_t *dir)
{
    if (dir != NULL) {
        if (dir->lock != 0) {
            UnLock(dir->lock);
        }
        free(dir);
    }
}

solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    BPTR lock = Lock((CONST_STRPTR)"", SHARED_LOCK);
    if (lock == 0) {
        solo_set_err(err_buf, err_cap, "Lock on current dir failed");
        return SOLO_ERROR;
    }
    if (!NameFromLock(lock, (STRPTR)buf, (LONG)cap)) {
        UnLock(lock);
        solo_set_err(err_buf, err_cap, "NameFromLock failed");
        return SOLO_ERROR;
    }
    UnLock(lock);
    return SOLO_OK;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    BPTR oldlock;
    if (lock == 0) {
        solo_set_err(err_buf, err_cap, "Lock on path failed");
        return SOLO_ERROR;
    }
    oldlock = CurrentDir(lock);
    UnLock(oldlock);
    return SOLO_OK;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    if (cap < 3) {
        solo_set_err(err_buf, err_cap, "buffer too small");
        return SOLO_ERROR;
    }
    strcpy(buf, "T:");
    return SOLO_OK;
}

#endif /* SOLO_IS_AMIGA */
