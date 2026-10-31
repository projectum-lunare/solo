# Design Philosophy

`solo` was created to solve a fundamental problem in systems software: modern systems abstraction libraries have bloated dependencies and systematically drop support for historical and embedded architectures, while vintage libraries lack clean modern paradigms (such as kernel-level sandboxing, RAII-like resource ownership, and dynamic event multiplexing).

---

## 1. Zero Assumptions About Compiler Modernity

The public surface of `solo` compiles cleanly under **ISO C90 / ANSI C89**. This guarantees that code linking against `libsolo` can be compiled on:
- GCC 2.7.2 on retro AmigaOS or Linux 2.0
- MSVC 4.0 or Open Watcom 1.9 on Windows NT 3.51 or MS-DOS
- Clang 18 or GCC 14 with `-Wall -Wextra -Werror` on modern Linux

---

## 2. Zero Heap Allocation on Common Paths

Memory allocation in kernel space, interrupt contexts, or high-throughput network daemons introduces latency jitter and potential out-of-memory panics.

Whenever feasible, `solo` avoids heap allocation on common execution paths:
- **Multiplexing:** The `solo_socket_poll` and `solo.socket.select` implementations maintain internal stack scratch buffers (`stack_pfds[64]`). Dynamic `malloc()` is only invoked if a process monitors more than 64 concurrent descriptors.
- **Diagnostics:** Error reporting uses fixed-capacity caller-provided buffers (`char *err_buf, size_t err_cap`) rather than dynamic string formatting.

---

## 3. Separation of Public C89 and Internal GNU89 Drivers

Platform drivers often require assembly instructions, compiler intrinsics (`__inline__`, `__asm__`), memory barrier instructions, or low-level struct alignments.

- **Public Surface (`include/solo/*.h`):** 100% pure ANSI C89.
- **Internal Drivers (`src/*/*_driver.c`):** Compiled under `gnu89`, permitting controlled access to inline assembly (`INT 14h`, `INT 17h`, Motorola 68000 registers) without polluting consumer code.

---

## 4. Preservation of Retro Semantics

Systems like MS-DOS, AmigaOS, and Atari ST are **not POSIX**. `solo` respects their native architecture:
- AmigaOS uses `bsdsocket.library` and `serial.device`.
- MS-DOS uses WATT-32 `select_s` and direct 8250 UART registers.
- Atari ST uses TOS MFP 68901 and MiNTNet.

`solo` provides identical high-level contracts while internally mapping to the genuine hardware/OS mechanisms of each platform.
