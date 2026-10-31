# Platform Detection (`solo.platform`)

The `platform` subsystem provides compile-time and runtime introspection of the host operating system, CPU architecture, bitness, and endianness.

---

## Introspection Properties

=== "C (C89)"

    ```c
    #include <stdio.h>
    #include <solo/solo.h>

    void print_environment(void) {
        printf("Operating System: %s\n", solo_platform_os());
        printf("Architecture:     %s\n", solo_platform_arch());
        printf("Word Size (bits): %d\n", solo_platform_bits());

        if (solo_platform_is_64bit() == SOLO_TRUE) {
            printf("Target is 64-bit.\n");
        }
        if (solo_platform_is_little_endian() == SOLO_TRUE) {
            printf("Target is Little-Endian.\n");
        } else {
            printf("Target is Big-Endian.\n");
        }
    }
    ```

=== "Lua"

    ```lua
    local platform = require("solo.platform")

    print("Operating System: " .. platform.os)
    print("Architecture:     " .. platform.arch)
    print("Word Size (bits): " .. platform.bits)
    print("Is 64-bit:        " .. tostring(platform.is_64bit))
    print("Is Little Endian: " .. tostring(platform.is_little_endian))

    -- Platform Family Boolean Flags:
    if platform.is_linux then
        print("Running on Linux")
    elseif platform.is_windows then
        print("Running on Windows")
    elseif platform.is_dos then
        print("Running on MS-DOS")
    elseif platform.is_amiga then
        print("Running on AmigaOS")
    end
    ```

---

## Operating System String Identifiers

`solo_platform_os()` / `platform.os` returns one of the canonical identifiers:
- `"linux"`
- `"freebsd"`, `"openbsd"`, `"netbsd"`
- `"darwin"` (macOS, iOS)
- `"windows"`
- `"windows_ce"`
- `"dos"`
- `"amigaos"`
- `"atarist"`
- `"kolibrios"`
- `"os2"`
- `"plan9"`

---

## Architecture String Identifiers

`solo_platform_arch()` / `platform.arch` returns:
- `"x86_64"`, `"x86"` (i386..i686)
- `"arm64"`, `"arm"`
- `"m68k"` (Motorola 68000..68060)
- `"riscv64"`, `"riscv32"`
- `"ppc64"`, `"ppc"`
- `"mips"`
