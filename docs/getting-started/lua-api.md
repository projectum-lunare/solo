# Lua API Usage

`solo` provides idiomatic Lua bindings with object-oriented wrappers, options tables, and automatic resource reclamation.

---

## Requiring Modules

You can load the umbrella module or individual submodules:

```lua
-- Load complete library
local solo = require("solo")

-- Or load lightweight, standalone submodules
local fs     = require("solo.fs")         -- or solo.filesystem
local socket = require("solo.socket")
local serial = require("solo.serial")
```

---

## Idiomatic Return Conventions

Functions in `solo` follow the standard Lua convention for fallible calls:
- **On Success:** Returns result object(s) or `true`.
- **On Failure:** Returns `nil`, followed by an error string (e.g. `nil, "connection refused"` or `nil, "timeout"`).

```lua
local sock, err = solo.socket.connect("127.0.0.1", 9000)
if not sock then
    print("Failed to connect: " .. tostring(err))
    return
end
```

---

## Object-Oriented Methods and Cleanup

All native handles (`sock`, `port`, `file`) expose object methods and automatic garbage collection (`__gc`):

```lua
local sock = solo.socket.connect("127.0.0.1", 8080)

-- Explicit close:
sock:close()

-- Or let Lua's garbage collector automatically reclaim the file descriptor
-- without memory leaks or descriptor exhaustion.
```

---

## Flexible Parameterization

Constructors accept either concise options tables or classic positional arguments:

=== "Options Table (Recommended)"

    ```lua
    local port = solo.serial.open("/dev/ttyUSB0", {
        baud      = solo.serial.baud.B115200,
        data_bits = 8,
        parity    = "none",
        stop_bits = 1,
        flow      = "none",
        timeout   = 1000
    })
    ```

=== "Positional Arguments"

    ```lua
    local port = solo.serial.open("/dev/ttyUSB0", 115200, 8, "N", 1, 1000)
    ```
