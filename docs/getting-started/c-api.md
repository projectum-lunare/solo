# C89 API Usage

The C API of `solo` is engineered for uncompromising portability and deterministic behavior across four decades of compiler technology.

---

## Strict ANSI C89 (ISO C90) Rules

All public headers in `include/solo/*.h` and all client-facing declarations adhere to ISO C90:
- **Variable Declarations:** Always at the top of statement blocks before any executable code.
- **No `long long` assumptions:** Exact 32/64 bitness handled cleanly through typedefs.
- **Strict Pointer Casting:** No implicit void pointer casts or non-standard pointer arithmetic.
- **C++ Safe:** Wrapped with `#ifdef __cplusplus extern "C" { #endif`.

---

## Canonical Return Statuses

Every non-trivial fallible operation in `solo` returns a canonical status code defined in `include/solo/types.h`:

```c
typedef enum solo_status {
    SOLO_OK          =  0,  /* Operation completed successfully */
    SOLO_ERROR       = -1,  /* General unrecoverable error */
    SOLO_AGAIN       = -2,  /* Non-blocking I/O would block (EAGAIN / EWOULDBLOCK) */
    SOLO_EOF         = -3,  /* End of stream or peer closed connection */
    SOLO_UNSUPPORTED = -4   /* Feature not available on current kernel / PAL driver */
} solo_status_t;
```

---

## Canonical Boolean Predicates

To avoid ambiguous C truth values, predicates never return raw untyped integers:

```c
typedef enum solo_bool {
    SOLO_FALSE = 0,
    SOLO_TRUE  = 1
} solo_bool_t;
```

Example usage:
```c
if (solo_fs_exists("/var/log/app.log") == SOLO_TRUE) {
    /* Safe execution path */
}
```

---

## Error Reporting Protocol

Functions taking an error buffer adhere to the following signature pattern:

```c
solo_socket_t *solo_socket_connect(
    const char *host,
    int port,
    solo_socket_mode_t mode,
    char *err_buf,
    size_t err_cap
);
```

1. If the function succeeds, it returns a valid handle and `err_buf` remains untouched (or empty).
2. If the function fails, it returns `NULL` (or `SOLO_ERROR`) and safely writes a human-readable, null-terminated diagnostic message into `err_buf` without exceeding `err_cap`.
