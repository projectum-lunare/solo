/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <solo/platform.h>

#if SOLO_IS_POSIX || SOLO_IS_PLAN9

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
    int res;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));

#if defined(S_IFLNK)
    if (!follow_links) {
        res = lstat(path, &s);
    } else {
        res = stat(path, &s);
    }
#else
    (void)follow_links;
    res = stat(path, &s);
#endif

    if (res != 0) {
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
    } else if (S_ISREG(s.st_mode)) {
        st->type = SOLO_FS_TYPE_FILE;
#if defined(S_ISLNK)
    } else if (S_ISLNK(s.st_mode)) {
        st->type = SOLO_FS_TYPE_SYMLINK;
#endif
#if defined(S_ISCHR) || defined(S_ISBLK)
    } else if (S_ISCHR(s.st_mode) || S_ISBLK(s.st_mode)) {
        st->type = SOLO_FS_TYPE_DEVICE;
#endif
#if defined(S_ISFIFO)
    } else if (S_ISFIFO(s.st_mode)) {
        st->type = SOLO_FS_TYPE_FIFO;
#endif
#if defined(S_ISSOCK)
    } else if (S_ISSOCK(s.st_mode)) {
        st->type = SOLO_FS_TYPE_SOCKET;
#endif
    } else {
        st->type = SOLO_FS_TYPE_UNKNOWN;
    }

    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    mode_t m = (mode > 0) ? (mode_t)mode : 0777;
    if (mkdir(path, m) != 0) {
        if (errno == EEXIST) {
            solo_set_err(err_buf, err_cap, "directory already exists");
        } else {
            solo_set_err_fmt(err_buf, err_cap, "mkdir failed: %s", strerror(errno));
        }
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
        errno = 0;
        de = readdir(dir->dirp);
        if (de == NULL) {
            if (errno != 0) {
                return SOLO_ERROR;
            }
            return SOLO_EOF;
        }

        /* Skip "." and ".." */
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        strncpy(ent->name, de->d_name, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';

        ent->type = SOLO_FS_TYPE_UNKNOWN;
#if defined(DT_DIR)
        if (de->d_type == DT_DIR) {
            ent->type = SOLO_FS_TYPE_DIR;
        } else if (de->d_type == DT_REG) {
            ent->type = SOLO_FS_TYPE_FILE;
#if defined(DT_LNK)
        } else if (de->d_type == DT_LNK) {
            ent->type = SOLO_FS_TYPE_SYMLINK;
#endif
#if defined(DT_CHR) && defined(DT_BLK)
        } else if (de->d_type == DT_CHR || de->d_type == DT_BLK) {
            ent->type = SOLO_FS_TYPE_DEVICE;
#endif
#if defined(DT_FIFO)
        } else if (de->d_type == DT_FIFO) {
            ent->type = SOLO_FS_TYPE_FIFO;
#endif
#if defined(DT_SOCK)
        } else if (de->d_type == DT_SOCK) {
            ent->type = SOLO_FS_TYPE_SOCKET;
#endif
        }
#endif
        if (ent->type == SOLO_FS_TYPE_UNKNOWN) {
            char full_path[SOLO_PATH_MAX];
            solo_fs_stat_t st;
            if (solo_fs_path_join(dir->path, ent->name, full_path, sizeof(full_path)) == SOLO_OK) {
                if (solo_pal_fs_stat(full_path, &st, SOLO_FALSE, NULL, 0) == SOLO_OK) {
                    ent->type = st.type;
                }
            }
        }

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
    const char *tmp = getenv("TMPDIR");
    if (tmp == NULL || tmp[0] == '\0') tmp = getenv("TEMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = getenv("TMP");
    if (tmp == NULL || tmp[0] == '\0') tmp = "/tmp";

    if (strlen(tmp) + 1 > cap) {
        solo_set_err(err_buf, err_cap, "buffer too small for temp dir path");
        return SOLO_ERROR;
    }

    strcpy(buf, tmp);
    return SOLO_OK;
}

#endif /* SOLO_IS_POSIX || SOLO_IS_PLAN9 */
