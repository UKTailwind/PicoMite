/* RUN-only regression test: GCC/ARMv6-M emits __aeabi_lmul for uint64_t multiplication.
 * Compile as an external ET_REL object, then RUN the .elf on RP2040 or RP2350.
 */
#include <picomite.h>

static volatile unsigned long long operand_a = 0xfedcba9876543210ULL;
static volatile unsigned long long operand_b = 0x1020304050607081ULL;

int main(void)
{
    unsigned long long a = operand_a;
    unsigned long long b = operand_b;
    if (a * b != 0x3d72edd03c543a10ULL) {
        pm_print("AEABI LMUL FAIL (1)\n");
        return 1;
    }
    operand_a = 0xffffffffffffffffULL;
    operand_b = 0xffffffffffffffffULL;
    a = operand_a;
    b = operand_b;
    if (a * b != 1ULL) {
        pm_print("AEABI LMUL FAIL (2)\n");
        return 2;
    }
    pm_print("AEABI LMUL OK\n");
    return 0;
}
