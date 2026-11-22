/* iotest.c: the I/O services, the mouse and gamepad_axes (tools/sdk). MIT License, as pmimg.h.

     RUN "iotest.app"              each check prints PASS or FAIL, then a count. Needs nothing wired: GP0-GP7 free (the
                                   PC3's rear connector), an RTC on the system I2C bus at 0x68 (the PC3's DS3231)
     RUN "iotest.app", "loop"      also COM1 and PWM through a wire from GP0 to GP1
     RUN "iotest.app", "leave"     sets up PWM on GP5, SPI on GP2-GP4, I2C2 on GP6/GP7, COM1 on GP1/GP0 and GP40 (an
                                   output, high), and ends without undoing any of it: MM.INFO(PIN GPn) should then show
                                   them all unused

   pins: GP0 COM1 TX, GP1 COM1 RX, GP2-GP4 SPI (SCK, TX, RX), GP6/GP7 I2C2 (SDA, SCL), GP5 the single-pin checks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmm2.h"

static int fails;
static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if(!ok) fails++;
}
static void checkr(const char *what, int r, int want) {   /* a status, with io_error()'s message for -4 */
    char b[160];
    snprintf(b, sizeof(b), "%s: %d%s%s", what, r, r == -4 ? " " : "", r == -4 ? cmm2->io_error() : "");
    check(b, r == want);
}

/* a pin's level sampled n times over about 20 ms: the share of highs, in percent */
static int duty_of(int gp) {
    int hi = 0, n = 20000;
    for(int i = 0; i < n; i++) {
        hi += cmm2->pin_read(gp);
        cmm2->delay_us(1);
    }
    return hi * 100 / n;
}

static void leave_it_all(void) {
    checkr("pwm_set GP5 1 kHz", cmm2->pwm_set(5, 1000, 32768), 0);
    checkr("bus_pins SPI GP4/GP3/GP2", cmm2->bus_pins(CMM2_BUS_SPI, 0, 4, 3, 2), 0);
    checkr("spi_open 0", cmm2->spi_open(0, 1000000, 0), 0);
    checkr("bus_pins I2C2 GP6/GP7", cmm2->bus_pins(CMM2_BUS_I2C, 1, 6, 7, 0), 0);
    checkr("i2c_open 1", cmm2->i2c_open(1, 100, 100), 0);
    checkr("bus_pins COM1 GP1/GP0", cmm2->bus_pins(CMM2_BUS_COM, 1, 1, 0, 0), 0);
    checkr("com_open 1", cmm2->com_open(1, 9600, 8, 0, 1, 0), 0);
    checkr("pin_setup GP40 OUT", cmm2->pin_setup(40, CMM2_PIN_OUT, 0), 0);
    cmm2->pin_write(40, 1);
    printf("ending with them all set up\n");
}

