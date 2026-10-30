# Native C on PicoMite: RUN "file.c", TCC and ELF programs

The RP2350 firmware includes a C compiler, [TinyCC](https://bellard.org/tcc/)
0.9.27 with a Thumb-2 back end. With it the firmware can:

- compile a C source and run it straight away: `RUN "hello.c"`;
- compile and link C sources into object files and executables with the
  `TCC` command, which takes TinyCC's own command line;
- run such an executable: `RUN "game.elf"`.

The code is native Cortex-M33 code. It runs from RAM and calls the firmware
directly for its C library, console, files, display and pins.

> **Limitations**
>
> - **RP2350 only.** The RP2040 builds have neither TinyCC nor the `TCC`
>   command, and `RUN "file.c"` is not recognised there.
> - **PSRAM is close to a requirement.** The compiler and the compiled
>   program both take their memory from the MMBasic heap. This includes the
>   compiler's tables, the generated code and data, a 32 KB stack and the
>   program's own `malloc()`. Without PSRAM only the SRAM heap is available,
>   which is roughly 100-360 KB depending on the build (much less with a big display mode).
>   That is enough for very small programs at best. Expect "Not enough
>   memory" or a failed compile on a board without PSRAM.
> - **Work in progress.** This is a first integration. See
>   [Known limitations](#known-limitations).

---

## Quick start

Save this as `hello.c` on A: or B::

```c
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Hello from C, argc=%d\n", argc);
    for (int i = 1; i < argc; i++)
        printf("  argv[%d] = %s\n", i, argv[i]);
    return 0;
}
```

```
> RUN "hello.c", "one two"
Hello from C, argc=3
  argv[1] = one
  argv[2] = two
```

To build an executable once and run it later without recompiling:

```
> TCC "-o hello.elf hello.c"
> RUN "hello.elf", "one two"
```

---

## RUN "file.c" [, "args"]

Compiles the C file in memory and runs it (`tcc -run`). Nothing is written
to the drive.

- A file name that ends in `.c` or `.C` selects this path. Drive letters and
  directories work as for BASIC files, for example `RUN "B:/src/test.c"`.
- `args` is split at blanks into `argv[1]`, `argv[2]` and so on. A word in
  double quotes may contain blanks. `argv[0]` is the file name.
- As with a BASIC program, RUN first clears the BASIC variables, open files
  and heap, which leaves the whole heap to the compiler.
- The predefined macro `__PICOMITE__` is set, so code can test
  `#ifdef __PICOMITE__`.
- Compiler messages appear on the console in the usual form
  (`file.c:3: error: ...`). If compilation fails, RUN then raises the MMBasic
  error **C compilation failed**.
- `#include "x.h"` looks next to the source file first, then in the
  directories given with `-I` (see `TCC`). `#include <x.h>` finds the
  [built-in headers](#built-in-headers).

The program ends when `main()` returns or `exit()` is called. The return
value is not reported.

---

## TCC "command line"

`TCC` runs TinyCC with the given command line, much as `tcc` runs on a PC.
It produces files and does not run anything unless you add `-run`.

```
TCC "-c util.c"                       ' object file util.o
TCC "-o game.elf main.c util.o"       ' executable game.elf
TCC "main.c util.c gfx.c"             ' executable main.elf (named after the first file)
TCC "@game.rsp"                       ' arguments read from a response file
TCC "-E main.c -o main.i"             ' preprocess only
TCC "-run test.c arg1 arg2"           ' compile and run, like RUN "test.c"
TCC "-v"                              ' version
TCC "-h"                              ' option summary
```

- The command line is one string expression, so it can be built at run time,
  for example `TCC "-o " + out$ + " " + src$`. Words are separated by blanks,
  and a word in double quotes (`CHR$(34)`) may contain blanks.
- `-D__PICOMITE__` is always added.
- Unlike RUN, `TCC` does not clear the BASIC variables, so it can be used from
  inside a BASIC program, for example as a build script.
- If TinyCC reports an error, the MMBasic error **TCC failed** follows its
  messages.

### Output kinds

| Options | Result |
|---|---|
| *(none of the below)* | **Executable**: every input linked into one ELF file, `-o` name or `<first file>.elf` |
| `-c` | One **object file** per source, `name.o` (or `-o` with a single source) |
| `-r` | All inputs merged into one relocatable object, with no checks |
| `-E` | Preprocessor output, to the console or the `-o` file |
| `-run file [args]` | Compile and run in memory, writing nothing; the words after `file` are the program's `argv` |

#### What "executable" means here

There is no dynamic loader on PicoMite and the load address (somewhere in the
heap) is not known in advance, so a traditional fixed-address ELF executable
cannot be produced. Instead:

- the executable is the whole program linked into **one relocatable ELF
  object** (the same as `-r`);
- `TCC` checks it the way a linker would: every symbol it still needs must
  exist in the firmware's run-time library, and the program must define
  `main()`. Otherwise you get `undefined symbol 'foo'` or
  `undefined symbol 'main'`, no file is written and **TCC failed** is raised;
- `RUN` loads the file, binds the remaining references to the firmware and
  calls `main()`.

### Useful options

| Option | Meaning |
|---|---|
| `-o file` | Output file name |
| `-c` | Compile only (object file per source) |
| `-r` | Merge into one relocatable object, unchecked |
| `-E` | Preprocess only |
| `-run` | Compile (and link) in memory, then run |
| `-Idir` | Add an include directory, for example `-IB:/inc` |
| `-Dsym[=val]` / `-Usym` | Define / undefine a macro |
| `-w` | Suppress all warnings |
| `-Wall`, `-Werror`, `-Wno-xxx` | Warning control |
| `-std=c99` / `-std=c11` | Language level (C99 is the default) |
| `-xc` / `-xb` / `-xn` | Treat the following files as C / binary (object) / by extension |
| `-nostdinc` | Do not search the built-in headers |
| `-v`, `-vv` | Version; `-vv` also lists the files opened |
| `-bench` | Compilation statistics |
| `@file` | Insert the arguments read from `file` (a response file) |

Lists of directories (for example `-I`) are separated with `;`, not `:`,
because `:` follows a drive letter. Path and extension comparisons ignore
case, so `X.C` is a C source.

Not supported: `-shared`, building archives (`-ar`), `-m32`/`-m64`, and
dynamic libraries. `-l`/`-L` only find static `lib*.a` archives built
elsewhere.

### Response files (projects)

A response file holds arguments separated by blanks or new lines, the same as
on the command line, and is given as `@name`:

```
' game.rsp
-IB:/game/inc
-DLEVELS=8
-o B:/game/game.elf
B:/game/src/main.c
B:/game/src/map.c
B:/game/src/sprites.c
```

```
> TCC "@B:/game/game.rsp"
> RUN "B:/game/game.elf"
```

Relative names inside a response file are relative to the **current
directory**, not to the response file's own directory.

### Separate compilation

```
> TCC "-c map.c"
> TCC "-c sprites.c"
> TCC "-o game.elf main.c map.o sprites.o"
> RUN "game.elf"
```

---

## RUN "file" [, "args"] for ELF files

If the file named in RUN is not a `.c` source and **starts with the ELF
signature** (`7F 45 4C 46`), RUN loads it as a TinyCC object
(`tcc -xb -run file args...`):

- an executable made by `TCC` runs at once;
- any other ELF object (`.o` from `-c`, or the output of `-r`) is linked to
  the firmware as it is loaded. If something is missing (an undefined symbol
  or no `main`), the usual link errors are printed, followed by the MMBasic
  error **Cannot run the ELF file**.

Arguments and the clearing of the BASIC state are the same as for
`RUN "file.c"`. Files that are not ELF are loaded as BASIC programs as before,
with the usual `.bas` default extension.

---

## The C environment

### Built-in headers

These headers are built into the firmware and found with `#include <...>`.
They are served from the virtual directory `rom:/include`; nothing needs to be
copied to a drive.

`assert.h`, `ctype.h`, `float.h`, `limits.h`, `math.h`, `stdalign.h`,
`stdarg.h`, `stdbool.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h`,
`stdnoreturn.h`, `string.h`, `time.h`, `picomite.h`.

They declare exactly what the run-time library below provides. A function
that is declared nowhere and not in the library ends with
`undefined symbol`.

### Run-time library

| Header | Functions |
|---|---|
| stdio.h | `printf fprintf vprintf vfprintf sprintf snprintf vsprintf vsnprintf puts putchar fputs fputc putc getchar fgetc getc fgets fopen fclose fread fwrite fseek ftell rewind feof ferror fflush`, `stdin stdout stderr` |
| stdlib.h | `malloc calloc realloc free exit abort atoi atol atof strtol strtoul strtoll strtoull strtod strtof abs labs llabs rand srand qsort bsearch` |
| string.h | `memcpy memmove memset memcmp memchr strlen strcpy strncpy strcat strncat strcmp strncmp strchr strrchr strstr strspn strcspn strpbrk strtok strdup` |
| ctype.h | `isalnum isalpha iscntrl isdigit isgraph islower isprint ispunct isspace isupper isxdigit tolower toupper` |
| math.h | `sin cos tan asin acos atan atan2 sinh cosh tanh exp exp2 log log2 log10 pow sqrt cbrt hypot fabs floor ceil round trunc fmod ldexp frexp modf` and `sinf cosf tanf atan2f sqrtf powf expf logf fabsf floorf ceilf` |
| time.h | `time` (the PicoMite clock, or -1 if it is not set), `clock` (milliseconds, `CLOCKS_PER_SEC` 1000) |

These are the firmware's own routines. Formatting is done by the SDK's
printf, which handles `%f`, `%e`, `%g` and `%lld`. The maths functions are the
SDK's fast float and double routines. `double` arithmetic is done in
software, `float` on the FPU.

### Console

- `stdout` and `stderr` print on the MMBasic console (serial/USB and the
  screen, as `PRINT` does). `\n` is sent as CR LF.
- `stdin` reads one line at a time, with echo and backspace editing. `getchar()`
  returns its characters one by one, ending with `'\n'`.
- For single keys without waiting, use `pm_inkey()` from `picomite.h`.

### Files

`fopen()` takes MMBasic file names (`"B:/data/log.txt"`, or a name relative
to the current directory on the current drive). Supported modes are `"r"`,
`"rb"`, `"w"` and `"wb"`. Append and update modes (`"a"`, `"r+"`, ...) are
not supported and `fopen` returns `NULL`. A program can have up to 8 files
open at once, within MMBasic's own limit of 10 open files.

File errors never stop the program with an MMBasic error. `fopen` returns
`NULL`, `fread`/`fwrite` return short counts, and `ferror()` is set.

### Memory

- `malloc()` takes memory from the same arena as the compiler, which is built
  from the MMBasic heap: SRAM first, then PSRAM.
- Everything is given back when the program ends, so an unfreed block does
  not leak.
- Until a C program is run, the compiler uses no heap at all. Only its code
  (about 180 KB of flash) and some 0.8 KB of static RAM are permanent.
- The compiler and the program run on a private **32 KB stack**. Deep
  recursion or large local arrays can overflow it, and there is no check for
  that.

### picomite.h: the firmware's display, keyboard, timing and pins

```c
#include <picomite.h>

int  pm_hres(void);                  /* MM.HRES */
int  pm_vres(void);                  /* MM.VRES */
void pm_cls(int colour);             /* CLS colour */
void pm_pixel(int x, int y, int colour);
void pm_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour);        /* filled */
void pm_circle(int x, int y, int r, int width, int colour, int fill);
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int fill);
void pm_refresh(void);               /* push the drawing to buffered panels */

void pm_print(const char *s);        /* PRINT without a new line */
int  pm_inkey(void);                 /* a key code, or -1 if none (INKEY$) */

unsigned long long pm_timer_us(void);/* microseconds since power-up */
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms);      /* PAUSE */

void pm_pin_mode(int gp, int mode);  /* SETPIN GPn, PM_DIN / PM_DOUT */
void pm_pin_write(int gp, int value);
int  pm_pin_read(int gp);
```

- Colours are 24-bit RGB as in MMBasic: `RGB(r,g,b)`, `PM_RED`, `PM_WHITE`
  and so on.
- `fill` is a colour, or `PM_NOFILL` for an outline.
- Pins are given by GP number.
- An MMBasic error in any of these calls (no display configured, an invalid
  or reserved pin, ...) ends the program and is reported as that MMBasic
  error.

Example:

```c
#include <picomite.h>

int main(void)
{
    pm_cls(PM_BLACK);
    for (int i = 0; i < pm_vres(); i += 8)
        pm_line(0, i, pm_hres() - 1, pm_vres() - 1 - i, 1, RGB(i, 255 - i, 128));
    pm_circle(pm_hres() / 2, pm_vres() / 2, 60, 2, PM_YELLOW, PM_NOFILL);
    pm_refresh();
    while (pm_inkey() < 0)
        pm_pause(10);
    return 0;
}
```

### Stopping a program

CTRL-C ends a C program and returns to the prompt, as it does for BASIC. It
is noticed whenever the program prints, reads the console, pauses or calls a
`pm_` function. A loop that does none of these cannot be interrupted, so add
a `pm_inkey()` or `pm_pause()` to long computations.

### Predefined macros

`__PICOMITE__`, `__TINYC__`, `__arm__`, `__ARM_EABI__`, `__STDC__`,
`__STDC_VERSION__` (199901L). `__DATE__` and `__TIME__` come from the PicoMite
clock. If the clock has not been set (`TIME$`, `DATE$`, RTC or NTP), they are
`??` and a warning is printed.

---

## Known limitations

- **RP2350 only. PSRAM is strongly recommended** (see the top of this
  document).
- No `scanf` family. Read lines with `fgets` and convert with `strtol`,
  `strtod` and similar.
- `fopen` supports read and write (truncate) modes only.
- No `setjmp`/`longjmp`, `signal`, `errno` values, locales, wide characters or
  threads.
- No debugger or backtrace support (`-g` is accepted but has no use here).
- A tight loop that never prints, reads input or calls a `pm_` function cannot
  be stopped with CTRL-C.
- The 32 KB stack is not guarded.
- Code is generated by TinyCC, unoptimised, and is several times slower than
  the firmware's own GCC-compiled code, but far faster than BASIC.
- Executables are relocatable objects bound at RUN time. They are not
  portable to other firmware versions if the run-time library changes, so
  recompile after a firmware update.

---

## Source files

| File | Purpose |
|---|---|
| `core/tcc/*.c, *.h` | TinyCC (library `PicoMiteTinyCC`), with PicoMite changes marked `TCC_PICOMITE` |
| `core/tcc/config.h` | TinyCC configuration: Thumb-2, EABI, softfp (the firmware's float ABI), `rom:/include` |
| `core/tcc/tcc_picomite.h` | Interface between TinyCC and the firmware |
| `core/tcc/tcc_picomite.c` | The firmware side: heap arena, files, console, run-time symbol table, private stack, MMBasic error trapping, `RUN` entry points, the `TCC` command |
| `core/tcc/include/*.h` | The built-in headers |
| `cmake/tcc_rom_headers.cmake` | Embeds the built-in headers in the firmware at build time |
| `cmake/flash_layout_check.ld` | Link-time check that the firmware ends before `FLASH_TARGET_OFFSET` |
| `core/Commands.c` (`do_run`) | `RUN "file.c"` and `RUN` of ELF files |
| `AllCommands.h` | The `TCC` command token (rp2350 builds) |
