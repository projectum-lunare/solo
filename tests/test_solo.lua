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

-- 5. Test Sockets (Enums, TCP & UDP, Sync & Async, Multiplexing)
print("\n[5] Testing solo.socket...")
local sock = solo.socket

-- 5.1 Verify Enums
print("    Testing Enums...")
assert(sock.type ~= nil, "sock.type must exist")
assert(sock.type.TCP == 1, "type.TCP must be 1")
assert(sock.type.UDP == 2, "type.UDP must be 2")
assert(sock.TCP == 1, "sock.TCP direct constant must be 1")
assert(sock.UDP == 2, "sock.UDP direct constant must be 2")

assert(sock.mode ~= nil, "sock.mode must exist")
assert(sock.mode.BLOCKING == 0, "mode.BLOCKING must be 0")
assert(sock.mode.NONBLOCKING == 1, "mode.NONBLOCKING must be 1")

assert(sock.status ~= nil, "sock.status must exist")
assert(sock.status.OK == 0, "status.OK must be 0")
assert(sock.status.ERROR == -1, "status.ERROR must be -1")
assert(sock.status.AGAIN == -2, "status.AGAIN must be -2")
assert(sock.status.CLOSED == -3, "status.CLOSED must be -3")

assert(sock.event ~= nil, "sock.event must exist")
assert(sock.event.READ == 1, "event.READ must be 1")
assert(sock.event.WRITE == 2, "event.WRITE must be 2")
assert(sock.event.ERR == 4, "event.ERR must be 4")
print("    ✓ Enums verified!")

-- 5.2 Test TCP Synchronous loopback
print("    Testing TCP Synchronous loopback...")
local tcp_port = 19935
local server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
if not server then
    tcp_port = 19936
    server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
end
assert(server, "Failed to bind server: " .. tostring(err))
assert(server:type() == "tcp", "server:type() must be 'tcp'")
assert(server:is_blocking() == true, "server must be blocking")

local client, err_c = sock.connect("127.0.0.1", tcp_port, sock.mode.BLOCKING)
assert(client, "Failed to connect client: " .. tostring(err_c))

local accepted, ip, port = server:accept()
assert(accepted, "Failed to accept client")
assert(type(ip) == "string", "client ip must be string")
assert(type(port) == "number", "client port must be number")

