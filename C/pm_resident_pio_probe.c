/* Build ET_REL ARMv6-M soft-float for flash LIBRARY CSUB. */
#include <stdint.h>
#include <picomite.h>
#include <picomite/resident.h>

/* JMP 0: one instruction that never drives GPIO. */
static void tick(void *p)
{
    ((uint32_t *)p)[0]++;
}
void ResidentPIO(long long *cmd)
{
    uint32_t *state;
    uint16_t insn = 0x0000;
    if (*cmd == 0) {
        int pio;
        for (pio = 0; pio < 3; ++pio)
            if (pm_resident_pio_claim(pio) == 0) break;
        if (pio == 3) { pm_print("PIO SKIP: no idle block\r\n"); return; }
        if (pm_resident_pio_program(&insn, 1) ||
            pm_resident_pio_init_machine(0, 0, 0x1f000, 0xc0000,
                                         0, 1000000u, 0, 0, 0) ||
            pm_resident_pio_start(0)) {
            pm_resident_pio_release();
            pm_print("PIO INIT FAILED\r\n");
            return;
        }
        pm_print("PIO CLAIMED\r\n");
    } else if (*cmd == 1) {
        state = pm_resident_state();
        if (!state) { pm_print("PIO RESIDENT INACTIVE\r\n"); return; }
        if (state[0]) pm_print("PIO RESIDENT TICKED\r\n");
        else pm_print("PIO RESIDENT ZERO\r\n");
    } else if (*cmd == 2) {
        pm_resident_pio_release();
        pm_resident_stop();
        pm_print("PIO RELEASED\r\n");
    } else if (*cmd == 3) {
        pm_print(pm_resident_start(tick, 50) == 0 ?
                 "PIO RESIDENT INSTALLED\r\n" : "PIO RESIDENT REFUSED\r\n");
    }
}
