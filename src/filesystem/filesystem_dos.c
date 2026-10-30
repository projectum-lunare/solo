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

#if SOLO_IS_DOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <dos.h>

#include "fs_internal.h"

struct solo_fs_dir {
    char path[SOLO_PATH_MAX];
    struct find_t c_file;
    int first;
    int finished;
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap)
{
    struct stat s;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
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
    char pattern[SOLO_PATH_MAX];
    solo_fs_dir_t *dir;
    size_t plen;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    plen = strlen(path);
    if (plen + 5 >= sizeof(pattern)) {
        solo_set_err(err_buf, err_cap, "path too long");
        return NULL;
    }

    strcpy(pattern, path);
    if (plen > 0 && pattern[plen - 1] != '/' && pattern[plen - 1] != '\\') {
        strcat(pattern, "\\*.*");
    } else {
        strcat(pattern, "*.*");
    }

    dir = (solo_fs_dir_t *)malloc(sizeof(solo_fs_dir_t));
    if (dir == NULL) {
        solo_set_err(err_buf, err_cap, "out of memory allocating directory handle");
        return NULL;
    }

    memset(dir, 0, sizeof(*dir));
    strncpy(dir->path, path, sizeof(dir->path) - 1);

    if (_dos_findfirst(pattern, _A_NORMAL | _A_SUBDIR | _A_RDONLY, &dir->c_file) != 0) {
        free(dir);
        solo_set_err(err_buf, err_cap, "no files found or invalid directory");
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

    while (1) {
        if (!dir->first) {
            if (_dos_findnext(&dir->c_file) != 0) {
                dir->finished = 1;
                return SOLO_EOF;
            }
        }
        dir->first = 0;

        if (strcmp(dir->c_file.name, ".") == 0 || strcmp(dir->c_file.name, "..") == 0) {
            continue;
        }

        strncpy(ent->name, dir->c_file.name, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';

        if (dir->c_file.attrib & _A_SUBDIR) {
            ent->type = SOLO_FS_TYPE_DIR;
        } else {
            ent->type = SOLO_FS_TYPE_FILE;
        }

        return SOLO_OK;
    }
}

void solo_pal_fs_dir_close(solo_fs_dir_t *dir)
{
    if (dir != NULL) {
        free(dir);
    }
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

    if (strlen(tmp) + 1 > cap) {
        solo_set_err(err_buf, err_cap, "buffer too small for temp dir path");
        return SOLO_ERROR;
    }
    strcpy(buf, tmp);
    return SOLO_OK;
}

#endif /* SOLO_IS_DOS */
