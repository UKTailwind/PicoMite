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
| `P` | pause |
| `F1` `F2` `F3` `F4` | fore, aft, left, right views |
| `Esc` | back to the title |

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

The galaxy, the market, the ship blueprints, the flight model, the tactics, the
spawning and the docking tests are all the original's, so the parts that are
here behave as they should. Ships are wireframe with the hidden faces removed,
decided from the blueprint's own face normals, which is how the original does
it. The cassette version has no missions, so none are missing.

These are not here yet, and all of them are in the cassette original:

- **Sound.** There is none at all - no laser, no explosions, no E.C.M., no
  docking. This is the biggest single difference from a BBC Micro.
- **In-flight messages.** No "INCOMING MISSILE", no bounty announcements, no
  system name as you arrive.
- **Cargo scooping and fuel scoops.** Kills drop canisters and you cannot pick
  them up, so part of the economy is missing.
- **The escape pod, the energy bomb, and the in-system jump.**
- **The other seven galaxies.** The galactic hyperdrive can be bought and does
  nothing.
- **Altitude and cabin temperature.** Both gauges are fixed at a dummy value.
  In the original the altitude bar is your height above the planet and you can
  fly into it, and cabin temperature rises near the sun - which is what makes
  fuel scooping both possible and dangerous.
- **Lasers per view.** The original sells front, rear, left and right mounts
  separately; here one laser serves every view.
- **The hyperspace effect.** The jump is instant. Launching has its tunnel.

And two things that are wrong rather than absent:

- The laser does not know which way you are looking. Firing and missile locking
  both test the ship's position in world coordinates, so in the rear, left and
  right views the crosshairs show one ship and the shot hits whatever is in
  front of you.
- The E.C.M. and the docking computer work whether or not you have bought them.

Getting clear of the safe zone to hyperspace takes that half a minute of flying
and there is no indicator telling you when you are out; press `H` and see.
