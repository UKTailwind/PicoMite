/* M2 NES-PAD: GP20=CLK, GP21=LATCH, GP26=DATA (active low).
 * Default test uses the pad; optional GP argument tests a manual input.
 * All pins must be free in PicoMite. No GPIO wiring is needed on M2. */
#include <picomite/input.h>
#include <picomite/system.h>
#include <picomite/basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* Existing PicoMite C symbols. EXT_DIG_OUT=8, EXT_NOT_CONFIG=0. */
extern void pm_pin_mode(int gp, int mode);
extern void pm_pin_write(int gp, int value);
extern int pm_pin_read(int gp);

enum { CLK=20, LATCH=21, DATA=26, DOUT=8, OFF=0 };
static volatile unsigned rises, falls;

static void on_edge(int gp, int level)
{
    (void)gp;
    if (level) ++rises;
    else ++falls;
}

struct claim_arg { int gp, pull; };
static void claim_do(void *p)
{
    struct claim_arg *a = p;
    pm_gpio_on_edge(a->gp, PM_GPIO_BOTH, a->pull, on_edge);
}
static int claim(int gp, int pull)
{
    struct claim_arg a = {gp, pull};
    int err = pm_try(claim_do, &a);
    if (err) {
        printf("SKIP: GP%d unavailable: %s\n", gp, pm_errmsg());
        pm_error_clear();
    }
    return !err;
}

static int manual_test(int gp)
{
    unsigned long long end;
    if (!claim(gp, PM_GPIO_PULLUP)) return 0;
    rises = falls = 0;
    printf("GP%d: connect briefly to GND and release (12 s)\n", gp);
    end = pm_timer_us() + 12000000ull;
    while (pm_timer_us() < end && (!rises || !falls)) pm_gpio_poll();
    pm_gpio_on_edge(gp, 0, 0, NULL);
    printf("rise=%u fall=%u\n", rises, falls);
    if (!rises || !falls) { puts("SKIP: no complete transition"); return 0; }
    puts("GPIO edge callback PASS");
    return 0;
}

static unsigned read_nes(void)
{
    unsigned buttons = 0;
    pm_pin_write(CLK, 0);
    pm_pin_write(LATCH, 1);
    pm_delay_us(15);
    pm_pin_write(LATCH, 0);
    pm_delay_us(15);
    pm_gpio_poll();
    for (int i = 0; i < 8; ++i) {
        if (!pm_pin_read(DATA)) buttons |= 1u << i;
        pm_gpio_poll();
        pm_pin_write(CLK, 1);
        pm_delay_us(15);
        pm_gpio_poll();
        pm_pin_write(CLK, 0);
        pm_delay_us(15);
        pm_gpio_poll();
    }
    return buttons;
}

static void run_nes(void *unused)
{
    unsigned last = 0, stable = 0, buttons = 0;
    unsigned long long end;
    (void)unused;
    pm_pin_mode(CLK, DOUT);
    pm_pin_mode(LATCH, DOUT);
    pm_pin_write(CLK, 0);
    pm_pin_write(LATCH, 0);
    rises = falls = 0;
    puts("M2 NES-PAD: hold ONE button (A is best) for 12 seconds");
    end = pm_timer_us() + 12000000ull;
    while (pm_timer_us() < end) {
        buttons = read_nes();
        if (buttons != 0 && buttons != 255 && buttons == last) ++stable;
        else stable = 0;
        last = buttons;
        if (stable >= 2 && rises && falls) break;
        pm_pause(1);
    }
    printf("NES pressed=0x%02x rise=%u fall=%u\n", buttons, rises, falls);
    if (stable >= 2 && rises && falls)
        puts("GPIO edge callback PASS");
    else
        puts("SKIP: no stable single-button NES response with both edges");
}

static int nes_test(void)
{
    const int pins[] = {DATA, CLK, LATCH};
    int claimed = 0;
    int err;
    for (int i = 0; i < 3; ++i) {
        if (!claim(pins[i], i == 0 ? PM_GPIO_PULLUP : PM_GPIO_NOPULL))
            goto cleanup;
        ++claimed;
    }
    /* Checked all three against BASIC reservations before driving outputs. */
    pm_gpio_on_edge(CLK, 0, 0, NULL);
    pm_gpio_on_edge(LATCH, 0, 0, NULL);
    err = pm_try(run_nes, NULL);
    pm_pin_write(CLK, 0);
    pm_pin_write(LATCH, 0);
    pm_pin_mode(CLK, OFF);
    pm_pin_mode(LATCH, OFF);
    if (err) {
        printf("FAIL: NES test: %s\n", pm_errmsg());
        pm_error_clear();
    }
cleanup:
    for (int i = 0; i < claimed; ++i)
        pm_gpio_on_edge(pins[i], 0, 0, NULL);
    return 0;
}

int main(int argc, char **argv)
{
    return argc > 1 ? manual_test(atoi(argv[1])) : nes_test();
}
