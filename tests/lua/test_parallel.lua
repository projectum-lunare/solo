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
-- solo.parallel (IEEE 1284 / SPP Parallel Port Driver Tests)
-- ============================================================================
-- Tests the parallel port driver in Lua, verifying open constructors, status
-- codes, port modes, status pin bitmasks, and options parsing.
describe("solo.parallel (Physical Parallel IEEE 1284 Driver Signatures)", function()

    -- 1. Constructor verification
    test("exposes open constructors for parallel and serial drivers", function()
        assert_eq(type(solo.parallel.open), "function")
        assert_eq(type(solo.serial.open), "function")
    end)

    -- 2. Parallel enum tables
    test("defines parallel status, mode, pin and port enum tables", function()
        local p = solo.parallel

        -- Return status codes
        assert_eq(type(p.status), "table")
        assert_eq(p.status.OK, 0)
        assert_eq(p.status.ERROR, -1)
        assert_eq(p.status.UNSUPPORTED, -2)

        -- IEEE 1284 / SPP operating modes
        assert_eq(type(p.mode), "table")
        assert_eq(p.mode.SPP, 0)
        assert_eq(p.mode.CENTRONICS, 0)
        assert_eq(p.mode.NIBBLE, 1)
        assert_eq(p.mode.BYTE, 2)
        assert_eq(p.mode.EPP, 3)
        assert_eq(p.mode.ECP, 4)

        -- Hardware status pin bitmasks
        assert_eq(type(p.pin), "table")
        assert_eq(p.pin.BUSY, 1)
        assert_eq(p.pin.ACK, 2)
        assert_eq(p.pin.PE, 4)
        assert_eq(p.pin.PAPER_OUT, 4)
        assert_eq(p.pin.SLCT, 8)
        assert_eq(p.pin.ONLINE, 8)
        assert_eq(p.pin.FAULT, 16)

        -- Legacy port name constants
        assert_eq(type(p.port), "table")
        assert_eq(p.port.LPT1, "LPT1")
        assert_eq(p.port.LPT2, "LPT2")
        assert_eq(p.port.LPT3, "LPT3")
    end)

    -- 3. Options table or string path constructor
    test("parallel open accepts options table or string path", function()
        -- Attempting to open a nonexistent device returns nil, error_string
        local port, err = solo.parallel.open({ path = "/nonexistent_lpt_device_12345" })
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    -- 4. Operating mode option
    test("parallel open accepts mode option", function()
        local port, err = solo.parallel.open({
            path = "/nonexistent_lpt_mode",
            mode = solo.parallel.mode.BYTE
        })
        assert_nil(port)
        assert_eq(type(err), "string")
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
