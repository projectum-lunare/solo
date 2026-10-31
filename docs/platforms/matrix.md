# Platform Support Matrix

`solo` targets over ten distinct operating system families, providing verified drivers and compile targets across 16-bit, 32-bit, and 64-bit systems.

---

## Detailed Capability Matrix

| Platform Family | `platform` | `filesystem` | `parallel` | `serial` | `socket` | `sandbox` |
| :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| **Linux (Modern & Embedded)** | ✅ | ✅ POSIX | ✅ `/dev/lp*` | ✅ termios | ✅ BSD / `poll(2)` | ✅ Landlock LSM |
| **OpenBSD / FreeBSD / NetBSD** | ✅ | ✅ POSIX | ✅ `/dev/lpt*` | ✅ termios | ✅ BSD / `poll(2)` | ✅ Pledge / Capsicum |
| **macOS (Darwin)** | ✅ | ✅ POSIX | ⚠️ Stub | ✅ termios | ✅ BSD / `poll(2)` | ⚠️ Stub |
| **Windows NT (10 / 11 / Server)** | ✅ | ✅ Win32 | ✅ Win32 LPT | ✅ Comm API | ✅ `WSAPoll` dynamic | ⚠️ Stub |
| **Windows NT (3.51 / 4.0 / 2000 / XP)** | ✅ | ✅ Win32 | ✅ Win32 LPT | ✅ Comm API | ✅ `select` (1024 fds) | ⚠️ Stub |
| **Windows CE / Pocket PC** | ✅ | ✅ WinCE | ✅ WinCE LPT | ✅ Comm API | ✅ Winsock CE | ⚠️ Stub |
| **MS-DOS (16/32-bit WATT-32)** | ✅ | ✅ DOS io | ✅ `INT 17h` / I/O | ✅ `INT 14h` / 8250 | ✅ WATT-32 BSD | ⚠️ Stub |
| **AmigaOS (m68k / AROS)** | ✅ | ✅ Lock DOS | ✅ `parallel.device` | ✅ `serial.device` | ✅ `bsdsocket.library` | ⚠️ Stub |
| **Atari ST (TOS / MiNT)** | ✅ | ✅ Fsfirst | ✅ Hardware I/O | ✅ MFP 68901 UART | ✅ MiNTNet Sockets | ⚠️ Stub |
| **KolibriOS** | ✅ | ⚠️ Stub | ✅ Syscall 46 / I/O | ✅ Syscall 46 / UART | ✅ Syscall 75 | ⚠️ Stub |
| **OS/2 / ArcaOS** | ✅ | ✅ access | ✅ DosOpen LPT | ✅ DosDevIOCtl | ✅ TCP/IP BSD | ⚠️ Stub |
| **Plan 9 from Bell Labs** | ✅ | ✅ access | ✅ `/dev/lp` | ✅ `/dev/eia*` | ✅ `/net/tcp` | ⚠️ Stub |
