# Elite on the PicoComputer 3

A port of the 1984 BBC Micro *Elite* by Ian Bell and David Braben, written in
MMBasic. The galaxy, the market, the ship blueprints and the flight model all
come from the published 6502 source, so Lave is the Lave you remember.

## Getting it running

You need a PicoMite HDMI build and a screen, and the firmware must be
**version 6.03.02b4 or above**. `PRINT MM.VER` at the prompt: it must report
6.030204 or more. Earlier firmware will not do - b3 and before had `MAX3D` set
to 8, where the bubble wants 12 objects for the station and a full complement
of ships, and the `DRAW3D` and `FRAMEBUFFER CLOSE` fixes this leans on all
landed after b3 was released.

It was written and timed on a PC3 running PicoMiteHDMIWEB at 378 MHz, where it
holds about 18 ms a frame; it will run slower on a slower clock. The program
sets `MODE 2` itself.

Load the program over the console with the crunching form of AUTOSAVE, which
strips the comments as it receives - the full source is more than program
memory will hold, and plain `AUTOSAVE` stops with `Not enough memory` part way
through:

```
AUTOSAVE C
    ... paste Bas/elite/elite.bas ...
    ctrl-Z
RUN
```

Or, from a PC with the development tools:

```
python Bas/elite_tools/pc3.py run Bas/elite/elite.bas
```

The title screen is a picture file. Copy `Bas/elite/data/title.jpg` to the
drive as `A:/title.jpg` - `pc3.py put Bas/elite/data/title.jpg A:/title.jpg`
will do it. Without the file the title screen draws the words instead, so
nothing is broken if you skip this.

## Starting

The title screen waits for you.

| | |
|---|---|
| any key | start a game |
| `H` | the controls, on one page |
| `Esc` | leave the program |

Leave it alone for twenty seconds and the demo plays a whole game by itself - 
trading at Lave, a fight on the way out, a hyperspace jump and a docking at the
far end. Press any key during the demo and you get a game of your own.

`Esc` in a game takes you back to the title.

## Flying

| | |
|---|---|
| `<` `>` or left/right arrows | roll |
| `S` / `X`, or down/up arrows | climb / dive |
| `Space` / `/` | faster / slower |
| `A` | fire |
| `T` then `M` | lock a missile, then launch it |
| `E` | E.C.M., which destroys every missile in the area |
| `C` | docking computer on and off |
| `H` | hyperspace |
| `J` | in-system jump, when nothing but rocks is about |
| `G` | galactic hyperdrive, if one is fitted |
| `Tab` | energy bomb |
| `Esc` | escape pod if one is fitted, otherwise back to the title |
| `P` | pause |
| `F1` `F2` `F3` `F4` | fore, aft, left, right views |

`F5` to `F10` reach the same six screens whether you are flying or docked:
galactic chart, short range chart, system data, market prices, status,
inventory. On the charts the arrows move the cursor and it picks out the
nearest system; `F7` then tells you about it. A view key or `Return` puts you
back where you were.

### The dashboard

Down the left: forward shield, aft shield, fuel, cabin temperature, laser
temperature, altitude. Down the right: speed, roll, dive/climb, and the four
energy banks. Red means trouble in both directions - a high reading is bad for
speed and the temperatures, a low one is bad for everything else.

The altitude bar is your height above the planet, and it reads full until you
are within about 65000 units of it; fly to one planet radius and you are dead.
Cabin temperature climbs as you approach the sun, reaches the fuel scooping
threshold at about 32000 units, and kills you at about 22400.

The ellipse is the scanner. Each contact is a dash with a stick down to the
plane you are flying in, so the stick tells you how far above or below you it
is. Yellow is a missile. The dial to its right is the compass: it points at the
station when you are near one and at the planet when you are not, yellow and
two rows deep when the thing is ahead of you, green and one row deep when it is
behind.

## Docked

| | |
|---|---|
| `F1` | launch |
| `F2` / `F3` | buy / sell cargo |
| `F4` | equip the ship |
| arrows | choose a row |
| `Space` | buy or sell one |
| `F` | fill the tank, on the equipment screen |
| `S` / `L` | save / load your commander |

Saving writes `A:/cmdr.txt`, which is plain text and one value to a line.

## Making a living

Buy what a system makes and sell it where they don't. Lave is a Rich
Agricultural world, so food and textiles are cheap there and
radioactives, computers and machinery are dear - an Industrial system further
along the chart is where you sell the one and buy the other. Prices are set the
moment you arrive and do not move while you trade.

A full tank is seven light years and costs 14 credits. On the galactic chart
the green circle is what the tank will reach; put the cursor on something
inside it, check `F7` for what is there, then launch.

**Hyperspace only works once you are clear of the station's zone**, which means
flying away from it for about half a minute at full speed. Press `H` when you
are out.

## Docking

The station turns all the time, and its docking slot is a letterbox. Getting in
means five things at once: the station not angry with you, its slot facing you,
the station nearly dead ahead, and your wings lined up with the long axis of
the slot - which means rolling to match a station that will not stop turning.

Below speed 5 a failed approach is a bump. Above it, it is the end of you.

The docking computer (`C`) does the whole thing, including the rolling.

## The law

Slaves, narcotics and firearms are contraband, and slaves and narcotics count
double. Leaving a station with any of it aboard goes straight onto your record,
and out in space it is what you are **carrying** that calls the police out - 
your record only makes things worse once a Viper is already watching you. Shoot
one and you are a Fugitive on the spot. Arriving somewhere new halves whatever
is on your record, because nobody that far away has heard the details.

Where you are matters as much as what you have done. An anarchy spawns roughly
four times the pirates of a Corporate State, which is what the government
column on the system data screen is telling you.

## How close is this to the real thing

Almost everything the cassette version does, it now does. The galaxy, the
market, the ship blueprints, the flight model, the tactics, the spawning, the
legal model and all five docking tests are the original's own arithmetic.
Ships are wireframe with the hidden faces removed, decided from the blueprint's
own face normals, as the original decides it. The cassette version has no
missions, and no mining or military lasers, so none of those are missing.

The sound is the original's ten effects, converted from the SFX table in its
source. Five of the ten are its exact numbers; the other five ask for sound
envelopes that the cassette *loader* defined rather than the game, so those are
approximated by a pitch sweep and are marked as approximated in the table and
in `tests/sfxtest.bas`, which plays all ten by name so they can be judged.

What is left:

- **The hyperspace countdown.** The jump happens at once behind its tunnel of
  rings; the original counts down from 15 while you keep flying, and you can be
  attacked during it.
- **No indicator for the safe zone.** Getting clear of it to hyperspace takes
  about half a minute of flying away from the station and nothing tells you
  when you are out; press `H` and see.
- **Equipment cannot be damaged.** In the original a hit can take out your
  E.C.M.

Everything else on this list has been closed: sound, in-flight messages, the
per-view laser mounts, cargo scooping and fuel scoops, the altitude and cabin
temperature gauges with the planet and the sun that drive them, collisions,
the escape pod, the energy bomb, the in-system jump, the other seven galaxies,
ships that fire missiles at you and jam yours with their own E.C.M., pilots
who bail out of a dying ship, and equipment that has to be bought before it
works.
