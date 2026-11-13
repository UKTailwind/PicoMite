/* API v30: explicit GPIO reservations survive BASIC END/RUN.
 * No physical GPIO configuration or external wiring is needed. */
#include <stdint.h>
#include <picomite.h>
#include <picomite/resident.h>

static void tick(void *state) { ++((uint32_t *)state)[0]; }
void ResidentGPIOClaim(long long *cmd)
{
    uint32_t *s = pm_resident_state();
    int gp;
    if (*cmd == 0) {
        if (pm_resident_start(tick, 50)) {
            pm_print("GPIO CLAIM START FAILED\r\n"); return;
        }
        s = pm_resident_state();
        /* Claims are passive: no pin mux/direction/value is changed. */
        for (gp = 0; gp < 48; ++gp)
            if (pm_resident_gpio_claim(gp) == 0) break;
        if (gp == 48) {
            pm_print("GPIO CLAIM SKIP: NO FREE GPIO\r\n");
            pm_resident_stop(); return;
        }
        /* The native GPIO API must be usable by the pin's owner. */
        pm_pin_mode(gp, PM_DIN);
        s[1] = (uint32_t)(gp + 1);
        if (pm_resident_stop() != -1)
            pm_print("GPIO CLAIM STOP GUARD FAILED\r\n");
        else
            pm_print("GPIO CLAIM INSTALLED\r\n");
    } else if (*cmd == 1) {
        if (!s || !s[1]) {
            pm_print("GPIO CLAIM MISSING\r\n"); return;
        }
        pm_print(pm_resident_active() && pm_resident_stop() == -1 ?
                 "GPIO CLAIM RETAINED\r\n" : "GPIO CLAIM LOST\r\n");
    } else if (*cmd == 2) {
        if (!s || !s[1]) {
            pm_print("GPIO CLAIM MISSING\r\n"); return;
        }
        gp = (int)s[1] - 1;
        pm_pin_mode(gp, 0); /* Restore EXT_NOT_CONFIG before release. */
        if (pm_resident_gpio_release(gp) || pm_resident_stop())
            pm_print("GPIO CLAIM RELEASE FAILED\r\n");
        else
            pm_print("GPIO CLAIM RELEASED\r\n");
    }
}
