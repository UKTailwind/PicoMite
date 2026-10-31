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
  (`fopen fclose fread fwrite fseek ftell rewind feof ferror`,
  `opendir readdir closedir`), `pm_alloc`, `pm_free` and `pm_settick`. Using one is reported as `undefined symbol`
  when the program is saved. `pm_execute`, `pm_eval_*` and `pm_try` work,
  and `pm_var` (`<picomite/vars.h>`) reaches the calling program's
  variables by name.
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
`stdnoreturn.h`, `string.h`, `time.h`, `unistd.h`, `sys/stat.h`,
`dirent.h`, `picomite.h`, and the firmware API `picomite/system.h`,
`picomite/memory.h`, `picomite/graphics.h`, `picomite/input.h`,
`picomite/basic.h`, `picomite/sprite.h`, `picomite/audio.h`,
`picomite/files.h`, `picomite/vars.h`, `picomite/framebuffer.h` and
`PicoCFunctions.h` (see [The firmware API](#the-firmware-api-picomite)).

They declare exactly what the run-time library below provides. A function
that is declared nowhere and not in the library ends with
`undefined symbol`.

### Run-time library

| Header | Functions |
|---|---|
| stdio.h | `printf fprintf vprintf vfprintf sprintf snprintf vsprintf vsnprintf puts putchar fputs fputc putc getchar fgetc getc fgets fopen fclose fread fwrite fseek ftell rewind feof ferror fflush remove rename`, `stdin stdout stderr` |
| unistd.h | `chdir rmdir unlink getcwd` |
| sys/stat.h | `stat mkdir` |
| dirent.h | `opendir readdir closedir` |
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
to the current directory on the current drive). A program can have up to 8
files open at once, within MMBasic's own limit of 10 open files.

| Mode | | As MMBasic's |
|---|---|---|
| `"r"` | read; the file must exist | `OPEN ... FOR INPUT` |
| `"w"` | write, emptied or created | `OPEN ... FOR OUTPUT` |
| `"a"` | write at the end, created if missing | `OPEN ... FOR APPEND` |
| `"r+"` | read and write from the start; the file must exist | `OPEN ... FOR RANDOM` |
| `"w+"` | read and write, emptied or created | `OUTPUT`, then `RANDOM` |
| `"a+"` | read from the start, write at the end | `OPEN ... FOR RANDOM` |

`"b"` and `"t"` are accepted and ignored. In `"a"` and `"a+"` every write
goes to the end, wherever `fseek` left the position.

File errors never stop the program with an MMBasic error. `fopen` returns
`NULL`, `fread`/`fwrite` return short counts, and `ferror()` is set.

The rest of the file system has two forms:

- **The C library's**: `remove`, `rename` (`stdio.h`), `chdir`, `rmdir`,
  `unlink`, `getcwd` (`unistd.h`), `stat`, `mkdir` (`sys/stat.h`) and
  `opendir`, `readdir`, `closedir` (`dirent.h`). They mean what C means and
  return -1 (or `NULL`) when they fail, never an MMBasic error;
  `pm_errmsg()` (`<picomite/basic.h>`) then says why.
  - `rename` works on the current drive only, as `RENAME` does.
  - `stat` fills `st_mode` (`S_ISDIR`, `S_ISREG`), `st_size` and
    `st_mtime`: `MM.INFO(MODIFIED)` as seconds from 1970, in the PicoMite
    clock's own time, as `time()` gives it.
  - `readdir` gives `d_name` and `d_type` (`DT_DIR` or `DT_REG`).
    `opendir` reads the whole listing at once with `DIR$`, so any number of
    directories can be open, and `DIR$` or `pm_dir` used meanwhile does not
    disturb them. A file made or removed after `opendir` is not seen.
- **MMBasic's**, in `<picomite/files.h>`: the commands and functions of the
  same name, with their errors (`pm_try` catches them).

```c
void pm_kill(const char *file);                   /* KILL file$ */
void pm_rename(const char *from, const char *to); /* RENAME from$ AS to$ */
void pm_copy(const char *from, const char *to);   /* COPY from$ TO to$ */
void pm_mkdir(const char *dir);                   /* MKDIR */
void pm_rmdir(const char *dir);                   /* RMDIR */
void pm_chdir(const char *dir);                   /* CHDIR */
void pm_drive(const char *drive);                 /* DRIVE "B:" */
char *pm_cwd(char *buf, int size);                /* CWD$ */
char *pm_drive_name(char *buf, int size);         /* MM.INFO$(DRIVE) */
char *pm_dir(const char *pattern, int what, char *buf, int size); /* DIR$ */
int  pm_exists_file(const char *file);            /* MM.INFO(EXISTS FILE) */
int  pm_exists_dir(const char *dir);              /* MM.INFO(EXISTS DIR) */
long long pm_filesize(const char *file);          /* MM.INFO(FILESIZE) */
char *pm_modified(const char *file, char *buf, int size); /* MM.INFO(MODIFIED) */
long long pm_free_space(void);                    /* MM.INFO(FREE SPACE) */
long long pm_disk_size(void);                     /* MM.INFO(DISK SIZE) */
```

`pm_dir(pattern, what, ...)` is `DIR$(pattern, FILE | DIR | ALL)` with
`what` `PM_DIR_FILE` (or `PM_DEFAULT`), `PM_DIR_DIR` or `PM_DIR_ALL`;
`pm_dir(NULL, ...)` is `DIR$()`, the next name. Names cannot contain `"`.

```c
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

FILE *log = fopen("B:/mmbg/log/game.log", "a");   /* OPEN ... FOR APPEND */
fprintf(log, "%s\n", line);
fclose(log);

DIR *d = opendir("B:/mmbg");
struct dirent *e;
while ((e = readdir(d)) != NULL)
    printf("%s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
closedir(d);
```

### Memory

- `malloc()` takes memory from the same arena as the compiler, which is built
  from the MMBasic heap: SRAM first, then PSRAM.
- Everything is given back when the program ends, so an unfreed block does
  not leak.
- Until a C program is run, the compiler uses no heap at all. Only its code
  (about 180 KB of flash) and some 0.8 KB of static RAM are permanent.
- The compiler and the program run on a private **32 KB stack**. Deep
  recursion or large local arrays can overflow it. The firmware's own stack
  check is moved onto this stack while the program runs, so a BASIC command
  or expression run from C (`pm_execute`, `pm_eval_*`, the wrappers below)
  that finds less than about 1 KB left fails with MMBasic's usual
  "Stack overflow" error instead of damaging memory.

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

The API has a version, `PM_API_VERSION` (`<picomite/system.h>`), now 11.
Each version adds functions without removing earlier ones. Check
`pm_api_version()` before using a newer API; an ELF linked against a firmware
that lacks a function reports `undefined symbol`.

| Version | Added |
|---|---|
| 1 | `picomite.h`; `system.h`, `memory.h`, `graphics.h`, `input.h` (`pm_inkey`, `pm_keydown`), `basic.h` (`pm_execute`, `pm_eval_*`, `pm_settick`), `sprite.h`, `audio.h`, `pm_colour`, `pm_load_image`, `pm_load_jpg` |
| 2 | `basic.h`: `pm_try`, `pm_execute_try`, `pm_eval_*_try`, `pm_errno`, `pm_errmsg`, `pm_error_clear`; `files.h`; `vars.h`; `input.h`: `pm_mouse`, `pm_usb_device`, `pm_cursor_*`; the C library's `remove`, `rename`, `unistd.h`, `sys/stat.h`, `dirent.h` and `fopen`'s `"a"`, `"r+"`, `"w+"`, `"a+"` |
| 3 | `framebuffer.h` |
| 4 | `graphics.h`: `pm_draw_rbox`; `audio.h`: `pm_play_tone`, `_wav`, `_flac`, `_mp3`, `_midifile`, `_pause`, `_resume`, `_next`, `_previous`, `_close`, `_volume`, `_bbc_sound`, `_bbc_envelope`, `pm_sound` |
| 5 | `serial.h`: typed COM1/COM2 UART open, read, write, status, flush and close |
| 6 | `i2c.h`: typed I2C0/I2C1 master open/close, read/write, repeated-START transfer, probe |
| 7 | `spi.h`: SPI0/SPI1 master, modes 0..3, 4..16 bits, binary transfers |
| 8 | `adc.h`: filtered ADC voltage and raw 12-bit conversion through existing PIN() services |
| 9 | `pwm.h`: shared BASIC/C PWM slice control, frequency and duty |
| 10 | `basic.h`: typed `pm_basic_sub()` / `pm_basic_function()` callbacks from C and CSUB |
| 11 | `audio.h`: signed-16 PCM one-shot and streaming callback (`pm_pcm_play/start/stop/playing/poll`); PWM/I2S/Bluetooth, not VS1053 |

Native PCM uses signed 16-bit mono/stereo frames, not bytes. `pm_pcm_play()`
copies the buffer into firmware memory and may outlive `RUN "file.c"`;
`pm_pcm_start()` calls the filling function outside the audio interrupt (zero
frames means EOF). `RUN .c/.elf` must call `pm_pcm_poll()` in long C loops;
its callback is detached when that program exits. A CSUB callback must use
persistent code and `userdata` (not stack locals), avoid blocking or BASIC
re-entry, and remains attached only while its BASIC program is running.
See [`C/pcm_stream.c`](../C/pcm_stream.c) for a TinyCC example.

Structs that
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

`pm_pause(ms)` waits as `PAUSE` does: the firmware's background work (USB
keyboard and mouse, the GUI cursor, the network) goes on meanwhile, and
CTRL-C ends the program. `pm_delay_us` is a bare delay for timing.

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
void pm_colour(int fc, int bc);
void pm_load_image(const char *file, int x, int y, int mode);
void pm_load_jpg(const char *file, int x, int y);
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
| `RBOX x, y, w, h [, r] [, c] [, fill]` | `pm_draw_rbox(x, y, w, h, r, c, fill)` |
| `MODE n` | `pm_mode(n)` (VGA and HDMI builds) |
| `COLOUR fc [, bc]` | `pm_colour(fc, bc)` |
| `LOAD IMAGE file$ [, x] [, y] [, mode]` | `pm_load_image(file, x, y, mode)` |
| `LOAD JPG file$ [, x] [, y]` | `pm_load_jpg(file, x, y)` |

