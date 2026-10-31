# Recipe: Centronics Impact Printing & Hardware Handshaking

Centronics parallel ports (IEEE 1284) communicate with impact matrix printers (e.g. Epson FX-80, LX-300) and POS receipt printers using active-low strobe lines and hardware status handshaking.

This recipe demonstrates how to check printer status flags (`BUSY`, `PE`, `FAULT`), send ESC/P formatting commands, and flush hardware buffers using `solo.parallel`.

---

## C89 Implementation

```c
#include <stdio.h>
#include <solo/solo.h>

int main(void) {
    char err[128];
    solo_parallel_t *lpt;
    int pins = 0;
    size_t written = 0;

    /* Open parallel port */
    lpt = solo_parallel_open("/dev/lp0", err, sizeof(err));
    if (!lpt) {
        /* On Windows, use "LPT1" */
        lpt = solo_parallel_open("LPT1", err, sizeof(err));
    }
    if (!lpt) {
        fprintf(stderr, "Failed to open parallel port: %s\n", err);
        return 1;
    }

    /* Query hardware status pins */
    if (solo_parallel_get_status(lpt, &pins) == SOLO_PARALLEL_OK) {
        if (pins & SOLO_PARALLEL_PIN_PE) {
            fprintf(stderr, "Error: Printer out of paper (PE set)!\n");
            solo_parallel_close(lpt);
            return 1;
        }
        if (pins & SOLO_PARALLEL_PIN_FAULT) {
            fprintf(stderr, "Error: Printer hardware fault!\n");
            solo_parallel_close(lpt);
            return 1;
        }
        if (!(pins & SOLO_PARALLEL_PIN_SLCT)) {
            fprintf(stderr, "Warning: Printer is OFFLINE.\n");
        }
    }

    printf("Printer online and ready. Sending print job...\n");

    /* ESC @ = Initialize printer; ESC E = Bold on; ESC F = Bold off */
    solo_parallel_write(lpt, "\x1B@", 2, &written);
    solo_parallel_write(lpt, "\x1BE*** SOLO SYSTEM REPORT ***\x1BF\n", 31, &written);
    solo_parallel_write(lpt, "Operating System: Multi-Platform Kernel\n", 40, &written);
    solo_parallel_write(lpt, "Hardware Port:    IEEE 1284 Centronics\n\n\n\n", 42, &written);

    /* Form Feed (page eject) */
    solo_parallel_write(lpt, "\x0C", 1, &written);

    /* Wait for physical printing mechanism to complete */
    printf("Waiting for printer buffer to drain...\n");
    solo_parallel_drain(lpt);

    solo_parallel_close(lpt);
    printf("Print job completed successfully.\n");
    return 0;
}
```

---

## Lua Implementation

```lua
local parallel = require("solo.parallel")

local lpt, err = parallel.open("/dev/lp0", {
    mode = parallel.mode.SPP
})

if not lpt then
    -- Fallback for Windows
    lpt, err = parallel.open("LPT1", { mode = parallel.mode.SPP })
end

if not lpt then
    error("Could not open parallel port: " .. tostring(err))
end

-- Validate status
if lpt:is_paper_empty() then
    error("Printer has no paper!")
end

if lpt:is_fault() then
    error("Printer hardware error reported!")
end

print("Printer is ready. Transmitting payload...")

-- ESC/P Commands:
lpt:write("\x1B@")                        -- Init printer
lpt:write("\x1BE--- POS RECEIPT ---\x1BF\n") -- Bold title
lpt:write("Item: Widget Pro       $19.99\n")
lpt:write("Tax (8%):              $ 1.60\n")
lpt:write("Total:                $21.59\n\n\n")
lpt:write("\x1Bm")                        -- Partial cut (on supported ESC/POS units)

lpt:drain()
lpt:close()
print("Receipt printed successfully.")
```
