/* hello.c: a loadable program that tries the services (tools/sdk). MIT License, as pmimg.h.

     RUN "hello.app" [, words$]       prints where it is, its arguments, the heap, the time and the clock; mallocs
                                      4 MB and fills it; times a loop
     RUN "hello.app", "code"          ends with 3 from main (the loader prints the code)
     RUN "hello.app", "leave"         leaves from inside a call, with 42
     RUN "hello.app", "loop"          prints a dot every 100 ms, calling poll(), until Ctrl-C
     RUN "hello.app", "take"          as loop, but takes the Ctrl-C itself (break_take) and returns 0
     RUN "hello.app", "keys"          shows the keys typed (con_getc) until q
     RUN "hello.app", "alloc"         takes 10 blocks of MMBasic's heap and leaves them for the loader to free
     RUN "hello.app", "quiet"         prints nothing and returns 0 (for a BASIC loop that runs it many times) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmm2.h"

static void deep(int n) {
    if(n == 0) {
        printf("leaving with 42 from inside a call\n");
        cmm2->leave(42);
        return;
    }
    deep(n - 1);
    printf("not reached\n");
}

static int dots(int take) {
    int n = 0;
    printf("a dot every 100 ms: Ctrl-C to end\n");
    for(;;) {
        uint64_t t = cmm2->micros() + 100000;
        while(cmm2->micros() < t)
            if(cmm2->poll()) {
                if(take && cmm2->break_take()) {
                    printf("\nCtrl-C taken after %d dots\n", n);
                    return 0;
                }
                printf("\npoll() saw Ctrl-C after %d dots\n", n);
                return 1;
            }
        cmm2->con_putc('.');
        n++;
    }
}

int main(int argc, char **argv) {
    int i, t[6];
    uint64_t t0, us;
    volatile float x = 0;
    const char *what = argc > 1 ? argv[1] : "";

    if(strcmp(what, "quiet") == 0) return 0;
    printf("Hello from a loadable program: main at %p, image at 0x%08lX, CPU %lu MHz, services version %lu\n",
           (void *)main, (unsigned long)cmm2_start->base, (unsigned long)(cmm2->cpu_hz() / 1000000),
           (unsigned long)cmm2->version);
    for(i = 0; i < argc; i++) printf("argv[%d] = \"%s\"\n", i, argv[i]);
    printf("heap: %lu KB at %p, stack near %p, PSRAM %lu KB\n", (unsigned long)(cmm2_start->heap_size / 1024),
           cmm2_start->heap, (void *)&i, (unsigned long)(cmm2->psram_size() / 1024));
    if(strcmp(what, "code") == 0) return 3;
    if(strcmp(what, "leave") == 0) deep(10);
    if(strcmp(what, "loop") == 0) return dots(0);
    if(strcmp(what, "take") == 0) return dots(1);
    if(strcmp(what, "keys") == 0) {
        int c;
        printf("type keys, q to end\n");
        for(;;) {
            while((c = cmm2->con_getc()) < 0)
                if(cmm2->poll()) return 1;
            printf("key %d\n", c);
            if(c == 'q') return 0;
        }
    }
    if(strcmp(what, "alloc") == 0) {
        for(i = 0; i < 10; i++) printf("alloc(%d): %p\n", 1000 * (i + 1), cmm2->alloc(1000 * (i + 1)));
        return 0;
    }
    char *m = malloc(4 << 20);
    printf("malloc(4 MB): %p\n", (void *)m);
    if(m) {
        t0 = cmm2->micros();
        memset(m, 0x55, 4 << 20);
        us = cmm2->micros() - t0;
        for(i = 0; i < (4 << 20) && m[i] == 0x55; i++) {}
        printf("filled in %lu us, %s\n", (unsigned long)us, i == (4 << 20) ? "read back OK" : "READ BACK WRONG");
        free(m);
    }
    cmm2->datetime(t);
    printf("MMBasic's clock: %04d-%02d-%02d %02d:%02d:%02d\n", t[0], t[1], t[2], t[3], t[4], t[5]);
    t0 = cmm2->micros();
    for(i = 0; i < 1000000; i++) x += i * 0.5f;
    us = cmm2->micros() - t0;
    printf("a million float multiply-adds: %lu us (x = %.0f)\n", (unsigned long)us, (double)x);
    return 0;
}