- An argument that BASIC lets you leave out is `PM_DEFAULT` in C, with the
  same default: the current font, scale, foreground and background colour
  (`FONT`, `COLOUR`), BOX's line width 1 and no fill. For `align`, `NULL` or
  `""` gives the default `"LT"`.
- `bc` may be `PM_TRANSPARENT` and `fill` may be `PM_NOFILL`, which is -1 as
  in BASIC.
- With `OPTION REFRESH` on, the display is refreshed after them, as after
  the commands.
- `pm_colour` sets the colours `PM_DEFAULT` stands for. As `COLOUR` does,
  it takes RGB colours, or with `OPTION LEGACY ON` the colour numbers 0..15;
  `bc` may be `PM_DEFAULT` to leave the background as it is.
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
`picomite.h`.

```c
int pm_keydown(int n); /* KEYDOWN(n) */
```

`pm_keydown(0)` is the number of keys held down, 1 to 6 the code of each,
as `KEYDOWN()` on a USB or PS/2 keyboard; n out of range is the same error.

```c
int  pm_mouse(int channel, int what);   /* DEVICE(MOUSE channel, what) */
int  pm_usb_device(int port);           /* MM.INFO(USB port) */
void pm_cursor_on(int no, int x, int y, int colour); /* GUI CURSOR ON [no [, x, y [, c]]] */
void pm_cursor_off(void);               /* GUI CURSOR OFF */
void pm_cursor_hide(void);              /* GUI CURSOR HIDE */
void pm_cursor_show(void);              /* GUI CURSOR SHOW */
void pm_cursor_colour(int colour);      /* GUI CURSOR COLOUR c */
void pm_cursor_link(int link);          /* GUI CURSOR LINK MOUSE / UNLINK MOUSE */
void pm_cursor_load(const char *file);  /* GUI CURSOR LOAD file$ */
void pm_cursor_move(int x, int y);      /* GUI CURSOR x, y */
```

