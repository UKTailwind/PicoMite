/* Stateless/native peripheral bridges independent of TinyCC. Hardware
 * resources shared with BASIC retain the firmware's existing validation. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include <stdint.h>
#include <limits.h>
#include "pico/time.h"
#include "core/tcc/include/picomite/serial.h"
#include "core/tcc/include/picomite/i2c.h"
#include "core/tcc/include/picomite/spi.h"
#include <string.h>
#include "core/tcc/include/picomite/system.h"

static int pm_runtime_adc_pin(int gp, int raw)
{
    int pin;
    if (!pm_runtime_is_running()) error("ADC requires a running C program");
    pin = codemap(gp);
    if (IsInvalidPin(pin)) error("Invalid ADC pin");
    if (ADCopen || ADCDualBuffering || dmarunning) error("ADC in use");
    if (ExtCurrentConfig[pin] != EXT_ANA_IN &&
        (!raw || ExtCurrentConfig[pin] != EXT_ADCRAW))
        error("Pin not configured for ADC");
    return pin;
}
static void pm_runtime_adc_read_do(void *p)
{
    struct { int gp; double value; } *a = p;
    a->value = (double)PinVal(pm_runtime_adc_pin(a->gp, 0));
}
static double pm_runtime_adc_read(int gp)
{
    struct { int gp; double value; } a = {gp, 0};
    pm_core_call(pm_runtime_adc_read_do, &a);
    return a.value;
}
static void pm_runtime_adc_raw_do(void *p)
{
    int *a = p;
    a[1] = (int)ExtInp(pm_runtime_adc_pin(a[0], 1));
}
static int pm_runtime_adc_read_raw(int gp)
{
    int a[2] = {gp, 0};
    pm_core_call(pm_runtime_adc_raw_do, a);
    return a[1];
}

typedef struct {
    int slice;
    unsigned freq;
    double a, b;
} pm_pwm_args;
static void pm_pwm_open_do(void *p)
{
    const pm_pwm_args *a = p;
    PWM_Configure(a->slice, a->freq, a->a, a->b);
}
static void pm_runtime_pwm_open(int slice, unsigned hz, double a, double b)
{
    pm_pwm_args args = {slice, hz, a, b};
    pm_core_call(pm_pwm_open_do, &args);
}
static void pm_pwm_duty_do(void *p)
{
    const pm_pwm_args *a = p;
    PWM_SetDuty(a->slice, a->a, a->b);
}
static void pm_runtime_pwm_set_duty(int slice, double a, double b)
{
    pm_pwm_args args = {slice, 0, a, b};
    pm_core_call(pm_pwm_duty_do, &args);
}
static void pm_pwm_frequency_do(void *p)
{
    const pm_pwm_args *a = p;
    PWM_SetFrequency(a->slice, a->freq);
}
static void pm_runtime_pwm_set_frequency(int slice, unsigned hz)
{
    pm_pwm_args args = {slice, hz, 0, 0};
    pm_core_call(pm_pwm_frequency_do, &args);
}
static void pm_pwm_close_do(void *p)
{ PWM_Close(*(int *)p); }
static void pm_runtime_pwm_close(int slice)
{ pm_core_call(pm_pwm_close_do, &slice); }


/* Per-program opaque resource handles: generation-checked so BASIC cannot
 * close/reopen a C-owned port and accidentally give it back to the old handle.
 * The control block is allocated only when a port is first opened. */
typedef struct {
    unsigned serial_owned, serial_epoch[2];
    unsigned i2c_owned, i2c_epoch[2], i2c_timeout[2];
    unsigned spi_owned, spi_epoch[2], spi_bits[2];
} pm_peripheral_state;
static pm_peripheral_state *pm_periph_state;
void pm_runtime_periph_begin(void) { pm_periph_state = NULL; }

static pm_peripheral_state *pm_periph_need_state(void)
{
    if (!pm_runtime_is_running()) error("No running C program");
    if (!pm_periph_state) {
        pm_periph_state = pm_runtime_malloc(sizeof(*pm_periph_state));
        if (!pm_periph_state) error("Not enough memory");
        memset(pm_periph_state, 0, sizeof(*pm_periph_state));
    }
    return pm_periph_state;
}

