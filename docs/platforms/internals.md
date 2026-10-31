# Platform Gotchas & Internals

Systems programming across 10 operating system families requires navigating subtle architectural differences, historical ABI decisions, and kernel quirks.

This document serves as an engineering post-mortem of how `solo` resolves these challenges.

---

## 1. Winsock vs POSIX: The `fd_set` Divergence

One of the most dangerous portability bugs in C networking involves `select()` and `fd_set`.

### In POSIX (Linux, BSD, macOS)
On POSIX systems, `fd_set` is a **fixed-size bitmask** containing an array of integers (typically totaling 1024 bits).
* Bit `n` corresponds to file descriptor integer `n`.
* If a high-volume server process has opened many files (pipes, logs, database connections) and receives socket file descriptor `1024` or higher, calling `FD_SET(1024, &fds)` writes past the bounds of the stack bitmask.
* **The Result:** Silent stack corruption and random memory crashes.
* **The `solo` Solution:** On POSIX, `solo` uses native `poll(2)` (`solo_posix_poll`), which accepts a dynamic array of `struct pollfd`. High descriptor IDs ($fd \ge 1024$) are handled transparently with zero risk of buffer overflows.

### In Winsock (Windows NT 3.51 to 11)
Microsoft chose a completely different ABI for Winsock in 1993:
```c
typedef struct fd_set {
    u_int   fd_count;               /* Number of sockets currently in array */
    SOCKET  fd_array[FD_SETSIZE];   /* Explicit array of SOCKET handles */
} fd_set;
```
* In Winsock, `SOCKET` values are arbitrary 32/64-bit kernel object pointers, **not sequential indices**.
* `FD_SET` does not index a bitmask; it simply appends the handle to `fd_array` and increments `fd_count`.
* **Redefining `FD_SETSIZE`:** Because it is an explicit array, redefining `#define FD_SETSIZE 1024` before including `<winsock2.h>` safely expands the array capacity from the default 64 to 1024 sockets without any ABI breakage or stack corruption.

---

## 2. Windows NT 3.51 PE Loader & Dynamic `WSAPoll`

When Microsoft introduced `WSAPoll` in Windows Vista (NT 6.0), developers attempted to use it to replace `select()`.

However, compiling code that calls `WSAPoll()` statically bakes an import reference into the PE binary's `.idata` section:

```
Import Address Table: WS2_32.DLL -> WSAPoll
```

If that executable is launched on **Windows NT 3.51, Windows NT 4.0, Windows 2000, or Windows XP**, the Windows PE Loader inspects the import table, discovers that `WSAPoll` does not exist in `WS2_32.DLL`, and immediately aborts execution with a fatal dialog before `main()` is ever reached:

> *"The procedure entry point WSAPoll could not be located in the dynamic link library WS2_32.DLL"*

### The `solo` Solution
`solo` uses dynamic runtime probing:
1. `GetModuleHandleA("ws2_32.dll")` is called.
2. `GetProcAddress(hws2, "WSAPoll")` queries the function pointer.
3. `solo` clones the exact memory ABI in [`solo_wsapollfd_t`](file:///home/gustavo/Projetos/solo/src/socket/socket_bsd.c).
4. If found (Vista..11), `WSAPoll` is invoked.
5. If `NULL` (NT 3.51..XP), execution transparently routes to `solo_poll_via_select()`.

---

## 3. AmigaOS: Sockets as Exec Signals

On Commodore Amiga (AmigaOS 3.x/4.x), networking is provided by `bsdsocket.library` (AmiTCP, Roadshow, Miami).

AmigaOS is not POSIX and lacks hardware interrupt signals like `SIGALRM` or `SIGIO`. Instead, multitasking is driven by the **Exec microkernel Signal System**:
* Each Amiga task has 32 signal bits allocated.
* When a network socket receives data, `bsdsocket.library` asserts an Exec Signal bit assigned to that process.
* Multiplexing is implemented via `WaitSelect()`, which maps BSD `select()` descriptor masks to Exec signal masks.
* **The Gotcha:** Trying to include `<poll.h>` or invoke `poll(2)` on AmigaOS will fail to compile. `solo` recognizes `SOLO_IS_AMIGA` and keeps network multiplexing directly on the native BSD `select()` engine.

---

## 4. MS-DOS & WATT-32 Packet Drivers

In 16-bit and 32-bit MS-DOS:
* Network cards use **Packet Drivers** (Class 1 Ethernet interface, hooked onto interrupt vectors between `0x60` and `0x7F`).
* There is no preemptive multitasking in the OS; background packet processing occurs during BIOS timer ticks (`INT 08h` / `INT 1Ch`) and serial UART interrupts.
* **WATT-32 Integration:** `solo` links with Waterloo TCP/IP (WATT-32). In non-blocking loops, `select_s` regularly calls `dbug_yield()` to allow DOS packet drivers to service the NIC ring buffers.

---

## 5. Linux Landlock LSM vs OpenBSD Pledge

Kernel-level sandboxing models differ radically across operating systems:

| Feature | Linux Landlock | OpenBSD Pledge / Unveil | FreeBSD Capsicum |
| :--- | :--- | :--- | :--- |
| **Enforcement Level** | LSM (Linux Security Module) | Kernel Syscall Filters | Capability Mode |
| **Minimum OS Version** | Linux 5.13 (2021) | OpenBSD 5.9 (Pledge) / 6.4 (Unveil) | FreeBSD 10.0 |
| **Model** | Filesystem Path Restriction | Syscall Allowlist + Path Restriction | File Descriptor Rights |
| **Inheritance** | Strictly inherited by `fork()` and `clone()` | Inherited by `fork()`, dropped by `execve()` | Inherited across descriptors |

`solo` bridges these paradigms into a uniform API: `unveil()` defines allowed filesystem scopes, and `lock()` seals the process.