- `pm_mouse(channel, what)` is `DEVICE(MOUSE channel, what)`, with `what`
  the letter BASIC takes: `'X'`, `'Y'` the position, `'L'`, `'R'`, `'M'` the
  buttons, `'W'` the wheel, `'B'` all buttons, `'D'` a double click (read
  once), `'T'` 3 for a mouse with a wheel. It reads the firmware's mouse
  state directly, so it can be polled in a loop.
- `pm_usb_device(n)` is `MM.INFO(USB n)`: on the USB keyboard builds, 0 the
  number of devices and 1 to 4 the type of each (2 a mouse). Elsewhere it is
  the error BASIC gives.
- The `pm_cursor_` functions are `GUI CURSOR`; `PM_DEFAULT` for what is left
  out of `GUI CURSOR ON`. `pm_cursor_move` calls the firmware directly; the
  others run the statement. `GUI CURSOR LINK MOUSE` with no mouse is "No
  mouse connected", as in BASIC: `pm_try` turns it into a test.

```c
/* MMbg's GuMsInit: the first USB mouse, the cursor linked to it */
int ch = 0;
for (int i = 1; i <= 4 && ch == 0; i++) {
    long long k;
    char e[24];
    snprintf(e, sizeof e, "MM.INFO(USB %d)", i);   /* ON ERROR SKIP 1 */
    if (pm_eval_int_try(e, &k) == 0 && k == 2)
        ch = i;
}
if (ch && pm_execute_try("GUI CURSOR ON 0, 320, 240") == 0 &&
    pm_execute_try("GUI CURSOR LINK MOUSE") != 0)
    pm_cursor_off();
...
if (pm_mouse(ch, 'L')) { int x = pm_mouse(ch, 'X'), y = pm_mouse(ch, 'Y'); ... }
```

#### `<picomite/framebuffer.h>`: FRAMEBUFFER and BLIT

```c
void pm_framebuffer_create(int buffer);           /* FRAMEBUFFER CREATE ('F') / CREATE 2 ('2') */
void pm_framebuffer_layer(int transparent);       /* FRAMEBUFFER LAYER [transparent] */
void pm_framebuffer_layer_top(int transparent);   /* FRAMEBUFFER LAYER TOP [transparent] */
void pm_framebuffer_write(int buffer);            /* FRAMEBUFFER WRITE N | F | L | T | 2 */
void pm_framebuffer_copy(int from, int to, int at_blank); /* FRAMEBUFFER COPY from, to [, B] */
void pm_framebuffer_merge(int colour, int mode, int rate); /* FRAMEBUFFER MERGE [c] [, mode [, rate]] */
void pm_framebuffer_sync(void);                   /* FRAMEBUFFER SYNC */
void pm_framebuffer_wait(void);                   /* FRAMEBUFFER WAIT */
void pm_framebuffer_close(int buffer);            /* FRAMEBUFFER CLOSE [buffer] */
void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2,
                         int w, int h, int colour); /* BLIT FRAMEBUFFER ... [, colour] */
void pm_blit(int x1, int y1, int x2, int y2, int w, int h); /* BLIT */
void pm_blit_read(int n, int x, int y, int w, int h);       /* BLIT READ */
void pm_blit_write(int n, int x, int y, int mode);          /* BLIT WRITE */
void pm_blit_load(int n, const char *file, int x, int y, int w, int h); /* BLIT LOAD */
void pm_blit_close(int n);                                  /* BLIT CLOSE */
```

- A buffer is its BASIC letter: `'N'` the display, `'F'` the framebuffer,
  `'L'` the layer, and on the RP2350 VGA/HDMI builds `'T'` the top layer and
  `'2'` framebuffer 2. What a build does not have is the error BASIC gives.
