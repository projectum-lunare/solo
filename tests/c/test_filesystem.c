/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * Filesystem Abstraction Tests
 * ============================================================================
 */

TEST test_filesystem_existence(void)
{
    /* 1. Current working directory */
    ASSERT_EQ(1, solo_fs_exists("."));

    /* 2. Known project header file */
    ASSERT_EQ(1, solo_fs_exists("include/solo/solo.h"));

    /* 3. Nonexistent absolute and relative paths */
    ASSERT_EQ(0, solo_fs_exists("/nonexistent_solo_xyz_12345"));
    ASSERT_EQ(0, solo_fs_exists("nonexistent_relative_xyz_67890"));

    /* 4. NULL and empty defense */
    ASSERT_EQ(0, solo_fs_exists(NULL));
    ASSERT_EQ(0, solo_fs_exists(""));

    PASS();
}

TEST test_filesystem_path_algebra(void)
{
    char out[SOLO_PATH_MAX];
    char sep = solo_fs_path_separator();

    ASSERT(sep == '/' || sep == '\\');

    /* Path join */
    ASSERT_EQ(SOLO_OK, solo_fs_path_join("foo", "bar", out, sizeof(out)));
    ASSERT(strstr(out, "foo") != NULL);
    ASSERT(strstr(out, "bar") != NULL);

    /* Path basename */
    ASSERT_EQ(SOLO_OK, solo_fs_path_basename("path/to/file.txt", out, sizeof(out)));
    ASSERT_STR_EQ("file.txt", out);

    ASSERT_EQ(SOLO_OK, solo_fs_path_basename("path/to/directory/", out, sizeof(out)));
    ASSERT_STR_EQ("directory", out);

    /* Path dirname */
    ASSERT_EQ(SOLO_OK, solo_fs_path_dirname("path/to/file.txt", out, sizeof(out)));
    ASSERT(strstr(out, "path") != NULL);

    /* Path extension */
    ASSERT_EQ(SOLO_OK, solo_fs_path_ext("archive.tar.gz", out, sizeof(out)));
    ASSERT_STR_EQ(".gz", out);

    ASSERT_EQ(SOLO_OK, solo_fs_path_ext("README", out, sizeof(out)));
    ASSERT_STR_EQ("", out);

    /* Path normalize */
    ASSERT_EQ(SOLO_OK, solo_fs_path_normalize("a//b/./c/../d", out, sizeof(out)));
    ASSERT(strstr(out, "c") == NULL);
    ASSERT(strstr(out, "d") != NULL);

    /* Path absolute */
    ASSERT(solo_fs_path_is_absolute("/usr/local") == 1);
    ASSERT(solo_fs_path_is_absolute("relative/path") == 0);

    PASS();
}

TEST test_filesystem_stat_and_predicates(void)
{
    char err[128];
    solo_fs_stat_t st;
    solo_fsize_t sz = 0;

    err[0] = '\0';
    ASSERT_EQ(SOLO_OK, solo_fs_stat("include/solo/solo.h", &st, err, sizeof(err)));
    ASSERT_EQ(SOLO_FS_TYPE_FILE, st.type);
    ASSERT(st.size > 0);

    ASSERT_EQ(1, solo_fs_is_file("include/solo/solo.h"));
    ASSERT_EQ(0, solo_fs_is_dir("include/solo/solo.h"));

    ASSERT_EQ(SOLO_OK, solo_fs_stat("src", &st, err, sizeof(err)));
    ASSERT_EQ(SOLO_FS_TYPE_DIR, st.type);
    ASSERT_EQ(1, solo_fs_is_dir("src"));
    ASSERT_EQ(0, solo_fs_is_file("src"));

    ASSERT_EQ(SOLO_OK, solo_fs_file_size("include/solo/solo.h", &sz));
    ASSERT(sz > 0);

    PASS();
}

