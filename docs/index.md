# solo

<p align="center">
  <strong>Lightweight, modular C89 and Lua systems programming library for modern and vintage operating systems.</strong>
</p>

---

## Overview

**`solo`** is an operating system abstraction library written in strict **ANSI C89 (ISO C90)** with ergonomic **Lua** bindings. It provides a cohesive, unified systems API covering modern and retro operating systems spanning four decades of computing history.

Whether you are targeting **Linux**, **FreeBSD**, **macOS**, **Windows 11**, **Windows NT 3.51**, **MS-DOS**, **AmigaOS**, **Atari ST**, **KolibriOS**, **OS/2**, or **Plan 9**, `solo` exposes identical semantics, canonical types, and runtime guarantees.

```mermaid
graph TD
    UserApp["Application (C89 or Lua)"]
    Solo["solo Monorepo Layer (libsolo / Lua)"]
    
    SubPlatform["solo.platform"]
    SubFS["solo.filesystem"]
    SubSocket["solo.socket"]
    SubSerial["solo.serial"]
    SubParallel["solo.parallel"]
    SubSandbox["solo.sandbox"]
    
    PAL["Platform Abstraction Layer (PAL)"]
    
    OSModern["Modern POSIX & Win32<br/>(Linux, BSD, macOS, Windows 10/11)"]
    OSVintage["Vintage Windows & Stacks<br/>(Windows NT 3.51..XP, WinCE)"]
    OSRetro["Retro Computing<br/>(MS-DOS, AmigaOS, Atari ST)"]
    OSExotic["Alternative Kernels<br/>(KolibriOS, OS/2, Plan 9)"]

    UserApp --> Solo
    Solo --> SubPlatform
    Solo --> SubFS
    Solo --> SubSocket
    Solo --> SubSerial
    Solo --> SubParallel
    Solo --> SubSandbox
    
    SubPlatform & SubFS & SubSocket & SubSerial & SubParallel & SubSandbox --> PAL
    PAL --> OSModern
    PAL --> OSVintage
    PAL --> OSRetro
    PAL --> OSExotic
```

---

## Core Subsystems

| Subsystem | Description | Lua Module | C Header |
| :--- | :--- | :--- | :--- |
| **`platform`** | Architecture, OS detection, bitness, endianness metadata | `require("solo.platform")` | [`solo/platform.h`](file:///home/gustavo/Projetos/solo/include/solo/platform.h) |
| **`filesystem`** | Safe filesystem manipulation, recursion, streaming (alias: `fs`) | `require("solo.fs")` | [`solo/filesystem.h`](file:///home/gustavo/Projetos/solo/include/solo/filesystem.h) |
| **`socket`** | Raw TCP & UDP IPv4 sockets, native `poll(2)` and dynamic `WSAPoll` | `require("solo.socket")` | [`solo/socket.h`](file:///home/gustavo/Projetos/solo/include/solo/socket.h) |
| **`serial`** | RS-232 UART driver, baud rates, parity, modem control lines | `require("solo.serial")` | [`solo/serial.h`](file:///home/gustavo/Projetos/solo/include/solo/serial.h) |
| **`parallel`** | IEEE 1284 Centronics printer port, raw pin access, strobe timing | `require("solo.parallel")` | [`solo/parallel.h`](file:///home/gustavo/Projetos/solo/include/solo/parallel.h) |
| **`sandbox`** | Kernel-level sandboxing (Linux Landlock, OpenBSD Pledge, Capsicum) | `require("solo.sandbox")` | [`solo/sandbox.h`](file:///home/gustavo/Projetos/solo/include/solo/sandbox.h) |

---

## Quick Example: Sockets

=== "C (ANSI C89)"

    ```c
    #include <stdio.h>
    #include <solo/solo.h>

    int main(void) {
        char err_buf[128];
        solo_socket_t *sock;
        size_t sent = 0;

        sock = solo_socket_connect("127.0.0.1", 8080, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
        if (!sock) {
            fprintf(stderr, "Connection failed: %s\n", err_buf);
            return 1;
        }

        if (solo_socket_send(sock, "PING\n", 5, &sent) == SOLO_SOCK_OK) {
            printf("Sent %lu bytes\n", (unsigned long)sent);
        }

        solo_socket_close(sock);
        return 0;
    }
    ```

=== "Lua"

    ```lua
    local solo = require("solo")

    local client, err = solo.socket.connect("127.0.0.1", 8080)
    if not client then
        error("Connection failed: " .. tostring(err))
    end

    client:write("PING\n")
    client:close()
    ```

---

## Guiding Principles

1. **Strict ANSI C89 (ISO C90) Public Surface:** Clean interop with antique compilers (Open Watcom, MSVC 4.0, vbcc, Aztec C) without breaking modern C99/C11/C++ toolchains.
2. **Deterministic Canonical Types:** No raw integers as pseudo-booleans. Canonical types like `solo_bool_t`, `solo_status_t`, and `solo_line_state_t` enforce compile-time clarity.
3. **Zero Dynamic Allocation on Common Paths:** Critical I/O routines (such as event multiplexing) utilize stack-allocated scratch buffers for up to 64 descriptors.
4. **Retro Without Compromise:** Windows NT 3.51, MS-DOS, AmigaOS, and Atari ST are first-class targets, not abandoned emulations.
