/* Build as ET_REL ARMv6-M soft-float and copy to B:/pm_resident_probe.elf.
 * Install via LIBRARY LOAD "pm_resident_probe.bas", O. */
#include <stdint.h>
#include <stdio.h>
#include <picomite.h>
#include <picomite/resident.h>

static void service(void *state)
{
    ++((uint32_t *)state)[0]; /* No BASIC calls or RUN-scoped resources. */
}
void ResidentProbe(long long *command)
{
    char b[64];
    if (*command == 0) {
        pm_print(pm_resident_start(service, 50) == 0
                 ? "RESIDENT INSTALLED\r\n" : "RESIDENT REFUSED\r\n");
    } else if (*command == 1) {
        uint32_t *state = pm_resident_state();
        if (!state) {
            pm_print("RESIDENT INACTIVE\r\n");
            return;
        }
        snprintf(b, sizeof b, "RESIDENT TICKS %lu\r\n",
                 (unsigned long)state[0]);
        pm_print(b);
    } else if (*command == 2) {
        pm_resident_stop();
        pm_print("RESIDENT STOPPED\r\n");
    }
}
