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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <solo/filesystem.h>
#include <solo/platform.h>
#include "fs_internal.h"

/* =============================================================================
 * 1. Path Algebra & Pure Normalization
 * ============================================================================= */

char solo_fs_path_separator(void)
{
#if SOLO_IS_WINDOWS || SOLO_IS_DOS || SOLO_IS_OS2
    return '\\';
#else
    return '/';
#endif
}

static int is_sep(char c)
{
    return (c == '/' || c == '\\');
}

solo_bool_t solo_fs_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return SOLO_FALSE;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return SOLO_TRUE;
    }
#if SOLO_IS_WINDOWS || SOLO_IS_DOS || SOLO_IS_OS2
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        if (path[2] == '/' || path[2] == '\\' || path[2] == '\0') {
            return SOLO_TRUE;
        }
    }
#elif SOLO_IS_AMIGA
    {
        const char *p = path;
        while (*p) {
            if (*p == ':') return SOLO_TRUE;
            if (*p == '/') break;
            p++;
        }
    }
#endif
    return SOLO_FALSE;
}

solo_status_t solo_fs_path_join(const char *p1, const char *p2, char *out, size_t out_cap)
{
    size_t l1, l2;
    char sep;

    if (out == NULL || out_cap == 0) {
        return SOLO_ERROR;
    }

    if (p1 == NULL || p1[0] == '\0') {
        if (p2 == NULL || p2[0] == '\0') {
            out[0] = '\0';
            return SOLO_OK;
        }
        if (strlen(p2) >= out_cap) return SOLO_ERROR;
        strcpy(out, p2);
        return SOLO_OK;
    }

    if (p2 == NULL || p2[0] == '\0') {
        if (strlen(p1) >= out_cap) return SOLO_ERROR;
        strcpy(out, p1);
        return SOLO_OK;
    }

    if (solo_fs_path_is_absolute(p2)) {
        if (strlen(p2) >= out_cap) return SOLO_ERROR;
        strcpy(out, p2);
        return SOLO_OK;
    }

    l1 = strlen(p1);
    l2 = strlen(p2);
    sep = solo_fs_path_separator();

    if (is_sep(p1[l1 - 1])) {
        if (l1 + l2 >= out_cap) return SOLO_ERROR;
        memcpy(out, p1, l1);
        memcpy(out + l1, p2, l2 + 1);
    } else {
        if (l1 + 1 + l2 >= out_cap) return SOLO_ERROR;
        memcpy(out, p1, l1);
        out[l1] = sep;
        memcpy(out + l1 + 1, p2, l2 + 1);
    }

    return SOLO_OK;
}

solo_status_t solo_fs_path_basename(const char *path, char *out, size_t out_cap)
{
    size_t len;
    long i, end;

    if (path == NULL || out == NULL || out_cap == 0) {
        return SOLO_ERROR;
    }

    len = strlen(path);
    if (len == 0) {
        if (out_cap < 2) return SOLO_ERROR;
        strcpy(out, ".");
        return SOLO_OK;
    }

    /* Strip trailing slashes */
    end = (long)len - 1;
    while (end > 0 && is_sep(path[end])) {
        end--;
    }

    if (end == 0 && is_sep(path[0])) {
        if (out_cap < 2) return SOLO_ERROR;
        out[0] = path[0];
        out[1] = '\0';
        return SOLO_OK;
    }

    i = end;
    while (i >= 0 && !is_sep(path[i])) {
        i--;
    }

    {
        size_t blen = (size_t)(end - i);
        if (blen >= out_cap) return SOLO_ERROR;
        memcpy(out, path + i + 1, blen);
        out[blen] = '\0';
    }

    return SOLO_OK;
}

