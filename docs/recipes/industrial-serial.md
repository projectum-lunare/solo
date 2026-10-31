# Recipe: Industrial Serial GPS & Sensor Reader

Industrial instrumentation (such as GPS receivers, electronic scales, and PLC controllers) transmits continuous ASCII records (e.g. NMEA 0183 sentences).

This recipe demonstrates how to configure UART parameters, query hardware Clear-To-Send (`CTS`) lines, and read framed lines with timeout recovery.

---

## C89 Implementation

```c
#include <stdio.h>
#include <string.h>
#include <solo/solo.h>

/* Reads an entire newline-terminated line from the serial port */
static int read_line(solo_serial_t *port, char *out, size_t max_len, int timeout_ms) {
    size_t pos = 0;
    while (pos + 1 < max_len) {
        char ch;
        size_t n = 0;
        solo_status_t st = solo_serial_read(port, &ch, 1, &n, timeout_ms);
        if (st == SOLO_OK && n == 1) {
            if (ch == '\r') continue;
            if (ch == '\n') {
                out[pos] = '\0';
                return 1;
            }
            out[pos++] = ch;
        } else if (st == SOLO_AGAIN) {
            /* Timeout expired */
            return 0;
        } else {
            /* Fatal UART error */
            return -1;
        }
    }
    out[pos] = '\0';
    return 1;
}

int main(void) {
    char err[128];
    solo_serial_opts_t opts;
    solo_serial_t *port;

    memset(&opts, 0, sizeof(opts));
    opts.baud = SOLO_SERIAL_BAUD_9600;
    opts.data_bits = SOLO_SERIAL_DATA_8;
    opts.parity = SOLO_SERIAL_PARITY_NONE;
    opts.stop_bits = SOLO_SERIAL_STOP_1;
    opts.flow_control = SOLO_SERIAL_FLOW_NONE;
    opts.timeout_ms = 2000; /* 2-second timeout */

    port = solo_serial_open("/dev/ttyUSB0", &opts, err, sizeof(err));
    if (!port) {
        fprintf(stderr, "Cannot open serial port: %s\n", err);
        return 1;
    }

    printf("Serial port opened successfully.\n");

    /* Check physical CTS pin before writing */
    if (solo_serial_get_cts(port) == SOLO_LINE_HIGH) {
        printf("[CTS ASSERTED] Remote device ready to receive.\n");
    }

    /* Send probe command and flush transmitter FIFO */
    solo_serial_write(port, "$PMTK000*32\r\n", 13, NULL);
    solo_serial_drain(port);

    /* Read incoming sentences */
    for (int i = 0; i < 5; i++) {
        char line[256];
        int res = read_line(port, line, sizeof(line), 2000);
        if (res > 0) {
            printf("[NMEA] %s\n", line);
        } else if (res == 0) {
            printf("[!] Read timeout expired\n");
        } else {
            printf("[!] Serial I/O error\n");
            break;
        }
    }

    solo_serial_close(port);
    return 0;
}
```

---

## Lua Implementation

```lua
local serial = require("solo.serial")

local port, err = serial.open("/dev/ttyUSB0", {
    baud      = serial.baud.B9600,
    data_bits = 8,
    parity    = "none",
    stop_bits = 1,
    timeout   = 2000
})

if not port then
    error("Failed to open serial device: " .. tostring(err))
end

-- Inspect modem control signals
if port:get_cts() == serial.line.HIGH then
    print("[CTS HIGH] Peripheral ready")
end

-- Send probe and ensure all bytes left the shift register
port:write("$PMTK000*32\r\n")
port:drain()

-- Read incoming data chunks
for i = 1, 5 do
    local chunk, read_err = port:read(128, 2000)
    if chunk then
        print("Received chunk: " .. chunk)
    else
        print("Timeout or read error: " .. tostring(read_err))
    end
end

port:close()
```
