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

#if SOLO_IS_OS2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>

#include "fs_internal.h"

struct solo_fs_dir {
    char path[SOLO_PATH_MAX];
    int finished;
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap)
{
    struct stat s;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid argument");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));
    if (stat(path, &s) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "stat failed: %s", strerror(errno));
        return SOLO_ERROR;
    }

    st->size = (solo_fsize_t)s.st_size;
    st->mtime = (long)s.st_mtime;
    st->atime = (long)s.st_atime;
    st->ctime = (long)s.st_ctime;
    st->mode = (int)(s.st_mode & 07777);
    st->is_readonly = !(s.st_mode & 0222);

    if (s.st_mode & S_IFDIR) {
        st->type = SOLO_FS_TYPE_DIR;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)mode;
    if (mkdir(path) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "mkdir failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    if (rmdir(path) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "rmdir failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    if (unlink(path) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "unlink failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    if (rename(oldpath, newpath) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "rename failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    if (chmod(path, mode) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "chmod failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "dir_open not implemented on OS/2");
    return NULL;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    (void)dir; (void)ent;
    return SOLO_EOF;
}

void solo_pal_fs_dir_close(solo_fs_dir_t *dir)
{
    if (dir) free(dir);
}

solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    if (getcwd(buf, (int)cap) == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "getcwd failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    if (chdir(path) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "chdir failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    const char *tmp = getenv("TEMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = getenv("TMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = "C:\\TEMP";

    if (strlen(tmp) + 1 > cap) return SOLO_ERROR;
    strcpy(buf, tmp);
    return SOLO_OK;
}

#endif /* SOLO_IS_OS2 */
