/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_FILESYSTEM_H
#define SOLO_FILESYSTEM_H

#include <stddef.h>
#include <solo/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * Types, Constants & Enumerations
 * =============================================================================
 */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
typedef unsigned long long solo_fsize_t;
#elif defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
__extension__ typedef unsigned long long solo_fsize_t;
#else
typedef unsigned long solo_fsize_t;
#endif

#ifndef SOLO_PATH_MAX
#define SOLO_PATH_MAX 1024
#endif

typedef enum {
    SOLO_FS_TYPE_UNKNOWN   = 0,
    SOLO_FS_TYPE_FILE      = 1,
    SOLO_FS_TYPE_DIR       = 2,
    SOLO_FS_TYPE_SYMLINK   = 3,
    SOLO_FS_TYPE_DEVICE    = 4,
    SOLO_FS_TYPE_FIFO      = 5,
    SOLO_FS_TYPE_SOCKET    = 6
} solo_fs_type_t;

typedef struct solo_fs_stat {
    solo_fs_type_t type;
    solo_fsize_t   size;         /* File size in bytes */
    long           mtime;        /* Modification timestamp (epoch seconds) */
    long           atime;        /* Last access timestamp (epoch seconds) */
    long           ctime;        /* Status change / creation timestamp */
    int            mode;         /* Permission bits (POSIX octal mode flags) */
    int            is_readonly;  /* 1 if read-only attribute is set, 0 otherwise */
} solo_fs_stat_t;

typedef struct solo_fs_dirent {
    char           name[256];
    solo_fs_type_t type;
} solo_fs_dirent_t;

typedef struct solo_fs_dir solo_fs_dir_t;
typedef struct solo_file solo_file_t;

#define SOLO_FS_SEEK_SET 0
#define SOLO_FS_SEEK_CUR 1
#define SOLO_FS_SEEK_END 2

/*
 * =============================================================================
 * 1. Path Algebra & Pure Normalization (No Syscalls / Zero Disk I/O)
 * =============================================================================
 */

/*
 * Joins two path components using the native separator.
 * Returns SOLO_OK on success, or SOLO_ERROR if out_cap is insufficient.
 */
solo_status_t solo_fs_path_join(const char *p1, const char *p2, char *out, size_t out_cap);

/*
 * Normalizes a path in-place or into out:
 * - Resolves '.' and '..' components
 * - Collapses repeated separators
 * Returns SOLO_OK on success, or SOLO_ERROR if out_cap is insufficient.
 */
solo_status_t solo_fs_path_normalize(const char *path, char *out, size_t out_cap);

/*
 * Extracts the base name (final component) of path.
 * Returns SOLO_OK on success, or SOLO_ERROR if out_cap is insufficient.
 */
solo_status_t solo_fs_path_basename(const char *path, char *out, size_t out_cap);

/*
 * Extracts the directory name (parent component) of path.
 * Returns SOLO_OK on success, or SOLO_ERROR if out_cap is insufficient.
 */
solo_status_t solo_fs_path_dirname(const char *path, char *out, size_t out_cap);

/*
 * Extracts the file extension (including the leading dot, e.g. ".txt") of path.
 * If no extension exists, writes an empty string.
 * Returns SOLO_OK on success, or SOLO_ERROR if out_cap is insufficient.
 */
solo_status_t solo_fs_path_ext(const char *path, char *out, size_t out_cap);

/*
 * Checks whether the given path is absolute.
 * Returns 1 if absolute, 0 if relative or invalid.
 */
int solo_fs_path_is_absolute(const char *path);

/*
 * Returns the primary path separator character for the current platform.
 */
char solo_fs_path_separator(void);

/*
 * =============================================================================
 * 2. File & Directory Inspection (Metadata & Stat)
 * =============================================================================
 */

/*
 * Checks whether a file, device node, or directory exists at path.
 * Backward-compatible entry point. Returns 1 if exists, 0 otherwise.
 */
int solo_fs_exists(const char *path);

/*
 * Retrieves metadata for the filesystem entry at path, following symlinks.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure (writing message to err_buf).
 */
solo_status_t solo_fs_stat(const char *path, solo_fs_stat_t *st, char *err_buf, size_t err_cap);

/*
 * Retrieves metadata for the filesystem entry at path without following symlinks.
 * On platforms without symlink support, behaves identically to solo_fs_stat.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure (writing message to err_buf).
 */
solo_status_t solo_fs_lstat(const char *path, solo_fs_stat_t *st, char *err_buf, size_t err_cap);

/*
 * Predicates: fast boolean checks.
 * Return 1 if true, 0 if false or on error.
 */
int solo_fs_is_file(const char *path);
int solo_fs_is_dir(const char *path);
int solo_fs_is_symlink(const char *path);

/*
 * Retrieves file size directly.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_file_size(const char *path, solo_fsize_t *out_size);

/*
 * =============================================================================
 * 3. File System Operations & Mutation
 * =============================================================================
 */