solo_status_t solo_fs_path_dirname(const char *path, char *out, size_t out_cap)
{
    size_t len;
    long end, i;

    if (path == NULL || out == NULL || out_cap == 0) {
        return SOLO_ERROR;
    }

    len = strlen(path);
    if (len == 0) {
        if (out_cap < 2) return SOLO_ERROR;
        strcpy(out, ".");
        return SOLO_OK;
    }

    end = (long)len - 1;
    while (end > 0 && is_sep(path[end])) {
        end--;
    }

    i = end;
    while (i >= 0 && !is_sep(path[i])) {
        i--;
    }

    if (i < 0) {
        if (out_cap < 2) return SOLO_ERROR;
        strcpy(out, ".");
        return SOLO_OK;
    }

    /* Collapse multiple separators */
    while (i > 0 && is_sep(path[i - 1])) {
        i--;
    }

    if (i == 0 && is_sep(path[0])) {
        if (out_cap < 2) return SOLO_ERROR;
        out[0] = path[0];
        out[1] = '\0';
        return SOLO_OK;
    }

    if ((size_t)i >= out_cap) return SOLO_ERROR;
    memcpy(out, path, (size_t)i);
    out[i] = '\0';
    return SOLO_OK;
}

solo_status_t solo_fs_path_ext(const char *path, char *out, size_t out_cap)
{
    char base[SOLO_PATH_MAX];
    long i, len;

    if (path == NULL || out == NULL || out_cap == 0) {
        return SOLO_ERROR;
    }

    out[0] = '\0';
    if (solo_fs_path_basename(path, base, sizeof(base)) != SOLO_OK) {
        return SOLO_ERROR;
    }

    len = (long)strlen(base);
    if (len <= 1) {
        return SOLO_OK;
    }

    for (i = len - 1; i > 0; i--) {
        if (base[i] == '.') {
            size_t ext_len = (size_t)(len - i);
            if (ext_len >= out_cap) return SOLO_ERROR;
            memcpy(out, base + i, ext_len + 1);
            return SOLO_OK;
        }
    }

    return SOLO_OK;
}

solo_status_t solo_fs_path_normalize(const char *path, char *out, size_t out_cap)
{
    char sep = solo_fs_path_separator();
    char buf[SOLO_PATH_MAX];
    char *tokens[128];
    int token_count = 0;
    int is_abs;
    size_t i, len;
    char *p, *start;
    char prefix[8];
    size_t prefix_len = 0;

    if (path == NULL || out == NULL || out_cap == 0) {
        return SOLO_ERROR;
    }

    len = strlen(path);
    if (len >= sizeof(buf)) {
        return SOLO_ERROR;
    }

    strcpy(buf, path);
    prefix[0] = '\0';

    /* Detect drive letter e.g. "C:\" */
    if (isalpha((unsigned char)buf[0]) && buf[1] == ':') {
        prefix[0] = buf[0];
        prefix[1] = ':';
        prefix[2] = '\0';
        prefix_len = 2;
        p = buf + 2;
    } else {
        p = buf;
    }

    is_abs = (*p == '/' || *p == '\\');

    /* Tokenize by / and \ */
    start = p;
    while (*start != '\0') {
        while (*start != '\0' && is_sep(*start)) {
            start++;
        }
        if (*start == '\0') break;

        p = start;
        while (*p != '\0' && !is_sep(*p)) {
            p++;
        }

        if (*p != '\0') {
            *p = '\0';
            p++;
        }

        if (strcmp(start, ".") == 0) {
            /* Skip current dir */
        } else if (strcmp(start, "..") == 0) {
            if (token_count > 0 && strcmp(tokens[token_count - 1], "..") != 0) {
                token_count--;
            } else if (!is_abs) {
                if (token_count < 128) {
                    tokens[token_count++] = start;
                }
            }
        } else {
            if (token_count < 128) {
                tokens[token_count++] = start;
            }
        }
        start = p;
    }

    /* Reconstruct path */
    out[0] = '\0';
    if (prefix_len > 0) {
        if (prefix_len >= out_cap) return SOLO_ERROR;
        strcpy(out, prefix);
    }

    if (is_abs) {
        size_t clen = strlen(out);
        if (clen + 1 >= out_cap) return SOLO_ERROR;
        out[clen] = sep;
        out[clen + 1] = '\0';
    }

    if (token_count == 0 && !is_abs) {
        if (strlen(out) + 2 >= out_cap) return SOLO_ERROR;
        strcat(out, ".");
        return SOLO_OK;
    }

    for (i = 0; i < (size_t)token_count; i++) {
        size_t cur_len = strlen(out);
        size_t t_len = strlen(tokens[i]);

        if (cur_len > 0 && out[cur_len - 1] != sep && out[cur_len - 1] != ':') {
            if (cur_len + 1 + t_len >= out_cap) return SOLO_ERROR;
            out[cur_len] = sep;
            out[cur_len + 1] = '\0';
        } else {
            if (cur_len + t_len >= out_cap) return SOLO_ERROR;
        }
        strcat(out, tokens[i]);
    }

    return SOLO_OK;
}

