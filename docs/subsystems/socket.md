# Network Sockets & Multiplexing (`solo.socket`)

The `socket` subsystem provides a cross-platform, non-blocking IPv4 TCP (streaming) and UDP (datagram) engine designed to maintain 100% semantic parity between modern POSIX workstations and vintage operating systems.

---

## 1. Constants & Enumerations

### Socket Type (`solo_socket_type_t`)

| C Constant | Lua Field (`solo.socket.type`) | Integer Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_SOCK_TCP` / `SOLO_SOCK_STREAM` | `TCP` | `1` | Stream-oriented connection (TCP over IPv4) |
| `SOLO_SOCK_UDP` / `SOLO_SOCK_DGRAM` | `UDP` | `2` | Datagram connectionless socket (UDP over IPv4) |

### Blocking Mode (`solo_socket_mode_t`)

| C Constant | Lua Field (`solo.socket.mode`) | Integer Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_SOCK_BLOCKING` | `BLOCKING` | `0` | Sockets block execution until I/O completes or times out |
| `SOLO_SOCK_NONBLOCKING` | `NONBLOCKING` | `1` | Sockets return immediately; return `SOLO_SOCK_AGAIN` if operation cannot complete |

### Readiness Polling Events (`solo_socket_event_t`)

| C Constant | Lua Field (`solo.socket.event`) | Bitmask Value | Description |
| :--- | :--- | :---: | :--- |
| `SOLO_SOCK_POLL_READ` | `READ` | `1 << 0` (`0x01`) | Socket has incoming data ready to read, or has pending incoming connections |
| `SOLO_SOCK_POLL_WRITE` | `WRITE` | `1 << 1` (`0x02`) | Socket has buffer capacity available to send data |
| `SOLO_SOCK_POLL_ERR` | `ERR` | `1 << 2` (`0x04`) | Socket encountered an asynchronous error, peer hangup (HUP), or invalid descriptor |

### Return Status Codes (`solo_socket_status_t`)

| C Constant | Integer Value | Description |
| :--- | :---: | :--- |
| `SOLO_SOCK_OK` | `0` | Operation completed successfully |
| `SOLO_SOCK_ERROR` | `-1` | Unrecoverable I/O or network stack error |
| `SOLO_SOCK_AGAIN` | `-2` | Non-blocking operation would block (`EAGAIN` / `EWOULDBLOCK`) |
| `SOLO_SOCK_CLOSED` | `-3` | Orderly disconnection / EOF received from peer |
| `SOLO_SOCK_TIMEOUT` | `-4` | Configured timeout expired before data could be sent/received |

---

## 2. C89 Function Reference

### `solo_socket_create`
```c
solo_socket_t *solo_socket_create(
    solo_socket_type_t type, 
    solo_socket_mode_t mode, 
    char *err_buf, 
    size_t err_cap
);
```
Creates an unbound IPv4 socket.
* **Parameters:**
  * `type`: `SOLO_SOCK_TCP` or `SOLO_SOCK_UDP`.
  * `mode`: `SOLO_SOCK_BLOCKING` or `SOLO_SOCK_NONBLOCKING`.
  * `err_buf`: Optional caller-allocated buffer for diagnostic messages (may be `NULL`).
  * `err_cap`: Capacity of `err_buf` in bytes.
* **Returns:** Pointer to `solo_socket_t` on success, or `NULL` on failure.

---

### `solo_socket_connect`
```c
solo_socket_t *solo_socket_connect(
    const char *host, 
    int port, 
    solo_socket_mode_t mode, 
    char *err_buf, 
    size_t err_cap
);
```
Creates a TCP socket and initiates a connection to `host`:`port`. Resolves IPv4 hostname or dotted quad address synchronously.
* **Returns:** Connected socket handle, or `NULL` on failure.

---

### `solo_socket_bind`
```c
solo_socket_t *solo_socket_bind(
    const char *host, 
    int port, 
    int backlog, 
    solo_socket_mode_t mode, 
    char *err_buf, 
    size_t err_cap
);
```
Creates a TCP server socket, binds to `host`:`port`, and begins listening with the specified connection `backlog` (typically `128`).
* **Returns:** Listening socket handle, or `NULL` on failure.

---

### `solo_socket_accept`
```c
solo_socket_t *solo_socket_accept(
    solo_socket_t *server, 
    char *client_ip, 
    size_t ip_len, 
    int *client_port, 
    char *err_buf, 
    size_t err_cap
);
```
Accepts an incoming TCP connection from a listening server socket.
* **Parameters:**
  * `client_ip`: Optional buffer to receive the client's IPv4 address string (e.g. `192.168.1.100`).
  * `ip_len`: Length of `client_ip` buffer (at least 16 bytes recommended).
  * `client_port`: Optional pointer to receive client's ephemeral port number.
