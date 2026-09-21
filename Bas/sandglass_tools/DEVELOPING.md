# Prince of Pico - the development kit

This is the half of the tools that a player does not need: the renderer, the
host-side reference implementations and the scenario harness. It stays in the
repository and is deliberately **not** in the zip handed to a player, which
carries only what is needed to convert the data and play the game.

Everything here assumes you have already followed `README.md`: a converted
`board/` directory, a board with the engine on it, and - for the scenarios - a
serial or network route to it (`Bas/elite_tools/pc3.py`,
`Bas/exile_tools/tftp.py`).

**The same rule applies to everything these tools produce.** The pictures
`rooms.py` draws are the game's own artwork. See "Keep what comes out to
yourself" in `README.md`.

## Looking at every room

`rooms.py` draws each screen of each level to a picture, without a board:

```
python rooms.py --data <the board/ directory> --out rooms --sheet
```

336 pictures - fourteen levels of twenty-four screens - in about five seconds,
plus one contact sheet per level with `--sheet`. `--level N` does one level and
`--scale N` makes them bigger.

It reads the same files the engine does and composes a screen the way
`DrawScreen` and `DrawFront` do, so what comes out is the scenery as the game
draws it. **The moving parts are not in it**: gates, loose floors, the plates,
spikes, slicer blades and torch flames are redrawn every frame by `DrawMovers`,
so a gate shows as an empty recess and a loose floor as a gap. Neither are the
characters, which is the point - these are the rooms.

Needs Pillow (`pip install pillow`). The pictures are the game's own artwork and
are no more yours to pass on than the data they come from.


## Checking your setup

`selftest.py` writes a sheet carrying a test pattern instead of any game data:

```
python selftest.py --out <directory> --width 512 --height 256
```

Copy the result to the board and run `blitspike.bas`. It checks that a sheet
loads into an image slot and draws back correctly, and reports what a frame's
drawing costs. Useful if something looks wrong, and it needs no game data at
all.

## Checking the engine against the host

Each part of the engine has a host-side reference in Python and a BASIC test
that runs the same cases on the board and prints a checksum. The two must
agree exactly; a demo that "looks right" is not evidence, these are.

| Host reference | Board test | What it proves |
|---|---|---|
| `verify.py` | `seqtest.bas` | every animation sequence walks cleanly |
| `animref.py --mode advance` | `animtest.bas` | one frame advance, every sequence, 24 frames |
| `animref.py --mode floor` | `floortest.bas` | the floor test under every pose and block |
| `animref.py --mode land` | `landtest.bas` | landing severity |
| `animref.py --mode grab` / `grabrule` | `grabtest.bas` | the ledge-grab gates and rule |
| `moverref.py --level 1` and `--level 3` | `movertest.bas` | plates, gates, spikes, loose floors, slicers, torches |

Each test prints PASS or FAIL and the numbers it compared. When a reference
changes, rerun it and copy its numbers into the test's `EXP_` constants.

`player.bas` is the assembled engine. It runs a short visual demo, then a
headless coverage pass of scripted scenarios and prints what each reached.

## Scenarios

The tests above each pin one piece of the engine against a reference written
on the host. That works where the original is small and self-contained, and
stops working for the game itself: the rules for what the character may walk
into involve the blueprint, the moving parts and his own state at once, and a
transcription of all that would be a second thing to keep right.

So the engine is the test rig. A file called `scen.txt` beside `player.bas` on
the board makes it play scenarios instead of the game, with nothing drawn and
no pacing, and check where the character ended up:

    SCEN he gets past a slicer that is not shut
    AT 4 12 4 0
    RUN ..>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    WANT BX LE 2
    WANT ALIVE 1
    END

`AT` is level, screen, block across and block down in the original numbering;
`RUN` is one character a frame, the same codes the demo scripts use; `WANT`
checks one of the names `ScenValue` knows when the scenario ends. The full
list of directives is in the comment above `RunScenarios`.

    python run_scentest.py scen/slicer.txt            put the file and run it
    python run_scentest.py scen/slicer.txt --engine   put the engine too
    python run_scentest.py                            every scenario in scen/

The engine goes to the board once - seconds over TFTP, a couple of minutes over
XMODEM - and a scenario goes in under a second, which is what makes it worth
writing one per reported bug.
`--engine` takes a path, so the same scenarios can be run against an older
build to show that they do notice the bug before they are trusted to show it
is gone. `--clean` takes `scen.txt` off the board again so the next `RUN`
plays the game.

No converted data ever leaves the board, so the blueprint cannot be read on
the host and a scenario cannot be written by guessing at the geometry.
`FIND lvl type` lists every block of a type on a level and `SHOW lvl scrn`
lays one screen out with its exits, which is how the example above knew where
that slicer was and what was on either side of it.

`scen/` holds the scenarios. Each one names the finding it came from.
