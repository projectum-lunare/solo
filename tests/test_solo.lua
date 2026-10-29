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

describe("solo.platform (System Architecture & Metadata)", function()
    local p = solo.platform

    test("exposes valid OS and CPU architecture identifiers", function()
        assert_eq(type(p.os), "string")
        assert_true(#p.os > 0)
        assert_eq(type(p.arch), "string")
        assert_true(#p.arch > 0)
    end)

    test("detects word size and bitness flags", function()
        assert_eq(type(p.bits), "number")
        assert_true(p.bits == 16 or p.bits == 32 or p.bits == 64)
        assert_eq(type(p.is_64bit), "boolean")
        assert_eq(type(p.is_32bit), "boolean")
        assert_eq(type(p.is_16bit), "boolean")
    end)

    test("detects endianness and enforces mutual exclusivity", function()
        assert_true(p.endian == "little" or p.endian == "big")
        assert_eq(type(p.is_little_endian), "boolean")
        assert_eq(type(p.is_big_endian), "boolean")
        assert_true(p.is_little_endian ~= p.is_big_endian)
    end)

    test("exposes valid target operating system boolean flags", function()
        assert_eq(type(p.is_linux), "boolean")
        assert_eq(type(p.is_darwin), "boolean")
        assert_eq(type(p.is_bsd), "boolean")
        assert_eq(type(p.is_haiku), "boolean")
        assert_true(p.is_posix or p.is_windows or p.is_amiga or p.is_plan9 or p.is_kolibri or p.is_dos or p.is_os2 or p.is_atarist)
    end)

    test("platform does not have filesystem methods", function()
        assert_nil(p.exists)
    end)
end)

describe("solo.filesystem (Filesystem Abstraction & fs Alias)", function()
    test("exposes identical solo.filesystem and solo.fs alias", function()
        assert_true(solo.filesystem ~= nil)
        assert_true(solo.fs ~= nil)
        assert_eq(solo.filesystem, solo.fs)
        assert_eq(type(solo.filesystem.exists), "function")
    end)

    test("checks existence of existing directories and files", function()
        assert_true(solo.filesystem.exists("."))
        assert_true(solo.fs.exists("."))
        assert_true(solo.fs.exists("include/solo/solo.h"))
    end)

    test("returns false for nonexistent absolute and relative paths", function()
        assert_false(solo.fs.exists("/nonexistent_xyz_998877"))
        assert_false(solo.fs.exists("nonexistent_path_xyz_123.bin"))
    end)

    test("submodules require('solo.fs') and require('solo.filesystem') resolve correctly", function()
        local req_fs = require("solo.fs")
        assert_eq(req_fs, solo.fs)
        local req_filesystem = require("solo.filesystem")
        assert_eq(req_filesystem, solo.filesystem)
    end)
end)

describe("solo.sandbox (Kernel Security & Isolation)", function()
    local s = solo.sandbox

    test("exposes backend name and version information", function()
        local backend, version = s.backend()
        assert_eq(type(backend), "string")
        assert_true(#backend > 0)
        assert_true(version == nil or type(version) == "number")
    end)

    test("provides sandbox API function signatures", function()
        assert_eq(type(s.is_supported), "function")
        assert_eq(type(s.unveil), "function")
        assert_eq(type(s.lock), "function")
        assert_eq(type(s.pledge), "function")
    end)

    test("unveil and pledge invocations return boolean status", function()
        assert_eq(type(s.unveil(".", "r")), "boolean")
        assert_eq(type(s.pledge("stdio rpath")), "boolean")
    end)

    test("defines sandbox backend, perm and status enum tables", function()
        assert_true(s.backend ~= nil)
        assert_eq(s.backend.NONE, 0)
        assert_eq(s.backend.LANDLOCK, 1)
        assert_eq(s.backend.OPENBSD, 2)
        assert_eq(s.backend.CAPSICUM, 3)

        assert_true(s.perm ~= nil)
        assert_eq(s.perm.NONE, 0)
        assert_eq(s.perm.READ, 1)
        assert_eq(s.perm.WRITE, 2)
        assert_eq(s.perm.EXEC, 4)

        assert_true(s.status ~= nil)
        assert_eq(s.status.OK, 0)
        assert_eq(s.status.ERROR, -1)
        assert_eq(s.status.UNSUPPORTED, -2)

        assert_eq(s.READ, 1)
        assert_eq(s.WRITE, 2)
        assert_eq(s.EXEC, 4)
    end)

    test("unveil accepts both string and integer enum permissions", function()
        assert_eq(type(s.unveil(".", "r")), "boolean")
        assert_eq(type(s.unveil(".", s.perm.READ)), "boolean")
    end)

    test("unsupported sandbox operations fail visibly with explanatory error", function()
        if solo.platform.is_linux then
            local ok, err, code = s.pledge("stdio")
            assert_false(ok)
            assert_eq(type(err), "string")
            assert_true(#err > 0)
            assert_eq(code, s.status.UNSUPPORTED)
        elseif not s.is_supported() then
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

describe("solo.parallel & solo.serial (Physical Device Driver Signatures)", function()
    test("exposes open constructors for parallel and serial drivers", function()
        assert_eq(type(solo.parallel.open), "function")
        assert_eq(type(solo.serial.open), "function")
    end)

    test("defines serial flow, parity, stop, data, baud, line and status enum tables", function()
        local s = solo.serial
        assert_eq(type(s.flow), "table")
        assert_eq(s.flow.NONE, 0)
        assert_eq(s.flow.HARDWARE, 1)
        assert_eq(s.flow.SOFTWARE, 2)
        assert_eq(s.flow.RTS_CTS, 1)
        assert_eq(s.flow.XON_XOFF, 2)

        assert_eq(type(s.parity), "table")
        assert_eq(s.parity.NONE, string.byte("n"))
        assert_eq(s.parity.EVEN, string.byte("e"))
        assert_eq(s.parity.ODD, string.byte("o"))
        assert_eq(s.parity.MARK, string.byte("m"))
        assert_eq(s.parity.SPACE, string.byte("s"))

        assert_eq(type(s.stop_bits), "table")
        assert_eq(s.stop_bits.ONE, 1)
        assert_eq(s.stop_bits.TWO, 2)
        assert_eq(s.stop.STOP_1, 1)
        assert_eq(s.stop.STOP_2, 2)

        assert_eq(type(s.data_bits), "table")
        assert_eq(s.data_bits.FIVE, 5)
        assert_eq(s.data_bits.SIX, 6)
        assert_eq(s.data_bits.SEVEN, 7)
        assert_eq(s.data_bits.EIGHT, 8)
        assert_eq(s.data.EIGHT, 8)

        assert_eq(type(s.baud), "table")
        assert_eq(s.baud.B9600, 9600)
        assert_eq(s.baud.B19200, 19200)
        assert_eq(s.baud.B38400, 38400)
        assert_eq(s.baud.B115200, 115200)

        assert_eq(type(s.line), "table")
        assert_eq(s.line.IGNORE, -1)
        assert_eq(s.line.OFF, 0)
        assert_eq(s.line.ON, 1)

        assert_eq(type(s.status), "table")
        assert_eq(s.status.OK, 0)
        assert_eq(s.status.ERROR, -1)
        assert_eq(s.status.UNSUPPORTED, -2)
    end)

    test("serial open accepts options table and string parity/flow", function()
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

    test("serial open accepts positional arguments with string parity", function()
        local port, err = solo.serial.open("/nonexistent_tty_device_12345", 9600, 8, "n", 1, 1)
        assert_nil(port)
        assert_eq(type(err), "string")
    end)

    test("defines parallel status, mode, pin and port enum tables", function()
        local p = solo.parallel
        assert_eq(type(p.status), "table")
        assert_eq(p.status.OK, 0)
        assert_eq(p.status.ERROR, -1)
        assert_eq(p.status.UNSUPPORTED, -2)

        assert_eq(type(p.mode), "table")
        assert_eq(p.mode.SPP, 0)
        assert_eq(p.mode.CENTRONICS, 0)
        assert_eq(p.mode.NIBBLE, 1)
        assert_eq(p.mode.BYTE, 2)
        assert_eq(p.mode.EPP, 3)
        assert_eq(p.mode.ECP, 4)

        assert_eq(type(p.pin), "table")
        assert_eq(p.pin.BUSY, 1)
        assert_eq(p.pin.ACK, 2)
        assert_eq(p.pin.PE, 4)
        assert_eq(p.pin.PAPER_OUT, 4)
        assert_eq(p.pin.SLCT, 8)
        assert_eq(p.pin.ONLINE, 8)
        assert_eq(p.pin.FAULT, 16)

        assert_eq(type(p.port), "table")
        assert_eq(p.port.LPT1, "LPT1")
        assert_eq(p.port.LPT2, "LPT2")
        assert_eq(p.port.LPT3, "LPT3")
    end)

    test("parallel open accepts options table or string path", function()
        local port, err = solo.parallel.open({ path = "/nonexistent_lpt_device_12345" })
        assert_nil(port)
        assert_eq(type(err), "string")
    end)
end)

describe("solo.socket (Network Sockets & Multiplexing)", function()
    local sock = solo.socket

    test("defines socket type constants", function()
        assert_true(sock.type ~= nil)
        assert_eq(sock.type.TCP, 1)
        assert_eq(sock.type.UDP, 2)
        assert_eq(sock.TCP, 1)
        assert_eq(sock.UDP, 2)
    end)

    test("defines socket mode constants", function()
        assert_true(sock.mode ~= nil)
        assert_eq(sock.mode.BLOCKING, 0)
        assert_eq(sock.mode.NONBLOCKING, 1)
    end)

    test("defines socket status constants", function()
        assert_true(sock.status ~= nil)
        assert_eq(sock.status.OK, 0)
        assert_eq(sock.status.ERROR, -1)
        assert_eq(sock.status.AGAIN, -2)
        assert_eq(sock.status.CLOSED, -3)
    end)

    test("defines socket event polling masks", function()
        assert_true(sock.event ~= nil)
        assert_eq(sock.event.READ, 1)
        assert_eq(sock.event.WRITE, 2)
        assert_eq(sock.event.ERR, 4)
    end)

    test("transfers data over synchronous TCP loopback", function()
        local tcp_port = 19935
        local server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
        if not server then
            tcp_port = 19936
            server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
        end
        assert_true(server ~= nil, "Failed to bind server: " .. tostring(err))
        assert_eq(server:type(), "tcp")
        assert_true(server:is_blocking())

        local client, err_c = sock.connect("127.0.0.1", tcp_port, sock.mode.BLOCKING)
        assert_true(client ~= nil, "Failed to connect client: " .. tostring(err_c))

        local accepted, ip, port = server:accept()
        assert_true(accepted ~= nil, "Failed to accept client")
        assert_eq(type(ip), "string")
        assert_eq(type(port), "number")

        local test_msg = "Hello TCP Synchronous!"
        local written = client:send(test_msg)
        assert_eq(written, #test_msg)

        local received = accepted:recv(128)
        assert_eq(received, test_msg)

        client:close()
        accepted:close()
        server:close()
    end)

    test("handles non-blocking TCP asynchronous loopback", function()
        local async_tcp_port = 19945
        local async_server, a_err = sock.bind("127.0.0.1", async_tcp_port, 5, sock.mode.NONBLOCKING)
        if not async_server then
            async_tcp_port = 19946
            async_server, a_err = sock.bind("127.0.0.1", async_tcp_port, 5, sock.mode.NONBLOCKING)
        end
        assert_true(async_server ~= nil, "Failed to bind async server: " .. tostring(a_err))
        assert_false(async_server:is_blocking())

        -- Non-blocking accept on idle server must return nil, "again"
        local non_client, non_err = async_server:accept()
        assert_nil(non_client)
        assert_eq(non_err, "again")

        -- Connect client
        local async_client = sock.connect("127.0.0.1", async_tcp_port)
        assert_true(async_client ~= nil, "Failed to connect to async server")

        -- Server wait for read readiness
        local ready = async_server:wait("r", 1000)
        assert_true(ready == "r" or ready == "rw")

        local client_sock = async_server:accept()
        assert_true(client_sock ~= nil, "Failed to accept client on ready server")
        client_sock:set_mode("nonblocking")
        assert_false(client_sock:is_blocking())

        -- Read on empty socket must return nil, "again"
        local empty_data, empty_err = client_sock:recv(128)
        assert_nil(empty_data)
        assert_eq(empty_err, "again")

        -- Send data from client
        local async_msg = "Hello TCP Asynchronous!"
        async_client:send(async_msg)

        -- Wait for read readiness
        assert_true(client_sock:wait(sock.event.READ, 1000) ~= nil)
        local async_recv = client_sock:recv(128)
        assert_eq(async_recv, async_msg)

        async_client:close()
        client_sock:close()
        async_server:close()
    end)

    test("transfers datagrams over synchronous UDP loopback", function()
        local udp_port = 19955
        local udp_server, u_err = sock.udp_bind("127.0.0.1", udp_port)
        if not udp_server then
            udp_port = 19956
            udp_server, u_err = sock.udp_bind("127.0.0.1", udp_port)
        end
        assert_true(udp_server ~= nil, "Failed to bind UDP server: " .. tostring(u_err))
        assert_eq(udp_server:type(), "udp")

        local udp_client = sock.udp()
        assert_true(udp_client ~= nil, "Failed to create UDP client")
        assert_eq(udp_client:type(), "udp")

        local udp_msg = "Hello UDP Datagram!"
        local sent = udp_client:sendto(udp_msg, "127.0.0.1", udp_port)
        assert_eq(sent, #udp_msg)

        local udp_data, src_ip, src_port = udp_server:recvfrom(128)
        assert_eq(udp_data, udp_msg)
        assert_true(type(src_ip) == "string" and #src_ip > 0)
        assert_true(type(src_port) == "number" and src_port > 0)

        -- Reply back to client
        local udp_reply = "Ack UDP Datagram!"
        udp_server:sendto(udp_reply, src_ip, src_port)
        local reply_data = udp_client:recvfrom(128)
        assert_eq(reply_data, udp_reply)

        udp_client:close()
        udp_server:close()
    end)

    test("multiplexes asynchronous UDP with select()", function()
        local async_udp_port = 19965
        local async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
        if not async_udp then
            async_udp_port = 19966
            async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
        end
        assert_true(async_udp ~= nil, "Failed to bind async UDP server")
        assert_false(async_udp:is_blocking())

        -- recvfrom on empty nonblocking UDP socket must return nil, "again"
        local d_none, err_none = async_udp:recvfrom(128)
        assert_nil(d_none)
        assert_eq(err_none, "again")

        -- wait timeout check
        local wait_res, wait_err = async_udp:wait("r", 50)
        assert_nil(wait_res)
        assert_eq(wait_err, "timeout")

        -- select check on idle socket
        local ready_r, ready_w = sock.select({ async_udp }, nil, 50)
        assert_eq(#ready_r, 0)

        -- Send datagram
        local u_cli = sock.udp()
        local async_udp_msg = "Async UDP Multiplexing Payload"
        u_cli:sendto(async_udp_msg, "127.0.0.1", async_udp_port)

        -- select should now identify async_udp as ready
        ready_r, ready_w = sock.select({ async_udp }, nil, 1000)
        assert_eq(#ready_r, 1)
        assert_eq(ready_r[1], async_udp)

        local recvd_async_udp = async_udp:recvfrom(128)
        assert_eq(recvd_async_udp, async_udp_msg)

        u_cli:close()
        async_udp:close()
    end)

    test("multiplexes multiple readers and writers with select()", function()
        local p1, p2 = 19988, 19989
        local u1 = sock.udp_bind("127.0.0.1", p1, sock.mode.NONBLOCKING)
        if not u1 then
            p1, p2 = 19996, 19997
            u1 = sock.udp_bind("127.0.0.1", p1, sock.mode.NONBLOCKING)
        end
        local u2 = sock.udp_bind("127.0.0.1", p2, sock.mode.NONBLOCKING)
        local sender = sock.udp()

        -- Neither u1 nor u2 has data to read; sender is ready to write
        local r_ready, w_ready = sock.select({ u1, u2 }, { sender }, 20)
        assert_eq(#r_ready, 0)
        assert_eq(#w_ready, 1)
        assert_eq(w_ready[1], sender)

        -- Send data to u1 only
        sender:sendto("multitest", "127.0.0.1", p1)

        r_ready, w_ready = sock.select({ u1, u2 }, nil, 1000)
        assert_eq(#r_ready, 1)
        assert_eq(r_ready[1], u1)

        local data = u1:recvfrom(64)
        assert_eq(data, "multitest")

        sender:close()
        u2:close()
        u1:close()
    end)

    test("honors socket timeout configuration", function()
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
        assert_nil(to_data)
        assert_eq(to_err, "again")

        to_client:close()
        to_acc:close()
        to_server:close()
    end)
end)

describe("solo standalone submodules (Independent C Module Loading)", function()
    test("loads solo.platform independently", function()
        local p = require("solo.platform")
        assert_eq(type(p.os), "string")
        assert_eq(p.os, solo.platform.os)
    end)

    test("loads solo.serial independently", function()
        local s = require("solo.serial")
        assert_eq(type(s.open), "function")
        assert_eq(s.flow.HARDWARE, 1)
        assert_eq(s.parity.NONE, string.byte("n"))
    end)

    test("loads solo.socket independently", function()
        local sock = require("solo.socket")
        assert_eq(type(sock.connect), "function")
    end)

    test("loads solo.fs independently", function()
        local fs = require("solo.fs")
        assert_eq(type(fs.exists), "function")
    end)

    test("loads solo.parallel independently", function()
        local par = require("solo.parallel")
        assert_eq(type(par.open), "function")
        assert_eq(par.mode.SPP, 0)
        assert_eq(par.pin.BUSY, 1)
    end)

    test("loads solo.sandbox independently", function()
        local sbx = require("solo.sandbox")
        assert_eq(type(sbx.is_supported), "function")
    end)
end)

if arg and arg[0] and arg[0]:match("test_solo%.lua$") then
    os.exit(T.summary())
end
