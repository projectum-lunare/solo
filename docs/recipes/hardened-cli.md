# Recipe: Defense-in-Depth CLI Sandboxing

Command-line utilities processing untrusted user input (such as parsers, converters, or image processors) should enforce the principle of least privilege inside the operating system kernel.

This recipe demonstrates how to:
1. Parse arguments and read local configuration files.
2. Whitelist only required directories (`unveil`).
3. Seal the process (`lock`) so that any attempted access to `/etc/shadow`, user home directories, or parent folders triggers an immediate permission fault (`EACCES`).

---

## C89 Implementation

```c
#include <stdio.h>
#include <stdlib.h>
#include <solo/solo.h>

int main(int argc, char **argv) {
    char err[128];
    const char *work_dir = ".";
    const char *temp_dir = "/tmp";
    char *config_data = NULL;
    size_t config_len = 0;

    printf("Starting secure CLI utility...\n");

    /* Step 1: Read trusted configuration before locking down */
    config_data = solo_fs_read_file("config.ini", &config_len);
    if (config_data) {
        printf("Loaded config (%lu bytes)\n", (unsigned long)config_len);
        free(config_data);
    }

    /* Step 2: Configure Sandbox Policy */
    /* Read-only on the working directory */
    solo_sandbox_unveil(work_dir, SOLO_SANDBOX_READ, err, sizeof(err));
    
    /* Read-write on /tmp for temporary scratch output */
    solo_sandbox_unveil(temp_dir, SOLO_SANDBOX_READ | SOLO_SANDBOX_WRITE, err, sizeof(err));

    /* Step 3: Seal the sandbox permanently */
    if (solo_sandbox_lock(err, sizeof(err)) == SOLO_SANDBOX_OK) {
        printf("Sandbox locked. Kernel restrictions active.\n");
    } else {
        printf("Sandboxing not supported on this OS (continuing unconfined).\n");
    }

    /* Step 4: Normal operation within bounds */
    solo_fs_write_file("/tmp/output.txt", "Processed data\n", 15);
    printf("Wrote output to /tmp/output.txt successfully.\n");

    /* Step 5: Proof of Security - Unauthorized access fails */
    if (solo_fs_exists("/etc/passwd") == SOLO_FALSE) {
        printf("[CONFIRMED] Host kernel blocked access to /etc/passwd.\n");
    }

    return 0;
}
```

---

## Lua Implementation

```lua
local solo = require("solo")
local fs = solo.fs
local sandbox = solo.sandbox

print("Backend: " .. tostring(sandbox.backend()))

-- 1. Read configuration
if fs.exists("config.json") then
    local conf = fs.read_file("config.json")
    print("Configuration loaded")
end

-- 2. Restrict process privileges
sandbox.unveil(".", "r")      -- Read-only in workspace
sandbox.unveil("/tmp", "rw")   -- Read-write in /tmp

-- 3. Lock sandbox
sandbox.lock()

-- 4. Permitted operation
fs.write_file("/tmp/result.txt", "OK\n")
print("Saved /tmp/result.txt")

-- 5. Prohibited operation raises error
local ok, err = pcall(function()
    fs.write_file("unauthorized.txt", "FAIL\n")
end)

if not ok then
    print("Security policy enforced by kernel: " .. tostring(err))
end
```
