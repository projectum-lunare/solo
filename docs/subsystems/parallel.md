# Parallel Port (IEEE 1284) (`solo.parallel`)

The `parallel` subsystem provides low-level control of Centronics and IEEE 1284 parallel printer ports (LPT), supporting impact printers, receipt printers, and custom embedded hardware.

---

## 1. Constants & Enumerations

### Operational Modes (`solo_parallel_mode_t`)

| C Constant | Lua Field (`solo.parallel.mode`) | Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_PARALLEL_MODE_SPP` | `SPP` | `0` | Standard Parallel Port (Centronics compatibility mode, forward data only) |
| `SOLO_PARALLEL_MODE_NIBBLE` | `NIBBLE` | `1` | IEEE 1284 4-bit reverse read channel |
| `SOLO_PARALLEL_MODE_BYTE` | `BYTE` | `2` | IEEE 1284 8-bit bidirectional channel (PS/2 port mode) |
| `SOLO_PARALLEL_MODE_EPP` | `EPP` | `3` | Enhanced Parallel Port (high-speed bus protocol) |
| `SOLO_PARALLEL_MODE_ECP` | `ECP` | `4` | Extended Capabilities Port (hardware FIFO and DMA transfers) |

### Status Handshake Pins (`solo_parallel_pin_t`)

| C Constant | Lua Field (`solo.parallel.pin`) | Bitmask Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_PARALLEL_PIN_BUSY` | `BUSY` | `1 << 0` (`0x01`) | Device is processing data / buffer full |
| `SOLO_PARALLEL_PIN_ACK` | `ACK` | `1 << 1` (`0x02`) | Acknowledge pulse (data byte received) |
| `SOLO_PARALLEL_PIN_PE` | `PE` | `1 << 2` (`0x04`) | Paper End / Paper Out |
| `SOLO_PARALLEL_PIN_SLCT` | `SLCT` | `1 << 3` (`0x08`) | Select / Device Online |
| `SOLO_PARALLEL_PIN_FAULT` | `FAULT` | `1 << 4` (`0x10`) | Hardware Error / Printer Fault |

---

## 2. C89 Function Reference

### `solo_parallel_open`
```c
solo_parallel_t *solo_parallel_open(
    const char *path, 
    char *err_buf, 
    size_t err_cap
);
```
Opens the parallel port. Typical device paths:
* **Linux:** `"/dev/lp0"`, `"/dev/parport0"`
* **BSD:** `"/dev/lpt0"`
* **Windows / DOS:** `"LPT1"`, `"LPT2"`, `"PRN"`
* **Amiga:** `"parallel.device"`
* **Returns:** Pointer to `solo_parallel_t`, or `NULL` on failure.

---

### `solo_parallel_write`
```c
solo_status_t solo_parallel_write(
    solo_parallel_t *port, 
    const char *buf, 
    size_t len, 
    size_t *written_bytes
);
```
Transmits raw bytes over the 8 data lines ($D_0..D_7$) with hardware strobe pulse generation.

---

### `solo_parallel_get_status`
```c
solo_parallel_status_t solo_parallel_get_status(
    solo_parallel_t *port, 
    int *pins_out
);
```
Reads physical status pins in real time. Returns a bitmask combining `SOLO_PARALLEL_PIN_*` flags.

---

### `solo_parallel_drain`
```c
solo_parallel_status_t solo_parallel_drain(solo_parallel_t *port);
```
Actively monitors the `BUSY` pin until the peripheral finishes processing its internal buffer.

---

## 3. Lua API Reference

```lua
local parallel = require("solo.parallel")

local lpt = parallel.open("/dev/lp0", {
    mode = parallel.mode.SPP
})

-- Pin Queries:
local pins = lpt:get_status()
if lpt:is_busy() then
    print("Printer is busy")
end

-- Stream Data:
lpt:write("ESC @\n")
lpt:drain()
lpt:close()
```
