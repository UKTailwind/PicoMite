/* TinyCC: <picomite/serial.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_SERIAL_H
#define _PICOMITE_SERIAL_H

typedef int pm_serial_t;

#define PM_SERIAL_7BIT   0x01u
#define PM_SERIAL_STOP2  0x02u
#define PM_SERIAL_EVEN   0x04u
#define PM_SERIAL_ODD    0x08u
#define PM_SERIAL_INVERT 0x10u

typedef struct pm_serial_config {
    int port;
    unsigned baudrate;
    unsigned rx_buffer_size;
    unsigned flags;
} pm_serial_config;

pm_serial_t pm_serial_open(const pm_serial_config *cfg);
int pm_serial_available(pm_serial_t port);
int pm_serial_read(pm_serial_t port, void *buf, unsigned len, unsigned timeout_ms);
int pm_serial_write(pm_serial_t port, const void *buf, unsigned len, unsigned timeout_ms);

int pm_serial_flush(pm_serial_t port, unsigned timeout_ms);
void pm_serial_close(pm_serial_t port);

#endif
