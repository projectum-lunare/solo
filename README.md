# solo ⚙️

> **Physical I/O, System Foundations & Hardware Abstractions**  
> _Part of the [Projectum Lunare](https://github.com/projectum-lunare) Ecosystem._

[![License: MPL-2.0](https://img.shields.io/badge/License-MPL_2.0-blue.svg)](https://mozilla.org/MPL/2.0/)
[![Language: ANSI C89](https://img.shields.io/badge/C-ANSI_C89-brightgreen.svg)]()
[![Lua: 5.4-5.5](https://img.shields.io/badge/Lua-5.4_--_5.5-blue.svg)]()

`solo` is a portable, zero-bloat system foundation library written in strict ANSI C (C89 / ISO C90) with first-class Lua bindings. It bridges hardware I/O, network communication, physical ports, persistent filesystems, and kernel-level sandboxing across **10 distinct operating system families** — from vintage platforms of the 1990s (DOS, Amiga, Windows CE, Atari ST) to modern POSIX workstations in 2026.

---

## 🏛️ Architecture & Core Modules

The library is structured as a cohesive monorepo divided into specialized subsystems, available both as a native C API (`libsolo.a` / `libsolo.so`) and as Lua modules (`solo.*`):

```
┌────────────────────────────────────────────────────────────────────────┐
│                   solo (libsolo / Lua API)                             │
├────────────────────────────────────────────────────────────────────────┤
│ • solo.platform   │ Compiler, OS & CPU detection, bits & endianness    │
│ • solo.filesystem │ Safe file and directory I/O (alias: solo.fs)       │
│ • solo.parallel   │ Centronics parallel port (IEEE 1284 / LPT)         │
│ • solo.serial     │ RS-232 serial UART with hardware flow control      │
│ • solo.socket     │ Raw TCP & UDP stream/datagram sockets              │
│ • solo.sandbox    │ Kernel sandboxing (Landlock, Pledge, Capsicum)     │
└────────────────────────────────────────────────────────────────────────┘
```

### 1. `platform` — Platform & Environment Foundation

- Comprehensive compile-time and runtime identification of compilers (`gcc`, `clang`, `watcom`, `sasc`, `vbcc`, `msvc`), architectures (`x86_64`, `x86`, `arm`, `arm64`, `m68k`, `ppc`, `mips`, `riscv64`, `wasm32`), bitness, and endianness.
- Pure metadata and environment introspection with zero I/O side-effects.

### 2. `filesystem` (alias `fs`) — Storage & File Operations

- Portable filesystem operations across vintage and modern operating systems without crashing on raw device nodes (`solo_fs_exists`).
- Clean separation between storage syscalls and environment metadata.
- Ergonomic alias: accessible interchangeably as `solo.filesystem` or `solo.fs` (both in the module table and via `require("solo.fs")`).

### 3. `parallel` — Centronics Parallel Port

- Direct hardware strobe, busy status check, and buffer drain for impact printers, matrix devices, and parallel links with typed enums (`solo.parallel.mode`, `solo.parallel.pin`, `solo.parallel.port`, `solo.parallel.status`).
- Backends: POSIX `/dev/lp0`, Windows NT `LPT1`, Windows CE, DOS `INT 17h` and direct I/O port strobe, Amiga `parallel.device`, Atari ST, KolibriOS, OS/2, Plan 9.

### 4. `serial` — RS-232 Serial UART

- Industrial-grade serial communication with typed enums (`solo.serial.flow`, `solo.serial.parity`, `solo.serial.stop_bits`, `solo.serial.data_bits`, `solo.serial.baud`, `solo.serial.line`, `solo.serial.status`): configurable baud rates, data bits, parity, stop bits, and full flow control (hardware RTS/CTS and software XON/XOFF).
- Direct modem line manipulation (`RTS`, `DTR`).
- Flexible initialization supporting both ergonomic options tables and classic positional parameters.
- Backends: POSIX termios, Windows NT Comm API, Windows CE, DOS BIOS `INT 14h` / 8250 UART, Amiga `serial.device`, Atari ST, KolibriOS, OS/2, Plan 9.

### 5. `socket` — Raw Network Sockets

- Unified TCP & UDP socket abstraction with typed Enums (`solo.socket.type`, `solo.socket.mode`, `solo.socket.event`, `solo.socket.status`).
- Synchronous and asynchronous operation modes (`sock:set_mode`, `sock:set_blocking`, `sock:set_timeout`).
- TCP client connections (`solo.socket.connect`), server binding (`solo.socket.bind`), and client acceptance (`sock:accept`).
- UDP datagram sockets (`solo.socket.udp`, `solo.socket.udp_bind`), datagram transmission (`sock:sendto`), and reception with sender addressing (`sock:recvfrom`).
- Asynchronous multiplexing via `solo.socket.select` and single-socket readiness waiting (`sock:wait`).
- Backends: BSD Sockets (POSIX, macOS, Linux, BSD), Winsock 2 (Windows NT / CE), WATT-32 (MS-DOS 32-bit), `bsdsocket.library` (AmigaOS m68k), OS/2, Plan 9, KolibriOS.

### 6. `sandbox` — Kernel-Level Sandboxing

- Enforces the principle of least privilege directly inside operating system kernels:
  - **Linux:** Modern Landlock LSM filesystem restriction and `prctl(PR_SET_NO_NEW_PRIVS)`.
  - **OpenBSD:** Unveil filesystem paths (`unveil`) and syscall restrictions (`pledge`).
  - **FreeBSD:** Capsicum capability mode.
  - **Other platforms:** Safe fallback stub with zero runtime overhead.

---

## 🌐 Platform Support Matrix

| Platform Family                   | `platform` | `filesystem` | `parallel`           | `serial`            | `socket`               | `sandbox`            |
| :-------------------------------- | :--------: | :----------- | :------------------- | :------------------ | :--------------------- | :------------------- |
| **Linux (Modern & Embedded)**     |     ✅     | ✅ POSIX     | ✅ `/dev/lp*`        | ✅ termios          | ✅ BSD Sockets         | ✅ Landlock LSM      |
| **OpenBSD / FreeBSD / NetBSD**    |     ✅     | ✅ POSIX     | ✅ `/dev/lpt*`       | ✅ termios          | ✅ BSD Sockets         | ✅ Pledge / Capsicum |
| **macOS (Darwin)**                |     ✅     | ✅ POSIX     | ⚠️ Stub              | ✅ termios          | ✅ BSD Sockets         | ⚠️ Stub              |
| **Windows NT (10 / 11 / Server)** |     ✅     | ✅ Win32     | ✅ Win32 LPT         | ✅ Comm API         | ✅ Winsock 2           | ⚠️ Stub              |
| **Windows CE / Pocket PC**        |     ✅     | ✅ WinCE     | ✅ WinCE LPT         | ✅ Comm API         | ✅ Winsock CE          | ⚠️ Stub              |
| **MS-DOS (16/32-bit WATT-32)**    |     ✅     | ✅ DOS io    | ✅ `INT 17h` / I/O   | ✅ `INT 14h` / 8250 | ✅ WATT-32 TCP/IP      | ⚠️ Stub              |
| **AmigaOS (m68k / AROS)**         |     ✅     | ✅ Lock DOS  | ✅ `parallel.device` | ✅ `serial.device`  | ✅ `bsdsocket.library` | ⚠️ Stub              |
| **Atari ST (TOS / MiNT)**         |     ✅     | ✅ Fsfirst   | ✅ Hardware I/O      | ✅ MFP 68901 UART   | ✅ Socket API          | ⚠️ Stub              |
| **KolibriOS**                     |     ✅     | ✅ Syscall   | ✅ Syscall 68        | ✅ Syscall 43       | ✅ Syscall 53          | ⚠️ Stub              |
| **OS/2 / ArcaOS**                 |     ✅     | ✅ access    | ✅ DosOpen LPT       | ✅ DosDevIOCtl      | ✅ TCP/IP Sockets      | ⚠️ Stub              |
| **Plan 9 from Bell Labs**         |     ✅     | ✅ access    | ✅ `/dev/lp`         | ✅ `/dev/eia*`      | ✅ `/net/tcp`          | ⚠️ Stub              |

---

## 🚀 Quickstart: Lua API

```lua
local solo = require("solo")

-- 1. Platform Detection
print("Running on: " .. solo.platform.os)
print("Architecture: " .. solo.platform.arch)
print("Is 64-bit: " .. tostring(solo.platform.is_64bit))

-- 2. Filesystem (usable as solo.filesystem or solo.fs)
if solo.fs.exists("/etc/hosts") then
    print("Found hosts file via solo.fs!")
end

-- 3. Kernel Sandboxing
local backend, version = solo.sandbox.backend()
print("Sandbox backend: " .. backend)

-- Restrict filesystem access to safe paths
solo.sandbox.unveil(".", "r")
solo.sandbox.lock()

-- 4. Network Sockets (socket)
local client, err = solo.socket.connect("127.0.0.1", 9100)
if client then
    client:write("Print payload via solo!\n")
    client:close()
end

-- 5. Serial Port (serial)
local port, err = solo.serial.open("/dev/ttyUSB0", {
    baud = solo.serial.baud.B115200,
    data_bits = 8,
    parity = "none",
    stop_bits = 1,
    flow = solo.serial.flow.HARDWARE
})
if port then
    port:write("AT\r\n")
    port:drain()
    port:close()
end

-- 6. Parallel Port (parallel / Centronics IEEE 1284)
local lpt, err = solo.parallel.open(solo.parallel.port.LPT1)
if lpt then
    lpt:write("Print payload via Centronics!\n\f")
    lpt:drain()
    lpt:close()
end
```

---

## 🔨 Quickstart: C API

```c
#include <stdio.h>
#include <solo/solo.h>

int main(void) {
    char *err = NULL;

    printf("solo version: %s\n", solo_version());
    printf("OS: %s, Arch: %s\n", solo_platform_os(), solo_platform_arch());

    if (solo_fs_exists("/etc/hosts")) {
        printf("Host configuration exists on disk.\n");
    }

    /* Connect to raw TCP socket */
    solo_socket_t *sock = solo_socket_connect("127.0.0.1", 1993, SOLO_SOCK_BLOCKING, &err);
    if (!sock) {
        fprintf(stderr, "Connection failed: %s\n", err);
        return 1;
    }

    size_t sent = 0;
    if (solo_socket_send(sock, "GET / HTTP/1.0\r\n\r\n", 18, &sent) == SOLO_SOCK_OK) {
        printf("Sent %zu bytes\n", sent);
    }
    solo_socket_close(sock);
    return 0;
}
```

---

## 📦 Building and Testing

`solo` requires only a standard ANSI C89 compiler (`gcc`, `clang`, `watcom`, `tcc`, `vbcc`, or `sasc`). The build defaults to `-std=c89` with zero warnings.

```bash
# Build static library, shared library, and Lua module:
xmake

# Run C API unit tests:
xmake test

# Run Lua test suite via Forge launcher:
xmake run test_lua

# Clean artifacts:
xmake clean
```

### Modular Builds & Feature Flags

`solo` is fully decoupled. If an application only needs specific subsystems, unneeded modules can be excluded at compile-time to reduce binary size and eliminate external library dependencies:

```bash
# Compile only specific modules (e.g. platform detection and serial UART):
make MODULES="platform serial"

# Build standalone Lua C submodules (build/lua/solo/*.so):
make modules
```

#### C Preprocessor Flags
- `-DSOLO_NO_SOCKET`: Exclude networking and TCP/UDP sockets.
- `-DSOLO_NO_SERIAL`: Exclude serial port abstractions.
- `-DSOLO_NO_PARALLEL`: Exclude parallel port abstractions.
- `-DSOLO_NO_FILESYSTEM`: Exclude filesystem abstractions.
- `-DSOLO_NO_SANDBOX`: Exclude OS kernel sandboxing.
- `-DSOLO_NO_PLATFORM`: Exclude platform metadata.

#### Standalone Lua Submodules
Submodules can be required independently without loading the umbrella library or its dependencies:
```lua
local platform = require("solo.platform") -- only platform detection (~17 KB)
local serial   = require("solo.serial")   -- only serial communication (~27 KB)
```

### Cross-Platform Multi-Target Builds (Forge)

`solo` uses [Forge](https://github.com/projectum-lunare/forge) for automated cross-compilation and platform management:

```bash
# Inspect SDK availability and platform toolchain status:
xmake forge list

# Build a specific target static library:
xmake forge posix     # build/posix/libsolo.a
xmake forge dos       # build/dos/libsolo.a (Open Watcom + WATT-32)
xmake forge os2       # build/os2/libsolo.a (Open Watcom OS/2)
xmake forge wince     # build/windows_ce/arm/libsolo.a (MS eVC 3.0 ARM via wibo)
xmake forge amiga     # build/amiga/libsolo.a (m68k-amigaos-gcc)
xmake forge atari     # build/atarist/libsolo.a (m68k-atari-mint-gcc)
xmake forge kolibri   # build/kolibri/libsolo.a (kos32-gcc)

# Build all available platforms in a multi-target matrix:
xmake forge all
```

### LuaRocks Installation

```bash
luarocks make solo-scm-1.rockspec
```

---

## 📜 License

Mozilla Public License Version 2.0 (MPL-2.0).  
Copyright (C) 2026 Gustavo Veiga.
