# Building & Compiling

`solo` uses [xmake](https://xmake.io) and [Forge](https://github.com/projectum-lunare/forge) as its cross-platform build engine.

---

## Prerequisites

- **xmake** (v2.8.0 or newer)
- Standard C compiler (GCC, Clang, MSVC, MinGW, or Open Watcom)
- *(Optional)* Lua (5.1, 5.2, 5.3, 5.4, or LuaJIT) if building Lua bindings

---

## Standard Build

Clone the repository and run:

```bash
# Build static library, shared library, and Lua bindings
xmake

# Run the comprehensive C API test suite
xmake test

# Run the Lua automated test suite
xmake run test_lua

# Run the standalone smoke executable
xmake run solo_smoke
```

---

## Modular Targets

If your target system has severe storage or memory limitations (e.g. vintage machines or embedded microcontrollers), you can build individual submodules independently:

```bash
# Build only the C static library:
xmake build solo_static

# Build only standalone Lua submodules:
xmake build solo.platform
xmake build solo.serial
xmake build solo.socket
xmake build solo.fs
xmake build solo.parallel
xmake build solo.sandbox
```

---

## Compile-Time Exclusion Flags

When embedding `solo` directly into proprietary build systems or embedded firmware, unwanted subsystems can be stripped completely via preprocessor defines:

| Preprocessor Flag | Excluded Subsystem |
| :--- | :--- |
| `-DSOLO_NO_SOCKET` | Excludes BSD Sockets, Winsock, and networking code. |
| `-DSOLO_NO_SERIAL` | Excludes RS-232 UART driver and modem controls. |
| `-DSOLO_NO_PARALLEL` | Excludes Centronics parallel port and strobe timing. |
| `-DSOLO_NO_FILESYSTEM` | Excludes file manipulation, directory iterators, and stats. |
| `-DSOLO_NO_SANDBOX` | Excludes Landlock, Pledge, and Capsicum kernel sandboxing. |
| `-DSOLO_NO_PLATFORM` | Excludes OS and CPU architecture detection metadata. |

---

## Installing via LuaRocks

`solo` includes a native Rockspec:

```bash
luarocks make solo-scm-1.rockspec
```