void pm_runtime_periph_end(void)
{
    pm_peripheral_state *p = pm_periph_state;
    int port, bus;
    if (!p) return;
    for (port = 1; port <= 2; ++port)
        if ((p->serial_owned & (1u << (port - 1))) &&
            p->serial_epoch[port - 1] == SerialPortEpoch(port) &&
            SerialPortIsOpen(port)) SerialClose(port);
    for (bus = 0; bus < 2; ++bus) {
        if ((p->i2c_owned & (1u << bus)) &&
            p->i2c_epoch[bus] == I2C_MasterEpoch(bus) &&
            I2C_MasterIsOpen(bus)) I2C_MasterClose(bus);
        if ((p->spi_owned & (1u << bus)) &&
            p->spi_epoch[bus] == SPI_MasterEpoch(bus) &&
            SPI_MasterIsOpen(bus)) SPI_MasterClose(bus);
    }
    pm_periph_state = NULL;
}

static int pm_rt_serial_require(pm_serial_t handle)
{
    pm_peripheral_state *p = pm_periph_state;
    unsigned h = (unsigned)handle;
    int port = (int)(h & 3u), idx = port - 1;
    if (!pm_runtime_is_running() || !p || port < 1 || port > 2 ||
        !(p->serial_owned & (1u << idx)) ||
        p->serial_epoch[idx] != SerialPortEpoch(port) ||
        (p->serial_epoch[idx] & 0x1fffffffu) != (h >> 2) ||
        !SerialPortIsOpen(port)) error("Serial port not owned by C program");
    return port;
}
static void pm_rt_serial_open_do(void *arg)
{
    struct { const pm_serial_config *cfg; int handle; } *a = arg;
    const pm_serial_config *c = a->cfg;
    pm_peripheral_state *p;
    unsigned flags, idx;
    int parity;
    if (!c || c->port < 1 || c->port > 2) error("Invalid serial configuration");
    flags = c->flags;
    if (flags & ~(PM_SERIAL_7BIT | PM_SERIAL_STOP2 | PM_SERIAL_EVEN |
                  PM_SERIAL_ODD | PM_SERIAL_INVERT)) error("Invalid serial flags");
    if ((flags & PM_SERIAL_EVEN) && (flags & PM_SERIAL_ODD))
        error("Conflicting serial parity");
    p = pm_periph_need_state();
    idx = (unsigned)c->port - 1;
    if (p->serial_owned & (1u << idx)) error("Serial port already open");
    parity = (flags & PM_SERIAL_EVEN) ? 1 : (flags & PM_SERIAL_ODD) ? 2 : 0;
    SerialOpenTyped(c->port, (int)(c->baudrate ? c->baudrate : 9600),
                    (int)(c->rx_buffer_size ? c->rx_buffer_size : 1024),
                    (flags & PM_SERIAL_7BIT) ? 7 : 8, parity,
                    (flags & PM_SERIAL_STOP2) ? 2 : 1,
                    (flags & PM_SERIAL_INVERT) ? 1 : 0);
    p->serial_epoch[idx] = SerialPortEpoch(c->port);
    p->serial_owned |= 1u << idx;
    a->handle = (int)(((p->serial_epoch[idx] & 0x1fffffffu) << 2) | (unsigned)c->port);
}
static pm_serial_t pm_runtime_serial_open(const pm_serial_config *cfg)
{
    struct { const pm_serial_config *cfg; int handle; } a = {cfg, -1};
    pm_core_call(pm_rt_serial_open_do, &a);
    return a.handle;
}
static void pm_rt_serial_require_do(void *p)
{
    int *a = p;
    a[1] = pm_rt_serial_require(a[0]);
}
static int pm_rt_serial_port_checked(pm_serial_t handle)
{
    int a[2] = {handle, 0};
    pm_core_call(pm_rt_serial_require_do, a);
    return a[1];
}
static int pm_runtime_serial_available(pm_serial_t handle)
{ return SerialRxStatus(pm_rt_serial_port_checked(handle)); }
static int pm_runtime_serial_read(pm_serial_t handle, void *buf, unsigned count, unsigned timeout_ms)
{
    unsigned n = 0;
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    int port = pm_rt_serial_port_checked(handle);
    if (count > INT_MAX || (count && !buf)) error("Invalid serial read buffer");
    while (n < count) {
        int c;
        if (timeout_ms && n && time_us_64() >= end) break;
        c = SerialGetchar(port);
        if (c >= 0) {
            ((unsigned char *)buf)[n++] = (unsigned char)c;
            if ((n & 63u) == 0) { pm_yield(); port = pm_rt_serial_port_checked(handle); }
            continue;
        }
        if (!timeout_ms || time_us_64() >= end) break;
        pm_yield(); port = pm_rt_serial_port_checked(handle);
    }
    return (int)n;
}
static int pm_runtime_serial_write(pm_serial_t handle, const void *buf, unsigned count, unsigned timeout_ms)
{
    unsigned n = 0;
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    int port = pm_rt_serial_port_checked(handle);
    if (count > INT_MAX || (count && !buf)) error("Invalid serial write buffer");
    while (n < count) {
        if (timeout_ms && n && time_us_64() >= end) break;
        if (SerialTxStatus(port) < TX_BUFFER_SIZE - 1) {
            SerialPutchar(port, ((const unsigned char *)buf)[n++]);
            if ((n & 63u) == 0) { pm_yield(); port = pm_rt_serial_port_checked(handle); }
            continue;
        }
        if (!timeout_ms || time_us_64() >= end) break;
        pm_yield(); port = pm_rt_serial_port_checked(handle);
    }
    return (int)n;
}
static int pm_runtime_serial_flush(pm_serial_t handle, unsigned timeout_ms)
{
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    int port = pm_rt_serial_port_checked(handle);
    while (!SerialTxIdle(port)) {
        if (!timeout_ms || time_us_64() >= end) return -1;
        pm_yield(); port = pm_rt_serial_port_checked(handle);
    }
    return 0;
}
static void pm_rt_serial_close_do(void *p)
{
    int port = pm_rt_serial_require(*(int *)p);
    pm_periph_state->serial_owned &= ~(1u << (port - 1));
    SerialClose(port);
}
static void pm_runtime_serial_close(pm_serial_t handle)
{ pm_core_call(pm_rt_serial_close_do, &handle); }