- An argument BASIC lets you leave out is `PM_DEFAULT`: `pm_framebuffer_close(PM_DEFAULT)`
  closes them all, as `FRAMEBUFFER CLOSE` does.
- Drawing goes to the buffer `pm_framebuffer_write` chose, as in BASIC.
- `pm_blit_framebuffer` calls the firmware directly (the copy of `BLIT
  FRAMEBUFFER` is shared with the command), so it is fit for every frame of
  an animation. The others run the statement.

```c
/* MMbg's animation: the background in F, the moving piece over N */
pm_framebuffer_create('F');
pm_framebuffer_write('F');
/* ... draw the board ... */
pm_framebuffer_copy('F', 'N', 1);
pm_framebuffer_write('N');
for (;;) {
    pm_blit_framebuffer('F', 'N', ox, oy, ox, oy, 29, 29, PM_DEFAULT); /* the background back */
    pm_circle(x + 14, y + 14, 13, 1, PM_WHITE, PM_YELLOW);           /* the piece */
    ...
}
```

#### `<picomite/sprite.h>`

```c
void pm_sprite_load(const char *file, int start, int mode); /* SPRITE LOAD file$ [, start] [, mode] */
void pm_sprite_show(int n, int x, int y, int layer, int rotation);
void pm_sprite_hide(int n);
void pm_sprite_write(int n, int x, int y, int orientation);
void pm_sprite_close_all(void);
```

The `SPRITE` subcommands of the same name, arguments in their BASIC order,
with `PM_DEFAULT` for an argument left out. `SHOW`, `HIDE`, `WRITE` and
`CLOSE ALL` call the firmware's sprite code directly, with the same checks
and errors as the commands, so they are fit for a game loop.

```c
pm_sprite_load("maxitrek.spr", PM_DEFAULT, PM_DEFAULT);
pm_sprite_show(enty, ex, ey, 1, PM_DEFAULT);   /* SPRITE SHOW enty, ex, ey, 1 */
pm_sprite_write(7, cx, cy, 4);                 /* SPRITE WRITE 7, cx, cy, 4 */
```

#### `<picomite/audio.h>`

```c
void pm_play_modfile(const char *file);                      /* PLAY MODFILE file$ */
void pm_play_modsample(int sample, int channel, int volume); /* PLAY MODSAMPLE s, ch [, vol] */
void pm_play_sound(int channel, const char *position, const char *type,
                   double frequency, int volume);            /* PLAY SOUND ch, pos$, type$, f [, vol] */
void pm_play_tone(double left, double right, int duration);  /* PLAY TONE l, r [, ms] */
void pm_play_wav(const char *file);                          /* PLAY WAV file$ */
void pm_play_flac(const char *file);                         /* PLAY FLAC file$ */
void pm_play_mp3(const char *file);                          /* PLAY MP3 file$ */
void pm_play_midifile(const char *file);                     /* PLAY MIDIFILE file$ */
void pm_play_pause(void);                                    /* PLAY PAUSE */
void pm_play_resume(void);                                   /* PLAY RESUME */
void pm_play_next(void);                                     /* PLAY NEXT */
void pm_play_previous(void);                                 /* PLAY PREVIOUS */
void pm_play_close(void);                                    /* PLAY CLOSE */
void pm_play_stop(void);                                     /* PLAY STOP */
void pm_play_volume(int left, int right);                    /* PLAY VOLUME l [, r] */
void pm_play_bbc_sound(int channel, int amplitude, int pitch, int duration); /* PLAY BBC SOUND */
void pm_play_bbc_envelope(const int e[14]);                  /* PLAY BBC ENVELOPE n, T, ... ALD */
char *pm_sound(char *buf, int size);                         /* MM.INFO(SOUND) */
```

The `PLAY` subcommands, with `PM_DEFAULT` for an argument left out.

- `pm_play_wav`, `_flac`, `_mp3` and `_midifile` take a file or a directory
  (all its files are played, `pm_play_next` / `_previous` move between them).
  The interrupt SUB of the BASIC form has no C form: poll `pm_sound()`,
  which is `"OFF"` when nothing plays.
- `pm_play_bbc_sound` and `pm_play_bbc_envelope` are the BBC Micro's
  `SOUND` and `ENVELOPE`: `e[]` holds the 14 values of the envelope in the
  order of `PLAY BBC ENVELOPE`. As on the BBC, a full channel queue makes
  `pm_play_bbc_sound` wait.
- As `PLAY STOP`, `pm_play_stop` does nothing when nothing is playing.
- Without `OPTION AUDIO` they fail as the commands do; `pm_try` turns that
  into a test.

```c
pm_play_sound(1, "B", "Q", 2101 - dur * 20, 15); /* PLAY SOUND 1,"B","Q",2101-dur*20,15 */

static const int env3[14] = {3, 1, 0,0,0, 0,0,0, 127, -40, 0, -127, 81, 0};
pm_play_bbc_envelope(env3);                      /* PLAY BBC ENVELOPE 3, 1, 0,0,0, ... */
pm_play_bbc_sound(0x10, 3, 6, 0);                /* PLAY BBC SOUND &H10, 3, 6, 0 */
```

#### `<picomite/serial.h>`: COM1/COM2 UART (API version 5)

This describes **version 5** of the native PicoMite TinyCC API. The functions
are defined in `<picomite/serial.h>` and exported to TinyCC-linked C programs.
The implementation uses the existing `io/Serial.c` UART, pin reservations and
RX/TX buffers; it does not tokenise BASIC commands.

