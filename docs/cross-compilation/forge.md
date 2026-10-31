# Cross-Compilation with Forge

`solo` integrates [Forge](https://github.com/projectum-lunare/forge) to automate cross-compiling the entire matrix of vintage and exotic targets without manual toolchain configuration.

---

## Listing Available Toolchains

```bash
xmake forge list
```

Forge inspects local compilers, SDK paths, and emulators (wibo, DOSBox, UAE).

---

## Building Specific Platform Targets

```bash
# Build for modern POSIX (Linux x86_64 / arm64)
xmake forge posix

# Build for MS-DOS (Open Watcom + WATT-32)
xmake forge dos

# Build for IBM OS/2 (Open Watcom OS/2)
xmake forge os2

# Build for Windows CE / Pocket PC (Microsoft eVC 3.0 ARM via wibo)
xmake forge wince

# Build for Commodore Amiga (m68k-amigaos-gcc)
xmake forge amiga

# Build for Atari ST (m68k-atari-mint-gcc)
xmake forge atari

# Build for KolibriOS (kos32-gcc)
xmake forge kolibri
```

---

## Building the Complete Matrix

To build static libraries for all installed platform toolchains in a single command:

```bash
xmake forge all
```

Output static libraries will be organized in:
```
build/
├── posix/libsolo.a
├── dos/libsolo.a
├── os2/libsolo.a
├── windows_ce/arm/libsolo.a
├── amiga/libsolo.a
├── atarist/libsolo.a
└── kolibri/libsolo.a
```
