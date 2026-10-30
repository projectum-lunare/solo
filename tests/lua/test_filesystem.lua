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
describe("solo.filesystem (Filesystem Abstraction & fs Alias)", function()
    local fs = solo.fs

    -- 1. Table alias parity
    test("exposes identical solo.filesystem and solo.fs alias", function()
        assert_true(solo.filesystem ~= nil)
        assert_true(solo.fs ~= nil)
        assert_eq(solo.filesystem, solo.fs)
        assert_eq(type(solo.filesystem.exists), "function")
        assert_eq(type(solo.filesystem.stat), "function")
        assert_eq(type(solo.filesystem.read), "function")
        assert_eq(type(solo.filesystem.write), "function")
        assert_eq(type(solo.filesystem.open), "function")
    end)

    -- 2. Existence verification
    test("checks existence of existing directories and files", function()
        assert_true(fs.exists("."))
        assert_true(fs.exists("include/solo/solo.h"))
        assert_false(fs.exists("/nonexistent_xyz_998877"))
        assert_false(fs.exists("nonexistent_path_xyz_123.bin"))
        assert_false(fs.exists(""))
        assert_false(fs.exists(nil))
    end)

    -- 3. Path Algebra (solo.fs.path)
    test("path manipulation and decomposition", function()
        assert_true(fs.path ~= nil)
        assert_true(fs.path.separator == "/" or fs.path.separator == "\\")

        -- Join
        local joined = fs.path.join("build", "nested", "file.txt")
        assert_true(joined:find("build") ~= nil)
        assert_true(joined:find("nested") ~= nil)
        assert_true(joined:find("file.txt") ~= nil)

        -- Basename & Dirname
        assert_eq(fs.path.basename("dir/sub/item.txt"), "item.txt")
        assert_true(fs.path.dirname("dir/sub/item.txt"):find("sub") ~= nil)

        -- Extension
        assert_eq(fs.path.ext("archive.tar.gz"), ".gz")
        assert_eq(fs.path.extension("test.lua"), ".lua")
        assert_eq(fs.path.ext("no_ext"), "")

        -- Normalize
        local norm = fs.path.normalize("a//b/./c/../d")
        assert_true(norm:find("c") == nil)
        assert_true(norm:find("d") ~= nil)

        -- Absolute check
        assert_true(fs.path.is_absolute("/usr/bin"))
        assert_false(fs.path.is_absolute("local/dir"))
    end)

    -- 4. Stat and metadata inspection
    test("retrieves metadata via stat and boolean predicates", function()
        local info, err = fs.stat("include/solo/solo.h")
        assert_true(info ~= nil)
        assert_eq(info.type, "file")
        assert_true(info.size > 0)
        assert_true(info.mtime > 0)

        assert_true(fs.is_file("include/solo/solo.h"))
        assert_false(fs.is_dir("include/solo/solo.h"))

        local dir_info = fs.stat("src")
        assert_true(dir_info ~= nil)
        assert_eq(dir_info.type, "dir")
        assert_true(fs.is_dir("src"))
        assert_false(fs.is_file("src"))

        local sz = fs.file_size("include/solo/solo.h")
        assert_true(type(sz) == "number")
        assert_true(sz > 0)

        local bad, bad_err = fs.stat("/nonexistent_stat_xyz_123")
        assert_eq(bad, nil)
        assert_true(bad_err ~= nil)
    end)

    -- 5. Whole-File I/O (Slurp / Spit / Append)
    test("reads, writes, and appends whole files", function()
        local test_file = "build/test_lua_io.txt"
        local ok, err = fs.write(test_file, "Line 1\n")
        assert_true(ok)

        ok, err = fs.append(test_file, "Line 2\n")
        assert_true(ok)

        local content, read_err = fs.read(test_file)
        assert_true(content ~= nil)
        assert_eq(content, "Line 1\nLine 2\n")

        fs.remove(test_file)
        assert_false(fs.exists(test_file))

        local missing, missing_err = fs.read("/nonexistent_read_xyz.txt")
        assert_eq(missing, nil)
        assert_true(missing_err ~= nil)
    end)

    -- 6. Directory and file mutation (mkdir, copy, rename, remove)
    test("creates directories, copies, renames and removes recursively", function()
        local dir_path = "build/test_lua_tree/sub/deep"
        assert_true(fs.mkdir(dir_path, true))
        assert_true(fs.is_dir(dir_path))

        local f1 = fs.path.join(dir_path, "file1.txt")
        assert_true(fs.write(f1, "payload 1"))

        local f2 = fs.path.join(dir_path, "file2.txt")
        assert_true(fs.copy(f1, f2))
        assert_true(fs.exists(f2))

        local f3 = fs.path.join(dir_path, "file3.txt")
        assert_true(fs.rename(f2, f3))
        assert_false(fs.exists(f2))
        assert_true(fs.exists(f3))

        -- Recursive remove
        assert_true(fs.remove("build/test_lua_tree", true))
        assert_false(fs.exists("build/test_lua_tree"))
    end)

    -- 7. Directory streaming iteration and listing
    test("iterates directory entries and produces list table", function()
        local count = 0
        local found_solo_h = false

        for entry in fs.dir("include/solo") do
            count = count + 1
            assert_true(entry.name ~= nil)
            assert_true(entry.type ~= nil)
            if entry.name == "solo.h" then
                found_solo_h = true
                assert_eq(entry.type, "file")
            end
        end

        assert_true(count > 0)
        assert_true(found_solo_h)

        local list = fs.list("include/solo")
        assert_true(type(list) == "table")
        assert_true(#list > 0)
    end)

    -- 8. File Streams & Lua 5.4 to-be-closed variable support
    test("streams data through file handles with seek and tell", function()
        local stream_path = "build/test_lua_stream.bin"
        local f, err = fs.open(stream_path, "w+")
        assert_true(f ~= nil)

        local written = f:write("1234567890")
        assert_eq(written, 10)
        assert_eq(f:tell(), 10)

        local new_pos = f:seek("set", 4)
        assert_eq(new_pos, 4)

        local chunk = f:read(3)
        assert_eq(chunk, "567")

        assert_true(f:flush())
        assert_true(f:close())

        -- Test <close> attribute compatibility
        do
            local stream <close> = assert(fs.open(stream_path, "r"))
            local whole = stream:read()
            assert_eq(whole, "1234567890")
        end -- automatically closed

        fs.remove(stream_path)
    end)

    -- 9. Environment & Enums
    test("exposes environment helpers and enum tables", function()
        local cwd = fs.cwd()
        assert_true(type(cwd) == "string")
        assert_true(#cwd > 0)

        local tmp = fs.temp_dir()
        assert_true(type(tmp) == "string")
        assert_true(#tmp > 0)

        -- Enums
        assert_true(fs.type ~= nil)
        assert_eq(fs.type.FILE, 1)
        assert_eq(fs.type.DIR, 2)
        assert_eq(fs.type.SYMLINK, 3)

        assert_true(fs.status ~= nil)
        assert_eq(fs.status.OK, 0)
        assert_eq(fs.status.ERROR, -1)
    end)

    -- 10. Independent submodule resolution
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
