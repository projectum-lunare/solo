/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_FS_INTERNAL_H
#define SOLO_FS_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/platform.h>
#include <solo/types.h>
#include <solo/filesystem.h>

#ifndef SOLO_PATH_MAX
#define SOLO_PATH_MAX 1024
#endif

/*
 * File stream handle implementation
 */
struct solo_file {
    FILE *fp;
    char mode[8];
};

/*
 * PAL (Platform Abstraction Layer) SPI Prototypes
 * Implemented across POSIX, Windows, DOS, Amiga, Atari, OS/2, Plan 9, etc.
 */
solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap);
solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent);
void solo_pal_fs_dir_close(solo_fs_dir_t *dir);
solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap);
solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap);

#endif /* SOLO_FS_INTERNAL_H */
