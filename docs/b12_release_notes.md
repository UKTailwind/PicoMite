**Beta for testing.** Please try it on programs you already have and report
anything that behaves differently; that is the most useful testing there is.
**No variant moves its A: drive this time** — the flash layout is unchanged
since b11, so every board keeps its files.

## New: sprites over a background on VGA and HDMI, without tearing

Until now the only way to overlay moving things on a VGA or HDMI build was to
draw into the live layer buffer, which core 1 is scanning out at the same
time, so it tears. `FRAMEBUFFER MERGE` now composites a second framebuffer
over the main one in a single pass:

```basic
FRAMEBUFFER CREATE          ' the background, F
FRAMEBUFFER CREATE 2        ' the moving things, 2
' ... draw the background once into F, the sprites into 2 each frame ...
FRAMEBUFFER MERGE 0, B      ' 2 over F into the display, at vertical blank
```

The optional colour is the one treated as transparent in buffer 2, following
the rule of the display mode: 0 to 15 in modes 2 and 3, 0 to 255 in mode 5,
an RGB value in mode 4, and in mode 1 the pixels are simply combined. `,B`
waits for vertical blank, so the display buffer is rewritten ahead of the
beam. RP2350 only, as `CREATE 2` is.

**It is fast enough to use every frame.** A 150 KB merge in mode 3 takes about
11 ms, roughly one frame at 75 Hz. Getting there took some care: the big
buffers live in PSRAM, and reading two PSRAM streams alternately defeats the
memory controller's bursts badly enough that the first version took 16 to 21
milliseconds. Each source is now staged through a small buffer in fast memory
and merged with one routine that handles four, eight and sixteen bit pixels a
word at a time.

`FRAMEBUFFER CREATE 2` also stops competing with the layer for memory. In
modes 1, 2 and 5 it takes the video memory the layer would have used when the
layer has not already claimed it, and `FRAMEBUFFER LAYER` skips that memory
when buffer 2 holds it. A layer that would then land in PSRAM is refused
rather than accepted and drawn too slowly.

## New: a library built from several files

```basic
LIBRARY LOAD "graphics.bas", "sound.bas", "maths.bas"
```

`LIBRARY LOAD` now takes a list. The files are read in the order given and
joined into one library, so the parts of a library can be kept in separate
source files and assembled on the board rather than pasted together first. Up
to eight files, and everything the command already did still applies: it must
be the program's first statement, `O` skips the question about replacing an
existing library, and `RAM` puts it in PSRAM instead of flash.

Two details worth knowing. A bare `O`, `OVERWRITE` or `RAM` anywhere in the
list is the option of that name rather than a filename, but a filename is
always a string expression, so a quoted `"O"` is unambiguously a file. And
nothing is written anywhere until every file has been read, so a mistyped name
in the last one leaves the library you already had untouched.

## New: GPS on a USB serial port

`OPEN "COMn:" AS GPS` accepted COM3 to COM6 but only ever started the parser
for COM1 and COM2, so a USB receiver enumerated, filled a buffer nobody read,
and `GPS()` answered "GPS not activated". USB CDC ports now work like the
UARTs.

## Fixed

- **`RAM ERASE ALL` cleared 128 KB beyond the end of the PSRAM.** The block it
  wiped was a fixed size that did not fit the space reserved for it, and since
  the memory window is wider than the part, the overrun wrapped round and
  cleared the bottom of the heap. Anything a program had there was silently
  destroyed.
- **The GPS monitor could overflow the stack.** Printing a sentence to a
  display console runs the background tasks once per character, and those
  include the GPS parser, which then printed the same sentence again. The
  UART path had the same hole.
- **`RAM FILE LOAD` wrote 1 KB past an internal table.** The PSRAM allocation
  map was declared with two different sizes in different files, so saving and
  restoring the interpreter's state copied more than the table holds. That
  path runs whenever a program loads an overlay.
- **`BLIT FLASH`, `BLIT FRAMEBUFFER` and `TILEMAP DRAW`** now accept
  framebuffer 2, so it can be drawn into by the same commands as any other
  buffer. `BLIT FRAMEBUFFER` accepts it as a source as well.

## For anyone building from source

- `FileLoadLibrary()` takes an array of filenames and a count. `lib_scan()`
  carries the hash and the embedded-routine count as running totals across the
  set, which matters because `MAXCFUNCTION` also sizes fixed arrays elsewhere
  and a per-file count would let several files overflow them.
  `SaveLibraryImage()` is unchanged: its address fixup already walked the text
  and the binary records in step, which is what makes concatenation work.
- `PSRAMblocksize` is derived from the slot count instead of being a fixed
  figure, and a static assert next to `cmd_psram` fails the build if a future
  heap increase would push the last RAM slot past the end of the reserve.
- The PSRAM allocation map's size is declared once, as `PSMAPWORDS` in
  Memory.h, rather than three times in three files.
- A static assert beside `SaveContext` now adds up what it copies and checks
  it against the space below the RAM slots.
- New board tests: `Testfiles/FramebufferMergeTest.bas` and
  `Testfiles/MultiFileLibraryTest.bas`, the latter with the three small library
  sources it loads.
