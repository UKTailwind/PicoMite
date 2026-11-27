# Chess with a chess clock thread

`chess_threads.bas` is TSCP, Tom Kerrigan's Simple Chess Program, as converted to MMBasic by Ceptimus and others, in
its version for VGA and HDMI displays (`chesscsubVGA.bas`), with the computer's search in a CSUB in the library. It
adds a chess clock that runs as a thread and fixes the board's redraw. It needs PicoMite **V8.00.00b5 or later**: its
library's search calls the firmware's `CSubYield`, which earlier firmware does not have. It runs on a VGA or HDMI
build or a PicoMitePC3 (MODE 1), or on an LCD panel with `FRAMEBUFFER CREATE`.

| File | |
|---|---|
| `chess_threads.bas` | the program |
| `chesslib.bas` | the search, a CSUB made by mmb2csub, for the library |
| `book.txt` | the opening book |
| `Tiles/` | the pieces |

Put the folder on the A: drive or an SD card, `CHDIR` to it and `RUN "chess_threads.bas"`. The first run installs
`chesslib.bas` in the library with `LIBRARY LOAD "chesslib.bas", O` and starts the program again. **The O replaces
whatever library the board had, without asking.** Later runs find the library already in place and go straight on.
Type `help` at the `tscp>` prompt for the commands: a move is typed as `e2e4`, and `on` makes the computer play the
side that is to move.

## What changed

**The clock.** Below the moves list, each side's total time and the time of its current or last move, with the
running side's figures counting up. The main program books the time at each move, take-back and new game
(`ClockMove`, `ClockReset`), so the totals are exact whatever the display is doing, and the clocks stop at the end of
a game (`ClockStop`). `ClockThread` draws them five times a second, and redraws only a field whose figures have
changed. The thread is started at the top of the program: a thread's stack must be in the processor's own memory,
and on a board with PSRAM the engine's arrays fill that otherwise.

**The search lets the clock run.** A CSUB is a single statement, and threads take turns between statements, so the
original search held every other thread for a whole depth of the search, seconds at a time. The copy of the search
kept in the program, commented out, now has a `THREAD YIELD` beside its time check, every 128 nodes, and
`chesslib.bas` was made from it again by mmb2csub, which turns the `THREAD YIELD` into a call to the firmware's
`CSubYield`. Each yield gives the clock thread one time slice, a millisecond. A slice every 16 ms or so is too
little to redraw the clock in, so `ClockThread` puts `THREAD HOLD` and `THREAD RELEASE` round each redraw, which is
then done in one turn of about 24 ms. The clock now never stops for more than about a quarter of a second while the
computer thinks.

**The board's redraw.** `draw_chessboard` remembered each square's piece but not its colour, so after a capture of a
like piece, a pawn taking a pawn for example, it did not redraw the square, and the captured piece stayed on show
until a later move redrew it. It now remembers the piece and its colour.

To make `chesslib.bas` again: in a copy of the program, uncomment the search, delete the `LIBRARY LOAD` line, and run

```
python mmb2csub.py chess.bas search --library chesslib.bas --lean --no-backup -O 2
```

`-O 2` rather than the default `-O s`, which made the search about a quarter slower. The program's comments mention
`chessiter.bas`, the same program in plain BASIC, which is not part of this folder.

## How it was tested

On a PicoMitePC3 at 315 MHz in MODE 1 (HDMI), driven over the console. The `bench` command searches 550,778 nodes in
70.7 s, the same nodes as the original program, which takes 68.0 s, so the clock costs the search about 4%. Games
against the computer, with the clock thread measuring its own longest gap between updates while the computer thought:
261 ms at most, against 789 ms before the `THREAD HOLD` and a whole depth of the search before the `THREAD YIELD`. The
LCD panel path is not tested.