/* =============================================================================
 * 2. File & Directory Inspection (Metadata & Stat)
 * ============================================================================= */

solo_bool_t solo_fs_exists(const char *path)
{
    solo_fs_stat_t st;
    if (path == NULL || path[0] == '\0') {
        return SOLO_FALSE;
    }
    return (solo_pal_fs_stat(path, &st, SOLO_TRUE, NULL, 0) == SOLO_OK) ? SOLO_TRUE : SOLO_FALSE;
}

solo_status_t solo_fs_stat(const char *path, solo_fs_stat_t *st, char *err_buf, size_t err_cap)
{
    return solo_pal_fs_stat(path, st, SOLO_TRUE, err_buf, err_cap);
}

solo_status_t solo_fs_lstat(const char *path, solo_fs_stat_t *st, char *err_buf, size_t err_cap)
{
    return solo_pal_fs_stat(path, st, SOLO_FALSE, err_buf, err_cap);
}

solo_bool_t solo_fs_is_file(const char *path)
{
    solo_fs_stat_t st;
    if (solo_pal_fs_stat(path, &st, SOLO_TRUE, NULL, 0) == SOLO_OK) {
        return (st.type == SOLO_FS_TYPE_FILE) ? SOLO_TRUE : SOLO_FALSE;
    }
    return SOLO_FALSE;
}

solo_bool_t solo_fs_is_dir(const char *path)
{
    solo_fs_stat_t st;
    if (solo_pal_fs_stat(path, &st, SOLO_TRUE, NULL, 0) == SOLO_OK) {
        return (st.type == SOLO_FS_TYPE_DIR) ? SOLO_TRUE : SOLO_FALSE;
    }
    return SOLO_FALSE;
}

solo_bool_t solo_fs_is_symlink(const char *path)
{
    solo_fs_stat_t st;
    if (solo_pal_fs_stat(path, &st, SOLO_FALSE, NULL, 0) == SOLO_OK) {
        return (st.type == SOLO_FS_TYPE_SYMLINK) ? SOLO_TRUE : SOLO_FALSE;
    }
    return SOLO_FALSE;
}

solo_status_t solo_fs_file_size(const char *path, solo_fsize_t *out_size)
{
    solo_fs_stat_t st;
    if (out_size == NULL) return SOLO_ERROR;
    if (solo_pal_fs_stat(path, &st, 1, NULL, 0) == SOLO_OK) {
        *out_size = st.size;
        return SOLO_OK;
    }
    return SOLO_ERROR;
}

/* =============================================================================
 * 3. File System Operations & Mutation
 * ============================================================================= */

solo_status_t solo_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap)
{
    if (path == NULL || path[0] == '\0') {
        solo_set_err(err_buf, err_cap, "path is empty");
        return SOLO_ERROR;
    }
    return solo_pal_fs_mkdir(path, mode, err_buf, err_cap);
}

