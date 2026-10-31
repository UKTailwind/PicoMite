/* Load into the PicoMite library first:
     FUNCTION Twice%(n AS INTEGER) AS INTEGER
       Twice% = n * 2
     END FUNCTION
   Then run this C file, or use the same API from a C CSUB. */
#include <picomite/basic.h>
#include <stdio.h>

int main(void)
{
    pm_basic_value in = { .type = PM_BASIC_INTEGER, .integer = 21 };
    pm_basic_value out = {0};
    pm_basic_function("Twice%", &in, 1, &out, NULL, 0);
    if (out.type != PM_BASIC_INTEGER || out.integer != 42)
        return 1;
    printf("BASIC FUNCTION returned %lld\n", out.integer);
    return 0;
}
