# Kernel Sandboxing (`solo.sandbox`)

The `sandbox` subsystem provides operating system-enforced application containment using native kernel security primitives.

---

## 1. Constants & Enumerations

### Backend Identifiers (`solo_sandbox_backend_t`)

| C Constant | Lua String / Backend | Description |
| :--- | :--- | :--- |
| `SOLO_SANDBOX_BACKEND_NONE` | `"none"` | No hardware/kernel sandbox available on host |
| `SOLO_SANDBOX_BACKEND_LANDLOCK` | `"landlock"` | Linux Landlock LSM ruleset engine |
| `SOLO_SANDBOX_BACKEND_OPENBSD` | `"openbsd"` | OpenBSD `unveil(2)` and `pledge(2)` |
| `SOLO_SANDBOX_BACKEND_CAPSICUM` | `"capsicum"` | FreeBSD Capsicum capability mode |

### Filesystem Permission Masks (`solo_sandbox_perm_t`)

| C Constant | Lua String | Bitmask Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_SANDBOX_PERM_READ` | `"r"` | `1 << 0` (`0x01`) | Read file contents, inspect directory entries |
| `SOLO_SANDBOX_PERM_WRITE` | `"w"` | `1 << 1` (`0x02`) | Create, modify, truncate, or delete files |
| `SOLO_SANDBOX_PERM_EXEC` | `"x"` | `1 << 2` (`0x04`) | Execute binary or script from path |

---

## 2. C89 Function Reference

### `solo_sandbox_backend`
```c
solo_sandbox_backend_t solo_sandbox_backend(int *version_out);
const char *solo_sandbox_backend_name(int *version_out);
```
Queries the active security subsystem and stores the kernel ABI version in `version_out` (e.g. Landlock ABI `1`, `2`, `3`, or `4`).

---

### `solo_sandbox_unveil`
```c
solo_sandbox_status_t solo_sandbox_unveil(
    const char *path, 
    int permissions, 
    char *err_buf, 
    size_t err_cap
);
```
Whitelists a filesystem directory or file. Multiple calls accumulate into the sandbox policy.
* **Returns:** `SOLO_SANDBOX_OK` on success, `SOLO_SANDBOX_UNSUPPORTED` if host lacks support, or `SOLO_SANDBOX_ERROR` on failure.

---

### `solo_sandbox_lock`
```c
solo_sandbox_status_t solo_sandbox_lock(char *err_buf, size_t err_cap);
```
Permanently seals the sandbox for the calling process and any child processes it spawns. Once locked, permissions cannot be widened.
* **Important:** Calling `solo_sandbox_lock()` without preceding `unveil()` calls completely locks down the entire filesystem, preventing opening any path.

---

### `solo_sandbox_pledge`
```c
solo_sandbox_status_t solo_sandbox_pledge(
    const char *promises, 
    char *err_buf, 
    size_t err_cap
);
```
Restricts available syscall categories on OpenBSD (e.g. `"stdio rpath wpath cpath inet"`). Returns `SOLO_SANDBOX_UNSUPPORTED` on non-OpenBSD platforms.

---

## 3. Lua API Reference

```lua
local sandbox = require("solo.sandbox")

-- Introspect backend
local name, ver = sandbox.backend()
print(string.format("Sandbox: %s (v%s)", name, tostring(ver)))

-- Grant read access to current directory and read/write to /tmp
sandbox.unveil(".", "r")
sandbox.unveil("/tmp", "rw")

-- Enforce restrictions permanently
sandbox.lock()
```
