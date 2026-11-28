/* cmm2_stdio.c: the C library's file calls as this app needs them (the link's --wrap options, make.py), for this app
   alone. MIT License, as cmm2img.h.
   - fopen and fdopen with a 16 KB buffer. newlib-nano gives each file BUFSIZ, 1 KB, so a download reached the card 1 KB
     at a time, each a FatFs write of its own: 164 KB/s (27 Nov 2026). curl's output files and mbedTLS's read of the CA
     bundle both go through these.
   - fgets reads a line's CR LF as LF, as a text-mode stream does on Windows, where curl expects it. MMBasic's PRINT #
     ends its lines with CR LF, and curl reads its option files (-K), header files (-H @file) and the like with fgets,
     so that a -K file written by a program refused "silent<CR>" as an unknown option (28 Nov 2026). curl reads data
     (-d @file, -T, the CA bundle) with fread, which is left as it is.
   - _write, the console's output: Ctrl-C ends the program there too (checked every 20 ms), as port/cmm2_net.c's pump
     does while it waits on the network. Without it --manual or --help all ran to the end whatever was pressed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmm2.h"

#define FILE_BUF 16384

FILE *__real_fopen(const char *name, const char *mode);
FILE *__real_fdopen(int fd, const char *mode);
char *__real_fgets(char *s, int n, FILE *f);
int __real__write(int fd, const char *p, int n);

static FILE *buffered(FILE *f) {
    if(f) setvbuf(f, NULL, _IOFBF, FILE_BUF);   /* (before any read or write, as setvbuf must be) */
    return f;
}

FILE *__wrap_fopen(const char *name, const char *mode) {
    return buffered(__real_fopen(name, mode));
}

FILE *__wrap_fdopen(int fd, const char *mode) {
    FILE *f = __real_fdopen(fd, mode);
    if(f && fd > 2) buffered(f);                /* (not the console's descriptors) */
    return f;
}

char *__wrap_fgets(char *s, int n, FILE *f) {
    char *r = __real_fgets(s, n, f);
    if(r) {
        size_t l = strlen(r);
        if(l >= 2 && r[l - 2] == '\r' && r[l - 1] == '\n') {
            r[l - 2] = '\n';
            r[l - 1] = 0;
        }
        else if(l >= 1 && r[l - 1] == '\r') {   /* a CR at the buffer's end: its LF next, if it has one */
            int c = getc(f);
            if(c == '\n') r[l - 1] = '\n';
            else if(c != EOF) ungetc(c, f);
        }
    }
    return r;
}

int __wrap__write(int fd, const char *p, int n) {
    static uint64_t last;
    static int stopping;
    if((fd == 1 || fd == 2) && !stopping) {
        uint64_t now = cmm2->micros();
        if(now - last >= 20000) {
            last = now;
            if(cmm2->poll()) {
                stopping = 1;                   /* (exit's flush comes back here) */
                __real__write(2, "\n^C\n", 4);
                exit(130);
            }
        }
    }
    return __real__write(fd, p, n);
}
