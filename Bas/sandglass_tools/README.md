# Prince of Pico

A dungeon game for the PicoComputer 3, in the style of the Apple II original.
The player's manual is in `docs/Prince_of_Pico_Player_Manual.html`.

**None of the game's own data is included here, and what you convert must not
be passed on.** See [Asset converter](#asset-converter) - it is the first thing
to do, and the game will not start without it.

## Quick start

Make a directory, unpack the zip into it, and run the one script:

```
setup.bat                      Windows
./setup.sh                     Linux and macOS
```

It fetches the game's own data from the published source release, converts it,
strips the engine, and leaves everything the board wants in a `board/`
subdirectory. If you already have a copy of the release, give it the path and
it downloads nothing:

```
setup.bat  C:\Users\you\Prince-of-Persia-Apple-II
./setup.sh ~/Prince-of-Persia-Apple-II
```

Then copy the whole of `board/` onto the board's drive and

```
LOAD "A:/player.bas", C
RUN
```

The rest of this file is what those steps are doing, and what to do when one of
them does not work. **What lands in `board/` is the game's own content and is
not yours to pass on** - see [Keep what comes out to
yourself](#4-keep-what-comes-out-to-yourself).

## The board

**Firmware 6.03.02b12 or later.** The RAM image slots the artwork is loaded
into arrived just before it, and b12 is what this has been built and played
against.

**PSRAM is required.** It is not an optimisation and there is no fallback: the
four artwork sheets and the interlude picture go into image slots 4 to 8,
which are the RAM slots, and those are carved out of the top of PSRAM. Without
it they have nowhere to go and the game stops as it starts. `OPTION LIST`
should show a line like `OPTION PSRAM PIN GP47`.

The game draws in `MODE 2`, 320 by 240 in sixteen colours, and needs two
framebuffers. Both the HDMI and the VGA builds do that; it has been played on
`PicoMiteHDMIWEB` and on `PicoMiteVGAUSB`, both RP2350B. A USB keyboard is
needed to play, so a build with USB host support.

About 1.1 MB of the drive, for the engine, the converted data and the title.

The console output goes to the serial port - the engine sets `OPTION CONSOLE
SERIAL` on its first line - so the screen belongs to the game alone.

## Asset converter

Nothing here contains any of the game's artwork, levels, animation tables or
music, and the engine ships with none in it either. It will not run at all
until you have fetched the game's own data and converted it yourself, which is
what this section is for. The code in this directory and in the engine is our
own work; the content is the game's, and it stays yours.

### 1. Python

Python 3.8 or later. The converter needs no packages beyond the standard
library - there is nothing to install.

### 2. The published source release

The data comes from Jordan Mechner's own publication of the original Apple II
source, on GitHub:

**<https://github.com/jmechner/Prince-of-Persia-Apple-II>**

Get your own copy of it. Either

```
git clone https://github.com/jmechner/Prince-of-Persia-Apple-II.git
```

or, on that page, **Code -> Download ZIP** and unpack it. The converter will
not fetch anything for you: it reads only the files you point it at.

What it needs from inside that copy is the directory `01 POP Source`, which
holds `Images/` and `Levels/`. The interlude picture, `PAC.PROOM`, is looked
for anywhere in the copy and is skipped if this one does not carry it.

### 3. Convert

Point `--source` at the top of your copy - the directory holding
`01 POP Source` - or at `01 POP Source` itself. Either works.

```
python convert.py --source Prince-of-Persia-Apple-II --out popdata
```

On Windows, with the paths written out:

```
python convert.py --source C:\Users\you\Prince-of-Persia-Apple-II --out C:\Users\you\popdata
```

If `Images/` and `Levels/` are not where it looked it stops and says so rather
than writing anything.

`--set game` is the default and is what you want: it leaves out the artwork
only the intro and the ending use, and fits four image slots. `--set full`
converts every table and needs five.

It writes:

| File | What it is | On the board |
|---|---|---|
| `sheet1.bmp` … `sheet4.bmp` | the artwork, one file per image slot, already in the display's colour format | yes |
| `art.bin` | where each image sits in which sheet, as the engine reads it | yes |
| `levels.dat` | the fifteen level layouts | yes |
| `frames.dat`, `seq.dat` | the frame table and the animation byte code | yes |
| `blocks.dat`, `tables.idx` | how a block draws, the screen geometry, the movers' tables and constants, and where everything sits | yes |
| `sounds.dat` | the twenty sound effects, as a frequency and a length each | yes |
| `cutroom.bmp` | the princess's room, for the interludes, if your copy has it | yes |
| `art.idx` | the same index as `art.bin`, in text, for reading | no |
| `convert.log` | a record of what was read and produced | no |

The thirteen marked **yes** go onto the board's drive, in the same directory as
the engine - about 580 KB of it, three quarters of that the four sheets.
`art.idx` and `convert.log` are for you to look at and the engine never opens
them.

### 4. Keep what comes out to yourself

**The converted files must not be distributed.** They are the game's own
artwork, levels, animation tables and sound written into another format, and
changing the format changes nothing about whose they are. The terms on the
release are explicit:

> As the author and copyright holder of this source code, I personally have no
> problem with anyone studying it, modifying it, attempting to run it, etc.
> Please understand that this does NOT constitute a grant of rights of any kind
> in Prince of Persia, which is an ongoing Ubisoft game franchise. Ubisoft alone
> has the right to make and distribute Prince of Persia games.
>
> - Jordan Mechner, in the release's own README

So the output directory is for your machine and your own board. Do not commit
it to a repository, put it in an archive or a disk image, attach it to a
release, post it, or hand it to anyone else - and that goes for anything made
out of it, the `sheetN.bmp` artwork on its own included. Nor for a board you
pass on with the files already on its drive.

Anyone else who wants to play fetches the release and runs the converter
themselves, exactly as you have just done. That is the whole reason the
converter exists rather than a pack of ready-made files.

The music is not ours to pass on either - see [Music](#music-optional).

## Things you can pick up

A flask and a sword drawn in the stone's own colours are nearly impossible to
spot, so their solid pixels are given a colour of their own: green for a potion,
gold for a blade. Only the solid pixels change. The dither around them stays
with the scenery, which matters for the sword, because its picture is a strip of
floor with the blade lying in it and colouring the whole image would tint the
floor.

Nothing else in the artwork uses either colour. They are set at the top of
`convert.py`, beside the rest of the palette, and can be changed there.

## Testing a later level

Four constants near the top of the engine begin play somewhere other than the
start of level one, so a later level can be looked at without playing up to it:

    Const BEGINLEVEL = 0        ' 0 for a real game
    Const BEGINSCRN = 1
    Const BEGINBX = 0
    Const BEGINBY = 0

`Const SNAPSHOT = n` saves that frame's picture to the drive beside the data.

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

## Title screen

`title.jpg` is supplied, and unlike everything else in this section it is ours
to pass on: it was drawn by a user of [TheBackShed](https://www.thebackshed.com)
forum rather than taken from the original. Put it beside the converted data and
it is shown before play until you press a key, or for eight seconds. Leave it
out and the game starts straight away.

It is `LOAD JPG`, not `LOAD IMAGE`. The JPG form is the one that carries a
seventh parameter, the scale, which shrinks the picture by averaging blocks of
pixels - 2 for a half - so the 640 by 480 picture fits this 320 by 240 screen
exactly. `LOAD IMAGE` takes six parameters and has no scale at all, and a
seventh argument there is a syntax error. The game's own palette is set aside
while the title is up, so the picture is drawn against the display's standard
colours rather than the dungeon's.

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

Copy `player.min.bas` to the board as `player.bas`. The stripped file is about
145 KB, some sixty per cent of the commented source. A `.map` file is written
beside it so a line number in an error message can be traced back to the
commented source.

Getting it there: an SD card is the simplest route - put the whole of `board/`
on one and the board reads it as `B:`. On a build with WiFi the board's own
TFTP server is much quicker than the console; otherwise it is XMODEM over the
console at about 5 KB a second, which for the whole set, engine and data
together, takes a shade under two minutes.

### Loading it

```
LOAD "A:/player.bas", C
```

The `C` crunches as it reads, throwing away the comments, the blank lines and
the spaces inside lines. Crunched, the program is **123K of program memory**.
The engine is written to be read and is close to what a board will hold, so
use the `C` everywhere: it is never wrong and it costs nothing.

It is only strictly needed where program memory is 144K, which is the HDMIWEB
build: there the stripped file does not load without it, and that has been
seen. The VGA build has 160K, which should be room enough to take it as it is,
though the `C` was used there too and the plain load has not been tried.

**A load that does not fit is quiet about it.** It prints `Error : Not enough
memory` in one line and leaves the PREVIOUS program in memory, so the board
goes on running whatever it had, which looks as though nothing happened. Read
that line before you type `RUN`.

**And check there is no `scen.txt` beside `player.bas`.** That file turns the
engine into a test rig: it runs the scenarios in it, prints the results to the
console and stops, without playing at all. Nothing here creates one, but if it
somehow appears, delete it.

### Two things that stop it starting

`Option Local Variables 128`, on the engine's fourth line, moves the balance
between global and local variable slots, and MMBasic will only do that while
no variable exists at all. Anything that has declared one stops the program
dead with `Error : Variables already declared`:

* **a library in flash.** Even a single `Const` in it counts. `LIBRARY DELETE`
  clears it, and `FLASH LIST` shows whether one is there.
* **a previous run.** Variables left behind by the last program count too.
  `CPU RESTART` clears them; the program stays in memory, so `RUN` follows.

## Notes

- The converter reads only the files you point it at and writes only into the
  directory you name.
- Converted output stays on your machine and must not be redistributed - see
  "Keep what comes out to yourself", above.
- The colour rule follows the original hardware: a lit pixel on its own takes
  one of two colours depending on its position, and two or more together read as
  white. The converter checks that the source data uses a single palette group
  and refuses to guess if it does not.
