# Exotic Kernels (KolibriOS, OS/2, Plan 9)

`solo` extends portability beyond mainstream operating systems to include hobbyist, research, and enterprise legacy platforms.

---

## 1. KolibriOS (MenuetOS Derivative)

- **Architecture:** 32-bit x86 written entirely in assembly.
- **Compiler:** `kos32-gcc`.
- **Drivers:**
  - Parallel & Serial: KolibriOS kernel Syscall 46 (Hardware I/O port direct access).
  - Networking: Syscall 75 (KolibriOS network stack).

---

## 2. IBM OS/2 & ArcaOS

- **Compiler:** Open Watcom OS/2 or GCC via EMX.
- **Drivers:**
  - Parallel: `DosOpen` on `LPT1` with `DosDevIOCtl`.
  - Serial: `DosDevIOCtl` (Category 1, asynchronous serial communications).
  - Networking: IBM TCP/IP Sockets subsystem.

---

## 3. Plan 9 from Bell Labs

- **Architecture:** Synthetic filesystem architecture where everything is a file.
- **Drivers:**
  - Serial: `/dev/eia0` and `/dev/eia0ctl`.
  - Parallel: `/dev/lp`.
  - Networking: Plan 9 native `/net/tcp` and `/net/udp` synthetic file trees.
