/* External-GCC ELF test for standard ARM EABI C helpers on RP2040/RP2350.
 * Compile for Cortex-M0+ and RUN the resulting ET_REL object.
 * Keep input operands volatile so the compiler cannot fold the tests.
 */
#include <picomite.h>
#include <stdint.h>
#include <string.h>

static volatile float f1 = 7.5f, f2 = 2.5f;
static volatile double d1 = 7.5, d2 = 2.5;
static volatile uint64_t u1 = 0xfedcba9876543210ULL;
static volatile uint64_t u2 = 0x1020304050607081ULL;
static volatile int si = -11;
static volatile unsigned ui = 11;

struct block { unsigned words[64]; };

int main(void)
{
    struct block a, b;
    float fa = f1, fb = f2;
    double da = d1, db = d2;
    uint64_t x = u1, y = u2;
    unsigned i;

    if (fa + fb != 10.0f || fa - fb != 5.0f ||
        fa * fb != 18.75f || fa / fb != 3.0f || !(fa > fb)) return 1;
    if (da + db != 10.0 || da - db != 5.0 ||
        da * db != 18.75 || da / db != 3.0 || !(da > db)) return 2;
    if ((int)fa != 7 || (unsigned)fb != 2 ||
        (float)si != -11.0f || (float)ui != 11.0f) return 3;
    if (x * y != 0x3d72edd03c543a10ULL) return 4;

    memset(&a, 0, sizeof a);
    for (i = 0; i < 64; ++i) b.words[i] = i + 1;
    memcpy(&a, &b, sizeof a);
    for (i = 0; i < 64; ++i) if (a.words[i] != i + 1) return 5;
    memset(&a, 0, sizeof a);
    for (i = 0; i < 64; ++i) if (a.words[i] != 0) return 6;

    pm_print("AEABI COVERAGE OK\n");
    return 0;
}
