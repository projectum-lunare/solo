# Recipes & How-To Guides

Production-ready implementation recipes demonstrating how to build robust, multi-platform applications with `solo`.

---

## Available Recipes

* [**Asynchronous Non-Blocking TCP Echo Server**](tcp-echo-server.md): Build an event-driven server handling multiple simultaneous connections with `solo_socket_poll` and `sock.select`.
* [**Industrial Serial GPS & Sensor Reader**](industrial-serial.md): Read continuous NMEA/sensor streams with hardware flow control, timeout recovery, and line status monitoring.
* [**Defense-in-Depth CLI Sandboxing**](hardened-cli.md): Secure a command-line tool using kernel sandboxing (Landlock & Pledge) to restrict filesystem access.
* [**Centronics Impact Printing & Hardware Handshaking**](parallel-strobe.md): Transmit raw ESC/P commands to receipt printers while polling hardware status pins.
