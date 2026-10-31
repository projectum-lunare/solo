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

#if SOLO_IS_PLAN9

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "fs_internal.h"

struct solo_fs_dir {
    DIR *dirp;
    char path[SOLO_PATH_MAX];
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, solo_bool_t follow_links, char *err_buf, size_t err_cap)
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
    st->is_readonly = !(s.st_mode & 0222) ? SOLO_TRUE : SOLO_FALSE;

    if (S_ISDIR(s.st_mode)) {
        st->type = SOLO_FS_TYPE_DIR;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    mode_t m = (mode > 0) ? (mode_t)mode : 0777;
    if (mkdir(path, m) != 0) {
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
    if (chmod(path, (mode_t)mode) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "chmod failed: %s", strerror(errno));
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    DIR *d;
    solo_fs_dir_t *dir;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    d = opendir(path);
    if (d == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "opendir failed: %s", strerror(errno));
        return NULL;
    }

    dir = (solo_fs_dir_t *)malloc(sizeof(solo_fs_dir_t));
    if (dir == NULL) {
        closedir(d);
        solo_set_err(err_buf, err_cap, "out of memory allocating directory handle");
        return NULL;
    }

    dir->dirp = d;
    strncpy(dir->path, path, sizeof(dir->path) - 1);
    dir->path[sizeof(dir->path) - 1] = '\0';
    return dir;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    struct dirent *de;

    if (dir == NULL || ent == NULL) {
        return SOLO_ERROR;
    }

    while (1) {
        de = readdir(dir->dirp);
        if (de == NULL) {
            return SOLO_EOF;
        }

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        strncpy(ent->name, de->d_name, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        ent->type = SOLO_FS_TYPE_FILE;
        return SOLO_OK;
    }
}

void solo_pal_fs_dir_close(solo_fs_dir_t *dir)
{
    if (dir != NULL) {
        if (dir->dirp != NULL) {
            closedir(dir->dirp);
        }
        free(dir);
    }
}

solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    if (getcwd(buf, cap) == NULL) {
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
    (void)err_buf; (void)err_cap;
    if (cap < 6) return SOLO_ERROR;
    strcpy(buf, "/tmp");
    return SOLO_OK;
}

#endif /* SOLO_IS_PLAN9 */
