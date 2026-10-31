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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fs_internal.h"

struct solo_fs_dir {
    int dummy;
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, solo_bool_t follow_links, char *err_buf, size_t err_cap)
{
    FILE *fp;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid argument");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));
    fp = fopen(path, "rb");
    if (fp == NULL) {
        solo_set_err(err_buf, err_cap, "file not found");
        return SOLO_ERROR;
    }

    fseek(fp, 0, SEEK_END);
    st->size = (solo_fsize_t)ftell(fp);
    fclose(fp);

    st->type = SOLO_FS_TYPE_FILE;
    st->mode = 0644;
    st->is_readonly = SOLO_FALSE;
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)path; (void)mode;
    solo_set_err(err_buf, err_cap, "mkdir unsupported on generic ISO C platform");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "rmdir unsupported on generic ISO C platform");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    if (remove(path) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "remove failed: %s", strerror(errno));
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
    (void)path; (void)mode;
    solo_set_err(err_buf, err_cap, "chmod unsupported on generic ISO C platform");
    return SOLO_UNSUPPORTED;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "directory listing unsupported on generic ISO C platform");
    return NULL;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    (void)dir; (void)ent;
    return SOLO_UNSUPPORTED;
}

void solo_pal_fs_dir_close(solo_fs_dir_t *dir)
{
    (void)dir;
}

solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    (void)buf; (void)cap;
    solo_set_err(err_buf, err_cap, "getcwd unsupported on generic ISO C platform");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    (void)path;
    solo_set_err(err_buf, err_cap, "setcwd unsupported on generic ISO C platform");
    return SOLO_UNSUPPORTED;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    const char *tmp = getenv("TMPDIR");
    if (tmp == NULL || tmp[0] == '\0') tmp = getenv("TEMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = getenv("TMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = "/tmp";

    if (strlen(tmp) + 1 > cap) return SOLO_ERROR;
    strcpy(buf, tmp);
    return SOLO_OK;
}
