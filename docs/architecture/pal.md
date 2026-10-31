# Platform Abstraction Layer (PAL)

The **Platform Abstraction Layer (PAL)** decouples high-level public APIs and Lua bindings from OS-specific system calls, hardware I/O ports, and kernel architectures.

---

## PAL Directory Organization

Each subsystem contains a dedicated `*_pal.h` file that detects the host environment and provides unified internal signatures:

- [`src/socket/socket_pal.h`](file:///home/gustavo/Projetos/solo/src/socket/socket_pal.h)
- [`src/serial/serial_pal.h`](file:///home/gustavo/Projetos/solo/src/serial/serial_pal.h)
- [`src/filesystem/filesystem_pal.h`](file:///home/gustavo/Projetos/solo/src/filesystem/filesystem_pal.h)
- [`src/parallel/parallel_pal.h`](file:///home/gustavo/Projetos/solo/src/parallel/parallel_pal.h)
- [`src/sandbox/sandbox_pal.h`](file:///home/gustavo/Projetos/solo/src/sandbox/sandbox_pal.h)

---

## PAL Compilation Pipeline

```mermaid
flowchart TD
    PublicAPI["Public C API (include/solo/*.h)"]
    LuaBinding["Lua Binding (src/*/*_lua.c)"]
    
    BackendDispatch["Subsystem Core Engine"]
    
    PALDetect["PAL Macro Detection<br/>SOLO_IS_POSIX, SOLO_IS_WINDOWS, SOLO_IS_DOS, etc."]
    
    DriverPOSIX["POSIX Driver (termios, poll, dirent)"]
    DriverWin32["Win32 Driver (Comm API, WSAPoll/select, FindFirstFile)"]
    DriverDOS["DOS Driver (INT 14h, INT 17h, WATT-32)"]
    DriverAmiga["Amiga Driver (serial.device, bsdsocket.library)"]
    DriverAtari["Atari Driver (MFP 68901, MiNTNet)"]
    DriverExotic["Exotic Drivers (KolibriOS, OS/2, Plan 9)"]

    PublicAPI --> BackendDispatch
    LuaBinding --> BackendDispatch
    BackendDispatch --> PALDetect
    PALDetect --> DriverPOSIX & DriverWin32 & DriverDOS & DriverAmiga & DriverAtari & DriverExotic
```

---

## PAL Rules for Contributors

1. **No direct standard includes in public headers:** User code including `<solo/solo.h>` should not leak `<windows.h>`, `<sys/socket.h>`, or `<dos.h>`.
2. **Abstract socket types:** Use `SOLO_PAL_SOCK_T` and `SOLO_SOCK_IS_VALID()` because Windows uses `SOCKET` (unsigned integer) while POSIX uses `int` (signed integer).
3. **Interrupt Handling:** Loop on transient interrupt signals (`EINTR` on POSIX, `WSAEINTR` on Windows) via `SOLO_PAL_IS_INTR(err)`.
