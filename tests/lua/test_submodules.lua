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
local assert_eq = T.assert_eq

local solo = require("solo")

-- ============================================================================
-- solo standalone submodules (Independent C Module Loading Tests)
-- ============================================================================
-- Verifies that each solo C submodule (platform, serial, socket, fs, parallel,
-- sandbox) can be independently required as a standalone shared object (.so)
-- without relying on the root 'solo' monolithic module initialization.
describe("solo standalone submodules (Independent C Module Loading)", function()

    -- 1. Independent solo.platform loading
    test("loads solo.platform independently", function()
        local p = require("solo.platform")
        assert_eq(type(p.os), "string")
        assert_eq(p.os, solo.platform.os)
    end)

    -- 2. Independent solo.serial loading
    test("loads solo.serial independently", function()
        local s = require("solo.serial")
        assert_eq(type(s.open), "function")
        assert_eq(s.flow.HARDWARE, 1)
        assert_eq(s.parity.NONE, string.byte("n"))
    end)

    -- 3. Independent solo.socket loading
    test("loads solo.socket independently", function()
        local sock = require("solo.socket")
        assert_eq(type(sock.connect), "function")
    end)

    -- 4. Independent solo.fs loading
    test("loads solo.fs independently", function()
        local fs = require("solo.fs")
        assert_eq(type(fs.exists), "function")
    end)

    -- 5. Independent solo.parallel loading
    test("loads solo.parallel independently", function()
        local par = require("solo.parallel")
        assert_eq(type(par.open), "function")
        assert_eq(par.mode.SPP, 0)
        assert_eq(par.pin.BUSY, 1)
    end)

    -- 6. Independent solo.sandbox loading
    test("loads solo.sandbox independently", function()
        local sbx = require("solo.sandbox")
        assert_eq(type(sbx.is_supported), "function")
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
