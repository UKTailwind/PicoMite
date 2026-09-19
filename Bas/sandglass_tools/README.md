# Asset converter

This directory holds the converter. It reads a copy of the published Apple II
source release that **you** supply, and writes the data files the engine needs
onto your own machine.

Nothing here contains any of the game's artwork, levels, animation tables or
music. The engine ships with no game data in it either, and will not run until
you have done the step below. That is deliberate: the published source release
is explicit that it grants no rights to the game itself, so the game's content
is never ours to hand on. The code in this directory and in the engine is our
own work, and the content stays yours.

## What you need

1. Python 3.8 or later.
2. Your own copy of the published source release, obtained by you. Clone or
   download it yourself; this converter will not fetch anything for you.

## Converting

```
python convert.py --source <path to your copy> --out <output directory>
```

`--source` can point either at the top of your copy or at the source directory
inside it. The converter finds `Images/` and `Levels/` and stops with a clear
message if they are not there.

It writes:

| File | What it is |
|---|---|
| `sheet1.bmp` … | the artwork, one file per image slot, already in the display's colour format |
| `art.idx` | where each image sits in which sheet |
| `levels.dat` | the fifteen level layouts |
| `frames.dat`, `seq.dat` | the frame table and the animation byte code |
| `blocks.dat`, `tables.idx` | how a block draws, the screen geometry, the movers' tables and constants, and where everything sits |
| `sounds.dat` | the twenty sound effects, as a frequency and a length each |
| `cutroom.bmp` | the princess's room, for the interludes, if your copy has it |
| `convert.log` | a record of what was read and produced |

Copy those onto the board's drive in the same directory as the engine, and run
the engine.

### Options

`--set game` (the default) leaves out the artwork only the intro and ending use,
and fits four image slots. `--set full` converts every table and needs five.

## The interludes

The game cuts away to the princess's room on the way into levels 2, 4, 6, 8, 9
and 12, and again at both endings. The room is a full-screen picture, packed in
a form of its own, and `packpic.py` unpacks it; the converter finds it and
writes `cutroom.bmp` without being asked. It is loaded into the image slot after
the four sheets, which is free.

The scene is the picture, held for a few seconds, and any key cuts it short.
The original animates the princess, the vizier and the mouse over a clean room;
the picture in the source release already has the princess and the vizier
painted into it, and the clean room is on the shipped disk as a block number
rather than as a file, so there is nothing to animate over. Drawing the
characters on top of this picture would put two of each in the room.

If your copy has no such picture the interludes are skipped and nothing else
changes.

You can unpack any of these pictures yourself:

    python packpic.py <packed file> -o out.bmp

## Title screen (optional)

Put a `title.bmp` beside the converted data and it is shown before play, until
you press a key. Leave it out and the game starts straight away.

The picture is scaled to the screen by the seventh parameter of `LOAD IMAGE`,
which bins pixels, so a 640 by 480 image fits exactly. The game's own palette is
set aside while the title is up, because a photograph-like image is dithered
against the display's standard colours rather than the dungeon's.

As with everything else, the title artwork is the game's and is not supplied
here.

## Music (optional)

The game asks for sixteen tunes by name. None of them are supplied here, and
none are needed: with no music files present the game plays exactly as it does
now, and every cue is simply ignored.

If you want music, obtain your own copies and put them in a `music` directory
beside the converted data, as WAV files named after the cue:

| File | Played when |
|---|---|
| `danger.wav` | the first level begins |
| `sword.wav` | you pick up the sword |
| `potion.wav` | you drink a potion that changes you |
| `stairs.wav` | the exit door opens |
| `upstairs.wav` | you climb the stairs to the next level |
| `victory.wav` | an opponent falls |
| `tragic.wav` | you die |
| `accid.wav`, `heroic.wav`, `rejoin.wav`, `shadow.wav`, `jaffar.wav`, `shortpot.wav`, `timer.wav`, `embrace.wav`, `heartbeat.wav` | the remaining cues, for the parts of the game not yet ported |

The files must be PCM WAV, mono or stereo, eight or sixteen bit, up to 48 kHz.
Anything the board cannot open is treated as a missing file.

The audio output does one thing at a time, so a sound effect is dropped while a
tune is playing rather than cutting the tune short. That is a property of the
hardware, not a choice.

As with the artwork and the levels, the music is the game's content and is not
ours to pass on. Obtain it yourself, and keep it on your own machine.

## Building for the board

MMBasic keeps the whole program text in memory, comments included, and the
engine is written to be read. Strip it before copying it over:

```
python build.py player.bas -o player.min.bas
```

Copy `player.min.bas` to the board as `player.bas`. The stripped program is
about seventy per cent of the size and leaves room to grow. A `.map` file is
written beside it so a line number in an error message can be traced back to
the commented source.

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

## Notes

- The converter reads only the files you point it at and writes only into the
  directory you name.
- Converted output should stay on your machine. Please do not redistribute it.
- The colour rule follows the original hardware: a lit pixel on its own takes
  one of two colours depending on its position, and two or more together read as
  white. The converter checks that the source data uses a single palette group
  and refuses to guess if it does not.
