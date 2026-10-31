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

#if SOLO_IS_ATARIST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osbind.h>

#include "fs_internal.h"

struct solo_fs_dir {
    char path[SOLO_PATH_MAX];
    _DTA dta;
    _DTA old_dta;
    int first;
    int finished;
};

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, solo_bool_t follow_links, char *err_buf, size_t err_cap)
{
    _DTA my_dta;
    _DTA *old_dta;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));
    old_dta = (_DTA *)Fgetdta();
    Fsetdta(&my_dta);

    if (Fsfirst(path, 0x10 | 0x01) != 0) { /* FA_SUBDIR | FA_RDONLY */
        Fsetdta(old_dta);
        solo_set_err(err_buf, err_cap, "Fsfirst failed");
        return SOLO_ERROR;
    }

    Fsetdta(old_dta);

    st->size = (solo_fsize_t)my_dta.dta_size;
    st->mtime = 0;
    st->atime = 0;
    st->ctime = 0;
    st->is_readonly = (my_dta.dta_attribute & 0x01) ? SOLO_TRUE : SOLO_FALSE;
    if (my_dta.dta_attribute & 0x10) {
        st->type = SOLO_FS_TYPE_DIR;
        st->mode = st->is_readonly ? 0555 : 0777;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
        st->mode = st->is_readonly ? 0444 : 0666;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)mode;
    if (Dcreate(path) != 0) {
        solo_set_err(err_buf, err_cap, "Dcreate failed on Atari GEMDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    if (Ddelete(path) != 0) {
        solo_set_err(err_buf, err_cap, "Ddelete failed on Atari GEMDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    if (Fdelete(path) != 0) {
        solo_set_err(err_buf, err_cap, "Fdelete failed on Atari GEMDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    if (Frename(0, oldpath, newpath) != 0) {
        solo_set_err(err_buf, err_cap, "Frename failed on Atari GEMDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    int attr = (mode & 0222) ? 0x00 : 0x01;
    if (Fattrib(path, 1, attr) < 0) {
        solo_set_err(err_buf, err_cap, "Fattrib failed on Atari GEMDOS");
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    solo_fs_dir_t *dir;
    char pattern[SOLO_PATH_MAX];
    size_t plen;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    plen = strlen(path);
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
    strncpy(dir->path, pattern, sizeof(dir->path) - 1);
    dir->first = 1;
    dir->finished = 0;
    return dir;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    _DTA *old_dta;
    int res;

    if (dir == NULL || ent == NULL || dir->finished) return SOLO_EOF;

    old_dta = (_DTA *)Fgetdta();
    Fsetdta(&dir->dta);

    while (1) {
        if (dir->first) {
            res = Fsfirst(dir->path, 0x10 | 0x01);
            dir->first = 0;
        } else {
            res = Fsnext();
        }

        if (res != 0) {
            Fsetdta(old_dta);
            dir->finished = 1;
            return SOLO_EOF;
        }

        if (strcmp(dir->dta.dta_name, ".") == 0 || strcmp(dir->dta.dta_name, "..") == 0) {
            continue;
        }

        strncpy(ent->name, dir->dta.dta_name, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        ent->type = (dir->dta.dta_attribute & 0x10) ? SOLO_FS_TYPE_DIR : SOLO_FS_TYPE_FILE;
        Fsetdta(old_dta);
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
    (void)err_buf; (void)err_cap;
    if (Dgetpath(buf, 0) != 0) return SOLO_ERROR;
    return SOLO_OK;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    (void)err_buf; (void)err_cap;
    if (Dsetpath(path) != 0) return SOLO_ERROR;
    return SOLO_OK;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    (void)err_buf; (void)err_cap;
    if (cap < 8) return SOLO_ERROR;
    strcpy(buf, "C:\\TEMP");
    return SOLO_OK;
}

#endif /* SOLO_IS_ATARIST */
