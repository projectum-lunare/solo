# Serial Port (UART) (`solo.serial`)

The `serial` subsystem provides complete RS-232 UART communication, hardware flow control (RTS/CTS), software flow control (XON/XOFF), modem line monitoring, and hardware buffer flushing.

---

## 1. Constants & Enumerations

### Baud Rates (`solo_serial_baud_t`)

| C Constant | Lua Field (`solo.serial.baud`) | Integer Baud |
| :--- | :--- | :---: |
| `SOLO_SERIAL_BAUD_300` | `B300` | `300` |
| `SOLO_SERIAL_BAUD_1200` | `B1200` | `1200` |
| `SOLO_SERIAL_BAUD_2400` | `B2400` | `2400` |
| `SOLO_SERIAL_BAUD_4800` | `B4800` | `4800` |
| `SOLO_SERIAL_BAUD_9600` | `B9600` | `9600` |
| `SOLO_SERIAL_BAUD_19200` | `B19200` | `19200` |
| `SOLO_SERIAL_BAUD_38400` | `B38400` | `38400` |
| `SOLO_SERIAL_BAUD_57600` | `B57600` | `57600` |
| `SOLO_SERIAL_BAUD_115200` | `B115200` | `115200` |

### Data Bits (`solo_serial_data_bits_t`)

| C Constant | Lua Field (`solo.serial.data_bits`) | Bit Count |
| :--- | :--- | :---: |
| `SOLO_SERIAL_DATA_5` | `D5` | `5` |
| `SOLO_SERIAL_DATA_6` | `D6` | `6` |
| `SOLO_SERIAL_DATA_7` | `D7` | `7` |
| `SOLO_SERIAL_DATA_8` | `D8` | `8` |

### Parity (`solo_serial_parity_t`)

| C Constant | Lua Field (`solo.serial.parity`) | Character / String |
| :--- | :--- | :---: |
| `SOLO_SERIAL_PARITY_NONE` | `NONE` | `'n'` / `"none"` |
| `SOLO_SERIAL_PARITY_EVEN` | `EVEN` | `'e'` / `"even"` |
| `SOLO_SERIAL_PARITY_ODD` | `ODD` | `'o'` / `"odd"` |
| `SOLO_SERIAL_PARITY_MARK` | `MARK` | `'m'` / `"mark"` |
| `SOLO_SERIAL_PARITY_SPACE` | `SPACE` | `'s'` / `"space"` |

### Stop Bits (`solo_serial_stop_bits_t`)

| C Constant | Lua Field (`solo.serial.stop_bits`) | Count |
| :--- | :--- | :---: |
| `SOLO_SERIAL_STOP_1` | `S1` | `1` |
| `SOLO_SERIAL_STOP_2` | `S2` | `2` |

### Flow Control (`solo_serial_flow_t`)

| C Constant | Lua Field (`solo.serial.flow`) | Mode Description |
| :--- | :--- | :--- |
| `SOLO_SERIAL_FLOW_NONE` | `NONE` (Default) | No flow control. Safe default preventing cable lockup. |
| `SOLO_SERIAL_FLOW_HARDWARE` | `HARDWARE` | Hardware RTS/CTS handshake pins. |
| `SOLO_SERIAL_FLOW_SOFTWARE` | `SOFTWARE` | Software in-band XON/XOFF flow control. |

### Signal Line Status (`solo_line_state_t`)

