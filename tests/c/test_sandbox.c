/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * Kernel Security Sandbox & Process Confinement Tests
 * ============================================================================
 * Solo provides portable sandboxing using native kernel mechanisms:
 * - Linux: Landlock LSM (Filesystem unveils & rulesets)
 * - OpenBSD: pledge() & unveil()
 * - FreeBSD: Capsicum capability mode
 *
 * NOTE ON TESTING ISOLATION:
 * Sandbox restrictions are irreversible within a process. Applying a sandbox
 * directly to the main test process would restrict subsequent tests. Therefore,
 * live confinement tests use fork() to isolate sandbox enforcement within a child
 * process, leaving the test runner parent unrestricted.
 */

/*
 * Verifies that the sandbox subsystem correctly detects the host kernel's
 * active sandboxing mechanism and ABI/protocol version.
 */
TEST test_sandbox(void)
{
    int version = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);
    const char *backend = solo_sandbox_backend_name(&version);

    /* The backend name must be a valid, non-empty descriptive string (e.g. "landlock") */
    ASSERT(backend != NULL);
    ASSERT(strlen(backend) > 0);

    /* Test predicate function solo_sandbox_is_supported() */
    if (b != SOLO_SANDBOX_BACKEND_NONE) {
        ASSERT_EQ(SOLO_TRUE, solo_sandbox_is_supported());
    } else {
        ASSERT_EQ(SOLO_FALSE, solo_sandbox_is_supported());
    }

    /* The backend enum must fall within valid known backend ranges */
    ASSERT(b >= SOLO_SANDBOX_BACKEND_NONE && b <= SOLO_SANDBOX_BACKEND_CAPSICUM);

    PASS();
}

/*
 * Verifies the binary values and bitmask semantics of sandbox constants
 * defined in solo/sandbox.h to ensure ABI stability.
 */
TEST test_sandbox_enums(void)
{
    /* Backend identifiers */
    ASSERT_EQ(0, SOLO_SANDBOX_BACKEND_NONE);
    ASSERT_EQ(1, SOLO_SANDBOX_BACKEND_LANDLOCK);
    ASSERT_EQ(2, SOLO_SANDBOX_BACKEND_OPENBSD);
    ASSERT_EQ(3, SOLO_SANDBOX_BACKEND_CAPSICUM);

    /* Filesystem permission bitmasks for solo_sandbox_unveil() */
    ASSERT_EQ(0, SOLO_SANDBOX_PERM_NONE);
    ASSERT_EQ(1 << 0, SOLO_SANDBOX_PERM_READ);
    ASSERT_EQ(1 << 1, SOLO_SANDBOX_PERM_WRITE);
    ASSERT_EQ(1 << 2, SOLO_SANDBOX_PERM_EXEC);

    /* Status return codes */
    ASSERT_EQ(0, SOLO_SANDBOX_OK);
    ASSERT_EQ(-1, SOLO_SANDBOX_ERROR);
    ASSERT_EQ(-4, SOLO_SANDBOX_UNSUPPORTED);

    PASS();
}

/*
 * Verifies that calling an unsupported sandbox primitive (such as pledge() on
 * Linux where Landlock only handles filesystem unveils) fails cleanly with
 * SOLO_SANDBOX_UNSUPPORTED and provides an explanatory error message.
 */
TEST test_sandbox_unsupported(void)
{
    char err_buf[128];
    solo_sandbox_status_t rc;

#if SOLO_IS_LINUX
    /* On Linux, Landlock does not support OpenBSD pledge(); must fail visibly with error message */
    err_buf[0] = '\0';
    rc = solo_sandbox_pledge("stdio", err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SANDBOX_UNSUPPORTED, rc);
    ASSERT(strlen(err_buf) > 0);
#elif !SOLO_IS_OPENBSD
    /* On stubs or non-OpenBSD, pledge must fail visibly with SOLO_SANDBOX_UNSUPPORTED */
    err_buf[0] = '\0';
    rc = solo_sandbox_pledge("stdio", err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SANDBOX_UNSUPPORTED, rc);
    ASSERT(strlen(err_buf) > 0);
#endif

    PASS();
}

