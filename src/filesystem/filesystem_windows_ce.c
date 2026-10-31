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

#if SOLO_IS_WINDOWS_CE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "fs_internal.h"

struct solo_fs_dir {
    HANDLE hFind;
    WIN32_FIND_DATAW findData;
    int first;
    int finished;
    char path[SOLO_PATH_MAX];
};

static void str_to_wstr(const char *s, WCHAR *ws, int max_len)
{
    MultiByteToWideChar(CP_ACP, 0, s, -1, ws, max_len);
}

static void wstr_to_str(const WCHAR *ws, char *s, int max_len)
{
    WideCharToMultiByte(CP_ACP, 0, ws, -1, s, max_len, NULL, NULL);
}

solo_status_t solo_pal_fs_stat(const char *path, solo_fs_stat_t *st, solo_bool_t follow_links, char *err_buf, size_t err_cap)
{
    WCHAR wpath[MAX_PATH];
    DWORD attr;
    HANDLE hFile;
    BY_HANDLE_FILE_INFORMATION bhfi;
    (void)follow_links;

    if (path == NULL || st == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to stat");
        return SOLO_ERROR;
    }

    memset(st, 0, sizeof(*st));
    str_to_wstr(path, wpath, MAX_PATH);

    attr = GetFileAttributesW(wpath);
    if (attr == 0xFFFFFFFFU) {
        solo_set_err_fmt(err_buf, err_cap, "GetFileAttributesW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }

    st->is_readonly = (attr & FILE_ATTRIBUTE_READONLY) ? SOLO_TRUE : SOLO_FALSE;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        st->type = SOLO_FS_TYPE_DIR;
        st->mode = st->is_readonly ? 0555 : 0777;
    } else {
        st->type = SOLO_FS_TYPE_FILE;
        st->mode = st->is_readonly ? 0444 : 0666;
    }

    hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        if (GetFileInformationByHandle(hFile, &bhfi)) {
            st->size = (((solo_fsize_t)bhfi.nFileSizeHigh) << 32) | (solo_fsize_t)bhfi.nFileSizeLow;
        }
        CloseHandle(hFile);
    }

    return SOLO_OK;
}

solo_status_t solo_pal_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    WCHAR wpath[MAX_PATH];
    (void)mode;
    str_to_wstr(path, wpath, MAX_PATH);
    if (!CreateDirectoryW(wpath, NULL)) {
        solo_set_err_fmt(err_buf, err_cap, "CreateDirectoryW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    WCHAR wpath[MAX_PATH];
    str_to_wstr(path, wpath, MAX_PATH);
    if (!RemoveDirectoryW(wpath)) {
        solo_set_err_fmt(err_buf, err_cap, "RemoveDirectoryW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_unlink(const char *path, char *err_buf, size_t err_cap)
{
    WCHAR wpath[MAX_PATH];
    str_to_wstr(path, wpath, MAX_PATH);
    if (!DeleteFileW(wpath)) {
        solo_set_err_fmt(err_buf, err_cap, "DeleteFileW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    WCHAR wold[MAX_PATH], wnew[MAX_PATH];
    str_to_wstr(oldpath, wold, MAX_PATH);
    str_to_wstr(newpath, wnew, MAX_PATH);
    if (!MoveFileW(wold, wnew)) {
        solo_set_err_fmt(err_buf, err_cap, "MoveFileW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_pal_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    WCHAR wpath[MAX_PATH];
    DWORD attr;
    str_to_wstr(path, wpath, MAX_PATH);
    attr = GetFileAttributesW(wpath);
    if (attr == 0xFFFFFFFFU) {
        solo_set_err_fmt(err_buf, err_cap, "GetFileAttributesW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }

    if (mode & 0222) attr &= ~FILE_ATTRIBUTE_READONLY;
    else attr |= FILE_ATTRIBUTE_READONLY;

    if (!SetFileAttributesW(wpath, attr)) {
        solo_set_err_fmt(err_buf, err_cap, "SetFileAttributesW failed (code %lu)", (unsigned long)GetLastError());
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_fs_dir_t *solo_pal_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    char search_pattern[SOLO_PATH_MAX];
    WCHAR wpattern[MAX_PATH];
    solo_fs_dir_t *dir;
    size_t plen;

    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return NULL;
    }

    plen = strlen(path);
    strcpy(search_pattern, path);
    if (plen > 0 && search_pattern[plen - 1] != '/' && search_pattern[plen - 1] != '\\') {
        strcat(search_pattern, "\\*");
    } else {
        strcat(search_pattern, "*");
    }

    str_to_wstr(search_pattern, wpattern, MAX_PATH);

    dir = (solo_fs_dir_t *)malloc(sizeof(solo_fs_dir_t));
    if (dir == NULL) {
        solo_set_err(err_buf, err_cap, "out of memory");
        return NULL;
    }

    memset(dir, 0, sizeof(*dir));
    strncpy(dir->path, path, sizeof(dir->path) - 1);

    dir->hFind = FindFirstFileW(wpattern, &dir->findData);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        solo_set_err_fmt(err_buf, err_cap, "FindFirstFileW failed (code %lu)", (unsigned long)GetLastError());
        return NULL;
    }

    dir->first = 1;
    dir->finished = 0;
    return dir;
}

solo_status_t solo_pal_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    char fname[256];
    if (dir == NULL || ent == NULL || dir->finished) return SOLO_EOF;

    while (1) {
        if (!dir->first) {
            if (!FindNextFileW(dir->hFind, &dir->findData)) {
                dir->finished = 1;
                return SOLO_EOF;
            }
        }
        dir->first = 0;

        wstr_to_str(dir->findData.cFileName, fname, sizeof(fname));
        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0) continue;

        strncpy(ent->name, fname, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        ent->type = (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SOLO_FS_TYPE_DIR : SOLO_FS_TYPE_FILE;
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
    (void)err_buf; (void)err_cap;
    if (cap < 2) return SOLO_ERROR;
    strcpy(buf, "\\");
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
    strcpy(buf, "\\Temp");
    return SOLO_OK;
}

#endif /* SOLO_IS_WINDOWS_CE */
