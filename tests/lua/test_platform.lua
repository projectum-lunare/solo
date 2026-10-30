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
local assert_eq, assert_true, assert_false, assert_nil = T.assert_eq, T.assert_true, T.assert_false, T.assert_nil

local solo = require("solo")

-- ============================================================================
-- solo.platform (System Architecture & Metadata Tests)
-- ============================================================================
-- Validates the runtime platform introspection table in Lua.
-- solo.platform exposes static metadata detecting the operating system,
-- CPU architecture, machine word bitness, and endianness.
describe("solo.platform (System Architecture & Metadata)", function()
    local p = solo.platform

    -- 1. Operating system and CPU architecture identification
    test("exposes valid OS and CPU architecture identifiers", function()
        -- p.os returns canonical lowercase OS name: "linux", "dos", "amigaos", etc.
        assert_eq(type(p.os), "string")
        assert_true(#p.os > 0)

        -- p.arch returns CPU architecture: "x86_64", "x86", "m68k", "arm", etc.
        assert_eq(type(p.arch), "string")
        assert_true(#p.arch > 0)
    end)

    -- 2. Machine word size and bitness flags
    test("detects word size and bitness flags", function()
        -- p.bits must be a number equal to 16, 32, or 64
        assert_eq(type(p.bits), "number")
        assert_true(p.bits == 16 or p.bits == 32 or p.bits == 64)

        -- Convenience boolean flags for bitness
        assert_eq(type(p.is_64bit), "boolean")
        assert_eq(type(p.is_32bit), "boolean")
        assert_eq(type(p.is_16bit), "boolean")
    end)

    -- 3. Memory endianness detection
    test("detects endianness and enforces mutual exclusivity", function()
        -- p.endian must be either "little" or "big"
        assert_true(p.endian == "little" or p.endian == "big")

        -- Boolean flags must be boolean and strictly mutually exclusive
        assert_eq(type(p.is_little_endian), "boolean")
        assert_eq(type(p.is_big_endian), "boolean")
        assert_true(p.is_little_endian ~= p.is_big_endian)
    end)

    -- 4. Target operating system boolean flags
    test("exposes valid target operating system boolean flags", function()
        assert_eq(type(p.is_linux), "boolean")
        assert_eq(type(p.is_darwin), "boolean")
        assert_eq(type(p.is_bsd), "boolean")
        assert_eq(type(p.is_haiku), "boolean")

        -- At least one target environment family flag must be true
        assert_true(p.is_posix or p.is_windows or p.is_amiga or p.is_plan9 or p.is_kolibri or p.is_dos or p.is_os2 or p.is_atarist)
    end)

    -- 5. Strict modular separation
    test("platform does not have filesystem methods", function()
        -- Filesystem routines belong exclusively to solo.fs / solo.filesystem
        assert_nil(p.exists)
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
