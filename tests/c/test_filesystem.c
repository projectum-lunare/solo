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
 * Tests the portable filesystem utility functions, primarily solo_fs_exists(),
 * ensuring consistent existence checks across relative, absolute, nonexistent,
 * and invalid paths on all platforms.
 */

TEST test_filesystem(void)
{
    /*
     * 1. Current working directory:
     *    The relative directory "." must always exist and return 1 (true).
     */
    ASSERT_EQ(1, solo_fs_exists("."));

    /*
     * 2. Known project header file:
     *    A known relative path within the repository must return 1 (true).
     */
    ASSERT_EQ(1, solo_fs_exists("include/solo/solo.h"));

    /*
     * 3. Nonexistent absolute path:
     *    A bogus absolute path must return 0 (false) without throwing errors.
     */
    ASSERT_EQ(0, solo_fs_exists("/nonexistent_solo_xyz_12345"));

    /*
     * 4. Nonexistent relative path:
     *    A bogus relative path must return 0 (false) without throwing errors.
     */
    ASSERT_EQ(0, solo_fs_exists("nonexistent_relative_xyz_67890"));

    /*
     * 5. NULL pointer defense:
     *    Passing a NULL pointer must be handled safely, returning 0 without segfault.
     */
    ASSERT_EQ(0, solo_fs_exists(NULL));

    PASS();
}

SUITE(filesystem_suite)
{
    RUN_TEST(test_filesystem);
}