local test_msg = "Hello TCP Synchronous!"
local written = client:send(test_msg)
assert(written == #test_msg, "Written bytes mismatch")

local received = accepted:recv(128)
assert(received == test_msg, "Received message mismatch: " .. tostring(received))

client:close()
accepted:close()
server:close()
print("    ✓ TCP Synchronous loopback passed!")

-- 5.3 Test TCP Asynchronous (Non-blocking) loopback
print("    Testing TCP Asynchronous (Non-blocking)...")
local async_tcp_port = 19945
local async_server, a_err = sock.bind("127.0.0.1", async_tcp_port, 5, sock.mode.NONBLOCKING)
if not async_server then
    async_tcp_port = 19946
    async_server, a_err = sock.bind("127.0.0.1", async_tcp_port, 5, sock.mode.NONBLOCKING)
end
assert(async_server, "Failed to bind async server: " .. tostring(a_err))
assert(async_server:is_blocking() == false, "server must be non-blocking")

-- Non-blocking accept on idle server must return nil, "again"
local non_client, non_err = async_server:accept()
assert(non_client == nil, "accept must be nil on idle non-blocking server")
assert(non_err == "again", "accept error must be 'again'")

-- Connect client
local async_client = sock.connect("127.0.0.1", async_tcp_port)
assert(async_client, "Failed to connect to async server")

-- Server wait for read readiness
local ready = async_server:wait("r", 1000)
assert(ready == "r" or ready == "rw", "async_server must be ready to accept")

local client_sock = async_server:accept()
assert(client_sock, "Failed to accept client on ready server")
client_sock:set_mode("nonblocking")
assert(client_sock:is_blocking() == false, "accepted client must be non-blocking")

-- Read on empty socket must return nil, "again"
local empty_data, empty_err = client_sock:recv(128)
assert(empty_data == nil, "recv on empty nonblocking socket must be nil")
assert(empty_err == "again", "recv error must be 'again'")

-- Send data from client
local async_msg = "Hello TCP Asynchronous!"
async_client:send(async_msg)

-- Wait for read readiness
assert(client_sock:wait(sock.event.READ, 1000) ~= nil, "client_sock must be readable")
local async_recv = client_sock:recv(128)
assert(async_recv == async_msg, "Received async data mismatch")

async_client:close()
client_sock:close()
async_server:close()
print("    ✓ TCP Asynchronous loopback passed!")

-- 5.4 Test UDP Synchronous
print("    Testing UDP Synchronous datagram...")
local udp_port = 19955
local udp_server, u_err = sock.udp_bind("127.0.0.1", udp_port)
if not udp_server then
    udp_port = 19956
    udp_server, u_err = sock.udp_bind("127.0.0.1", udp_port)
end
assert(udp_server, "Failed to bind UDP server: " .. tostring(u_err))
assert(udp_server:type() == "udp", "udp_server:type() must be 'udp'")

local udp_client = sock.udp()
assert(udp_client, "Failed to create UDP client")
assert(udp_client:type() == "udp", "udp_client:type() must be 'udp'")

local udp_msg = "Hello UDP Datagram!"
local sent = udp_client:sendto(udp_msg, "127.0.0.1", udp_port)
assert(sent == #udp_msg, "Sent bytes mismatch")

local udp_data, src_ip, src_port = udp_server:recvfrom(128)
assert(udp_data == udp_msg, "Received UDP datagram mismatch: " .. tostring(udp_data))
assert(type(src_ip) == "string" and #src_ip > 0, "src_ip must be valid string")
assert(type(src_port) == "number" and src_port > 0, "src_port must be valid port number")

-- Reply back to client
local udp_reply = "Ack UDP Datagram!"
udp_server:sendto(udp_reply, src_ip, src_port)
local reply_data = udp_client:recvfrom(128)
assert(reply_data == udp_reply, "UDP client reply mismatch")

udp_client:close()
udp_server:close()
print("    ✓ UDP Synchronous datagram passed!")

-- 5.5 Test UDP Asynchronous (Non-blocking) & Multiplexing (select)
print("    Testing UDP Asynchronous & select multiplexing...")
local async_udp_port = 19965
local async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
if not async_udp then
    async_udp_port = 19966
    async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
end
assert(async_udp, "Failed to bind async UDP server")
assert(async_udp:is_blocking() == false, "UDP server must be non-blocking")

-- recvfrom on empty nonblocking UDP socket must return nil, "again"
local d_none, err_none = async_udp:recvfrom(128)
assert(d_none == nil, "recvfrom must return nil on empty async UDP")
assert(err_none == "again", "recvfrom error must be 'again'")

-- wait timeout check
local wait_res, wait_err = async_udp:wait("r", 50)
assert(wait_res == nil and wait_err == "timeout", "wait must timeout")

-- select check on idle socket
local ready_r, ready_w = sock.select({ async_udp }, nil, 50)
assert(#ready_r == 0, "select must return empty table on timeout")

-- Send datagram
local u_cli = sock.udp()
local async_udp_msg = "Async UDP Multiplexing Payload"
u_cli:sendto(async_udp_msg, "127.0.0.1", async_udp_port)

-- select should now identify async_udp as ready
ready_r, ready_w = sock.select({ async_udp }, nil, 1000)
assert(#ready_r == 1 and ready_r[1] == async_udp, "select must detect ready UDP reader")

local recvd_async_udp = async_udp:recvfrom(128)
assert(recvd_async_udp == async_udp_msg, "Async UDP payload mismatch")

u_cli:close()
async_udp:close()
print("    ✓ UDP Asynchronous & select passed!")

-- 5.6 Test Socket Timeout
print("    Testing Socket Timeouts...")
local to_port = 19975
local to_server = sock.bind("127.0.0.1", to_port, 5)
if not to_server then
    to_port = 19976
    to_server = sock.bind("127.0.0.1", to_port, 5)
end
local to_client = sock.connect("127.0.0.1", to_port)
local to_acc = to_server:accept()

to_acc:set_timeout(100) -- 100ms timeout
local to_data, to_err = to_acc:recv(64)
assert(to_data == nil, "recv on timed-out socket must be nil")
assert(to_err == "again", "recv error on timeout must be 'again'")

to_client:close()
to_acc:close()
to_server:close()
print("    ✓ Socket Timeouts passed!")

print("\n========================================")
print(">>> ALL LUA TESTS PASSED SUCCESSFULLY! <<<")
print("========================================")
