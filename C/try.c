#include <stdio.h>
#include <picomite/basic.h>
#include <picomite/sprite.h>
static void show(void *p) { pm_sprite_show(*(int *)p, 10, 10, 1, PM_DEFAULT); }
int main(void)
{
    int n = 99;
    long long v;
    char s[40];
    if (pm_try(show, &n))
        printf("%d %s\n", pm_errno(), pm_errmsg());
    if (pm_execute_try("GUI CURSOR ON 0, 320, 240"))
        printf("no cursor: %s\n", pm_errmsg());
    if (!pm_eval_int_try("MM.VRES", &v) && !pm_eval_str_try("DATE$", s, sizeof s))
        printf("%lld %s\n", v, s);
    pm_error_clear();
    return 0;
}
