# Retro Stacks (MS-DOS, AmigaOS, Atari ST)

Systems like MS-DOS, Commodore Amiga, and Atari ST are **not POSIX**. They rely on vintage hardware architectures and non-POSIX C runtimes.

---

## 1. MS-DOS (16-bit Real Mode & 32-bit Protected Mode)

- **Compiler:** Open Watcom C/C++ or DJGPP.
- **Serial Driver:** Direct 8250 / 16550 UART port I/O (`inportb`/`outportb`) or BIOS `INT 14h`.
- **Parallel Driver:** BIOS `INT 17h` and direct strobe port flipping (`0x378`, `0x278`).
- **Networking:** WATT-32 TCP/IP stack (`select_s`).

---

## 2. AmigaOS (Motorola 68000..68060 / AROS)

- **Compiler:** `m68k-amigaos-gcc` or `vbcc`.
- **Filesystem:** AmigaDOS `Lock()`, `Examine()`, `ExNext()`.
- **Serial Driver:** Native Amiga `serial.device` through `CreateExtIO` and `DoIO`.
- **Parallel Driver:** Native Amiga `parallel.device`.
- **Networking:** `bsdsocket.library` (AmiTCP, Miami, Genesis, Roadshow).

---

## 3. Atari ST (TOS / MiNT)

- **Compiler:** `m68k-atari-mint-gcc`.
- **Filesystem:** GEMDOS `Fsfirst()` and `Fsnext()`.
- **Serial Driver:** MFP 68901 UART direct register mapping.
- **Networking:** MiNTNet BSD socket abstraction.