* **Returns:** New client `solo_socket_t*`, or `NULL` if no connection is available or an error occurs. In non-blocking mode, if no connection is pending, returns `NULL` with `err_buf` empty.

---

### `solo_socket_send` / `solo_socket_recv`
```c
solo_socket_status_t solo_socket_send(
    solo_socket_t *sock, 
    const char *buf, 
    size_t len, 
    size_t *sent
);

solo_socket_status_t solo_socket_recv(
    solo_socket_t *sock, 
    char *buf, 
    size_t max_len, 
    size_t *received
);
```
Sends or receives stream data over a connected TCP socket.
* **Returns:**
  * `SOLO_SOCK_OK`: Data transferred (*sent / *received populated).
  * `SOLO_SOCK_AGAIN`: Socket would block in non-blocking mode.
  * `SOLO_SOCK_CLOSED`: Remote peer closed connection cleanly (EOF on recv).
  * `SOLO_SOCK_ERROR`: Fatal socket error.

---

### `solo_socket_sendto` / `solo_socket_recvfrom`
```c
solo_socket_status_t solo_socket_sendto(
    solo_socket_t *sock, 
    const char *buf, 
    size_t len,
    const char *host, 
    int port, 
    size_t *sent
);

solo_socket_status_t solo_socket_recvfrom(
    solo_socket_t *sock, 
    char *buf, 
    size_t max_len,
    char *src_ip, 
    size_t ip_len, 
    int *src_port,
    size_t *received
);
```
Transfers connectionless UDP datagrams. `recvfrom` captures the origin sender's IPv4 address and port.

---

### `solo_socket_poll`
```c
int solo_socket_poll(
    solo_pollfd_t *fds, 
    size_t nfds, 
    int timeout_ms
);
```
Multiplexes events across multiple sockets simultaneously.
* **`solo_pollfd_t` Structure:**
  ```c
  typedef struct solo_pollfd {
      solo_socket_t *sock;    /* Target socket handle */
      int events;             /* Input bitmask: SOLO_SOCK_POLL_READ | SOLO_SOCK_POLL_WRITE */
      int revents;            /* Output bitmask: Events that occurred */
  } solo_pollfd_t;
  ```
* **Parameters:**
  * `timeout_ms`: `< 0` blocks indefinitely; `0` returns immediately; `> 0` waits up to specified milliseconds.
* **Returns:** Count of descriptors ready (`> 0`), `0` on timeout, or `SOLO_SOCK_ERROR` on failure.

---

## 3. Lua API Reference

### Module Constructors

```lua
local sock = require("solo.socket")
```

| Lua Function | Description | Return Value |
| :--- | :--- | :--- |
| `sock.connect(host, port [, mode])` | Connects TCP client | `client_obj` or `nil, err` |
| `sock.bind(host, port [, mode, backlog])` | Binds TCP server listener | `server_obj` or `nil, err` |
| `sock.udp([mode])` | Creates unbound UDP socket | `udp_obj` or `nil, err` |
| `sock.udp_bind(host, port [, mode])` | Creates bound UDP socket | `udp_obj` or `nil, err` |
| `sock.select(readers, writers [, timeout_ms])` | Multiplexes tables of sockets | `ready_readers, ready_writers` |

### Socket Object Methods

* `sock:read(max_len)`: Reads up to `max_len` bytes. Returns `data_str` or `nil, err`.
* `sock:write(str)`: Transmits entire string. Returns `sent_bytes` or `nil, err`.
* `sock:accept()`: Accepts client from server socket. Returns `client_sock, client_ip, client_port`.
* `sock:sendto(data, host, port)`: Sends UDP datagram. Returns `sent_bytes` or `nil, err`.
* `sock:recvfrom(max_len)`: Receives UDP datagram. Returns `data_str, sender_ip, sender_port`.
* `sock:set_mode(mode)`: Switches between `sock.mode.BLOCKING` and `sock.mode.NONBLOCKING`.
* `sock:set_timeout(timeout_ms)`: Configures I/O timeout in milliseconds.
* `sock:wait(events [, timeout_ms])`: Waits on single socket (`"r"`, `"w"`, `"rw"`). Returns `true` or `nil, err`.
* `sock:close()`: Closes socket immediately.
