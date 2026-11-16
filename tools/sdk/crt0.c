/* crt0.c: the start of a loadable program (tools/sdk). MIT License, as pmimg.h.

   The loader calls _start(start) on the program's own stack with its .bss zeroed. _start notes the services, runs the
   constructors, calls main(), runs the destructors and leaves through exit(), which flushes stdio and calls _exit()
   (syscalls.c), which goes back to MMBasic with main's value. */
#include <stdlib.h>
#include "cmm2.h"

const struct cmm2_services *cmm2;
cmm2img_start *cmm2_start;

extern void (*__preinit_array_start[])(void), (*__preinit_array_end[])(void);
extern void (*__init_array_start[])(void), (*__init_array_end[])(void);
extern void (*__fini_array_start[])(void), (*__fini_array_end[])(void);
extern void cmm2_mem_bind(void);

#ifdef CMM2_MODULE
/* a module (build.py --module, PMIMG_KIND_MODULE): the loader calls _start(start) once, with its .bss zeroed, and it
   returns module_main()'s table of functions. It never ends by itself: the firmware calls its functions while the
   BASIC program runs, and its forget() before the heap it is in goes. */
extern const void *module_main(void);
const void *_start(cmm2img_start *s) __attribute__((section(".text.entry"), used));
#else
extern int main(int argc, char **argv);
int _start(cmm2img_start *s) __attribute__((section(".text.entry"), used));
#endif

/* The whole newlib (build.py --libc full) has a constructor that gives this to atexit, so that exit() would run the
   destructors (and _fini, which -nostartfiles leaves out): _start runs them itself, as with newlib-nano, so here it
   does nothing, and they run once */
void __libc_fini_array(void) {}

#ifdef CMM2_MODULE
const void *_start(cmm2img_start *s) {
    void (**f)(void);
    cmm2 = s->svc;
    cmm2_start = s;
    cmm2_mem_bind();
    for(f = __preinit_array_start; f < __preinit_array_end; f++) (*f)();
    for(f = __init_array_start; f < __init_array_end; f++) (*f)();
    return module_main();                       /* (its destructors never run: it goes with the heap) */
}
#else
int _start(cmm2img_start *s) {
    void (**f)(void);
    int r;
    cmm2 = s->svc;
    cmm2_start = s;
    cmm2_mem_bind();
    for(f = __preinit_array_start; f < __preinit_array_end; f++) (*f)();
    for(f = __init_array_start; f < __init_array_end; f++) (*f)();
    r = main(s->argc, s->argv);
    for(f = __fini_array_end; f > __fini_array_start; ) (*--f)();
    exit(r);
}
#endif
