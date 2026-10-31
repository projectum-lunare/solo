# Parallel Port (IEEE 1284) (`solo.parallel`)

The `parallel` subsystem provides low-level control of Centronics and IEEE 1284 parallel printer ports (LPT).

---

## Port Opening and Data Transmission

=== "C (C89)"

    ```c
    #include <stdio.h>
    #include <solo/solo.h>

    void print_receipt(void) {
        char err_buf[128];
        solo_parallel_t *lpt = solo_parallel_open("/dev/lp0", SOLO_PARALLEL_MODE_SPP, err_buf, sizeof(err_buf));
        size_t written = 0;

        if (!lpt) {
            fprintf(stderr, "Parallel port error: %s\n", err_buf);
            return;
        }

        solo_parallel_write(lpt, "ESC @ Hello Printer!\n", 21, &written);
        solo_parallel_drain(lpt);
        solo_parallel_close(lpt);
    }
    ```

=== "Lua"

    ```lua
    local parallel = require("solo.parallel")

    local lpt, err = parallel.open("/dev/lp0", {
        mode = parallel.mode.SPP
    })

    if not lpt then
        error("LPT Open failed: " .. tostring(err))
    end

    lpt:write("Test Page Header\n")
    lpt:drain()
    lpt:close()
    ```

---

## Real-Time Hardware Pin Status

The driver monitors physical Centronics status pins:
- **BUSY:** Printer processing buffer
- **ACK:** Acknowledge pulse
- **PE:** Paper Empty / Paper Out
- **SLCT:** Printer Selected / Online
- **FAULT:** Hardware error condition

```c
solo_line_state_t busy = solo_parallel_get_pin(lpt, SOLO_PARALLEL_PIN_BUSY);
if (busy == SOLO_LINE_HIGH) {
    /* Peripheral is busy */
}
```

In Lua:
```lua
if lpt:is_busy() then
    print("Printer is currently busy...")
end
```
