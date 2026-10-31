# Windows (NT 3.51..11, CE)

`solo` maintains runtime binary and source compatibility across 30 years of Windows versions, from **Windows NT 3.51 (1995)** to modern **Windows 11 (2026)** and **Windows CE 3.0 / Pocket PC**.

---

## The Vintage NT 3.51 PE Compatibility Guarantee

Calling functions introduced in modern Windows directly (such as `WSAPoll`, introduced in Vista) causes a fatal error during binary loading on Windows NT 3.51, 4.0, 2000, and XP:

```
"The procedure entry point WSAPoll could not be located in the dynamic link library WS2_32.DLL"
```

To ensure `solo` binaries boot and run flawlessly on vintage NT:
1. `WSAPoll` is never statically linked.
2. The runtime engine calls `GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSAPoll")`.
3. If `WSAPoll` is not present, `solo` transparently routes polling requests to `solo_poll_via_select()`.

---

## Winsock `FD_SETSIZE` Expansion

Unlike POSIX (where `fd_set` is a fixed-size bitmask and redefining `FD_SETSIZE` causes heap/stack corruption), Winsock defines `fd_set` as an array of `SOCKET` handles:

```c
typedef struct fd_set {
    u_int   fd_count;
    SOCKET  fd_array[FD_SETSIZE];
} fd_set;
```

`solo` defines `#define FD_SETSIZE 1024` before including `<winsock2.h>`. This allows the `select()` engine on Windows NT 3.51, 4.0, and 2000 to safely monitor up to 1024 concurrent sockets with zero memory safety issues.
