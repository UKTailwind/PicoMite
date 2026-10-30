# Native C on PicoMite: RUN "file.c", TCC, ELF programs and C CSUBs

The RP2350 firmware includes a C compiler, [TinyCC](https://bellard.org/tcc/)
0.9.27 with a Thumb-2 back end. With it the firmware can:

- compile a C source and run it straight away: `RUN "hello.c"`;
- compile and link C sources into object files and executables with the
  `TCC` command, which takes TinyCC's own command line;
- run such an executable: `RUN "game.elf"`;
- build a CSUB from C sources or objects right in a BASIC program
  ([C CSUBs](#c-csubs)), with no PC toolchain and no hex.

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

## C CSUBs

A CSUB can be written in C instead of hex. In place of the hex words, the
body of the CSUB holds file names and options in double quotes:

```basic
CSUB Sum INTEGER, INTEGER, INTEGER
  "sum.c"
END CSUB

Sum 2, 3, r%
PRINT r%
```

```c
/* sum.c - the CSUB's arguments, as for any CSUB: pointers to its BASIC values */
void Sum(long long *a, long long *b, long long *r)
{
    *r = *a + *b;
}
```

The declaration line and the way the CSUB is called work exactly as for a hex
CSUB: the type list, by-reference variables, temporaries for expressions and
up to 16 arguments. Existing hex CSUBs are not affected in any way.

### What the body may hold

The quoted strings are a TinyCC command line, so all of these work and can be
mixed:

```basic
CSUB Sum INTEGER, INTEGER, INTEGER
  "sum.c", "sumExt.c"                 ' several sources
  "util.o"                            ' an object from TCC "-c util.c"
  "lib.elf"                           ' or an ELF made by TCC (it is an object too)
  "-IB:/inc -DFAST"                   ' options: -I, -D, -U, -L, -l, -w, -W...
  "@sum.rsp"                          ' a response file
END CSUB
```

- Strings are separated by commas or blanks and may continue over several
  lines. A string may hold several words.
- Hex and strings cannot be mixed in one CSUB.
- `-o`, `-c`, `-r`, `-E` and `-run` are refused, since the CSUB decides the
  output.
- **Relative names are relative to the program's directory**: the directory
  of the `.bas` file it was loaded from, as reported by MM.INFO(CURRENT). This
  applies to sources, objects, `-I` and `-L` directories and response files.
  `"sum.c"` therefore finds the file next to the program whatever the current
  directory is. A program that was not loaded from a file (one typed into the
  editor) uses the current directory.
- **Inside a response file, relative names are relative to the response
  file's own directory**, and response files may include further response
  files. Note that this differs from the `TCC` command, where TinyCC reads
  `@file` itself relative to the current directory.

### The entry point

The CSUB enters the C function with **the CSUB's name**: `Sum` above. The
match is case sensitive, so use the name exactly as it is written on the
CSUB line. If no such function exists, `main` is used, as with
armcfgen-built CSUBs. Other functions in the sources are ordinary helpers.

### When it is compiled

The CSUB is compiled and linked **when the program is saved to flash**:
`LOAD`, `AUTOSAVE`, saving from the editor, or loading into a RAM slot. It is
stored as an ordinary CSUB record, linked for the exact address it occupies.
After that:

- RUN does not compile anything, and the CSUB runs from flash with no RAM
  used, like a hex CSUB;
- the source files are no longer needed;
- compiler messages appear while the program is saved, and the save is
  abandoned with "CSUB: C compilation failed" (the program memory is left
  empty, as for any failed save).

### What the C code can use

- **Locals and constants: yes.** String literals, floating-point constants
  and `const` objects with an initializer (`static const int table[] = {...}`)
  go into the CSUB's read-only data, stored in flash with the code.
- **Variables: no.** As for any CSUB, there is no data segment, so a global
  or `static` variable that is not `const` is refused when the program is
  saved: `'count' is a variable (.bss): a CSUB can have constants and locals
  but no variables`. Keep state in the BASIC variables passed as arguments.
- **The firmware's run-time library.** All of [the run-time
  library](#run-time-library) and `picomite.h` can be called directly
  (`sin`, `memcpy`, `printf`, `pm_line`...). The calls are bound to the
  firmware by address when the program is saved.
- **The classic CSUB interface**: `#include "PicoCFunctions.h"` (built in,
  see [PicoCFunctions.h](#picocfunctionsh-the-classic-csub-interface)) and
  its CallTable (`MMPrintString`, `DrawLine`, `CFuncmSec` and so on). The C
  source of an existing CSUB usually compiles as it is.
- `<picomite/system.h>` and `<picomite/graphics.h>` (but not
  `<picomite/memory.h>`, which needs a C program's run).
- `printf`, `puts` and the other console *output* functions work and print
  on the MMBasic console.
- **Not available in a CSUB**, because a CSUB runs outside any C program:
  `malloc calloc realloc free strdup exit abort`, console input
  (`getchar fgetc getc fgets`, `stdin`) and files
  (`fopen fclose fread fwrite fseek ftell rewind feof ferror`), `pm_alloc`,
  `pm_free` and `pm_settick`. Using one is reported as `undefined symbol`
  when the program is saved. `pm_execute` and `pm_eval_*` work.
- An MMBasic error in a `pm_` call (an invalid pin, for example) is reported
  as that error at the CSUB's call, as for any MMBasic command.

### Firmware updates

A C CSUB is bound to the addresses of the firmware it was built with.
Each record carries the firmware's id and its own address:

- **RUN** checks the program's C CSUBs. If they were built for another
  firmware, it prints "Recompiling the C CSUBs for this firmware", saves the
  program again from flash (recompiling them from their sources) and runs it.
  If the sources are gone, the save fails with the compiler's error.
- **A call** of a C CSUB that does not match (for a program run in some other
  way, or a CSUB record moved to another place in flash or RAM) stops with
  "C CSUB built for another firmware or place: LOAD the program again".

### Limits

- A C CSUB is not position independent. A program copied to another flash
  slot or RAM slot without being saved again must be loaded again.
- C CSUBs cannot go into the library (`LIBRARY SAVE` refuses them).
- Each CSUB gets its own copy of everything it is linked from. Ten CSUBs
  built from one `util.c` hold ten copies of it in flash.
- `CFuncmSec` and the other interrupt vectors may point into a C CSUB, as
  they may for a hex CSUB.

---

## The C environment

### Built-in headers

These headers are built into the firmware and found with `#include <...>`.
They are served from the virtual directory `rom:/include`; nothing needs to be
copied to a drive.

`assert.h`, `ctype.h`, `float.h`, `limits.h`, `math.h`, `stdalign.h`,
`stdarg.h`, `stdbool.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h`,
`stdnoreturn.h`, `string.h`, `time.h`, `picomite.h`, and the firmware API
`picomite/system.h`, `picomite/memory.h`, `picomite/graphics.h`,
`picomite/input.h`, `picomite/basic.h` and
`PicoCFunctions.h` (see [The firmware API](#the-firmware-api-picomite)).

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

### The firmware API: `<picomite/...>`

`picomite.h` above is the original, small API and stays as it is. The
firmware's services are being opened up further in separate headers under
`picomite/`, one for each subsystem. They may repeat a few declarations of
`picomite.h`, so a program can include either or both.

All of them are version 1 of the API (`PM_API_VERSION`). A function that a
later firmware adds is listed with the version that added it. Structs that
the firmware fills start with a `size` field, which the program sets to
`sizeof` the struct before the call, so a program built against an older
header keeps working when the firmware's struct grows.

#### `<picomite/system.h>`

```c
unsigned pm_api_version(void);     /* the firmware's PM_API_VERSION */
int  pm_get_info(pm_info *info);   /* firmware, board, memory, display, PM_CAP_... */
void pm_yield(void);               /* service USB, keyboard, network, CTRL-C */
int  pm_abort_requested(void);     /* has CTRL-C been pressed? */
unsigned long long pm_timer_us(void);
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms);
```

`pm_info` holds the firmware version string (e.g. "7.00.00RC6"), the device
(`MM.DEVICE$`), the CPU clock in kHz, the SRAM heap and PSRAM sizes,
`MM.HRES`/`MM.VRES`, and capability bits: `PM_CAP_DISPLAY`,
`PM_CAP_FRAMEBUFFER`, `PM_CAP_PSRAM`, `PM_CAP_SDCARD`, `PM_CAP_USB_HOST`,
`PM_CAP_WIFI`, `PM_CAP_BLUETOOTH`, `PM_CAP_HDMI` and `PM_CAP_VGA`.

```c
pm_info info;
info.size = sizeof info;
pm_get_info(&info);
printf("%s on %s, %u KB PSRAM\n", info.firmware, info.device, info.psram / 1024);
```

`pm_yield()` belongs in any long loop that neither prints nor draws. It keeps
USB and the keyboard serviced and ends the program at once if CTRL-C has
been pressed. A program that needs to tidy up first can poll
`pm_abort_requested()` instead and return from `main()`.

#### `<picomite/memory.h>`

```c
void *pm_alloc(size_t size, int where);  /* PM_MEM_ANY, PM_MEM_SRAM, PM_MEM_PSRAM */
void  pm_free(void *p);
int   pm_mem_where(const void *p);       /* PM_MEM_SRAM, PM_MEM_PSRAM or 0 */
int   pm_get_mem_info(pm_mem_info *m);   /* SRAM and PSRAM: total and free */
```

- `pm_alloc` takes a zeroed block straight from the MMBasic heap, so the
  program chooses where it goes. Use PSRAM for big buffers, and SRAM for
  speed or for anything DMA reads.
- Blocks are whole 256-byte pages, so use it for buffers and `malloc()` for
  small objects.
- Like `malloc()` memory, every block is given back when the program ends.
- `NULL` means there is no room where asked.
- Not available in a C CSUB.

#### `<picomite/graphics.h>`

The drawing functions of `picomite.h`, and:

```c
void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int c, int bc);
void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill);
void pm_mode(int mode);
int  pm_font_width(int font, int scale);
int  pm_font_height(int font, int scale);
int  pm_get_framebuffer(pm_framebuffer *fb);
unsigned pm_fb_colour(int rgb);
```

The functions named after a BASIC command take its arguments in its order,
check them the same way and fail with the same errors:

| BASIC | C |
|---|---|
| `TEXT x, y, s$, align$, font, scale, c, bc` | `pm_text(x, y, s, align, font, scale, c, bc)` |
| `BOX x, y, w, h, lw, c, fill` | `pm_draw_box(x, y, w, h, lw, c, fill)` |
| `MODE n` | `pm_mode(n)` (VGA and HDMI builds) |

- An argument that BASIC lets you leave out is `PM_DEFAULT` in C, with the
  same default: the current font, scale, foreground and background colour
  (`FONT`, `COLOUR`), BOX's line width 1 and no fill. For `align`, `NULL` or
  `""` gives the default `"LT"`.
- `bc` may be `PM_TRANSPARENT` and `fill` may be `PM_NOFILL`, which is -1 as
  in BASIC.
- With `OPTION REFRESH` on, the display is refreshed after them, as after
  the commands.
- `pm_box` (from `picomite.h`) keeps its older form: a filled box between
  two corners. BASIC's BOX is `pm_draw_box`.

```c
/* Text MM.HRES\2, 20, "SCORE", "CM", 1, 2, RGB(CYAN)
   Box 10, 40, 100, 50, 2, RGB(WHITE), RGB(BLUE) */
pm_text(pm_hres() / 2, 20, "SCORE", "CM", 1, 2, PM_CYAN, PM_DEFAULT);
pm_draw_box(10, 40, 100, 50, 2, PM_WHITE, PM_BLUE);
```

- `pm_get_framebuffer` gives the buffer that drawing goes to: the display
  memory on VGA and HDMI, or the buffer of `FRAMEBUFFER CREATE`/`WRITE`. It
  returns its address, size, stride and format:

  | `format` | Pixel |
  |---|---|
  | `PM_FB_MONO` | 1 bit: pixel x is bit `x & 7` of byte `x / 8` |
  | `PM_FB_RGB121` | 4 bits: even x in the low nibble, odd x in the high one |
  | `PM_FB_RGB332` | 1 byte |
  | `PM_FB_RGB555` | 2 bytes, little-endian |

  It returns -1 when drawing goes straight to a panel (an SPI LCD with no
  framebuffer).
- `pm_fb_colour` converts an RGB colour to a pixel value of the current
  format.
- Writing pixels directly is far faster than `pm_pixel()`. Call
  `pm_refresh()` afterwards for buffered panels.
- The buffer and its format change with `MODE`, `RESOLUTION` and
  `FRAMEBUFFER WRITE`, so get it again after any of these.

```c
#include <picomite/graphics.h>

int main(void)
{
    pm_framebuffer fb;
    fb.size = sizeof fb;
    if (pm_get_framebuffer(&fb) || fb.format != PM_FB_RGB332)
        return 1;
    unsigned char *p = fb.pixels;
    for (int y = 0; y < fb.height; y++)
        for (int x = 0; x < fb.width; x++)
            p[y * fb.stride + x] = (unsigned char)(x ^ y);
    pm_text(8, 8, "framebuffer", "LT", 1, 2, PM_WHITE, PM_TRANSPARENT);
    return 0;
}
```

#### `<picomite/input.h>`

```c
int pm_inkey(void);   /* INKEY$: a key code, or -1 if no key is waiting */
```

The codes are those of `INKEY$`: ASCII, and from 128 the keys MMBasic names
(UP 128, DOWN 129, LEFT 130, RIGHT 131, INSERT 132, DEL 127, HOME 134,
END 135, PGUP 136, PGDN 137, F1 145 to F12 156). The same function is in
`picomite.h`. For `KEYDOWN()` use `pm_eval_int("KEYDOWN(n)")` (below).

#### `<picomite/basic.h>`: MMBasic's commands and functions

```c
void      pm_execute(const char *cmd);                     /* EXECUTE cmd$ */
long long pm_eval_int(const char *expr);                   /* EVAL(s$) */
double    pm_eval_float(const char *expr);
char     *pm_eval_str(const char *expr, char *buf, int size);
void      pm_settick(int period, void (*fn)(void), int nbr); /* SETTICK */
```

These make every MMBasic command and function available to C with exactly
its BASIC meaning, including those with no C function of their own yet:
`SPRITE`, `PLAY`, `LOAD IMAGE`, `LOAD JPG`, `BLIT`, `COLOUR`, `OPTION ...`,
`KEYDOWN()`, `MM.INFO()` and so on.

- **`pm_execute`** runs one statement as `EXECUTE` does: as if typed at the
  prompt, a single statement (no `:`) and not `RUN`. A C program has no BASIC
  variables or SUBs, so put the values into the text:

  ```c
  char cmd[64];
  pm_execute("SPRITE LOAD \"maxitrek.spr\"");
  snprintf(cmd, sizeof cmd, "SPRITE SHOW %d,%d,%d,1", n, x, y);
  pm_execute(cmd);
  pm_execute("PLAY MODFILE \"ptsfx2.mod\"");
  ```

- **`pm_eval_int`, `pm_eval_float` and `pm_eval_str`** give the value of an
  expression as `EVAL()` does. `pm_eval_int` rounds a float as BASIC does
  when it assigns one to an integer. Asking for a number from a string
  expression, or the other way round, is an error.

  ```c
  if (pm_eval_int("KEYDOWN(0)") == 0) ...
  char dev[40];
  pm_eval_str("MM.DEVICE$", dev, sizeof dev);
  ```

- An MMBasic error in the command or expression ends the program with that
  error. In a C CSUB, it is reported at the CSUB's call.
- The text is tokenised on every call (as `EXECUTE` does), which takes some
  tens of microseconds. That is fine for setting things up and for an
  occasional sound or sprite, but a hot loop is better served by the C
  functions where there is one.
- What the commands start keeps going after the program, as after a BASIC
  program run from the prompt: `RUN "x.c"` ends as `RUN` does, which stops
  sound and closes sprites and files.

**`pm_settick`** is `SETTICK period, sub, nbr` with a C function for the
SUB:

- `nbr` is 1 to 4, and period 0 stops that tick.
- A BASIC tick SUB runs between statements. Likewise the C function runs
  between the program's own calls of the firmware: in `pm_pause`,
  `pm_yield`, output, input and the `pm_` drawing calls. It is never called
  from an interrupt, and never while it is still running, so it may use
  everything the program does.
- A program that computes for a long time without any of these calls gets
  no ticks until it makes one.
- Not available in a C CSUB.

```c
static void clock_tick(void) { /* SUB clock */ }
...
pm_settick(100, clock_tick, 4);  /* SETTICK 100, clock, 4 */
pm_settick(0, NULL, 4);          /* SETTICK 0, 0, 4 */
```

### PicoCFunctions.h: the classic CSUB interface

The firmware includes the repository's `PicoCFunctions.h`, so
`#include "PicoCFunctions.h"` works anywhere: in `RUN "x.c"`, in `TCC` and in
C CSUBs. It is the interface hex CSUBs are written against. Through the
CallTable it gives the console, the drawing routines and fonts, the
framebuffer pointers, pins, PIO, audio output, the `CFuncmSec`/`CFuncInt*`
interrupt vectors, `Option` and the BASIC variable table.

- Compiled on the PicoMite, the header gets the firmware's own build
  (`PICOMITE`, `PICOMITEVGA`, `PICOMITEWEB`, `GUICONTROLS`, ...) from
  predefined `__PM_...` macros instead of the defines at its top, so `Option`
  and the variable records match the firmware it runs on.
- `NOP()` does nothing under TinyCC, which has no ARM inline assembler.
- Its functions are called through the CallTable, whose offsets never
  change. Code that uses only this header does not depend on the firmware's
  symbol addresses.

### Stopping a program

CTRL-C ends a C program and returns to the prompt, as it does for BASIC. It
is noticed whenever the program prints, reads the console, pauses or calls a
`pm_` function. A loop that does none of these cannot be interrupted, so add
`pm_yield()` (`<picomite/system.h>`) to long computations.

### Predefined macros

`__PICOMITE__`, `__TINYC__`, `__arm__`, `__ARM_EABI__`, `__STDC__`,
`__STDC_VERSION__` (199901L), `__PM_FIRMWARE__`, and `__PM_<macro>` for each
of the firmware's build macros (for `PicoCFunctions.h`). `__DATE__` and `__TIME__` come from the PicoMite
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
- The 32 KB stack is not guarded as it is used. The bottom of it holds a
  pattern that is checked when the program ends, and an overwritten pattern
  is reported as "C stack overflow (32 KB)", but by then whatever lay below
  the stack in the heap may have been damaged. Keep big arrays `static` or
  in `malloc()` memory, not in locals.
- Code is generated by TinyCC, unoptimised, and is several times slower than
  the firmware's own GCC-compiled code, but far faster than BASIC.
- Executables are relocatable objects bound to the firmware by name at RUN
  time, so they keep working after a firmware update. Only a function that
  the new firmware no longer provides makes RUN fail with
  `undefined symbol`.
- C CSUBs have the limits listed in [C CSUBs](#limits).

---

## Source files

| File | Purpose |
|---|---|
| `core/tcc/*.c, *.h` | TinyCC (library `PicoMiteTinyCC`), with PicoMite changes marked `TCC_PICOMITE` |
| `core/tcc/config.h` | TinyCC configuration: Thumb-2, EABI, softfp (the firmware's float ABI), `rom:/include` |
| `core/tcc/tcc_picomite.h` | Interface between TinyCC and the firmware |
| `core/tcc/tcc_picomite.c` | The firmware side: heap arena, files, console, run-time symbol table, private stack, MMBasic error trapping, `RUN` entry points, the `TCC` command |
| `core/tcc/include/*.h` | The built-in headers |
| `core/tcc/include/picomite/*.h` | The firmware API headers (`<picomite/...>`) |
| `PicoCFunctions.h` | The CSUB interface, also built in for TinyCC |
| `cmake/tcc_rom_headers.cmake` | Embeds the built-in headers in the firmware at build time |
| `cmake/flash_layout_check.ld` | Link-time check that the firmware ends before `FLASH_TARGET_OFFSET` |
| `core/Commands.c` (`do_run`) | `RUN "file.c"`, `RUN` of ELF files, rebuilding C CSUBs for a new firmware |
| `PicoMite.c` / `misc/FileIO.c` (`SaveProgramToFlash`, `SaveProgramToRAM`) | Compiling C CSUBs as the program is saved |
| `core/CFunction.c` (`CallCFunction`) | The check before a C CSUB is called |
| `core/tcc/tccrun.c` (`tcc_pm_relocate_csub`) | Linking a C CSUB image for its address |
| `AllCommands.h` | The `TCC` command token (rp2350 builds) |