##### Scope and prerequisites

* Supports **COM1 (UART0)** and **COM2 (UART1)** only. USB CDC COM3-COM6 and
  Bluetooth COM7 are intentionally not part of this initial API. They have
  different buffering and flow-control semantics.
* Use PicoMite `SETPIN` to assign the corresponding COM TX/RX pins first.
  The same pin availability and UART ownership checks as BASIC `OPEN` apply;
  occupied pins, the serial console and GPS cannot be taken over.
* The integrated TinyCC adapter is currently built for **RP2350** variants.
  The underlying serial driver changes are common to RP2040 and RP2350.
* Requires `pm_api_version() >= 5`. These entry points are **not available to
  external CSUBs**; the legacy `CallTable` layout is unchanged.
* The transmit ring has 256 slots (255 usable bytes); the configuration
  controls the receive ring only.

##### Interface

```c
#include <picomite/serial.h>

pm_serial_config cfg = {
    .port = 1,
    .baudrate = 115200,
    .rx_buffer_size = 256,
    .flags = 0             /* 8 data bits, no parity, 1 stop bit */
};
pm_serial_t com = pm_serial_open(&cfg);
```

`baudrate` defaults to 9600 when zero; `rx_buffer_size` defaults to 1024.
Valid custom receive buffers are 2..65536 bytes. Baud rates follow the
existing PicoMite COM range (up to 921600; minimum depends on CPU clock).
Flags are `PM_SERIAL_7BIT`, `PM_SERIAL_STOP2`, `PM_SERIAL_EVEN`,
`PM_SERIAL_ODD` and `PM_SERIAL_INVERT`. Even and odd parity cannot be
combined. `pm_serial_open` returns an opaque handle (`pm_serial_t`).

`pm_serial_available(com)` returns queued input byte count. `pm_serial_read`
and `pm_serial_write` accept a byte pointer, a maximum length and an overall
timeout in milliseconds; both return the **number actually transferred**
(0..length), including partial results. A timeout of zero is nonblocking.
While waiting, they service PicoMite's background tasks and Ctrl+C.

`pm_serial_flush(com, timeout_ms)` waits until **both** the software TX queue
and UART hardware transmitter are idle: 0 on success, -1 on timeout.
`pm_serial_close(com)` closes without waiting; flush first if every outgoing
byte must be transmitted.

Invalid arguments, an unconfigured/busy UART and invalid/stale handles are
MMBasic errors, catchable with `pm_try()` in a C program. A C-owned port is
automatically closed when the C program terminates normally, with an MMBasic
error or on Ctrl+C. Port generations prevent it from accidentally closing a
port that BASIC closed and reopened in the meantime.

##### Example

[`C/serial_echo.c`](../C/serial_echo.c) waits for bytes on COM1 at 115200 8N1
and echoes them. Copy the source to a PicoMite-accessible volume, assign COM1
GPIOs using BASIC `SETPIN`, and run it with `RUN "B:/serial_echo.c"` (adjust
the path to the copy). The example has no fixed pin numbers because they
depend on the board and its currently reserved peripherals.

This is a **polling API**, not an IRQ callback API. It deliberately never
runs C code inside a UART interrupt. Concurrent BASIC `OPEN`/`CLOSE` on a
C-owned port is unsupported; a handle is rejected if the port was replaced.

#### `<picomite/i2c.h>`: I2C0/I2C1 master (API version 6)

The firmware's BASIC `I2C` and `I2C2` commands and the native C API use the
same low-level master transfer service in `io/I2C.c`, without invoking the
MMBasic tokenizer. Slave mode remains available in BASIC only.

```c
#include <picomite/i2c.h>

pm_i2c_t bus = pm_i2c_open(0, 400, 1000); /* I2C0, 400 kHz, 1000 ms */
unsigned char reg = 0x00, value;
int status = pm_i2c_transfer(bus, 0x68, &reg, 1, &value, 1);
pm_i2c_close(bus);
```

The C caller must assign both I2C pins using BASIC `SETPIN` before running.
`pm_i2c_open(bus, speed_khz, timeout_ms)` accepts bus 0 or 1, speed 100,
400 or 1000 kHz (0 defaults to 100), and 100..10000 ms (0 defaults to 1000).
The operation refuses buses already open for BASIC, configured as slaves,
reserved by SYSTEM I2C, or using reserved GPIOs. No pin reassignment occurs.
Handles belong to the current C run and are invalidated by BASIC `I2C CLOSE`.
The bus is closed automatically on normal exit, error, or Ctrl+C.

`pm_i2c_transfer` sends `tx` then reads `rx` under a **repeated START**,
with a STOP at the end. `pm_i2c_write`/`pm_i2c_read` each issue a STOP.
Both lengths must not be zero and may be at most 65535 bytes each;
addresses are 7-bit (0..127). The buffers are used directly, without
allocating copies. Each transfer phase has the configured timeout.

Transfer functions return **0** on success, **1** on NACK/short transfer,
**2** on timeout. The status is independent of BASIC's `MM.I2C`.
`pm_i2c_probe` returns 1 on ACK, 0 on NACK, -1 on timeout; like BASIC
`I2C CHECK`, it reads one byte and can have device-specific side effects.
Incorrect arguments or unavailable resources raise PicoMite errors,
catchable with `pm_try()`. The C API is unavailable to external CSUBs;
no existing `CallTable` entries change.