TEST test_filesystem_mutation_and_iteration(void)
{
    char err[128];
    char *read_buf = NULL;
    size_t read_len = 0;
    solo_fs_dir_t *dir;
    solo_fs_dirent_t ent;
    int found_file = 0;

    /* 1. Recursive mkdir */
    err[0] = '\0';
    ASSERT_EQ(SOLO_OK, solo_fs_mkdir_p("build/test_fs_c/nested/deep", 0777, err, sizeof(err)));
    ASSERT_EQ(1, solo_fs_is_dir("build/test_fs_c/nested/deep"));

    /* 2. Write file */
    ASSERT_EQ(SOLO_OK, solo_fs_write_file("build/test_fs_c/nested/deep/item.txt", "hello", 5, err, sizeof(err)));
    ASSERT_EQ(1, solo_fs_is_file("build/test_fs_c/nested/deep/item.txt"));

    /* 3. Append file */
    ASSERT_EQ(SOLO_OK, solo_fs_append_file("build/test_fs_c/nested/deep/item.txt", " world", 6, err, sizeof(err)));

    /* 4. Read file */
    ASSERT_EQ(SOLO_OK, solo_fs_read_file("build/test_fs_c/nested/deep/item.txt", (void **)&read_buf, &read_len, err, sizeof(err)));
    ASSERT_EQ(11, read_len);
    ASSERT_STR_EQ("hello world", read_buf);
    free(read_buf);

    /* 5. Copy file */
    ASSERT_EQ(SOLO_OK, solo_fs_copy_file("build/test_fs_c/nested/deep/item.txt",
                                         "build/test_fs_c/nested/deep/copy.txt", 1, err, sizeof(err)));
    ASSERT_EQ(1, solo_fs_is_file("build/test_fs_c/nested/deep/copy.txt"));

    /* 6. Rename file */
    ASSERT_EQ(SOLO_OK, solo_fs_rename("build/test_fs_c/nested/deep/copy.txt",
                                      "build/test_fs_c/nested/deep/renamed.txt", err, sizeof(err)));
    ASSERT_EQ(0, solo_fs_exists("build/test_fs_c/nested/deep/copy.txt"));
    ASSERT_EQ(1, solo_fs_exists("build/test_fs_c/nested/deep/renamed.txt"));

    /* 7. Directory iteration */
    dir = solo_fs_dir_open("build/test_fs_c/nested/deep", err, sizeof(err));
    ASSERT(dir != NULL);

    while (solo_fs_dir_next(dir, &ent) == SOLO_OK) {
        if (strcmp(ent.name, "item.txt") == 0 || strcmp(ent.name, "renamed.txt") == 0) {
            found_file++;
            ASSERT_EQ(SOLO_FS_TYPE_FILE, ent.type);
        }
    }
    solo_fs_dir_close(dir);
    ASSERT_EQ(2, found_file);

    /* 8. Remove all */
    ASSERT_EQ(SOLO_OK, solo_fs_remove_all("build/test_fs_c", err, sizeof(err)));
    ASSERT_EQ(0, solo_fs_exists("build/test_fs_c"));

    PASS();
}

TEST test_filesystem_file_stream(void)
{
    char err[128];
    char buf[16];
    size_t io_bytes = 0;
    long pos = 0;
    solo_file_t *f;

    err[0] = '\0';
    f = solo_file_open("build/test_c_stream.bin", "w+", err, sizeof(err));
    ASSERT(f != NULL);

    ASSERT_EQ(SOLO_OK, solo_file_write(f, "abcdefgh", 8, &io_bytes));
    ASSERT_EQ(8, io_bytes);

    ASSERT_EQ(SOLO_OK, solo_file_tell(f, &pos));
    ASSERT_EQ(8, pos);

    ASSERT_EQ(SOLO_OK, solo_file_seek(f, 2, SOLO_FS_SEEK_SET, &pos));
    ASSERT_EQ(2, pos);

    memset(buf, 0, sizeof(buf));
    ASSERT_EQ(SOLO_OK, solo_file_read(f, buf, 4, &io_bytes));
    ASSERT_EQ(4, io_bytes);
    ASSERT(memcmp(buf, "cdef", 4) == 0);

    ASSERT_EQ(SOLO_OK, solo_file_flush(f));
    solo_file_close(f);

    ASSERT_EQ(SOLO_OK, solo_fs_remove("build/test_c_stream.bin", err, sizeof(err)));
    ASSERT_EQ(0, solo_fs_exists("build/test_c_stream.bin"));

    PASS();
}

TEST test_filesystem_environment(void)
{
    char cwd[SOLO_PATH_MAX];
    char temp[SOLO_PATH_MAX];
    char err[128];

    err[0] = '\0';
    ASSERT_EQ(SOLO_OK, solo_fs_getcwd(cwd, sizeof(cwd), err, sizeof(err)));
    ASSERT(strlen(cwd) > 0);

    ASSERT_EQ(SOLO_OK, solo_fs_temp_dir(temp, sizeof(temp), err, sizeof(err)));
    ASSERT(strlen(temp) > 0);

    PASS();
}

SUITE(filesystem_suite)
{
    RUN_TEST(test_filesystem_existence);
    RUN_TEST(test_filesystem_path_algebra);
    RUN_TEST(test_filesystem_stat_and_predicates);
    RUN_TEST(test_filesystem_mutation_and_iteration);
    RUN_TEST(test_filesystem_file_stream);
    RUN_TEST(test_filesystem_environment);
}
