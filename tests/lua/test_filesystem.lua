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
local assert_eq, assert_true, assert_false = T.assert_eq, T.assert_true, T.assert_false

local solo = require("solo")

-- ============================================================================
-- solo.filesystem (Filesystem Abstraction & fs Alias Tests)
-- ============================================================================
-- Tests the portable filesystem functions and verifies that solo.filesystem and
-- its short alias solo.fs point to the exact same table instance.
describe("solo.filesystem (Filesystem Abstraction & fs Alias)", function()

    -- 1. Table alias parity
    test("exposes identical solo.filesystem and solo.fs alias", function()
        assert_true(solo.filesystem ~= nil)
        assert_true(solo.fs ~= nil)

        -- solo.fs and solo.filesystem must reference the identical table object
        assert_eq(solo.filesystem, solo.fs)
        assert_eq(type(solo.filesystem.exists), "function")
    end)

    -- 2. Existence verification of real files and directories
    test("checks existence of existing directories and files", function()
        -- Current directory "." must exist
        assert_true(solo.filesystem.exists("."))
        assert_true(solo.fs.exists("."))

        -- Known project header file must exist
        assert_true(solo.fs.exists("include/solo/solo.h"))
    end)

    -- 3. Nonexistent paths
    test("returns false for nonexistent absolute and relative paths", function()
        assert_false(solo.fs.exists("/nonexistent_xyz_998877"))
        assert_false(solo.fs.exists("nonexistent_path_xyz_123.bin"))
    end)

    -- 4. Edge cases: empty strings, directories, and deep paths
    test("filesystem exists returns false for empty strings and handles directory paths", function()
        assert_false(solo.fs.exists(""))
        assert_true(solo.fs.exists("src"))
        assert_true(solo.fs.exists("include"))
        assert_true(solo.fs.exists("tests"))
        assert_false(solo.fs.exists("deeply/nested/nonexistent/path/file.txt"))
    end)

    -- 5. Independent submodule resolution
    test("submodules require('solo.fs') and require('solo.filesystem') resolve correctly", function()
        local req_fs = require("solo.fs")
        assert_eq(req_fs, solo.fs)

        local req_filesystem = require("solo.filesystem")
        assert_eq(req_filesystem, solo.filesystem)
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