static int pm_rt_i2c_require(pm_i2c_t handle)
{
    unsigned h = (unsigned)handle;
    int bus = (int)(h & 3u) - 1;
    pm_peripheral_state *p = pm_periph_state;
    if (!pm_runtime_is_running() || !p || bus < 0 || bus > 1 ||
        !(p->i2c_owned & (1u << bus)) ||
        p->i2c_epoch[bus] != I2C_MasterEpoch(bus) ||
        (p->i2c_epoch[bus] & 0x1fffffffu) != (h >> 2) ||
        !I2C_MasterIsOpen(bus)) error("I2C bus not owned by C program");
    return bus;
}
static void pm_rt_i2c_open_do(void *arg)
{
    unsigned *a = arg;
    unsigned speed_khz = a[1] ? a[1] : 100;
    unsigned timeout_ms = a[2] ? a[2] : 1000;
    int bus = (int)a[0];
    pm_peripheral_state *p;
    if (bus < 0 || bus > 1) error("Invalid I2C bus");
    p = pm_periph_need_state();
    if (p->i2c_owned & (1u << bus)) error("I2C bus already owned by C program");
    I2C_MasterOpen(bus, (int)speed_khz, timeout_ms);
    p->i2c_owned |= 1u << bus;
    p->i2c_epoch[bus] = I2C_MasterEpoch(bus);
    p->i2c_timeout[bus] = timeout_ms;
    a[3] = (unsigned)(((p->i2c_epoch[bus] & 0x1fffffffu) << 2) | (unsigned)(bus + 1));
}
static pm_i2c_t pm_runtime_i2c_open(int bus, unsigned speed_khz, unsigned timeout_ms)
{
    unsigned a[4] = {(unsigned)bus, speed_khz, timeout_ms, 0};
    pm_core_call(pm_rt_i2c_open_do, a);
    return (pm_i2c_t)a[3];
}
static void pm_rt_i2c_require_do(void *arg)
{ int *a = arg; a[1] = pm_rt_i2c_require(a[0]); }
static int pm_rt_i2c_bus_checked(pm_i2c_t handle)
{
    int a[2] = {handle, -1};
    pm_core_call(pm_rt_i2c_require_do, a);
    return a[1];
}
static int pm_runtime_i2c_transfer(pm_i2c_t handle, unsigned addr,
                                    const void *tx, unsigned txlen,
                                    void *rx, unsigned rxlen)
{
    int bus = pm_rt_i2c_bus_checked(handle);
    int status;
    if (addr > 0x7fu || (!txlen && !rxlen) || txlen > 65535u ||
        rxlen > 65535u || (txlen && !tx) || (rxlen && !rx))
        error("Invalid I2C transfer");
    status = I2C_MasterTransfer(bus, addr, tx, txlen, rx, rxlen, 0,
                                pm_periph_state->i2c_timeout[bus]);
    pm_yield();
    return status;
}
static int pm_runtime_i2c_write(pm_i2c_t h, unsigned addr, const void *tx, unsigned n)
{ return pm_runtime_i2c_transfer(h, addr, tx, n, NULL, 0); }
static int pm_runtime_i2c_read(pm_i2c_t h, unsigned addr, void *rx, unsigned n)
{ return pm_runtime_i2c_transfer(h, addr, NULL, 0, rx, n); }
static int pm_runtime_i2c_probe(pm_i2c_t h, unsigned addr)
{
    unsigned char data = 0;
    int st = pm_runtime_i2c_read(h, addr, &data, 1);
    return st == 0 ? 1 : st == 1 ? 0 : -1;
}
static void pm_rt_i2c_close_do(void *arg)
{
    int bus = pm_rt_i2c_require(*(int *)arg);
    pm_periph_state->i2c_owned &= ~(1u << bus);
    I2C_MasterClose(bus);
}
static void pm_runtime_i2c_close(pm_i2c_t h)
{ pm_core_call(pm_rt_i2c_close_do, &h); }

