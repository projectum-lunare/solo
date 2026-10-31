# Kernel Sandboxing (`solo.sandbox`)

The `sandbox` subsystem provides unified kernel-level sandboxing, enforcing the principle of least privilege directly inside operating system kernels.

---

## Supported Kernel Backends

- **Linux:** Modern **Landlock LSM** filesystem restriction rulesets and `prctl(PR_SET_NO_NEW_PRIVS)`.
- **OpenBSD:** Native path restriction via `unveil(2)` and syscall restriction via `pledge(2)`.
- **FreeBSD:** Capability mode via `capsicum(4)`.
- **Other Platforms:** Safe fallback stubs returning `SOLO_SANDBOX_UNSUPPORTED` with zero runtime overhead or instability.

---

## Sandboxing Workflow

Sandboxing follows a strict two-stage phase:
1. **Unveil / Allow:** Grant read, write, or execute access to specific directory paths.
2. **Lock:** Seal the sandbox. Once locked, rules cannot be relaxed by the process or its descendants.

=== "C (C89)"

    ```c
    #include <stdio.h>
    #include <solo/solo.h>

    void isolate_process(void) {
        /* Restrict process to read current directory and /tmp for writes */
        solo_sandbox_unveil(".", "r");
        solo_sandbox_unveil("/tmp", "rw");

        /* Lock down process permanently */
        if (solo_sandbox_lock() == SOLO_SANDBOX_OK) {
            printf("Process successfully isolated by host kernel.\n");
        }
    }
    ```

=== "Lua"

    ```lua
    local sandbox = require("solo.sandbox")

    local backend, ver = sandbox.backend()
    print(string.format("Kernel backend: %s (v%s)", backend, tostring(ver)))

    -- Allow reading current directory only
    sandbox.unveil(".", "r")

    -- Seal sandbox
    sandbox.lock()

    -- Attempting to open unauthorized paths will now raise permission denied (EACCES)
    ```

> [!WARNING] Landlock Process Inheritance
> On Linux, the Landlock ruleset is process-wide and inherited by all child threads and fork descendants. Calling `lock()` without prior `unveil()` calls locks the entire filesystem, prohibiting all subsequent file opens.
