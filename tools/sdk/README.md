# PicoMite loadable programs: the SDK

A loadable program is C or C++ built separately from the firmware and run with `RUN "name.app" [, args$]`. It gets the
firmware's services through a table (`loader/pmimg.h`) and runs at full speed from the PSRAM, then returns to MMBasic.
The design is in `docs/Loader_Plan.md`.

So far this is the PC3 build only, and the services are the console, time, `poll`, `leave`, `alloc`, the
firmware's `memcpy`/`memset`/`memmove`, `psram_size`, the files and directories, the display, the keyboard and
gamepads, the mouse, sound, and I/O (pins, ADC, PWM, I2C, SPI and the COM ports); and USER modules (below).

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
- Sound: `cmm2->audio_start(fill)` plays a 16-bit stereo stream through OPTION AUDIO's output and returns its rate
  (44.1 kHz) or a negative error. The firmware calls `fill(buf, frames)` from an interrupt each time a buffer is
  wanted (1024 frames, 23 ms): it must be quick and call no service, `malloc` or stdio. Change what `fill` reads
  between `audio_lock()` and `audio_unlock()`. The sound stops when the program ends. `examples/sndtest.c` tries
  it all.
- I/O is by GP numbers. A bus (I2C, SPI, COM1/COM2) uses the pins BASIC's SETPIN or OPTION SYSTEM gave it, or the
  ones `cmm2->bus_pins()` names. Pins the board keeps for itself are refused. Whatever the program sets up is undone
  when it ends. `examples/iotest.c` tries it all.

## USER modules

A USER module adds commands and functions of your own to MMBasic, written in C. Build it with `--user`:

```
python build.py out/proforma.mod examples/user/proforma.c --user
```

copy the `.mod` to A: or B:, then:

```
USER LOAD "proforma"           (.mod is added to a name without an extension)
USER HELLO "Peter"
PRINT USER(ADD 2, 3.5)
USER CLOSE
```

`examples/user/proforma.c` is the one to start from: its comments explain how a module reads its part of the statement
with the interpreter's own routines (`cmm2_start->basic`, `loader/pmuser.h`). `examples/user/modtest.c` tries the
services from inside a module.

- A module's functions run on MMBasic's stack, inside the statement: keep big buffers in `B->temp()` or `malloc`'s
  memory. An error (`B->error()`) leaves the module as it leaves any command, and `ON ERROR SKIP` catches it.
- It goes with the heap: RUN, NEW, LOAD, CHAIN, EDIT and the file manager drop it (its `forget()` is called first, and
  the files it left open are closed). The next USER loads it again from the same file, so its variables start again.
- It can call the services (`cmm2->...`) as a program does, but not `leave()`, and it should not change the screen's
  mode. `poll()` lets Ctrl-C break the statement, as in any command.
- `--user` gives it the firmware's `snprintf`, `sprintf` and `printf`, so it carries no printf of its own. `%f` is
  right, but `%e` and `%g` are not (1e6 comes out as 10.000000e+05, and `%g` keeps trailing zeros: 0.5 as 0.500000):
  format those with `B->float_to_str()`.
- Its C library's heap is 64 KB unless `--heap` says otherwise.
