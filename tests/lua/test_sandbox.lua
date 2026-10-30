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
-- solo.sandbox (Kernel Security & Process Confinement Tests)
-- ============================================================================
-- Tests the kernel sandboxing bindings (Landlock LSM on Linux, pledge/unveil on
-- OpenBSD, Capsicum on FreeBSD).
--
-- NOTE ON PROCESS ISOLATION:
-- Sandboxing restrictions are permanent for the calling process and cannot be
-- relaxed once locked. Therefore, live enforcement tests execute in a separate
-- child Lua process via os.execute() so that the test runner's own execution
-- environment remains unrestricted.
describe("solo.sandbox (Kernel Security & Isolation)", function()
    local s = solo.sandbox

    -- 1. Introspection of backend engine and version
    test("exposes backend name and version information", function()
        -- s.backend() returns: backend_name_string, abi_version_number
        local backend, version = s.backend()
        assert_eq(type(backend), "string")
        assert_true(#backend > 0)
        assert_true(version == nil or type(version) == "number")
    end)

    -- 2. API function presence
    test("provides sandbox API function signatures", function()
        assert_eq(type(s.is_supported), "function")
        assert_eq(type(s.unveil), "function")
        assert_eq(type(s.lock), "function")
        assert_eq(type(s.pledge), "function")
    end)

    -- 3. Path validation
    test("unveil on nonexistent path fails visibly without altering sandbox state", function()
        -- Unveiling a path that does not exist must fail gracefully (returning false, err)
        local ok, err, code = s.unveil("/nonexistent_sandbox_path_12345", "r")
        assert_false(ok)
        assert_true(err ~= nil)
    end)

    -- 4. Enum tables and bitmask constants
    test("defines sandbox backend, perm and status enum tables", function()
        -- Backend identifiers
        assert_true(s.backend ~= nil)
        assert_eq(s.backend.NONE, 0)
        assert_eq(s.backend.LANDLOCK, 1)
        assert_eq(s.backend.OPENBSD, 2)
        assert_eq(s.backend.CAPSICUM, 3)

        -- Permission bitmasks for unveil()
        assert_true(s.perm ~= nil)
        assert_eq(s.perm.NONE, 0)
        assert_eq(s.perm.READ, 1)
        assert_eq(s.perm.WRITE, 2)
        assert_eq(s.perm.EXEC, 4)

        -- Return status codes
        assert_true(s.status ~= nil)
        assert_eq(s.status.OK, 0)
        assert_eq(s.status.ERROR, -1)
        assert_eq(s.status.UNSUPPORTED, -4)

        -- Convenience top-level permission aliases
        assert_eq(s.READ, 1)
        assert_eq(s.WRITE, 2)
        assert_eq(s.EXEC, 4)
    end)

    -- 5. Live filesystem confinement in child process
    test("enforces filesystem isolation in child process after unveil and lock", function()
        local backend, ver = s.backend()
        if backend == "landlock" and ver and ver > 0 then
            local lua_bin = os.getenv("LUA_BIN") or "/usr/bin/lua5.5"
            -- Child process unveils "." (read-only) and locks sandbox:
            -- 1. Reading "xmake.lua" (inside ".") must succeed.
            -- 2. Reading "/etc/passwd" (outside ".") must be blocked by the kernel.
            local code = [=[
                local s = require("solo.sandbox")
                assert(s.unveil(".", "r"))
                assert(s.lock())
                local f1 = io.open("xmake.lua", "r")
                if not f1 then os.exit(1) end
                f1:close()
                local f2 = io.open("/etc/passwd", "r")
                if f2 then f2:close() os.exit(2) end
                os.exit(0)
            ]=]
            local cmd = string.format('LUA_CPATH="./build/lua/?.so;./?.so;;" %s -e %q 2>/dev/null', lua_bin, code)
            local res = os.execute(cmd)
            local status = (res == 0 or res == true)
            assert_true(status)
        end
    end)

    -- 6. Total lockdown without unveil
    test("lock without unveil in child process restricts entire filesystem", function()
        local backend, ver = s.backend()
        if backend == "landlock" and ver and ver > 0 then
            local lua_bin = os.getenv("LUA_BIN") or "/usr/bin/lua5.5"
            -- Locking immediately without unveiling grants zero filesystem paths.
            -- Any attempt to open a file must be denied by Landlock.
            local code = [=[
                local s = require("solo.sandbox")
                assert(s.lock())
                local f = io.open("/etc/passwd", "r")
                if f then f:close() os.exit(2) end
                os.exit(0)
            ]=]
            local cmd = string.format('LUA_CPATH="./build/lua/?.so;./?.so;;" %s -e %q 2>/dev/null', lua_bin, code)
            local res = os.execute(cmd)
            local status = (res == 0 or res == true)
            assert_true(status)
        end
    end)

    -- 7. Unsupported primitive handling
    test("unsupported sandbox operations fail visibly with explanatory error", function()
        if solo.platform.is_linux then
            -- On Linux, pledge() is unsupported (pledge is OpenBSD-specific); must return UNSUPPORTED
            local ok, err, code = s.pledge("stdio")
            assert_false(ok)
            assert_eq(type(err), "string")
            assert_true(#err > 0)
            assert_eq(code, s.status.UNSUPPORTED)
        elseif not s.is_supported() then
            -- On platforms without sandbox support, all operations must fail with UNSUPPORTED
            local ok_u, err_u, code_u = s.unveil(".", "r")
            assert_false(ok_u)
            assert_eq(type(err_u), "string")
            assert_eq(code_u, s.status.UNSUPPORTED)

            local ok_p, err_p, code_p = s.pledge("stdio")
            assert_false(ok_p)
            assert_eq(type(err_p), "string")
            assert_eq(code_p, s.status.UNSUPPORTED)

            local ok_l, err_l, code_l = s.lock()
            assert_false(ok_l)
            assert_eq(type(err_l), "string")
            assert_eq(code_l, s.status.UNSUPPORTED)
        end
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
