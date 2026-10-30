# TinyCC COM1/COM2 UART API

This describes **version 5** of the native PicoMite TinyCC API. The functions
are defined in `<picomite/serial.h>` and exported to TinyCC-linked C programs.
The implementation uses the existing `io/Serial.c` UART, pin reservations and
RX/TX buffers; it does not tokenise BASIC commands.

## Scope and prerequisites

* Supports **COM1 (UART0)** and **COM2 (UART1)** only. USB CDC COM3-COM6 and
  Bluetooth COM7 are intentionally not part of this initial API. They have
  different buffering and flow-control semantics.
* Use PicoMite `SETPIN` to assign the corresponding COM TX/RX pins first.
  The same pin availability and UART ownership checks as BASIC `OPEN` apply;
  occupied pins, the serial console and GPS cannot be taken over.
* The integrated TinyCC adapter is currently built for **RP2350** variants.
  The underlying serial driver changes are common to RP2040 and RP2350.
* Requires `pm_api_version() >= 5`. These entry points are **not available to
  external CSUBs**; the legacy `CallTable` layout is unchanged.
* The transmit ring has 256 slots (255 usable bytes); the configuration
  controls the receive ring only.

## Interface

```c
#include <picomite/serial.h>

pm_serial_config cfg = {
    .port = 1,
    .baudrate = 115200,
    .rx_buffer_size = 256,
    .flags = 0             /* 8 data bits, no parity, 1 stop bit */
};
pm_serial_t com = pm_serial_open(&cfg);
```

`baudrate` defaults to 9600 when zero; `rx_buffer_size` defaults to 1024.
Valid custom receive buffers are 2..65536 bytes. Baud rates follow the
existing PicoMite COM range (up to 921600; minimum depends on CPU clock).
Flags are `PM_SERIAL_7BIT`, `PM_SERIAL_STOP2`, `PM_SERIAL_EVEN`,
`PM_SERIAL_ODD` and `PM_SERIAL_INVERT`. Even and odd parity cannot be
combined. `pm_serial_open` returns an opaque handle (`pm_serial_t`).

`pm_serial_available(com)` returns queued input byte count. `pm_serial_read`
and `pm_serial_write` accept a byte pointer, a maximum length and an overall
timeout in milliseconds; both return the **number actually transferred**
(0..length), including partial results. A timeout of zero is nonblocking.
While waiting, they service PicoMite's background tasks and Ctrl+C.

`pm_serial_flush(com, timeout_ms)` waits until **both** the software TX queue
and UART hardware transmitter are idle: 0 on success, -1 on timeout.
`pm_serial_close(com)` closes without waiting; flush first if every outgoing
byte must be transmitted.

Invalid arguments, an unconfigured/busy UART and invalid/stale handles are
MMBasic errors, catchable with `pm_try()` in a C program. A C-owned port is
automatically closed when the C program terminates normally, with an MMBasic
error or on Ctrl+C. Port generations prevent it from accidentally closing a
port that BASIC closed and reopened in the meantime.

## Example

[`C/serial_echo.c`](../C/serial_echo.c) waits for bytes on COM1 at 115200 8N1
and echoes them. Copy the source to a PicoMite-accessible volume, assign COM1
GPIOs using BASIC `SETPIN`, and run it with `RUN "B:/serial_echo.c"` (adjust
the path to the copy). The example has no fixed pin numbers because they
depend on the board and its currently reserved peripherals.

This is a **polling API**, not an IRQ callback API. It deliberately never
runs C code inside a UART interrupt. Concurrent BASIC `OPEN`/`CLOSE` on a
C-owned port is unsupported; a handle is rejected if the port was replaced.