Example: [`C/i2c_reg_read.c`](../C/i2c_reg_read.c) reads one 8-bit register
from a device specified by command-line arguments. It requires an actual
device supporting this register-read protocol.

#### `<picomite/spi.h>`: SPI0/SPI1 master (API version 7)

The C API uses `io/SPI.c`'s low-level service, also used by BASIC's SPI
and SPI2 operations. It bypasses the MMBasic tokenizer.

```c
#include <picomite/spi.h>
pm_spi_t port = pm_spi_open(0, 1000000, 0, 8); /* SPI0, 1 MHz, mode 0, 8-bit */
unsigned char tx[2] = {0x9f, 0};
unsigned char rx[2];
pm_spi_transfer(port, tx, rx, 2); /* full-duplex, two words */
pm_spi_close(port);
```

Configure TX/RX/SCK through BASIC `SETPIN` before running. The API rejects
pins or controllers reserved by SYSTEM SPI, and buses already open from
BASIC. `pm_spi_open(bus, speed_hz, mode, bits)` accepts bus 0/1, SPI mode
0..3, and 4..16 bits per word. Zero speed defaults to 1 MHz; zero bits
means 8-bit. `pm_spi_transfer` sends and receives simultaneously; for
read-only or write-only use `pm_spi_read` and `pm_spi_write` (read transmits
zero dummy words). The count is **words**, not bytes: buffers use `uint8_t`
for 4..8 bits, `uint16_t` for 9..16 bits. For 16-bit data, pointers must be
2-byte aligned. The functions process chunks of 64 words and service
PicoMite background tasks between chunks. Up to 65535 words per call.

No automatic chip select is configured: manage CS with a separate GPIO as
required by the target SPI device. The full-duplex API does not support
half-duplex 3-wire SPI or DMA. Error conditions and stale descriptors raise
PicoMite errors, catchable using `pm_try()`. All C-owned SPI buses close
on program exit, error, or Ctrl+C. External CSUBs cannot use this API.

[`C/spi_loopback.c`](../C/spi_loopback.c) tests SPI0 with TX joined to RX.

#### `<picomite/adc.h>`: ADC input (API version 8)

```c
#include <picomite/adc.h>
double pm_adc_read(int gp);   /* voltage, filtered as BASIC PIN(GP...) */
int pm_adc_read_raw(int gp);  /* one 12-bit conversion; -1 on ADC fault */
```

Both functions accept a **GPIO number**, not PicoMite's internal pin index.
Configure it with BASIC `SETPIN GPn, AIN` first (for example GP26 on
RP2350A or GP40 on RP2350B). No GPIO is claimed or reconfigured by the C
API. `pm_adc_read()` requires the `AIN` configuration and returns the same
voltage as BASIC `PIN(GPn)`, using its sorted, trimmed-mean filter and the
firmware's `VCC` scaling. `pm_adc_read_raw()` returns a single
unfiltered 0..4095 conversion from `AIN` or `ADCRAW` pin mode.

Both functions reject unconfigured or reserved pins, an active BASIC
`ADC OPEN` session, and ongoing ADC DMA/buffer capture. These conditions
raise PicoMite errors (catchable with `pm_try()`), not silent zero readings.
`pm_adc_read_raw()` returns -1 when the ADC hardware reports a conversion
fault; this is distinct from those configuration errors. The functions are
available to TinyCC-linked programs only, not external CSUBs. They have no
open/close lifetime and do not allocate memory.

[`C/adc_read.c`](../C/adc_read.c) shows voltage and raw readings for a GPIO
selected on the command line.

#### `<picomite/pwm.h>`: PWM outputs (API version 9)

```c
#include <picomite/pwm.h>
void pm_pwm_open(int slice, unsigned frequency_hz, double duty_a, double duty_b);
void pm_pwm_set_duty(int slice, double duty_a, double duty_b);
void pm_pwm_set_frequency(int slice, unsigned frequency_hz);
void pm_pwm_close(int slice);
```

Select PWM slice 0..7 on RP2350A or 0..11 on RP2350B. Configure the
corresponding pins with BASIC `SETPIN GPn,PWMnA` / `PWMnB` beforehand.
All functions use the **slice number**, not a C-owned handle. `pm_pwm_open`
starts or reconfigures the slice, including PWM already running under BASIC.
Duty arguments are percentages (0..100); **-1** leaves an output unchanged
when it is already active, or leaves an unconfigured output unused. At least
one channel must be specified when opening. The period/divider are shared
by A and B. `pm_pwm_set_frequency` preserves the current hardware compare
ratios and phase-correct setting; `pm_pwm_open` selects ordinary PWM mode.
For `pm_pwm_set_duty`, **-1** means do not change that channel.

BASIC `PWM`, BASIC `SERVO` and C `pm_pwm_open` now share the same
frequency/divider/compare-value calculation and `set_PWM` output path. BASIC
retains negative-duty inversion and phase-correct options; `PWM SYNC` remains
the common register-based enable/synchronisation mechanism. There is no
C-only resource owner or automatic close at program exit, error or Ctrl+C.
System-controlled slices (audio, backlight, camera, FAST TIMER, Stepper)
remain protected; errors are catchable using `pm_try()` in C programs.
External CSUBs cannot use this API.