static int pm_rt_spi_require(pm_spi_t h)
{
    unsigned u = (unsigned)h;
    int bus = (int)(u & 3u) - 1;
    pm_peripheral_state *p = pm_periph_state;
    if (!pm_runtime_is_running() || !p || bus < 0 || bus > 1 ||
        !(p->spi_owned & (1u << bus)) ||
        p->spi_epoch[bus] != SPI_MasterEpoch(bus) ||
        (p->spi_epoch[bus] & 0x1fffffffu) != (u >> 2) ||
        !SPI_MasterIsOpen(bus)) error("SPI bus not owned by C program");
    return bus;
}
static void pm_rt_spi_open_do(void *arg)
{
    unsigned *a = arg;
    int bus = (int)a[0];
    unsigned speed = a[1] ? a[1] : 1000000;
    unsigned mode = a[2], bits = a[3] ? a[3] : 8;
    pm_peripheral_state *p;
    if (bus < 0 || bus > 1 || mode > 3 || bits < 4 || bits > 16)
        error("Invalid SPI configuration");
    p = pm_periph_need_state();
    if (p->spi_owned & (1u << bus)) error("SPI bus already owned by C program");
    SPI_MasterOpen(bus, speed, mode, bits);
    p->spi_owned |= 1u << bus;
    p->spi_epoch[bus] = SPI_MasterEpoch(bus);
    p->spi_bits[bus] = bits;
    a[4] = (unsigned)(((p->spi_epoch[bus] & 0x1fffffffu) << 2) | (unsigned)(bus + 1));
}
static pm_spi_t pm_runtime_spi_open(int bus, unsigned speed, unsigned mode, unsigned bits)
{
    unsigned a[5] = {(unsigned)bus, speed, mode, bits, 0};
    pm_core_call(pm_rt_spi_open_do, a);
    return (pm_spi_t)a[4];
}
static void pm_rt_spi_require_do(void *arg)
{ int *a = arg; a[1] = pm_rt_spi_require(a[0]); }
static int pm_rt_spi_bus_checked(pm_spi_t h)
{
    int a[2] = {h, -1};
    pm_core_call(pm_rt_spi_require_do, a);
    return a[1];
}
static int pm_runtime_spi_transfer(pm_spi_t h, const void *tx, void *rx, unsigned count)
{
    int bus = pm_rt_spi_bus_checked(h);
    unsigned done = 0, bytes = pm_periph_state->spi_bits[bus] <= 8 ? 1 : 2;
    if (count > 65535u || (count && !tx && !rx) ||
        (bytes == 2 && (((uintptr_t)tx & 1u) || ((uintptr_t)rx & 1u))))
        error("Invalid SPI transfer");
    while (done < count) {
        unsigned n = count - done;
        int sent;
        if (n > 64) n = 64;
        sent = SPI_MasterTransfer(bus,
            tx ? (const unsigned char *)tx + done * bytes : NULL,
            rx ? (unsigned char *)rx + done * bytes : NULL, n);
        if (sent != (int)n) error("SPI transfer failed");
        done += n;
        pm_yield(); bus = pm_rt_spi_bus_checked(h);
    }
    return (int)done;
}
static int pm_runtime_spi_write(pm_spi_t h, const void *tx, unsigned n)
{ return pm_runtime_spi_transfer(h, tx, NULL, n); }
static int pm_runtime_spi_read(pm_spi_t h, void *rx, unsigned n)
{ return pm_runtime_spi_transfer(h, NULL, rx, n); }
static void pm_rt_spi_close_do(void *arg)
{
    int bus = pm_rt_spi_require(*(int *)arg);
    pm_periph_state->spi_owned &= ~(1u << bus);
    SPI_MasterClose(bus);
}
static void pm_runtime_spi_close(pm_spi_t h)
{ pm_core_call(pm_rt_spi_close_do, &h); }

