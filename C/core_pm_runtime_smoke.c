/* RP2040/RP2350 native C runtime smoke: a single ARMv6-M ET_REL object.
 * Copy this source and its compiled .elf to B:, then RUN the .elf.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <picomite/system.h>
#include <picomite/basic.h>
#include <picomite/vars.h>

int main(int argc, char **argv)
{
    char line[80];
    int *v;
    FILE *f;
    struct stat st;
    pm_info info;
    pm_varinfo vi;
    long long *arr;
    int i;
    (void)argc; (void)argv;
    if (pm_api_version() != PM_API_VERSION) return 1;
    info.size = sizeof(info);
    if (pm_get_info(&info) || info.api_version != PM_API_VERSION) return 2;
    v = calloc(16, sizeof(*v));
    if (!v) return 3;
    for (i = 0; i < 16; ++i) v[i] = i * 11;
    v = realloc(v, 32 * sizeof(*v));
    if (!v) return 4;
    if (v[15] != 165 || sqrt(81.0) != 9.0) { free(v); return 5; }
    memset(v + 16, 0, 16 * sizeof(*v));
    free(v);
    if (pm_eval_int("2+3") != 5) return 6;
    arr = pm_dim("test%(3)", &vi);
    if (!arr || vi.elements < 3) return 7;
    for (i = 0; i < vi.elements; ++i) arr[i] = i * i;
    if (stat("B:/core_pm_runtime_smoke.c", &st) || !S_ISREG(st.st_mode)) return 8;
    f = fopen("B:/core_pm_runtime_smoke.c", "r");
    if (!f) return 9;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 10; }
    fclose(f);
    puts("COMMON C RUNTIME OK");
    return 0;
}
