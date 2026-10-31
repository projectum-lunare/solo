# Canonical Type System

All subsystems of `solo` are unified by the canonical type system declared in `include/solo/types.h`.

---

## 1. Status Codes (`solo_status_t`)

All fallible non-constructor operations return `solo_status_t`:

```c
typedef enum solo_status {
    SOLO_OK          =  0,  /* Success */
    SOLO_ERROR       = -1,  /* General failure */
    SOLO_AGAIN       = -2,  /* Non-blocking operation would block */
    SOLO_EOF         = -3,  /* End of file / stream reached */
    SOLO_UNSUPPORTED = -4   /* Operation unsupported by host kernel */
} solo_status_t;
```

---

## 2. Canonical Booleans (`solo_bool_t`)

C89 lacks a native `bool` type. Many C libraries resort to returning ambiguous `int` values (where `0` might mean success or false depending on the function).

`solo` enforces an explicit canonical boolean enum:

```c
typedef enum solo_bool {
    SOLO_FALSE = 0,
    SOLO_TRUE  = 1
} solo_bool_t;
```

Functions returning predicates (e.g. `solo_fs_exists`, `solo_platform_is_64bit`, `solo_platform_is_little_endian`) always return `solo_bool_t`.

---

## 3. Physical Signal Line State (`solo_line_state_t`)

Modem and serial UART line queries (such as CTS, DSR, CD, RI) do not return raw integers or booleans, because hardware status lines can be in a high voltage state, low voltage state, or an unreadable error state (e.g., disconnected cable or hardware fault):

```c
typedef enum solo_line_state {
    SOLO_LINE_LOW   = 0,   /* Physical line is LOW / DEASSERTED */
    SOLO_LINE_HIGH  = 1,   /* Physical line is HIGH / ASSERTED */
    SOLO_LINE_ERROR = -1   /* Hardware fault or unsupported pin */
} solo_line_state_t;
```

Example usage:
```c
solo_line_state_t cts = solo_serial_get_cts(port);
if (cts == SOLO_LINE_HIGH) {
    /* Safe to transmit hardware-flowed data */
} else if (cts == SOLO_LINE_LOW) {
    /* Remote device buffer full */
} else {
    /* Driver error or disconnected UART */
}
```
