#include <stdio.h>
#include <picomite/basic.h>
#include <picomite/vars.h>

static unsigned char *s;
static pm_varinfo si;
static void too_long(void *p) { (void)p; pm_str_set(s, si.size, "this is longer than sixteen"); }

int main(void)
{
    pm_varinfo vi;
    long long *a = pm_dim("buf%(9)", &vi);
    double *x, *m;
    char t[40];
    int i;

    printf("buf%%: type %d, %d dims, %d elements of %d bytes\n", vi.type, vi.ndims, vi.elements, vi.element_size);
    for (i = 0; i < vi.elements; i++)
        a[i] = i * i;
    pm_execute("PRINT \"BASIC sees buf%(3) =\"; buf%(3)");
    printf("EVAL buf%%(4) = %lld\n", pm_eval_int("buf%(4)"));
    pm_execute("MATH SET 7, buf%()");
    printf("after MATH SET: a[0] = %lld, a[9] = %lld\n", a[0], a[9]);

    x = pm_dim("x!", NULL);
    *x = 1.5;
    printf("x!*2 = %g\n", pm_eval_float("x!*2"));

    m = pm_dim("m!(2, 3)", &vi);
    printf("m!: %d dims: %d x %d\n", vi.ndims, vi.dims[0], vi.dims[1]);
    m[1 + 2 * vi.dims[0]] = 42; /* m!(1, 2) */
    printf("m!(1,2) = %g\n", pm_eval_float("m!(1, 2)"));
    printf("element address ok: %d\n", (double *)pm_var("m!(1, 2)", NULL) == &m[1 + 2 * vi.dims[0]]);

    s = pm_dim("s$ LENGTH 16", &si);
    pm_str_set(s, si.size, "hello");
    pm_execute("PRINT s$; \" \"; LEN(s$)");
    pm_execute("s$ = s$ + \" world\"");
    printf("back in C: %s\n", pm_str_get(s, t, sizeof t));
    if (pm_try(too_long, NULL))
        printf("caught: %s\n", pm_errmsg());
    if (pm_try((void (*)(void *))pm_erase, "nosuch%"))
        printf("caught: %s\n", pm_errmsg());
    pm_erase("buf%");
    return 0;
}