/*
 * Creates a single directory at path with the given permission mode (e.g. 0755).
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_mkdir(const char *path, int mode, char *err_buf, size_t err_cap);

/*
 * Recursively creates parent directories as needed (equivalent to mkdir -p).
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_mkdir_p(const char *path, int mode, char *err_buf, size_t err_cap);

/*
 * Removes an empty directory at path.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_rmdir(const char *path, char *err_buf, size_t err_cap);

/*
 * Unlinks / deletes a file or symbolic link at path.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_remove(const char *path, char *err_buf, size_t err_cap);

/*
 * Recursively deletes a file or directory tree (equivalent to rm -rf).
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_remove_all(const char *path, char *err_buf, size_t err_cap);

/*
 * Renames or moves a file or directory from oldpath to newpath.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_rename(const char *oldpath, const char *newpath, char *err_buf, size_t err_cap);

/*
 * Copies a regular file from src to dst.
 * If overwrite is 0 and dst already exists, fails with SOLO_ERROR.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_copy_file(const char *src, const char *dst, int overwrite, char *err_buf, size_t err_cap);

/*
 * Changes the permission mode of a file or directory.
 * Returns SOLO_OK on success, or SOLO_ERROR / SOLO_UNSUPPORTED on failure.
 */
solo_status_t solo_fs_chmod(const char *path, int mode, char *err_buf, size_t err_cap);

/*
 * =============================================================================
 * 4. Directory Iteration (Streaming Iterator)
 * =============================================================================
 */

/*
 * Opens a directory for streaming iteration.
 * Returns an opaque solo_fs_dir_t handle, or NULL on failure.
 */
solo_fs_dir_t *solo_fs_dir_open(const char *path, char *err_buf, size_t err_cap);

/*
 * Reads the next directory entry.
 * Skips "." and ".." automatically.
 * Returns:
 *   SOLO_OK   : ent populated with the next entry name and type.
 *   SOLO_EOF  : no more entries in directory.
 *   SOLO_ERROR: iteration failed.
 */
solo_status_t solo_fs_dir_next(solo_fs_dir_t *dir, solo_fs_dirent_t *ent);

/*
 * Closes a directory handle and releases resources.
 */
void solo_fs_dir_close(solo_fs_dir_t *dir);

/*
 * =============================================================================
 * 5. File Content Operations (Slurp / Spit & Streaming Handles)
 * =============================================================================
 */

/*
 * Reads the entire contents of a file into a newly allocated buffer.
 * Caller is responsible for freeing *out_buf with free().
 * Null-terminates *out_buf (*out_buf[*out_len] == '\0') for text convenience.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_read_file(const char *path, void **out_buf, size_t *out_len, char *err_buf, size_t err_cap);

/*
 * Writes data into path, overwriting any existing contents.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_write_file(const char *path, const void *buf, size_t len, char *err_buf, size_t err_cap);

/*
 * Appends data to path, creating the file if it does not exist.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_append_file(const char *path, const void *buf, size_t len, char *err_buf, size_t err_cap);

/*
 * Opens a file stream with mode string ("r", "w", "a", "r+", "w+", "a+").
 * Returns an allocated solo_file_t handle, or NULL on failure.
 */
solo_file_t *solo_file_open(const char *path, const char *mode, char *err_buf, size_t err_cap);

/*
 * Reads up to count bytes from file into buf.
 * Populates *bytes_read with the actual number of bytes read.
 * Returns SOLO_OK on success, or SOLO_EOF / SOLO_ERROR.
 */
solo_status_t solo_file_read(solo_file_t *f, void *buf, size_t count, size_t *bytes_read);

/*
 * Writes up to count bytes from buf into file.
 * Populates *bytes_written with the actual number of bytes written.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_file_write(solo_file_t *f, const void *buf, size_t count, size_t *bytes_written);

/*
 * Sets the file position indicator.
 * whence: SOLO_FS_SEEK_SET, SOLO_FS_SEEK_CUR, SOLO_FS_SEEK_END.
 * If new_pos is non-NULL, writes the resulting file offset.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_file_seek(solo_file_t *f, long offset, int whence, long *new_pos);

/*
 * Obtains the current file position indicator.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_file_tell(solo_file_t *f, long *pos);

/*
 * Flushes buffered output to disk.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_file_flush(solo_file_t *f);

/*
 * Closes the file stream and releases allocated memory.
 */
void solo_file_close(solo_file_t *f);

/*
 * =============================================================================
 * 6. Environment & Special Directories
 * =============================================================================
 */

/*
 * Gets the current working directory of the process.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_getcwd(char *buf, size_t cap, char *err_buf, size_t err_cap);

/*
 * Changes the current working directory of the process.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_setcwd(const char *path, char *err_buf, size_t err_cap);

/*
 * Obtains the path to the operating system's temporary directory.
 * Returns SOLO_OK on success, or SOLO_ERROR on failure.
 */
solo_status_t solo_fs_temp_dir(char *buf, size_t cap, char *err_buf, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_FILESYSTEM_H */
