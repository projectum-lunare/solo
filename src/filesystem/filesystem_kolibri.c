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

#if SOLO_IS_KOLIBRI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_internal.h"

struct solo_fs_dir {
    char path[SOLO_PATH_MAX];
    int finished;
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap)
{
    (void)follow_links;
    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid argument");
        return SOLO_ERROR;
    }
    memset(st, 0, sizeof(*st));
    st->type = SOLO_FS_TYPE_FILE;
    st->mode = 0666;
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)path; (void)mode;
    solo_set_err(err_buf, err_cap, "mkdir not implemented on KolibriOS");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "rmdir not implemented on KolibriOS");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "unlink not implemented on KolibriOS");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    (void)oldpath; (void)newpath;
    solo_set_err(err_buf, err_cap, "rename not implemented on KolibriOS");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)path; (void)mode;
    solo_set_err(err_buf, err_cap, "chmod not implemented on KolibriOS");
    return SOLO_UNSUPPORTED;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "dir_open not implemented on KolibriOS");
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
    (void)err_buf; (void)err_cap;
    if (cap < 2) return SOLO_ERROR;
    strcpy(buf, "/");
    return SOLO_OK;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    (void)path; (void)err_buf; (void)err_cap;
    return SOLO_OK;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    (void)err_buf; (void)err_cap;
    if (cap < 6) return SOLO_ERROR;
    strcpy(buf, "/tmp");
    return SOLO_OK;
}

#endif /* SOLO_IS_KOLIBRI */
