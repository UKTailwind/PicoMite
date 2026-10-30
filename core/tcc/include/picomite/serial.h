/* PicoMite TinyCC serial API (version 5). Native UARTs only: COM1/COM2.
   Requires the corresponding TX/RX GPIOs to be configured by BASIC SETPIN.
   Intended for standalone C programs run by TinyCC, not external CSUBs. */
#ifndef _PICOMITE_SERIAL_H
#define _PICOMITE_SERIAL_H

typedef int pm_serial_t;

#define PM_SERIAL_7BIT   0x01u
#define PM_SERIAL_STOP2  0x02u
#define PM_SERIAL_EVEN   0x04u
#define PM_SERIAL_ODD    0x08u
#define PM_SERIAL_INVERT 0x10u

typedef struct pm_serial_config {
    int port;                 /* 1 or 2 (COM1 / COM2) */
    unsigned baudrate;       /* 0 for default 9600, else up to 921600 */
    unsigned rx_buffer_size; /* 0 for 1024 bytes, else 2..65536 */
    unsigned flags;          /* zero: 8N1, or PM_SERIAL_* */
} pm_serial_config;

/* All functions require a handle returned by pm_serial_open().
   Invalid arguments raise a PicoMite error (catch with pm_try()). */
pm_serial_t pm_serial_open(const pm_serial_config *cfg);
int pm_serial_available(pm_serial_t port); /* queued RX bytes */
int pm_serial_read(pm_serial_t port, void *buf, unsigned len, unsigned timeout_ms);
int pm_serial_write(pm_serial_t port, const void *buf, unsigned len, unsigned timeout_ms);
/* read/write return 0..len; timeout 0 is nonblocking. They may return
   a partial transfer on timeout; user must handle that. */
int pm_serial_flush(pm_serial_t port, unsigned timeout_ms); /* 0 idle, -1 timeout */
void pm_serial_close(pm_serial_t port); /* close is immediate; flush first */

#endif /* _PICOMITE_SERIAL_H */