/*
 * Tests active filesystem confinement using Landlock on Linux.
 *
 * SCENARIO:
 * 1. Forks a child process.
 * 2. In the child, unveils the current directory "." with READ permission.
 * 3. Enforces the sandbox via solo_sandbox_lock().
 * 4. Verifies that reading a file inside "." (e.g. "xmake.lua") succeeds.
 * 5. Verifies that opening an external system file (e.g. "/etc/passwd") is
 *    denied by the kernel (Landlock returns EACCES / NULL).
 * 6. Parent waits for child and asserts exit code 0.
 */
TEST test_sandbox_fork(void)
{
#if SOLO_IS_LINUX
    int version = 0;
    int status = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);

    /* Only run live enforcement if Landlock is actively supported by the host kernel */
    if (b == SOLO_SANDBOX_BACKEND_LANDLOCK && version > 0) {
        pid_t pid = fork();
        ASSERT(pid >= 0);

        if (pid == 0) {
            /* --- CHILD PROCESS --- */
            char err_buf[128];
            FILE *f;
            solo_sandbox_status_t rc;

            /* Unveil current working directory for read access */
            err_buf[0] = '\0';
            rc = solo_sandbox_unveil(".", SOLO_SANDBOX_PERM_READ, err_buf, sizeof(err_buf));
            if (rc != SOLO_SANDBOX_OK) {
                _exit(1);
            }

            /* Irrevocably lock the sandbox ruleset */
            err_buf[0] = '\0';
            rc = solo_sandbox_lock(err_buf, sizeof(err_buf));
            if (rc != SOLO_SANDBOX_OK) {
                _exit(2);
            }

            /* Permitted read: local file in unveiled path must succeed */
            f = fopen("xmake.lua", "r");
            if (!f) {
                _exit(3);
            }
            fclose(f);

            /* Forbidden read: file outside unveiled hierarchy must be blocked by Landlock */
            f = fopen("/etc/passwd", "r");
            if (f) {
                fclose(f);
                _exit(4); /* Security breach: file was opened despite sandbox! */
            }

            _exit(0); /* Success: sandbox behavior matches security policy */
        }

        /* --- PARENT PROCESS --- */
        status = 0;
        waitpid(pid, &status, 0);
        ASSERT(WIFEXITED(status));
        ASSERT_EQ(0, WEXITSTATUS(status));
    }
#endif

    PASS();
}

/*
 * Tests complete filesystem lockout when locking without any unveils.
 *
 * SCENARIO:
 * 1. Forks a child process.
 * 2. In the child, immediately invokes solo_sandbox_lock() without any unveils.
 * 3. Under Landlock rules, a locked ruleset with no unveiled paths blocks all
 *    new filesystem openings across the entire system.
 * 4. Verifies that opening "/etc/passwd" is denied.
 * 5. Parent verifies child exits cleanly with code 0.
 */
TEST test_sandbox_lock_without_unveil_fork(void)
{
#if SOLO_IS_LINUX
    int version = 0;
    int status = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);

    if (b == SOLO_SANDBOX_BACKEND_LANDLOCK && version > 0) {
        pid_t pid = fork();
        ASSERT(pid >= 0);

        if (pid == 0) {
            /* --- CHILD PROCESS --- */
            char err_buf[128];
            FILE *f;
            solo_sandbox_status_t rc;

            /* Lock immediately without granting any path permissions */
            err_buf[0] = '\0';
            rc = solo_sandbox_lock(err_buf, sizeof(err_buf));
            if (rc != SOLO_SANDBOX_OK) {
                _exit(1);
            }

            /* Any filesystem open attempt must be blocked */
            f = fopen("/etc/passwd", "r");
            if (f) {
                fclose(f);
                _exit(2); /* Failure: access was not blocked */
            }

            _exit(0); /* Success */
        }

        /* --- PARENT PROCESS --- */
        status = 0;
        waitpid(pid, &status, 0);
        ASSERT(WIFEXITED(status));
        ASSERT_EQ(0, WEXITSTATUS(status));
    }
#endif

    PASS();
}

SUITE(sandbox_suite)
{
    RUN_TEST(test_sandbox);
    RUN_TEST(test_sandbox_enums);
    RUN_TEST(test_sandbox_unsupported);
    RUN_TEST(test_sandbox_fork);
    RUN_TEST(test_sandbox_lock_without_unveil_fork);
}