solo_status_t solo_fs_mkdir_p(const char *path, int mode, char *err_buf, size_t err_cap)
{
    char tmp[SOLO_PATH_MAX];
    char *p;
    size_t len;

    if (path == NULL || path[0] == '\0') {
        solo_set_err(err_buf, err_cap, "path is empty");
        return SOLO_ERROR;
    }

    len = strlen(path);
    if (len >= sizeof(tmp)) {
        solo_set_err(err_buf, err_cap, "path too long");
        return SOLO_ERROR;
    }

    strcpy(tmp, path);

    /* Strip trailing slashes */
    while (len > 1 && is_sep(tmp[len - 1])) {
        tmp[--len] = '\0';
    }

    p = tmp;
    /* Skip root slash or drive prefix */
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        p += 2;
    }
    while (*p && is_sep(*p)) {
        p++;
    }

    while (*p) {
        while (*p && !is_sep(*p)) {
            p++;
        }
        if (*p) {
            *p = '\0';
            if (!solo_fs_exists(tmp)) {
                if (solo_pal_fs_mkdir(tmp, mode, err_buf, err_cap) != SOLO_OK) {
                    return SOLO_ERROR;
                }
            }
            *p = solo_fs_path_separator();
            p++;
            while (*p && is_sep(*p)) p++;
        }
    }

    if (!solo_fs_exists(tmp)) {
        return solo_pal_fs_mkdir(tmp, mode, err_buf, err_cap);
    }
    return SOLO_OK;
}

solo_status_t solo_fs_rmdir(const char *path, char *err_buf, size_t err_cap)
{
    if (path == NULL) return SOLO_ERROR;
    return solo_pal_fs_rmdir(path, err_buf, err_cap);
}

solo_status_t solo_fs_remove(const char *path, char *err_buf, size_t err_cap)
{
    if (path == NULL) return SOLO_ERROR;
    return solo_pal_fs_unlink(path, err_buf, err_cap);
}

solo_status_t solo_fs_remove_all(const char *path, char *err_buf, size_t err_cap)
{
    solo_fs_stat_t st;

    if (path == NULL || path[0] == '\0') {
        solo_set_err(err_buf, err_cap, "path is null or empty");
        return SOLO_ERROR;
    }

    if (solo_pal_fs_stat(path, &st, 0, err_buf, err_cap) != SOLO_OK) {
        return SOLO_ERROR;
    }

    if (st.type == SOLO_FS_TYPE_DIR) {
        solo_fs_dir_t *dir = solo_pal_fs_dir_open(path, err_buf, err_cap);
        solo_fs_dirent_t ent;

        if (dir != NULL) {
            while (solo_pal_fs_dir_next(dir, &ent) == SOLO_OK) {
                char child[SOLO_PATH_MAX];
                if (solo_fs_path_join(path, ent.name, child, sizeof(child)) == SOLO_OK) {
                    if (solo_fs_remove_all(child, err_buf, err_cap) != SOLO_OK) {
                        solo_pal_fs_dir_close(dir);
                        return SOLO_ERROR;
                    }
                }
            }
            solo_pal_fs_dir_close(dir);
        }
        return solo_pal_fs_rmdir(path, err_buf, err_cap);
    } else {
        return solo_pal_fs_unlink(path, err_buf, err_cap);
    }
}

solo_status_t solo_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap)
{
    if (oldpath == NULL || newpath == NULL) {
        solo_set_err(err_buf, err_cap, "path argument is null");
        return SOLO_ERROR;
    }
    return solo_pal_fs_rename(oldpath, newpath, err_buf, err_cap);
}

solo_status_t solo_fs_copy_file(const char *src, const char *dst, solo_bool_t overwrite, char *err_buf, size_t err_cap)
{
    FILE *in, *out;
    char chunk[8192];
    size_t n;

    if (src == NULL || dst == NULL) {
        solo_set_err(err_buf, err_cap, "source or destination path is null");
        return SOLO_ERROR;
    }

    if (!overwrite && solo_fs_exists(dst)) {
        solo_set_err(err_buf, err_cap, "destination file already exists");
        return SOLO_ERROR;
    }

    in = fopen(src, "rb");
    if (in == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "failed to open source '%s'", src);
        return SOLO_ERROR;
    }

    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        solo_set_err_fmt(err_buf, err_cap, "failed to open destination '%s'", dst);
        return SOLO_ERROR;
    }

    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            solo_set_err(err_buf, err_cap, "write error during file copy");
            return SOLO_ERROR;
        }
    }

    fclose(in);
    fclose(out);
    return SOLO_OK;
}

solo_status_t solo_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap)
{
    if (path == NULL) return SOLO_ERROR;
    return solo_pal_fs_chmod(path, mode, err_buf, err_cap);
}

/* =============================================================================
 * 4. Directory Iteration
 * ============================================================================= */

