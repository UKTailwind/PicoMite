/* API v31: build as ARMv6-M/soft-float ET_REL for a flash LIBRARY CSUB.
 * Test with a freely configurable GPIO input (e.g. GP26 only when unused).
 * Connect the input briefly to GND and release with PULLUP configured. */
#include <stdint.h>
#include <stdio.h>
#include <picomite.h>
#include <picomite/resident.h>

static void timer_callback(void *state)
{
    ++((uint32_t *)state)[0];
}
static void gpio_callback(int gp, unsigned events, void *state)
{
    uint32_t *s = state;
    (void)gp;
    if (events & PM_RESIDENT_GPIO_RISE) ++s[1];
    if (events & PM_RESIDENT_GPIO_FALL) ++s[2];
}
void ResidentGpioProbe(long long *command, long long *gp)
{
    char b[96];
    uint32_t *s;
    if (*command == 0) {
        int had_resident = pm_resident_active();
        if (!had_resident && pm_resident_start(timer_callback, 50)) {
            pm_print("RESIDENT GPIO: INSTALL REFUSED\r\n");
            return;
        }
        if (pm_resident_gpio_claim((int)*gp)) {
            if (!had_resident) pm_resident_stop();
            pm_print("RESIDENT GPIO: PIN REFUSED\r\n");
            return;
        }
        if (pm_resident_gpio_on_edge((int)*gp, PM_RESIDENT_GPIO_BOTH,
                                     PM_RESIDENT_GPIO_PULLUP, gpio_callback)) {
            pm_resident_gpio_release((int)*gp);
            if (!had_resident) pm_resident_stop();
            pm_print("RESIDENT GPIO: EDGE REFUSED\r\n");
            return;
        }
        pm_print("RESIDENT GPIO: INSTALLED\r\n");
    } else if (*command == 1) {
        s = pm_resident_state();
        if (!s) { pm_print("RESIDENT GPIO: INACTIVE\r\n"); return; }
        snprintf(b, sizeof b, "RESIDENT GPIO: tick=%lu rise=%lu fall=%lu\r\n",
                 (unsigned long)s[0], (unsigned long)s[1],
                 (unsigned long)s[2]);
        pm_print(b);
    } else if (*command == 2) {
        pm_resident_gpio_off();
        if (pm_resident_gpio_release((int)*gp) == 0 &&
            pm_resident_stop() == 0)
            pm_print("RESIDENT GPIO: STOPPED\r\n");
        else
            pm_print("RESIDENT GPIO: RELEASE FAILED\r\n");
    } else if (*command == 3) {
        pm_resident_gpio_off();
        pm_print("RESIDENT GPIO: DETACHED (GPIO STILL CLAIMED)\r\n");
    }
}
