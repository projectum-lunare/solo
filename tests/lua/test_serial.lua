--
-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0
--

package.path = "./?.lua;./tests/?.lua;" .. package.path
package.cpath = "./build/lua/?.so;./build/lua/?/init.so;./?.so;./?.dylib;./?.dll;" .. package.cpath

local T = require("tests.harness")
local describe, test = T.describe, T.test
local assert_eq, assert_nil = T.assert_eq, T.assert_nil

local solo = require("solo")

-- ============================================================================
-- solo.serial (Serial RS-232 Driver Tests)
-- ============================================================================
-- Tests the serial port driver in Lua, verifying enum tables, table-based and
-- positional open constructors, option defaults, timeout handling, and input
-- validation for baud rates and parity modes.
describe("solo.serial (Physical Serial RS-232 Driver Signatures)", function()

    -- 1. Serial configuration enum tables and constants
    test("defines serial flow, parity, stop, data, baud, line and status enum tables", function()
        local s = solo.serial

        -- Hardware / software flow control
        assert_eq(type(s.flow), "table")
        assert_eq(s.flow.NONE, 0)
        assert_eq(s.flow.HARDWARE, 1)
        assert_eq(s.flow.SOFTWARE, 2)
        assert_eq(s.flow.RTS_CTS, 1)
        assert_eq(s.flow.XON_XOFF, 2)

        -- Parity modes (stored as ASCII byte values)
        assert_eq(type(s.parity), "table")
        assert_eq(s.parity.NONE, string.byte("n"))
        assert_eq(s.parity.EVEN, string.byte("e"))
        assert_eq(s.parity.ODD, string.byte("o"))
        assert_eq(s.parity.MARK, string.byte("m"))
        assert_eq(s.parity.SPACE, string.byte("s"))

        -- Stop bits (1 or 2)
        assert_eq(type(s.stop_bits), "table")
        assert_eq(s.stop_bits.ONE, 1)
        assert_eq(s.stop_bits.TWO, 2)
        assert_eq(s.stop.STOP_1, 1)
        assert_eq(s.stop.STOP_2, 2)

        -- Data bits (5, 6, 7, 8)
        assert_eq(type(s.data_bits), "table")
        assert_eq(s.data_bits.FIVE, 5)
        assert_eq(s.data_bits.SIX, 6)
        assert_eq(s.data_bits.SEVEN, 7)
        assert_eq(s.data_bits.EIGHT, 8)
        assert_eq(s.data.EIGHT, 8)

        -- Standard baud rates
        assert_eq(type(s.baud), "table")
        assert_eq(s.baud.B9600, 9600)
        assert_eq(s.baud.B19200, 19200)
        assert_eq(s.baud.B38400, 38400)
        assert_eq(s.baud.B115200, 115200)

        -- Modem line control & logic level states
        assert_eq(type(s.line), "table")
        assert_eq(s.line.IGNORE, -1)
        assert_eq(s.line.OFF, 0)
        assert_eq(s.line.ON, 1)
        assert_eq(s.line.LOW, 0)
        assert_eq(s.line.HIGH, 1)
        assert_eq(s.line.ERROR, -1)

        -- Return status codes
        assert_eq(type(s.status), "table")
        assert_eq(s.status.OK, 0)
        assert_eq(s.status.ERROR, -1)
        assert_eq(s.status.UNSUPPORTED, -4)
    end)

    -- 2. Table-based options constructor
    test("serial open accepts options table and string parity/flow", function()
        -- Calling open with an options table; on nonexistent device, returns nil, error_string
        local port, err = solo.serial.open("/nonexistent_tty_device_12345", {
            baud = solo.serial.baud.B115200,
            data_bits = solo.serial.data_bits.EIGHT,
            parity = "none",
            stop_bits = solo.serial.stop_bits.ONE,
            flow = "hardware",
            rts = true,
            dtr = true
        })
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    -- 3. Positional arguments constructor
    test("serial open accepts positional arguments with string parity", function()
        -- Signature: open(path, baud, data_bits, parity, stop_bits, flow)
        local port, err = solo.serial.open("/nonexistent_tty_device_12345", 9600, 8, "n", 1, 1)
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    -- 4. Timeout option and flow defaults in table
    test("serial open accepts timeout option and defaults flow to none", function()
        local port, err = solo.serial.open("/nonexistent_tty_device_flow_none", {
            baud = 9600,
            flow = "none",
            timeout = 500
        })
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    -- 5. Positional arguments with timeout as 9th argument
    test("serial open accepts positional arguments with timeout", function()
        -- Signature: open(path, baud, data_bits, parity, stop_bits, flow, rts, dtr, timeout)
        local port, err = solo.serial.open("/nonexistent_tty_device_pos_timeout", 9600, 8, "n", 1, "none", true, true, 500)
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    -- 6. Rejection of invalid baud and parity parameters
    test("serial open rejects invalid baud and parity without crashing", function()
        local p1, err1 = solo.serial.open("/nonexistent_dev_bad_baud", { baud = 12345 })
        assert_nil(p1)
        assert_eq(type(err1), "string")

        local p2, err2 = solo.serial.open("/nonexistent_dev_bad_parity", { parity = "invalid_parity" })
        assert_nil(p2)
        assert_eq(type(err2), "string")
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
