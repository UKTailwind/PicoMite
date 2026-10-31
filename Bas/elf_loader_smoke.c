/* Compile externally as ELF32/ARM ET_REL for cortex-m0plus, soft-float.
   One object tests both RUN "..." (main) and CSUB Hello (Hello).
   CallTable slot 1 has putConsole(int).  No TinyCC or libc needed.
   Example:
    C:\Users\New\.pico-sdk\toolchain\13_3_Rel1\bin\arm-none-eabi-gcc.exe \
     -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -ffreestanding \
     -fno-unwind-tables -O1 -c Bas\elf_loader_smoke.c -o elf_loader_smoke.o
 */
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
