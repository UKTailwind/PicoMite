# PicoMite loadable programs: the SDK

A loadable program is C or C++ built separately from the firmware and run with `RUN "name.app" [, args$]`. It gets the
firmware's services through a table (`loader/pmimg.h`) and runs at full speed from the PSRAM, then returns to MMBasic.
The design is in `docs/Loader_Plan.md`.

So far (Phase 1 of the plan) this is the PC3 build only, and the services are the console, time, `poll`, `leave`,
`alloc`, the firmware's `memcpy`/`memset`/`memmove`, and `psram_size`. Files, the display, input, sound and I/O follow
in later phases.

## Building

You need the Arm GNU Toolchain 13.3 (the firmware's own) and Python 3. `build.py` finds the compiler in
`ARM_TOOLCHAIN_DIR`, on the PATH, or in the toolchain's usual Windows folder.

```
python build.py out/hello.app examples/hello.c
```

Copy `out/hello.app` to A:, B: or C: (by TFTP, or a file transfer that keeps the exact size: XMODEM pads the file
and the loader then refuses it), then:

```
RUN "hello.app"
RUN "hello.app", "code"
```

Options:

| Option | Meaning |
|---|---|
| `--stack BYTES` | the program's stack (default 32 KB, from the SRAM heap) |
| `--heap BYTES` | the least heap it can run with; RUN refuses it if there is less |
| `--psram` | marks it as needing PSRAM |
| `--base ADDR` | the link address (default 0x11000000). `--base 0x30000000` makes a copy the loader must move: the test of the relocations |
| `--no-printf-float` | leaves out printf's `%f` (4 KB) |
| `--libc full` | the whole newlib rather than newlib-nano |
| `--cflag`, `--cxxflag`, `--ldflag`, `-I` | passed on |

Do not put the firmware's include folders on `-I`: Windows ignores case, so its headers would be taken in place of a
program's own.

## Writing a program

Include `cmm2.h`. The names are the Colour Maximite 2's, so a program's portable parts build for either:
`cmm2->con_puts("hi\r\n")`, `cmm2_start->heap_size`. Ordinary C works through `syscalls.c`: `printf`, `malloc`,
`time`, `clock_gettime`, termios on the console.

- Call `cmm2->poll()` often (each frame of a game, or every 10-20 ms): the USB keyboard, the network and Ctrl-C are
  looked after only then. When it returns 1, end the program.
- Size buffers from `cmm2_start->heap_size`, never from a fixed figure: run from a BASIC program, the program has
  what the BASIC program's variables leave.
- A program cannot use SVC or PendSV (the firmware owns them), and must leave the interrupts, DMA, PIO and core 1
  alone.
- A fault in a program restarts the board.
