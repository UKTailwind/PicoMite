/* Compile externally as ELF32/ARM ET_REL for cortex-m0plus, soft-float.
   One object tests both RUN "..." (main) and CSUB Hello (Hello).
   CallTable slot 1 has putConsole(int).  No TinyCC or libc needed. */
extern const void *const __picomite_calltable[];

static void put(int c)
{
    ((void (*)(int))__picomite_calltable[1])(c);
}

static void say(const char *s)
{
    while (*s) put((unsigned char)*s++);
}

void Hello(void)
{
    say("ELF CSUB OK\r\n");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    say("ELF RUN OK\r\n");
    return 0;
}
