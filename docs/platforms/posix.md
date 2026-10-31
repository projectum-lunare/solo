# Modern POSIX Platforms

Modern POSIX encompasses Linux distributions, BSD family systems (FreeBSD, OpenBSD, NetBSD), macOS (Darwin), Solaris/illumos, and Haiku.

---

## Driver Implementation Details

- **Filesystem:** POSIX.1-2001 calls (`stat`, `lstat`, `opendir`, `readdir`, `mkdir`, `unlink`, `rmdir`).
- **Networking:** Standard `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, and `<poll.h>`.
- **Serial:** POSIX `termios` (`tcgetattr`, `tcsetattr`, `tcsendbreak`, `tcdrain`, `ioctl(TIOCMGET)`).
- **Parallel:** Linux `/dev/lp[0-2]` or BSD `/dev/lpt[0-2]`.

---

## Non-Blocking I/O & Multiplexing

On POSIX platforms, sockets utilize native non-blocking flags (`O_NONBLOCK` via `fcntl`) and multiplexing through `poll(2)`. This bypasses the historic `FD_SETSIZE` (1024) limit, preventing silent memory corruption when descriptor values exceed 1023.