solo_fs_dir_t *solo_fs_dir_open(const char *path, char *err_buf, size_t err_cap)
{
    return solo_pal_fs_dir_open(path, err_buf, err_cap);
}

solo_status_t solo_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent)
{
    return solo_pal_fs_dir_next(dir, ent);
}

void solo_fs_dir_close(solo_fs_dir_t *dir)
{
    solo_pal_fs_dir_close(dir);
}

/* =============================================================================
 * 5. File Content Operations (Slurp / Spit & Streaming)
 * ============================================================================= */

solo_status_t solo_fs_read_file(const char *path, void **out_buf, size_t *out_len, char *err_buf, size_t err_cap)
{
    FILE *fp;
    long fsize;
    char *buffer;
    size_t read_bytes;

    if (path == NULL || out_buf == NULL || out_len == NULL) {
        solo_set_err(err_buf, err_cap, "invalid null argument to read_file");
        return SOLO_ERROR;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "failed to open file '%s'", path);
        return SOLO_ERROR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        solo_set_err(err_buf, err_cap, "failed to seek to file end");
        return SOLO_ERROR;
    }

    fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        solo_set_err(err_buf, err_cap, "failed to determine file size");
        return SOLO_ERROR;
    }
    fseek(fp, 0, SEEK_SET);

    buffer = (char *)malloc((size_t)fsize + 1);
    if (buffer == NULL) {
        fclose(fp);
        solo_set_err(err_buf, err_cap, "out of memory allocating file read buffer");
        return SOLO_ERROR;
    }

    read_bytes = (fsize > 0) ? fread(buffer, 1, (size_t)fsize, fp) : 0;
    buffer[read_bytes] = '\0';
    fclose(fp);

    *out_buf = buffer;
    *out_len = read_bytes;
    return SOLO_OK;
}

solo_status_t solo_fs_write_file(const char *path, const void *buf, size_t len, char *err_buf, size_t err_cap)
{
    FILE *fp;
    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return SOLO_ERROR;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "failed to open file '%s' for writing", path);
        return SOLO_ERROR;
    }

    if (len > 0 && buf != NULL) {
        if (fwrite(buf, 1, len, fp) != len) {
            fclose(fp);
            solo_set_err(err_buf, err_cap, "failed to write all bytes to file");
            return SOLO_ERROR;
        }
    }

    fclose(fp);
    return SOLO_OK;
}

solo_status_t solo_fs_append_file(const char *path, const void *buf, size_t len, char *err_buf, size_t err_cap)
{
    FILE *fp;
    if (path == NULL) {
        solo_set_err(err_buf, err_cap, "path is null");
        return SOLO_ERROR;
    }

    fp = fopen(path, "ab");
    if (fp == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "failed to open file '%s' for appending", path);
        return SOLO_ERROR;
    }

    if (len > 0 && buf != NULL) {
        if (fwrite(buf, 1, len, fp) != len) {
            fclose(fp);
            solo_set_err(err_buf, err_cap, "failed to append all bytes to file");
            return SOLO_ERROR;
        }
    }

    fclose(fp);
    return SOLO_OK;
}

solo_file_t *solo_file_open(const char *path, const char *mode, char *err_buf, size_t err_cap)
{
    solo_file_t *f;
    char bmode[8];
    FILE *fp;

    if (path == NULL || mode == NULL) {
        solo_set_err(err_buf, err_cap, "path or mode is null");
        return NULL;
    }

    /* Convert mode to binary equivalent for consistent cross-platform behavior */
    if (strcmp(mode, "r") == 0) strcpy(bmode, "rb");
    else if (strcmp(mode, "w") == 0) strcpy(bmode, "wb");
    else if (strcmp(mode, "a") == 0) strcpy(bmode, "ab");
    else if (strcmp(mode, "r+") == 0) strcpy(bmode, "rb+");
    else if (strcmp(mode, "w+") == 0) strcpy(bmode, "wb+");
    else if (strcmp(mode, "a+") == 0) strcpy(bmode, "ab+");
    else strncpy(bmode, mode, sizeof(bmode) - 1);
    bmode[sizeof(bmode) - 1] = '\0';

    fp = fopen(path, bmode);
    if (fp == NULL) {
        solo_set_err_fmt(err_buf, err_cap, "failed to open file '%s'", path);
        return NULL;
    }

    f = (solo_file_t *)malloc(sizeof(solo_file_t));
    if (f == NULL) {
        fclose(fp);
        solo_set_err(err_buf, err_cap, "out of memory allocating solo_file_t");
        return NULL;
    }

    f->fp = fp;
    strncpy(f->mode, bmode, sizeof(f->mode) - 1);
    f->mode[sizeof(f->mode) - 1] = '\0';
    return f;
}