[`C/pwm_sweep.c`](../C/pwm_sweep.c) sweeps duty on slice 0, channel A.

#### Native BASIC callbacks (API v10)

`pm_basic_sub()` and `pm_basic_function()` call existing BASIC definitions
by name, **without tokenising generated BASIC statements**. Arguments are
`pm_basic_value` entries (`PM_BASIC_INTEGER`, `PM_BASIC_FLOAT`,
`PM_BASIC_STRING`); conversion into BASIC parameter types follows the
ordinary rules of `DefinedSubFun()`. Only scalar BYVAL parameters and scalar
results are supported in v10. BYREF, arrays and structures are rejected.

A standalone `RUN "file.c"` or `RUN "file.elf"` starts a new program context:
only definitions in FLASH `LIBRARY` are visible. The dormant `ProgMemory`
program is not imported. The library's top-level initialization is run as
in BASIC `RUN`. A C CSUB uses its BASIC caller's already-prepared index,
seeing caller SUB/FUNCTIONs and `LIBRARY`. RAM libraries are discarded at a
new RUN by existing `ClearRuntime()` behavior.

```c
#include <picomite/basic.h>
#include <stdio.h>

int main(void) {
    pm_basic_value arg = { .type = PM_BASIC_INTEGER, .integer = 12 };
    pm_basic_value answer;
    pm_basic_function("Scale%", &arg, 1, &answer, NULL, 0);
    printf("%lld\n", answer.integer);
    return 0;
}
```

For a string result, pass a writable C buffer and its capacity; the result
points into that buffer. Calls use the ordinary MMBasic error mechanism
and `pm_try()` may catch errors. For a classic externally compiled CSUB, include `PicoCFunctions.h` and call
`pm_basic_sub()` / `pm_basic_function()` with the same `pm_basic_value` ABI.
The CSUB uses the new append-only CallTable entry **0x1CC** and shares the
BASIC VM implementation with TinyCC. The old `RunBasicSub()` entry at 0x74
remains intact for source and binary compatibility. An externally compiled
CSUB must run on firmware exposing this new CallTable slot. Existing slots
are unchanged; do not call 0x1CC on older firmware.

#### `<picomite/basic.h>`: MMBasic's commands and functions

```c
void      pm_execute(const char *cmd);                     /* EXECUTE cmd$ */
long long pm_eval_int(const char *expr);                   /* EVAL(s$) */
double    pm_eval_float(const char *expr);
char     *pm_eval_str(const char *expr, char *buf, int size);
void      pm_settick(int period, void (*fn)(void), int nbr); /* SETTICK */

int  pm_try(void (*fn)(void *arg), void *arg);             /* ON ERROR SKIP */
int  pm_execute_try(const char *cmd);
int  pm_eval_int_try(const char *expr, long long *value);
int  pm_eval_float_try(const char *expr, double *value);
int  pm_eval_str_try(const char *expr, char *buf, int size);
int  pm_errno(void);                                         /* MM.ERRNO */
const char *pm_errmsg(void);                                 /* MM.ERRMSG$ */
void pm_error_clear(void);                                   /* ON ERROR CLEAR */
```

These make every MMBasic command and function available to C with exactly
its BASIC meaning, including those with no C function of their own yet:
`BLIT`, `OPTION ...`, `MM.INFO()` and so on. Where a C function exists
(`<picomite/sprite.h>`, `<picomite/audio.h>`, `pm_colour`, `pm_keydown`, ...)
use it instead.

- **`pm_execute`** runs one statement as `EXECUTE` does: a single statement
  (no `:`), as a statement of a running program. A C program has no BASIC
  variables or SUBs, so put the values into the text:

  ```c
  char cmd[64];
  snprintf(cmd, sizeof cmd, "BLIT %d,%d,%d,%d,%d,%d", x1, y1, x2, y2, w, h);
  pm_execute(cmd);
  pm_execute("OPTION KEYBOARD REPEAT 500,50");
  ```

- **`pm_eval_int`, `pm_eval_float` and `pm_eval_str`** give the value of an
  expression as `EVAL()` does. `pm_eval_int` rounds a float as BASIC does
  when it assigns one to an integer. Asking for a number from a string
  expression, or the other way round, is an error.

  ```c
  int vres = (int)pm_eval_int("MM.VRES");
  char dev[40];
  pm_eval_str("MM.DEVICE$", dev, sizeof dev);
  ```

