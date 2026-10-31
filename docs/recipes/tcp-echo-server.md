# Recipe: Asynchronous Non-Blocking TCP Echo Server

This recipe demonstrates how to implement an event-driven TCP echo server that multiplexes multiple concurrent client connections on a single thread using `solo_socket_poll` (in C) and `solo.socket.select` (in Lua).

---

## C89 Implementation

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <solo/solo.h>

#define MAX_CLIENTS 32
#define PORT 8080

int main(void) {
    char err[128];
    solo_socket_t *server;
    solo_socket_t *clients[MAX_CLIENTS];
    solo_pollfd_t pfds[MAX_CLIENTS + 1];
    size_t num_clients = 0;
    size_t i;

    memset(clients, 0, sizeof(clients));

    /* Create non-blocking server socket */
    server = solo_socket_bind("0.0.0.0", PORT, 128, SOLO_SOCK_NONBLOCKING, err, sizeof(err));
    if (!server) {
        fprintf(stderr, "Failed to bind server: %s\n", err);
        return 1;
    }
    printf("Echo server listening on 0.0.0.0:%d (Non-blocking)\n", PORT);

    while (1) {
        size_t nfds = 0;
        int ready;

        /* Index 0: Server listening socket (watch for incoming connections) */
        pfds[nfds].sock = server;
        pfds[nfds].events = SOLO_SOCK_POLL_READ;
        pfds[nfds].revents = 0;
        nfds++;

        /* Client sockets (watch for incoming data) */
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != NULL) {
                pfds[nfds].sock = clients[i];
                pfds[nfds].events = SOLO_SOCK_POLL_READ;
                pfds[nfds].revents = 0;
                nfds++;
            }
        }

        ready = solo_socket_poll(pfds, nfds, 1000); /* 1000ms timeout */
        if (ready <= 0) {
            continue;
        }

        /* 1. Check for new client connections */
        if (pfds[0].revents & SOLO_SOCK_POLL_READ) {
            char client_ip[32];
            int client_port = 0;
            solo_socket_t *new_cli = solo_socket_accept(server, client_ip, sizeof(client_ip), 
                                                        &client_port, err, sizeof(err));
            if (new_cli) {
                solo_socket_set_mode(new_cli, SOLO_SOCK_NONBLOCKING, NULL, 0);
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] == NULL) {
                        clients[i] = new_cli;
                        num_clients++;
                        printf("[+] Connection from %s:%d (slot %lu)\n", client_ip, client_port, (unsigned long)i);
                        break;
                    }
                }
                if (i == MAX_CLIENTS) {
                    printf("[-] Server full, rejecting client\n");
                    solo_socket_close(new_cli);
                }
            }
        }

        /* 2. Process data from connected clients */
        for (i = 1; i < nfds; i++) {
            if (pfds[i].revents & (SOLO_SOCK_POLL_READ | SOLO_SOCK_POLL_ERR)) {
                char buf[512];
                size_t recvd = 0;
                solo_socket_t *sock = pfds[i].sock;
                solo_socket_status_t status = solo_socket_recv(sock, buf, sizeof(buf), &recvd);

                if (status == SOLO_SOCK_OK && recvd > 0) {
                    /* Echo data back */
                    size_t sent = 0;
                    solo_socket_send(sock, buf, recvd, &sent);
                } else if (status == SOLO_SOCK_CLOSED || status == SOLO_SOCK_ERROR) {
                    /* Client disconnected or error occurred */
                    for (size_t c = 0; c < MAX_CLIENTS; c++) {
                        if (clients[c] == sock) {
                            printf("[-] Disconnected client in slot %lu\n", (unsigned long)c);
                            solo_socket_close(clients[c]);
                            clients[c] = NULL;
                            num_clients--;
                            break;
                        }
                    }
                }
            }
        }
    }

    solo_socket_close(server);
    return 0;
}
```

---

## Lua Implementation

```lua
local sock = require("solo.socket")

local server, err = sock.bind("0.0.0.0", 8080, sock.mode.NONBLOCKING)
if not server then
    error("Server bind failed: " .. tostring(err))
end

print("Lua Echo Server listening on 0.0.0.0:8080...")
local clients = {}

while true do
    -- Build polling table: server + all connected clients
    local readers = { server }
    for _, cli in ipairs(clients) do
        table.insert(readers, cli)
    end

    local ready_readers = sock.select(readers, nil, 1000)

    for _, r in ipairs(ready_readers) do
        if r == server then
            -- New incoming connection
            local new_client, ip, port = server:accept()
            if new_client then
                new_client:set_mode(sock.mode.NONBLOCKING)
                table.insert(clients, new_client)
                print(string.format("[+] Client connected from %s:%d", ip, port))
            end
        else
            -- Data from client
            local data, err_msg = r:read(512)
            if data and #data > 0 then
                r:write(data) -- Echo
            else
                -- Disconnected or error
                print("[-] Client disconnected")
                r:close()
                for idx, c in ipairs(clients) do
                    if c == r then
                        table.remove(clients, idx)
                        break
                    end
                end
            end
        end
    end
end
```