solo_status_t solo_file_read(solo_file_t *f, void *buf, size_t count, size_t *bytes_read)
{
    size_t n;
    if (f == NULL || f->fp == NULL || buf == NULL) return SOLO_ERROR;
    if (count == 0) {
        if (bytes_read) *bytes_read = 0;
        return SOLO_OK;
    }

    n = fread(buf, 1, count, f->fp);
    if (bytes_read) *bytes_read = n;

    if (n == 0) {
        if (feof(f->fp)) return SOLO_EOF;
        if (ferror(f->fp)) return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_file_write(solo_file_t *f, const void *buf, size_t count, size_t *bytes_written)
{
    size_t n;
    if (f == NULL || f->fp == NULL) return SOLO_ERROR;
    if (count == 0) {
        if (bytes_written) *bytes_written = 0;
        return SOLO_OK;
    }

    n = fwrite(buf, 1, count, f->fp);
    if (bytes_written) *bytes_written = n;

    if (n < count && ferror(f->fp)) {
        return SOLO_ERROR;
    }
    return SOLO_OK;
}

solo_status_t solo_file_seek(solo_file_t *f, long offset, int whence, long *new_pos)
{
    int w = SEEK_SET;
    if (f == NULL || f->fp == NULL) return SOLO_ERROR;

    if (whence == SOLO_FS_SEEK_SET) w = SEEK_SET;
    else if (whence == SOLO_FS_SEEK_CUR) w = SEEK_CUR;
    else if (whence == SOLO_FS_SEEK_END) w = SEEK_END;
    else return SOLO_ERROR;

    if (fseek(f->fp, offset, w) != 0) {
        return SOLO_ERROR;
    }

    if (new_pos) {
        *new_pos = ftell(f->fp);
    }
    return SOLO_OK;
}

solo_status_t solo_file_tell(solo_file_t *f, long *pos)
{
    long p;
    if (f == NULL || f->fp == NULL || pos == NULL) return SOLO_ERROR;
    p = ftell(f->fp);
    if (p < 0) return SOLO_ERROR;
    *pos = p;
    return SOLO_OK;
}

solo_status_t solo_file_flush(solo_file_t *f)
{
    if (f == NULL || f->fp == NULL) return SOLO_ERROR;
    return (fflush(f->fp) == 0) ? SOLO_OK : SOLO_ERROR;
}

void solo_file_close(solo_file_t *f)
{
    if (f != NULL) {
        if (f->fp != NULL) {
            fclose(f->fp);
            f->fp = NULL;
        }
        free(f);
    }
}

/* =============================================================================
 * 6. Environment & Special Directories
 * ============================================================================= */

solo_status_t solo_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    if (buf == NULL || cap == 0) {
        solo_set_err(err_buf, err_cap, "invalid buffer");
        return SOLO_ERROR;
    }
    return solo_pal_fs_getcwd(buf, cap, err_buf, err_cap);
}

solo_status_t solo_fs_setcwd(const char *path, char *err_buf, size_t err_cap)
{
    if (path == NULL || path[0] == '\0') {
        solo_set_err(err_buf, err_cap, "path is null or empty");
        return SOLO_ERROR;
    }
    return solo_pal_fs_setcwd(path, err_buf, err_cap);
}

solo_status_t solo_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap)
{
    if (buf == NULL || cap == 0) {
        solo_set_err(err_buf, err_cap, "invalid buffer");
        return SOLO_ERROR;
    }
    return solo_pal_fs_temp_dir(buf, cap, err_buf, err_cap);
}