int main(int argc, char **argv) {
    const char *what = argc > 1 ? argv[1] : "";
    uint8_t w[8], r[8];
    int m[4], v[6], b, d;
    uint32_t uv;

    if(cmm2->version < 1 || cmm2->size < sizeof(struct cmm2_services)) {
        printf("firmware too old for iotest\n");
        return 1;
    }
    if(strcmp(what, "leave") == 0) {
        leave_it_all();
        return 0;
    }

    /* what is refused */
    checkr("pin_setup GP48: no such GP", cmm2->pin_setup(48, CMM2_PIN_IN, 0), -1);
    checkr("pin_setup GP8 (the console): in use", cmm2->pin_setup(8, CMM2_PIN_IN, 0), -2);
    checkr("pin_setup GP47 (the PSRAM): in use", cmm2->pin_setup(47, CMM2_PIN_IN, 0), -2);
    checkr("pin_setup GP14 (the PC3's HDMI): not the program's", cmm2->pin_setup(14, CMM2_PIN_IN, 0), -1);
    checkr("pin_setup GP5 mode 9", cmm2->pin_setup(5, 9, 0), -3);
    checkr("adc_read GP5 not analog", cmm2->adc_read(5, &uv), -3);
    checkr("spi_xfer on a bus not open", cmm2->spi_xfer(0, w, r, 1), -3);
    checkr("com_write on a port not open", cmm2->com_write(1, w, 1), -3);
    checkr("com_open 7: no such port", cmm2->com_open(7, 9600, 8, 0, 1, 0), -1);
    b = cmm2->com_open(2, 9600, 8, 0, 1, 0);                    /* (the PC3's console is COM2) */
    checkr("com_open 2 (the console): the firmware's error", b, -4);

    /* one pin: pulls, an output read back, open drain */
    checkr("pin_setup GP5 IN pull-up", cmm2->pin_setup(5, CMM2_PIN_IN, CMM2_PULL_UP), 0);
    cmm2->delay_us(100);
    check("GP5 pulled up reads 1", cmm2->pin_read(5) == 1);
    checkr("pin_setup GP5 IN pull-down", cmm2->pin_setup(5, CMM2_PIN_IN, CMM2_PULL_DOWN), 0);
    cmm2->delay_us(100);
    check("GP5 pulled down reads 0", cmm2->pin_read(5) == 0);
    checkr("pin_setup GP5 OUT", cmm2->pin_setup(5, CMM2_PIN_OUT, 0), 0);
    cmm2->pin_write(5, 1);
    check("GP5 output 1 reads 1", cmm2->pin_read(5) == 1);
    cmm2->pin_write(5, 0);
    check("GP5 output 0 reads 0", cmm2->pin_read(5) == 0);
    checkr("pin_setup GP5 open drain pull-up", cmm2->pin_setup(5, CMM2_PIN_OD, CMM2_PULL_UP), 0);
    cmm2->pin_write(5, 0);
    cmm2->delay_us(100);
    check("GP5 open drain 0 reads 0", cmm2->pin_read(5) == 0);
    cmm2->pin_write(5, 1);
    cmm2->delay_us(100);
    check("GP5 open drain let go reads 1 (the pull-up)", cmm2->pin_read(5) == 1);
    checkr("pin_setup GP5 OFF", cmm2->pin_setup(5, CMM2_PIN_OFF, 0), 0);

    /* PWM, sampled on its own pin */
    checkr("pwm_set GP5 1 kHz 25%", cmm2->pwm_set(5, 1000, 16384), 0);
    d = duty_of(5);
    printf("GP5 high %d%% of the time\n", d);
    check("about 25%", d >= 20 && d <= 30);
    checkr("pwm_set GP4 (slice 2's A; GP5 is its B) 75%", cmm2->pwm_set(4, 1000, 49152), 0);
    d = duty_of(5);
    printf("GP5 still high %d%% of the time\n", d);
    check("GP5 still about 25%", d >= 20 && d <= 30);
    checkr("pwm_stop GP5", cmm2->pwm_stop(5), 0);
    check("GP5 stopped reads 0", duty_of(5) == 0);
    checkr("pwm_stop GP4", cmm2->pwm_stop(4), 0);

    /* an analog input: the first of GP40-46 that is free */
    for(b = 40; b <= 46; b++) if(cmm2->pin_setup(b, CMM2_PIN_ANALOG, 0) == 0) break;
    if(b <= 46) {
        d = cmm2->adc_read(b, &uv);
        printf("GP%d: %d of 65535, %lu uV\n", b, d, (unsigned long)uv);
        check("adc_read in range", d >= 0 && d <= 65535 && uv <= 3600000);
        cmm2->pin_setup(b, CMM2_PIN_OFF, 0);
    } else printf("no free analog input in GP40-46\n");

    /* the system I2C bus: the RTC's seconds, minutes and hours against datetime */
    checkr("i2c_open 0 (the system's): as it is", cmm2->i2c_open(0, 400, 100), 0);
    w[0] = 0;
    b = cmm2->i2c_xfer(0, 0x68, w, 1, r, 3, 1);
    checkr("i2c_xfer RTC 0x68", b, 0);
    if(b == 0) {
        int t[6];
        cmm2->datetime(t);
        printf("RTC %02x:%02x:%02x, datetime %02d:%02d:%02d\n", r[2] & 0x3F, r[1], r[0], t[3], t[4], t[5]);
        check("RTC minutes as datetime's (or one on)", ((r[1] >> 4) * 10 + (r[1] & 15) - t[4] + 60) % 60 <= 1);
    }
    checkr("i2c_xfer 0x77 (nothing there): a NACK", cmm2->i2c_xfer(0, 0x77, NULL, 0, NULL, 0, 1), 1);
    checkr("i2c_close 0 (the system's: stays open)", cmm2->i2c_close(0), 0);
    checkr("i2c_xfer RTC again", cmm2->i2c_xfer(0, 0x68, w, 1, r, 1, 1), 0);

    /* I2C2 on GP6/GP7, nothing on it */
    checkr("i2c_open 1, no pins", cmm2->i2c_open(1, 100, 100), -3);
    checkr("bus_pins I2C2 GP6/GP7", cmm2->bus_pins(CMM2_BUS_I2C, 1, 6, 7, 0), 0);
    checkr("i2c_open 1", cmm2->i2c_open(1, 100, 100), 0);
    checkr("bus_pins I2C2 again while open: in use", cmm2->bus_pins(CMM2_BUS_I2C, 1, 6, 7, 0), -2);
    checkr("i2c_xfer 0x50 on an empty bus: a NACK", cmm2->i2c_xfer(1, 0x50, w, 1, NULL, 0, 1), 1);
    checkr("i2c_close 1", cmm2->i2c_close(1), 0);

    /* SPI on GP2-GP4, nothing on it */
    checkr("bus_pins SPI GP4/GP3/GP2", cmm2->bus_pins(CMM2_BUS_SPI, 0, 4, 3, 2), 0);
    checkr("spi_open 0 1 MHz", cmm2->spi_open(0, 1000000, 0), 0);
    memcpy(w, "\x55\xAA\x00\xFF", 4);
    checkr("spi_xfer 4 bytes", cmm2->spi_xfer(0, w, r, 4), 0);
    checkr("spi_xfer, nothing out", cmm2->spi_xfer(0, NULL, r, 4), 0);
    checkr("spi_xfer, nothing kept", cmm2->spi_xfer(0, w, NULL, 4), 0);
    checkr("spi_xfer, neither", cmm2->spi_xfer(0, NULL, NULL, 40), 0);
    checkr("spi_close 0", cmm2->spi_close(0), 0);

    /* COM1 on GP1 (RX) / GP0 (TX) */
    checkr("bus_pins COM1 GP1/GP0", cmm2->bus_pins(CMM2_BUS_COM, 1, 1, 0, 0), 0);
    checkr("com_open 1 115200 8N1", cmm2->com_open(1, 115200, 8, 0, 1, 0), 0);
    checkr("com_write 5 bytes", cmm2->com_write(1, (const uint8_t *)"hello", 5), 5);
    cmm2->delay_us(2000);
    b = cmm2->com_avail(1);
    d = cmm2->com_read(1, r, sizeof(r));
    if(strcmp(what, "loop") == 0) check("hello came back", d == 5 && memcmp(r, "hello", 5) == 0);
    else check("nothing came back (no wire)", b == 0 && d == 0);
    checkr("com_close 1", cmm2->com_close(1), 0);
    checkr("com_connected 1", cmm2->com_connected(1), 1);
    printf("USB serial devices: COM3 %d, COM4 %d, COM5 %d, COM6 %d\n", cmm2->com_connected(3), cmm2->com_connected(4),
           cmm2->com_connected(5), cmm2->com_connected(6));
    checkr("com_connected 4 (none on the COM10 PC3)", cmm2->com_connected(4), 0);

    /* the mouse and the pads, whatever is plugged in */
    b = cmm2->mouse(m);
    printf("mouse: %d, at %d,%d, buttons %d, wheel %d\n", b, m[0], m[1], m[2], m[3]);
    for(int n = 1; n <= 4; n++) {
        b = cmm2->gamepad_axes(n, v);
        if(b >= 0) printf("pad %d: buttons %04X, LX %d LY %d RX %d RY %d L %d R %d\n", n, b, v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    printf("%d failed\n", fails);
    return fails ? 1 : 0;
}