typedef struct { const char *name; const void *address; } pm_peripheral_symbol;
#define PM_PERIPH_AS(n,f) {#n, (const void *)&f},
static const pm_peripheral_symbol pm_peripheral_symbols[] = {
    PM_PERIPH_AS(pm_adc_read, pm_runtime_adc_read)
    PM_PERIPH_AS(pm_adc_read_raw, pm_runtime_adc_read_raw)
    PM_PERIPH_AS(pm_pwm_open, pm_runtime_pwm_open)
    PM_PERIPH_AS(pm_pwm_set_duty, pm_runtime_pwm_set_duty)
    PM_PERIPH_AS(pm_pwm_set_frequency, pm_runtime_pwm_set_frequency)
    PM_PERIPH_AS(pm_pwm_close, pm_runtime_pwm_close)
    PM_PERIPH_AS(pm_serial_open, pm_runtime_serial_open)
    PM_PERIPH_AS(pm_serial_read, pm_runtime_serial_read)
    PM_PERIPH_AS(pm_serial_write, pm_runtime_serial_write)
    PM_PERIPH_AS(pm_serial_available, pm_runtime_serial_available)
    PM_PERIPH_AS(pm_serial_flush, pm_runtime_serial_flush)
    PM_PERIPH_AS(pm_serial_close, pm_runtime_serial_close)
    PM_PERIPH_AS(pm_i2c_open, pm_runtime_i2c_open)
    PM_PERIPH_AS(pm_i2c_close, pm_runtime_i2c_close)
    PM_PERIPH_AS(pm_i2c_transfer, pm_runtime_i2c_transfer)
    PM_PERIPH_AS(pm_i2c_write, pm_runtime_i2c_write)
    PM_PERIPH_AS(pm_i2c_read, pm_runtime_i2c_read)
    PM_PERIPH_AS(pm_i2c_probe, pm_runtime_i2c_probe)
    PM_PERIPH_AS(pm_spi_open, pm_runtime_spi_open)
    PM_PERIPH_AS(pm_spi_close, pm_runtime_spi_close)
    PM_PERIPH_AS(pm_spi_transfer, pm_runtime_spi_transfer)
    PM_PERIPH_AS(pm_spi_write, pm_runtime_spi_write)
    PM_PERIPH_AS(pm_spi_read, pm_runtime_spi_read)
};
const void *pm_runtime_peripheral_resolve(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_peripheral_symbols)/sizeof(pm_peripheral_symbols[0]); ++i)
        if (!strcmp(pm_peripheral_symbols[i].name, name))
            return pm_peripheral_symbols[i].address;
    return NULL;
}
unsigned pm_runtime_peripheral_firmware_id(void)
{
    uint32_t h = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_peripheral_symbols)/sizeof(pm_peripheral_symbols[0]); ++i) {
        uint32_t v = (uint32_t)(uintptr_t)pm_peripheral_symbols[i].address;
        for (j = 0; j < 4; ++j, v >>= 8) h = (h ^ (v & 255u)) * 16777619u;
    }
    return h ? h : 1u;
}
