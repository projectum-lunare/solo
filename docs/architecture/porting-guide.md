# Platform Abstraction Layer (PAL) Porting Guide

This guide details how to extend `solo` to support a **new operating system or kernel target** (such as QNX, Zephyr RTOS, Haiku native APIs, or Redox OS).

---

## 1. Architecture Overview

`solo` strictly decouples public consumer-facing headers from internal platform-specific drivers:

```
[ Application Code ] ──> include/solo/*.h (Strict ANSI C89)
                             │
                             ▼
                     src/*/*_driver.c (gnu89 implementation)
                             │
                             ▼
                     src/*/*_pal.h (Platform Abstraction Layer)
                             │
    ┌────────────────┬───────┴────────┬────────────────┐
    ▼                ▼                ▼                ▼
[ POSIX PAL ]  [ Win32 PAL ]    [ DOS/Amiga PAL ]  [ YOUR NEW PAL ]
```

### The Cardinal Rules of Porting:
1. **Never pollute `include/solo/*.h`:** Public headers must never include OS-specific headers (such as `<windows.h>`, `<sys/socket.h>`, `<dos.h>`, or RTOS headers).
2. **Strict ANSI C89 in Public APIs:** All structs, typedefs, and function prototypes visible to consumers must compile under C89 (`-std=c89`).
3. **`gnu89` Scoping for Drivers:** Driver implementation files (`*_driver.c`) may use GCC compiler extensions (`__asm__`, inline assembly, `#pragma`, register calling conventions).

---

## 2. Step-by-Step Porting Checklist

### Step 1: Define the Platform Identifier

In [`src/platform/platform_pal.h`](file:///home/gustavo/Projetos/solo/src/platform):
1. Add compile-time detection for your OS:
   ```c
   #elif defined(__QNX__) || defined(__QNXNTO__)
       #define SOLO_IS_QNX 1
       #define SOLO_IS_POSIX 1
   ```
2. In [`src/platform/platform.c`](file:///home/gustavo/Projetos/solo/src/platform/platform.c), register the string identifier:
   ```c
   #if SOLO_IS_QNX
       return "qnx";
   #endif
   ```
3. In [`src/platform/platform_lua.c`](file:///home/gustavo/Projetos/solo/src/platform/platform_lua.c), register the boolean flag:
   ```c
   lua_pushboolean(L, SOLO_IS_QNX ? 1 : 0);
   lua_setfield(L, -2, "is_qnx");
   ```

---

### Step 2: Implement the Socket PAL

In [`src/socket/socket_pal.h`](file:///home/gustavo/Projetos/solo/src/socket/socket_pal.h):
1. Define the native socket handle type:
   ```c
   #define SOLO_PAL_SOCK_T int
   #define SOLO_PAL_INVALID_SOCKET (-1)
   #define SOLO_SOCK_IS_VALID(s) ((s) != NULL && (s)->fd >= 0)
   ```
2. Map interrupt errors:
   ```c
   #define SOLO_PAL_IS_INTR(e) ((e) == EINTR)
   ```
3. Map non-blocking would-block checks:
   ```c
   #define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
   ```
4. Implement `solo_pal_strerror` to format native error numbers without heap allocations.

---

### Step 3: Implement the Serial UART Driver

In [`src/serial/serial_pal.h`](file:///home/gustavo/Projetos/solo/src/serial/serial_pal.h):
1. Define the internal `struct serial_port`:
   ```c
   struct serial_port {
       int fd;             /* File descriptor or hardware handle */
       int timeout_ms;     /* Current read/write timeout */
       /* OS-specific termios or register state */
   };
   ```
2. Implement the mandatory PAL lifecycle primitives:
   * `solo_pal_serial_open()`
   * `solo_pal_serial_close()`
   * `solo_pal_serial_read()`
   * `solo_pal_serial_write()`
   * `solo_pal_serial_drain()`
   * `solo_pal_serial_set_timeout()`
   * `solo_pal_serial_set_rts()` / `solo_pal_serial_set_dtr()`
   * `solo_pal_serial_get_cts()` / `solo_pal_serial_get_dsr()`

---

### Step 4: Implement Filesystem PAL

In [`src/filesystem/filesystem_pal.h`](file:///home/gustavo/Projetos/solo/src/filesystem/filesystem_pal.h):
* Implement stat inspection, directory iteration (`opendir`/`readdir` or native equivalent), and directory tree manipulation.
* Ensure path algebra functions (`join`, `normalize`, `dirname`, `basename`) remain pure string manipulations with **zero syscalls**.

---

### Step 5: Implement Kernel Sandboxing (or Fallback)

If your OS does not have a native kernel sandbox (such as Linux Landlock or OpenBSD Pledge), `solo` provides a standardized fallback:

In [`src/sandbox/sandbox_pal.h`](file:///home/gustavo/Projetos/solo/src/sandbox/sandbox_pal.h):
```c
#if SOLO_IS_YOUR_OS
    #define SOLO_PAL_SANDBOX_BACKEND SOLO_SANDBOX_BACKEND_NONE
    #define SOLO_PAL_SANDBOX_NAME    "none"
#endif
```

All functions will cleanly return `SOLO_SANDBOX_UNSUPPORTED` (`-4`), allowing applications to gracefully detect that the platform lacks hardware containment without aborting.

---

### Step 6: Add Cross-Compilation Target to Forge

In [`xmake.lua`](file:///home/gustavo/Projetos/solo/xmake.lua):
Register your toolchain in Forge:
```lua
target("solo_qnx")
    set_kind("static")
    set_toolchains("qnx-gcc")
    add_defines("SOLO_IS_QNX=1")
    add_files("src/**.c")
```