- An MMBasic error in the command or expression ends the program with that
  error (in a C CSUB, it is reported at the CSUB's call), unless it is made
  in `pm_try`: see below.
- A C program counts as a running program: what MMBasic allows only at the
  prompt (`EDIT`, `AUTOSAVE`, `OPTION LCDPANEL`, `XMODEM` and so on) fails
  with "Invalid in a program". What a BASIC program may do but would replace
  the program, and with it the memory the C program runs in, fails with
  "Not possible from a C program": `RUN`, `CHAIN`, `NEW`, `LOAD file$`,
  `FLASH RUN`/`CHAIN`, `RAM RUN`/`CHAIN`. `END` ends the C program at
  once and goes back to the prompt, as `END` in a BASIC program does
  (`exit()` is the C program's ordinary end).
- Unlike `EXECUTE`, a command may take as long as it needs (EXECUTE restarts
  the board after 1 s): it is the program's own statement, as in a BASIC
  program. Likewise the messages the firmware prints only at the prompt
  (`Playing ...` of `PLAY MODFILE`, for instance) are not printed while a C
  program runs.
- The text is tokenised on every call (as `EXECUTE` does), which takes some
  tens of microseconds. That is fine for setting things up and for an
  occasional sound or sprite, but a hot loop is better served by the C
  functions where there is one.
- What the commands start keeps going after the program, as after a BASIC
  program run from the prompt: `RUN "x.c"` ends as `RUN` does, which stops
  sound and closes sprites and files.

**`pm_try`** is `ON ERROR SKIP 1` for a C function: an MMBasic error in
any `pm_` call that `fn(arg)` makes, directly or deeper down, comes back to
`pm_try` instead of ending the program.

- It returns 0, or the error's number. `pm_errno()` and `pm_errmsg()` then
  give `MM.ERRNO` and `MM.ERRMSG$`. As `ON ERROR SKIP` does, `pm_try` clears
  them first, and `pm_error_clear()` is `ON ERROR CLEAR`.
- What `fn` was doing is abandoned at the error, as a skipped statement is:
  the rest of `fn` does not run. Memory it took with `malloc` is not freed.
- A `pm_settick` function is outside it, as a BASIC interrupt is outside the
  `ON ERROR` of the code it interrupts: an error there ends the program.
- CTRL-C and `exit()` still end the program.
- `pm_execute_try` and `pm_eval_*_try` are `pm_try` of a single call, the
  usual `ON ERROR SKIP : statement : IF MM.ERRNO ...`. On an error the value
  is 0 (the string `""`).
- Works in a C CSUB too.

```c
/* ON ERROR SKIP : GUI CURSOR ON 0, 320, 240 : IF MM.ERRNO THEN ... */
if (pm_execute_try("GUI CURSOR ON 0, 320, 240"))
    printf("no cursor: %s\n", pm_errmsg());

static void show(void *p) { pm_sprite_show(*(int *)p, 10, 10, 1, PM_DEFAULT); }
...
if (pm_try(show, &n) != 0)  /* any pm_ call, any depth */
    ...
```

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

#### `<picomite/vars.h>`: BASIC variables and arrays

```c
void *pm_var(const char *ref, pm_varinfo *info);   /* VARADDR, and what it is */
void *pm_dim(const char *decl, pm_varinfo *info);  /* DIM decl */
void  pm_erase(const char *names);                 /* ERASE */
char *pm_str_get(const void *mmstr, char *buf, int size);
void  pm_str_set(void *mmstr, int capacity, const char *s);
```

A C program can make BASIC variables with `DIM` and reach any variable by
its BASIC name: those it made, and in a C CSUB those of the BASIC program
that called it.

- `pm_var(ref, info)` gives the address of the variable's data, as
  `VARADDR` does: `"n%"`, `"x!"`, `"name$"`, an element `"grid%(3, 4)"`, or
  a whole array `"buf%()"` (its first element). It is an error if the
  variable does not exist.
- `pm_dim(decl, info)` runs `DIM decl` (`"buf%(511)"`, `"INTEGER n(10)"`,
  `"s$ LENGTH 16"`, `"m!(2, 3)"`) and gives the address of the variable it
  declares first.
- `info` (or `NULL`) is filled with what the variable is: `type`
  (`PM_VAR_INT`, `PM_VAR_FLOAT`, `PM_VAR_STRING`), a string's capacity
  `size`, `constant`, the dimensions `ndims` and `dims[]` (elements of
  each, `OPTION BASE` taken into account), `elements` and `element_size`.
- The data are MMBasic's: `long long` for an integer, `double` for a float,
  and for a string a length byte followed by the characters, `size + 1`
  bytes per element (`pm_str_get`, `pm_str_set`, which is "String too
  long" when it does not fit, as an assignment). In an array the first
  index varies fastest: `m!(i, j)` is element `i + j * dims[0]` (with
  `OPTION BASE 0`).
- The address does not move while the variable exists, so it can be kept
  and used in a loop. A firmware command given the variable by name works on
  the same memory: `MATH`, `SORT`, `LONGSTRING`, `WEB TCP CLIENT STREAM` and
  others that take a BASIC array now work from C through `pm_execute`.
- The variables a C program makes stay after it ends, as a BASIC program's
  do, until the next `RUN`.
- Errors are MMBasic's; `pm_try` catches them.

```c
#include <picomite/basic.h>
#include <picomite/vars.h>

pm_varinfo vi;
long long *buf = pm_dim("buf%(9)", &vi);    /* DIM buf%(9) */
for (int i = 0; i < vi.elements; i++)
    buf[i] = i * i;
pm_execute("MATH SET 7, buf%()");          /* BASIC's MATH on C's array */

unsigned char *s = pm_dim("s$ LENGTH 16", &vi);
pm_str_set(s, vi.size, "hello");
pm_execute("PRINT s$; LEN(s$)");
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
- No `setjmp`/`longjmp`, `signal`, `errno` values, locales, wide characters or
  threads.
- No debugger or backtrace support (`-g` is accepted but has no use here).
- A tight loop that never prints, reads input or calls a `pm_` function cannot
  be stopped with CTRL-C.
- The 32 KB stack is guarded only where the firmware checks its stack (in
  BASIC commands and expressions run from C, which report "Stack overflow").
  The program's own code is not checked: the bottom of the stack holds a
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