| C Constant | Lua Field (`solo.serial.line`) | Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_LINE_LOW` | `LOW` | `0` | Physical line is DEASSERTED / LOW |
| `SOLO_LINE_HIGH` | `HIGH` | `1` | Physical line is ASSERTED / HIGH |
| `SOLO_LINE_ERROR` | `ERROR` | `-1` | Hardware fault, pin unsupported, or invalid handle |

---

## 2. C89 Function Reference

### `solo_serial_open`
```c
solo_serial_t *solo_serial_open(
    const char *path, 
    const solo_serial_opts_t *opts, 
    char *err_buf, 
    size_t err_cap
);
```
Opens a physical UART or virtual COM device.
* **`solo_serial_opts_t` Fields:**
  ```c
  typedef struct {
      int baud;         /* solo_serial_baud_t (default: 9600) */
      int data_bits;    /* solo_serial_data_bits_t (5, 6, 7, 8) */
      char parity;      /* solo_serial_parity_t ('n', 'e', 'o', 'm', 's') */
      int stop_bits;    /* solo_serial_stop_bits_t (1, 2) */
      int flow_control; /* solo_serial_flow_t (0: none, 1: hardware, 2: software) */
      int rts;          /* solo_serial_line_t (-1: ignore, 0: off, 1: on) */
      int dtr;          /* solo_serial_line_t (-1: ignore, 0: off, 1: on) */
      int timeout_ms;   /* 0 = non-blocking, >0 = timeout ms, <0 = indefinite */
  } solo_serial_opts_t;
  ```
* **Returns:** Active handle to `solo_serial_t`, or `NULL` on failure.

---

### `solo_serial_write`
```c
solo_status_t solo_serial_write(
    solo_serial_t *port, 
    const char *buf, 
    size_t len, 
    size_t *written_bytes
);
```
Writes bytes to the serial transmitter queue.
* **Returns:**
  * `SOLO_OK`: Bytes queued successfully (*written_bytes populated).
  * `SOLO_AGAIN`: Timeout expired before any bytes could be accepted.
  * `SOLO_ERROR`: I/O error or disconnected device.

---

### `solo_serial_read`
```c
solo_status_t solo_serial_read(
    solo_serial_t *port, 
    char *buf, 
    size_t max_len, 
    size_t *read_bytes, 
    int timeout_ms
);
```
Reads available bytes from receiver FIFO.
* **Parameters:**
  * `timeout_ms`: If `< 0`, uses port default timeout. If `0`, non-blocking check. If `> 0`, overrides timeout for this call.
* **Returns:** `SOLO_OK` on data, `SOLO_AGAIN` on timeout, `SOLO_ERROR` on failure.

---

### `solo_serial_drain`
```c
solo_serial_status_t solo_serial_drain(solo_serial_t *port);
```
Blocks execution until all pending bytes in the hardware transmitter FIFO and UART shift register have physically been emitted onto the wire.

---

### Modem Status & Control Functions

```c
/* Control output lines */
solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state);
solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state);

/* Query input lines (returns SOLO_LINE_HIGH, SOLO_LINE_LOW, or SOLO_LINE_ERROR) */
solo_line_state_t solo_serial_get_cts(solo_serial_t *port);
solo_line_state_t solo_serial_get_dsr(solo_serial_t *port);
solo_line_state_t solo_serial_get_cd(solo_serial_t *port);
solo_line_state_t solo_serial_get_ri(solo_serial_t *port);
```

---

## 3. Lua API Reference

### Opening Port

```lua
local serial = require("solo.serial")

-- Options Table:
local port, err = serial.open("/dev/ttyUSB0", {
    baud = serial.baud.B115200,
    data_bits = 8,
    parity = "none",
    stop_bits = 1,
    flow = "none",
    timeout = 1000
})
```

### Port Methods

* `port:read([max_len, timeout_ms])`: Reads up to `max_len` bytes. Returns `data_str` or `nil, err`.
* `port:write(str)`: Writes bytes. Returns `written_count` or `nil, err`.
* `port:drain()`: Flushes physical hardware FIFO. Returns `true` or `nil, err`.
* `port:set_timeout(timeout_ms)`: Sets default timeout.
* `port:set_rts(bool)` / `port:set_dtr(bool)`: Asserts/deasserts RTS/DTR output lines.
* `port:get_cts()`, `port:get_dsr()`, `port:get_cd()`, `port:get_ri()`: Returns `serial.line.HIGH` or `serial.line.LOW`.
* `port:close()`: Closes device node immediately.
