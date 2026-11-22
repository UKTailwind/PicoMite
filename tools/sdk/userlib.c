/* userlib.c: a USER module's printf family, the firmware's (tools/sdk; build.py --user links it). MIT License, as
   pmimg.h.

   snprintf, sprintf, vsnprintf and vsprintf format with the firmware's own vsnprintf (cmm2_start->basic, loader/pmuser.h),
   so the module carries no printf of its own: the pro-forma is 3 KB instead of 17 KB. The firmware's is the Pico SDK's
   compact printf: %d, %s, %x and %f are right, but %e and %g are not (1e6 comes out as 10.000000e+05, and %g keeps
   trailing zeros: 0.5 as 0.500000), so format those with cmm2_start->basic->float_to_str. printf, vprintf, puts and
   putchar format the same way and print through MMBasic's PRINT (the console and the screen), a newline as CR LF, as
   a program's stdout has it. fprintf and the rest of stdio on a FILE are newlib's own, without floats: to write a
   float to a file, snprintf it into a buffer and fputs that. */
#include <stdarg.h>
#include <stddef.h>
#include "cmm2user.h"

#define LINE 256                                /* printf's room on the stack: longer text is formatted in temp() */

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
    return cmm2_start->basic->vsnprintf(out, n, fmt, ap);
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
    return vsnprintf(out, 0x7FFFFFFF, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

/* s printed as PRINT prints, each newline that has no CR before it given one */
static void out(const char *s) {
    char seg[LINE + 2];
    int k = 0;
    for(const char *p = s; *p; p++) {
        if(*p == '\n' && (p == s || p[-1] != '\r')) seg[k++] = '\r';
        seg[k++] = *p;
        if(k >= LINE) { seg[k] = 0; cmm2_start->basic->print(seg); k = 0; }
    }
    seg[k] = 0;
    if(k) cmm2_start->basic->print(seg);
}

int vprintf(const char *fmt, va_list ap) {
    char line[LINE];
    va_list again;
    int r;
    va_copy(again, ap);
    r = vsnprintf(line, sizeof(line), fmt, ap);
    if(r >= (int)sizeof(line)) {                /* (a long one: in memory freed when the statement ends) */
        char *big = cmm2_start->basic->temp(r + 1);
        vsnprintf(big, r + 1, fmt, again);
        out(big);
    } else if(r > 0) out(line);
    va_end(again);
    return r;
}

int printf(const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int puts(const char *s) {                       /* (GCC turns printf("text\n") into puts("text")) */
    out(s);
    cmm2_start->basic->print("\r\n");
    return 1;
}

int putchar(int c) {                            /* (and printf("%c", c) or printf("x") into putchar) */
    char s[2] = {(char)c, 0};
    out(s);
    return (unsigned char)c;
}
