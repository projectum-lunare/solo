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
-- solo.socket (Network Sockets & Event Multiplexing Tests)
-- ============================================================================
-- Tests the Lua object-oriented socket abstraction:
-- - Synchronous and asynchronous TCP client/server loopbacks
-- - UDP datagram transfers and replies
-- - Multiplexing via sock.select() and socket:wait()
-- - Configurable timeouts (returning nil, "again" or "timeout")
-- - Multi-client concurrent connections and 64 KB streaming transfers
-- - Safety guards against operations on closed sockets and GC finalizer integrity
describe("solo.socket (Network Sockets & Multiplexing)", function()
    local sock = solo.socket

    -- 1. Socket protocol types
    test("defines socket type constants", function()
        assert_true(sock.type ~= nil)
        assert_eq(sock.type.TCP, 1)
        assert_eq(sock.type.UDP, 2)
        -- Convenience top-level aliases
        assert_eq(sock.TCP, 1)
        assert_eq(sock.UDP, 2)
    end)

    -- 2. Blocking / Non-blocking modes
    test("defines socket mode constants", function()
        assert_true(sock.mode ~= nil)
        assert_eq(sock.mode.BLOCKING, 0)
        assert_eq(sock.mode.NONBLOCKING, 1)
    end)

    -- 3. Return status codes
    test("defines socket status constants", function()
        assert_true(sock.status ~= nil)
        assert_eq(sock.status.OK, 0)
        assert_eq(sock.status.ERROR, -1)
        assert_eq(sock.status.AGAIN, -2)
        assert_eq(sock.status.CLOSED, -3)
    end)

    -- 4. Event polling bitmasks
    test("defines socket event polling masks", function()
        assert_true(sock.event ~= nil)
        assert_eq(sock.event.READ, 1)
        assert_eq(sock.event.WRITE, 2)
        assert_eq(sock.event.ERR, 4)
    end)

    -- 5. Synchronous TCP Loopback Transfer
    test("transfers data over synchronous TCP loopback", function()
        local tcp_port = 19935
        -- Bind server socket on loopback interface
        local server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
        if not server then
            tcp_port = 19936
            server, err = sock.bind("127.0.0.1", tcp_port, 5, sock.mode.BLOCKING)
        end
        assert_true(server ~= nil, "Failed to bind server: " .. tostring(err))
        assert_eq(server:type(), "tcp")
        assert_true(server:is_blocking())
        assert_false(server:is_nonblocking())

        -- Connect client socket
        local client, err_c = sock.connect("127.0.0.1", tcp_port, sock.mode.BLOCKING)
        assert_true(client ~= nil, "Failed to connect client: " .. tostring(err_c))

        -- Accept client connection on server
        local accepted, ip, port = server:accept()
        assert_true(accepted ~= nil, "Failed to accept client")
        assert_eq(type(ip), "string")
        assert_eq(type(port), "number")

        -- Client sends message -> server receives and asserts content
        local test_msg = "Hello TCP Synchronous!"
        local written = client:send(test_msg)
        assert_eq(written, #test_msg)

        local received = accepted:recv(128)
        assert_eq(received, test_msg)

        -- Clean up
        client:close()
        accepted:close()
        server:close()
    end)

    -- 6. Asynchronous / Non-Blocking TCP Loopback
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

        -- Server waits for incoming connection readiness
        local ready = async_server:wait("r", 1000)
        assert_true(ready == "r" or ready == "rw")

        local client_sock = async_server:accept()
        assert_true(client_sock ~= nil, "Failed to accept client on ready server")
        client_sock:set_mode("nonblocking")
        assert_false(client_sock:is_blocking())
        assert_true(client_sock:is_nonblocking())

        -- Read on empty socket must return nil, "again"
        local empty_data, empty_err = client_sock:recv(128)
        assert_nil(empty_data)
        assert_eq(empty_err, "again")

        -- Send data from client
        local async_msg = "Hello TCP Asynchronous!"
        async_client:send(async_msg)

        -- Wait for read readiness and receive data
        assert_true(client_sock:wait(sock.event.READ, 1000) ~= nil)
        local async_recv = client_sock:recv(128)
        assert_eq(async_recv, async_msg)

        async_client:close()
        client_sock:close()
        async_server:close()
    end)

    -- 7. Synchronous UDP Datagram Loopback
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

        -- Send datagram
        local udp_msg = "Hello UDP Datagram!"
        local sent = udp_client:sendto(udp_msg, "127.0.0.1", udp_port)
        assert_eq(sent, #udp_msg)

        -- Server receives datagram along with sender IP and port
        local udp_data, src_ip, src_port = udp_server:recvfrom(128)
        assert_eq(udp_data, udp_msg)
        assert_true(type(src_ip) == "string" and #src_ip > 0)
        assert_true(type(src_port) == "number" and src_port > 0)

        -- Reply back to client sender address
        local udp_reply = "Ack UDP Datagram!"
        udp_server:sendto(udp_reply, src_ip, src_port)
        local reply_data = udp_client:recvfrom(128)
        assert_eq(reply_data, udp_reply)

        udp_client:close()
        udp_server:close()
    end)

    -- 8. Asynchronous UDP Multiplexing with select()
    test("multiplexes asynchronous UDP with select()", function()
        local async_udp_port = 19965
        local async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
        if not async_udp then
            async_udp_port = 19966
            async_udp = sock.udp_bind("127.0.0.1", async_udp_port, sock.mode.NONBLOCKING)
        end
        assert_true(async_udp ~= nil, "Failed to bind async UDP server")
        assert_false(async_udp:is_blocking())

        -- recvfrom on empty non-blocking UDP socket must return nil, "again"
        local d_none, err_none = async_udp:recvfrom(128)
        assert_nil(d_none)
        assert_eq(err_none, "again")

        -- wait timeout check on idle socket
        local wait_res, wait_err = async_udp:wait("r", 50)
        assert_nil(wait_res)
        assert_eq(wait_err, "timeout")

        -- select check on idle socket returns 0 ready readers
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

    -- 9. Multiplexing Multiple Readers and Writers with select()
    test("multiplexes multiple readers and writers with select()", function()
        local p1, p2 = 19988, 19989
        local u1 = sock.udp_bind("127.0.0.1", p1, sock.mode.NONBLOCKING)
        if not u1 then
            p1, p2 = 19996, 19997
            u1 = sock.udp_bind("127.0.0.1", p1, sock.mode.NONBLOCKING)
        end
        local u2 = sock.udp_bind("127.0.0.1", p2, sock.mode.NONBLOCKING)
        local sender = sock.udp()

        -- Neither u1 nor u2 has data to read; sender socket is ready to write
        local r_ready, w_ready = sock.select({ u1, u2 }, { sender }, 20)
        assert_eq(#r_ready, 0)
        assert_eq(#w_ready, 1)
        assert_eq(w_ready[1], sender)

        -- Send data to u1 only
        sender:sendto("multitest", "127.0.0.1", p1)

        -- select reveals u1 is ready for reading while u2 remains idle
        r_ready, w_ready = sock.select({ u1, u2 }, nil, 1000)
        assert_eq(#r_ready, 1)
        assert_eq(r_ready[1], u1)

        local data = u1:recvfrom(64)
        assert_eq(data, "multitest")

        sender:close()
        u2:close()
        u1:close()
    end)

    -- 10. Configurable Timeout Handling
    test("honors socket timeout configuration", function()
        local to_port = 19975
        local to_server = sock.bind("127.0.0.1", to_port, 5)
        if not to_server then
            to_port = 19976
            to_server = sock.bind("127.0.0.1", to_port, 5)
        end
        local to_client = sock.connect("127.0.0.1", to_port)
        local to_acc = to_server:accept()

        -- Configure 100ms receive timeout
        to_acc:set_timeout(100)
        local to_data, to_err = to_acc:recv(64)
        assert_nil(to_data)
        assert_eq(to_err, "again")

        to_client:close()
        to_acc:close()
        to_server:close()
    end)

    -- 11. Concurrent TCP Client Connections
    test("handles multiple concurrent TCP client connections", function()
        local s_port = 19960
        local server = sock.bind("127.0.0.1", s_port, 10)
        if not server then
            s_port = 19961
            server = sock.bind("127.0.0.1", s_port, 10)
        end
        assert_true(server ~= nil)

        -- Connect two separate clients
        local c1 = sock.connect("127.0.0.1", s_port)
        local a1 = server:accept()
        local c2 = sock.connect("127.0.0.1", s_port)
        local a2 = server:accept()

        c1:send("client1_msg")
        c2:send("client2_msg")

        local m1 = a1:recv(64)
        local m2 = a2:recv(64)

        assert_eq(m1, "client1_msg")
        assert_eq(m2, "client2_msg")

        c1:close()
        c2:close()
        a1:close()
        a2:close()
        server:close()
    end)

    -- 12. Mass TCP Streaming Transfer (64 KB)
    test("transfers large streaming payloads (64KB) over TCP loopback", function()
        local s_port = 19965
        local server = sock.bind("127.0.0.1", s_port, 5)
        if not server then
            s_port = 19966
            server = sock.bind("127.0.0.1", s_port, 5)
        end
        local client = sock.connect("127.0.0.1", s_port)
        local acc = server:accept()

        -- Generate 64 KB pattern
        local payload = string.rep("0123456789ABCDEF", 4096)
        client:send(payload)

        -- Receive chunks and reassemble
        local received = {}
        local total = 0
        while total < #payload do
            local chunk = acc:recv(math.min(4096, #payload - total))
            if chunk and #chunk > 0 then
                table.insert(received, chunk)
                total = total + #chunk
            end
        end
        local full = table.concat(received)
        assert_eq(#full, #payload)
        assert_eq(full, payload)

        client:close()
        acc:close()
        server:close()
    end)

    -- 13. Closed Socket Safety Guards
    test("guards against operations on closed sockets", function()
        local s = sock.udp()
        s:close()
        -- Attempting to use a closed socket must raise a Lua error matching "closed"
        local ok, err = pcall(function() s:sendto("data", "127.0.0.1", 1234) end)
        assert_false(ok)
        assert_true(err ~= nil and err:match("closed") ~= nil)
    end)

    -- 14. Garbage Collector Stress & Finalizer Safety
    test("safely garbage collects sockets without memory faults", function()
        -- Allocates and closes sockets, verifying that gc finalizers run cleanly without segfaults
        for i = 1, 50 do
            local s = sock.udp()
            if i % 2 == 0 then
                s:close()
            end
        end
        collectgarbage("collect")
        collectgarbage("collect")
    end)
end)

if arg and arg[0] and arg[0]:match("test_.*%.lua$") then
    os.exit(T.summary())
end
