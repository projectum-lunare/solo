-- solo - Physical I/O, System Foundations & Hardware Abstractions
-- Projectum Lunare <https://github.com/projectum-lunare/solo>
-- Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
-- Mozilla Public License Version 2.0

package.cpath = "./?.so;./?.dylib;./?.dll;" .. package.cpath

local solo = require("solo")

print("========================================")
print(" solo Lua Test Suite")
print(" Version: " .. solo.version())
print("========================================")

-- 1. Test Platform (Metadata only)
print("\n[1] Testing solo.platform...")
local p = solo.platform
assert(type(p.os) == "string", "platform.os must be string")
assert(type(p.arch) == "string", "platform.arch must be string")
assert(type(p.bits) == "number", "platform.bits must be number")
assert(type(p.endian) == "string", "platform.endian must be string")
print(string.format("    Detected: OS='%s', Arch='%s', Bitness=%d, Endian='%s'", p.os, p.arch, p.bits, p.endian))
assert(p.exists == nil, "platform must not have filesystem methods")
print("    ✓ platform passed!")

-- 2. Test Filesystem & fs alias
print("\n[2] Testing solo.filesystem & solo.fs alias...")
assert(solo.filesystem ~= nil, "solo.filesystem must exist")
assert(solo.fs ~= nil, "solo.fs alias must exist")
assert(solo.filesystem == solo.fs, "solo.fs must be identical to solo.filesystem")
assert(type(solo.filesystem.exists) == "function", "filesystem.exists must be a function")
assert(solo.filesystem.exists(".") == true, "Current directory must exist")
assert(solo.fs.exists(".") == true, "solo.fs.exists must work")
assert(solo.fs.exists("/nonexistent_xyz_998877") == false, "Nonexistent file must return false")

local req_fs = require("solo.fs")
assert(req_fs == solo.fs, "require('solo.fs') must resolve correctly")
local req_filesystem = require("solo.filesystem")
assert(req_filesystem == solo.filesystem, "require('solo.filesystem') must resolve correctly")
print("    ✓ filesystem & fs alias passed!")

-- 3. Test Sandbox
print("\n[3] Testing solo.sandbox...")
local s = solo.sandbox
local backend, version = s.backend()
print(string.format("    Active sandbox backend: '%s' (version %s)", backend, tostring(version)))
assert(type(backend) == "string", "backend must be a string")
assert(type(s.is_supported) == "function", "is_supported must be a function")
print("    ✓ sandbox passed!")

-- 4. Test Parallel & Serial API availability
print("\n[4] Testing solo.parallel & solo.serial APIs...")
assert(type(solo.parallel.open) == "function", "parallel.open must exist")
assert(type(solo.serial.open) == "function", "serial.open must exist")
print("    ✓ parallel and serial signatures verified!")

-- 5. Test Socket (Loopback socket communication)
print("\n[5] Testing solo.socket loopback...")
local sock = solo.socket
local port = 19935
local server, err = sock.bind("127.0.0.1", port, 5)
if not server then
    port = 19936
    server, err = sock.bind("127.0.0.1", port, 5)
end
assert(server, "Failed to bind server: " .. tostring(err))

local client, err_c = sock.connect("127.0.0.1", port)
assert(client, "Failed to connect client: " .. tostring(err_c))

local accepted, err_a = server:accept()
assert(accepted, "Failed to accept client: " .. tostring(err_a))

local test_msg = "Hello from solo.socket!"
local written = client:write(test_msg)
assert(written == #test_msg, "Written bytes mismatch")

local received = accepted:read(128)
assert(received == test_msg, "Received message mismatch: " .. tostring(received))

client:close()
accepted:close()
server:close()
print("    ✓ socket loopback data exchange passed!")

print("\n========================================")
print(">>> ALL LUA TESTS PASSED SUCCESSFULLY! <<<")
print("========================================")
