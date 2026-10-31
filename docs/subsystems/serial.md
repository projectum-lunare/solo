# Serial Port (UART) (`solo.serial`)

The `serial` subsystem provides complete RS-232 UART communication, hardware flow control, modem line management, and buffer draining.

---

## Configuration & Opening

=== "C (C89)"

    ```c
    #include <stdio.h>
    #include <solo/solo.h>

    void open_modem(void) {
        char err_buf[128];
        solo_serial_t *port = solo_serial_open(
            "/dev/ttyUSB0",
            SOLO_SERIAL_BAUD_115200,
            SOLO_SERIAL_DATA_8,
            SOLO_SERIAL_PARITY_NONE,
            SOLO_SERIAL_STOP_1,
            SOLO_SERIAL_FLOW_NONE,
            1000, /* 1000 ms timeout */
            err_buf,
            sizeof(err_buf)
        );

        if (!port) {
            fprintf(stderr, "Failed to open serial port: %s\n", err_buf);
            return;
        }

        solo_serial_write(port, "ATZ\r\n", 5, NULL);
        solo_serial_drain(port);
        solo_serial_close(port);
    }
    ```

=== "Lua"

    ```lua
    local serial = require("solo.serial")

    local port, err = serial.open("/dev/ttyUSB0", {
        baud      = serial.baud.B115200,
        data_bits = 8,
        parity    = "none",
        stop_bits = 1,
        flow      = "none",
        timeout   = 1000
    })

    if not port then
        error("Open failed: " .. tostring(err))
    end

    port:write("ATZ\r\n")
    port:drain()
    local response = port:read(128)
    print("Modem response: " .. tostring(response))
    port:close()
    ```

---

## Safe Flow Control Defaults

Hardware flow control (`RTS/CTS`) defaults to **NONE (`SOLO_SERIAL_FLOW_NONE`)**. 
This is an intentional design choice: enabling hardware RTS/CTS by default causes write calls to hang indefinitely on 3-wire null-modem cables, microcontroller breakout boards, and USB UART adapters where CTS is left disconnected.

---

## Modem Control Lines & Semantic Logic State

Control lines are queried with typed `solo_line_state_t` returns:

```c
solo_line_state_t cts = solo_serial_get_cts(port);
if (cts == SOLO_LINE_HIGH) {
    /* CTS is asserted */
}
```

In Lua:
```lua
if port:get_cts() == serial.line.HIGH then
    -- CTS is asserted
end
```

Available status pin methods:
- `set_rts(state)` / `set_dtr(state)`
- `get_cts()`, `get_dsr()`, `get_cd()`, `get_ri()`
