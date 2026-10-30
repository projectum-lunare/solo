--
-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0
--

-- Configure search paths for modules and tests
package.path = "./?.lua;./tests/?.lua;" .. package.path
package.cpath = "./build/lua/?.so;./build/lua/?/init.so;./?.so;./?.dylib;./?.dll;" .. package.cpath

local harness = require("tests.harness")

io.write("==> Running Solo Lua Test Suite...\n")

-- Modular Lua test suites under tests/lua/
local suites = {
    "tests.lua.test_platform",
    "tests.lua.test_filesystem",
    "tests.lua.test_sandbox",
    "tests.lua.test_serial",
    "tests.lua.test_parallel",
    "tests.lua.test_socket",
    "tests.lua.test_submodules",
}

local filter = arg and arg[1]
for _, suite_name in ipairs(suites) do
    if not filter or suite_name:find(filter, 1, true) then
        local ok, err = pcall(require, suite_name)
        if not ok then
            io.stderr:write(string.format("Failed to load test suite %s: %s\n", suite_name, tostring(err)))
            os.exit(1)
        end
    end
end

local code = harness.summary()
os.exit(code)
