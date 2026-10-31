# Platform Detection (`solo.platform`)

The `platform` subsystem provides compile-time and runtime introspection of operating system families, CPU architectures, bitness, and byte order.

---

## 1. Introspection Functions

### Host Metadata
```c
const char *solo_platform_os(void);     /* Returns OS identifier string */
const char *solo_platform_arch(void);   /* Returns CPU architecture string */
int         solo_platform_bits(void);   /* Returns 16, 32, or 64 */
```

### Canonical Boolean Predicates
```c
solo_bool_t solo_platform_is_64bit(void);
solo_bool_t solo_platform_is_little_endian(void);
solo_bool_t solo_platform_is_big_endian(void);
```

---

## 2. Platform Family Flags

| C Macro Flag | Lua Property | Supported OS Targets |
| :--- | :--- | :--- |
| `SOLO_IS_POSIX` | `platform.is_posix` | Linux, FreeBSD, OpenBSD, NetBSD, macOS, Haiku, Solaris |
| `SOLO_IS_LINUX` | `platform.is_linux` | Linux distributions (glibc, musl, uClibc, Android) |
| `SOLO_IS_BSD` | `platform.is_bsd` | FreeBSD, OpenBSD, NetBSD, DragonFly BSD |
| `SOLO_IS_DARWIN` | `platform.is_darwin` | macOS, iOS, iPadOS |
| `SOLO_IS_WINDOWS` | `platform.is_windows` | Windows NT (3.51..11, Server), WinCE / Pocket PC |
| `SOLO_IS_DOS` | `platform.is_dos` | MS-DOS, FreeDOS, PC-DOS (16/32-bit via Watcom/DJGPP) |
| `SOLO_IS_AMIGA` | `platform.is_amiga` | AmigaOS (Classic m68k, AROS, MorphOS) |
| `SOLO_IS_ATARIST` | `platform.is_atarist`| Atari ST / TT / Falcon (TOS, FreeMiNT) |
| `SOLO_IS_KOLIBRI` | `platform.is_kolibri`| KolibriOS (32-bit x86 assembly OS) |
| `SOLO_IS_OS2` | `platform.is_os2` | IBM OS/2 Warp 3/4, ArcaOS, eComStation |
| `SOLO_IS_PLAN9` | `platform.is_plan9` | Plan 9 from Bell Labs, 9front |

---

## 3. Lua API Reference

```lua
local platform = require("solo.platform")

print("OS:        " .. platform.os)
print("Arch:      " .. platform.arch)
print("Word size: " .. platform.bits)

if platform.is_64bit then
    print("Host has 64-bit pointers and registers")
end

if platform.is_little_endian then
    print("Host byte order is Little Endian (x86, ARM, RISC-V)")
else
    print("Host byte order is Big Endian (m68k, SPARC, PPC)")
end
```
