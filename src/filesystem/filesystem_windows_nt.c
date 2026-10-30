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

#if SOLO_IS_WINDOWS && !SOLO_IS_WINDOWS_CE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

#include "fs_internal.h"

struct solo_fs_dir {
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    int first;
    int finished;
    char path[SOLO_PATH_MAX];
};

static void win_filetime_to_epoch(const FILETIME *ft, long *out_epoch)
{
    ULARGE_INTEGER ull;
    ull.LowPart = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    if (ull.QuadPart >= 116444736000000000ULL) {
        *out_epoch = (long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
    } else {
        *out_epoch = 0;
    }
}

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, int follow_links, char *err_buf, size_t err_cap)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        solo_set_err_fmt(err_buf, err_cap, "GetFileAttributesEx failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }

    st->size = (((solo_fsize_t)fad.nFileSizeHigh) << 32) | (solo_fsize_t)fad.nFileSizeLow;
    win_filetime_to_epoch(&fad.ftLastWriteTime, &st->mtime);
    win_filetime_to_epoch(&fad.ftLastAccessTime, &st->atime);
    win_filetime_to_epoch(&fad.ftCreationTime, &st->ctime);

    st->is_readonly = (fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 1 : 0;

    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        st->type = SOLO_FS_TYPE_DIR;
        st->mode = st->is_readonly ? 0555 : 0777;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
        st->mode = st->is_readonly ? 0444 : 0666;
    }

#if defined(FILE_ATTRIBUTE_REPARSE_POINT)
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        st->type = SOLO_FS_TYPE_SYMLINK;
    }
#endif

    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    (void)mode;
    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS) {
            solo_set_err(err_buf, err_cap, "directory already exists");
        } else {
            solo_set_err_fmt(err_buf, err_cap, "CreateDirectory failed (code %lu)", (unsigned long)err);
        }
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    if (!RemoveDirectoryA(path)) {
        solo_set_err_fmt(err_buf, err_cap, "RemoveDirectory failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    if (!DeleteFileA(path)) {
        solo_set_err_fmt(err_buf, err_cap, "DeleteFile failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    if (!MoveFileExA(oldpath, newpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        solo_set_err_fmt(err_buf, err_cap, "MoveFile failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        solo_set_err_fmt(err_buf, err_cap, "GetFileAttributes failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }

    if (mode & 0222) {
        attr &= ~FILE_ATTRIBUTE_READONLY;
    } else {
        attr |= FILE_ATTRIBUTE_READONLY;
    }

    if (!SetFileAttributesA(path, attr)) {
        solo_set_err_fmt(err_buf, err_cap, "SetFileAttributes failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    char search_pattern[SOLO_PATH_MAX];
    solo_fs_dir_t *dir;
    size_t plen;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    plen = strlen(path);
    if (plen + 3 >= sizeof(search_pattern)) {
        solo_set_err(err_buf, err_cap, "path too long for search pattern");
        return NULL;
    }

    strcpy(search_pattern, path);
    if (plen > 0 && search_pattern[plen - 1] != '/' && search_pattern[plen - 1] != '\\') {
        strcat(search_pattern, "\\*");
    } else {
        strcat(search_pattern, "*");
    }

    dir = (solo_fs_dir_t *)malloc(sizeof(solo_fs_dir_t));
    if (dir == NULL) {
        solo_set_err(err_buf, err_cap, "out of memory allocating directory handle");
        return NULL;
    }

    memset(dir, 0, sizeof(*dir));
    strncpy(dir->path, path, sizeof(dir->path) - 1);

    dir->hFind = FindFirstFileA(search_pattern, &dir->findData);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        free(dir);
        solo_set_err_fmt(err_buf, err_cap, "FindFirstFile failed (code %lu)", (unsigned long)err);
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
            if (!FindNextFileA(dir->hFind, &dir->findData)) {
                dir->finished = 1;
                return SOLO_EOF;
            }
        }
        dir->first = 0;

        if (strcmp(dir->findData.cFileName, ".") == 0 || strcmp(dir->findData.cFileName, "..") == 0) {
            continue;
        }

        strncpy(ent->name, dir->findData.cFileName, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';

        if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
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
        if (dir->hFind != INVALID_HANDLE_VALUE && dir->hFind != NULL) {
            FindClose(dir->hFind);
        }
        free(dir);
    }
}

solo_status_t solo_pal_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    DWORD len = GetCurrentDirectoryA((DWORD)cap, buf);
    if (len == 0 || len >= cap) {
        solo_set_err_fmt(err_buf, err_cap, "GetCurrentDirectory failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    if (!SetCurrentDirectoryA(path)) {
        solo_set_err_fmt(err_buf, err_cap, "SetCurrentDirectory failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    DWORD len = GetTempPathA((DWORD)cap, buf);
    if (len == 0 || len >= cap) {
        solo_set_err_fmt(err_buf, err_cap, "GetTempPath failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    if (len > 1 && (buf[len - 1] == '\\' || buf[len - 1] == '/')) {
        buf[len - 1] = '\0';
    }
    return SOLO_OK;
}

#endif /* SOLO_IS_WINDOWS && !SOLO_IS_WINDOWS_CE */
