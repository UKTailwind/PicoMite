# FRAMEBUFFER WARPH and WARPV: per-row and per-column scrolling

*23 November 2026. Design only: nothing is built yet. It covers the PicoMite's VGA and HDMI builds and the
Colour Maximite 2, so that one BASIC program gives the same picture on both. Decisions marked **(Peter)** are his,
from the review on 23 Nov. Section 11 lists what is still open.*

## 1. What it is for

Many old games scrolled each line or column of the screen by its own amount:
- the road in a racing game, whose curves are each line moved sideways by a different amount, and whose hills come
  from choosing which line of the road picture each screen line shows;
- parallax, where the sky, far hills, near hills and ground move at different speeds;
- water, heat haze and "wobble" effects, which are a sine table of line offsets that moves a little each frame;
- column effects such as the screen "melt" between two pictures, or a playfield whose columns scroll vertically.

The hardware of the day did this at no cost, with a raster interrupt or a copper list that changed the scroll
register at chosen lines.

MMBasic can do this today only with a BLIT FRAMEBUFFER for each line, and two of them for a line that wraps. That is
about 480 interpreted statements a frame in a 320x240 mode, and the time goes on interpreting them, not on moving
the pixels. WARPH and WARPV do the whole job in one statement. Their offsets come from an integer array, which BASIC
updates each frame with an array operation (MATH SCALE, MATH ADD, MATH C_ADD, MATH SLICE), not with a loop over the
lines.

**It is a copy from one buffer to another, not a scanout effect (Peter).** On the PicoMite, applying offsets in core
1's line loop was considered and rejected: "experience has shown the timings are so marginal it is unlikely to work".
The CMM2's hardware routes (the LTDC window and a copper list, docs/Game_Graphics_Ideas.md items 1 and 2 in the CMM2
tree) would work on the CMM2 only. WARPH and WARPV work the same on both machines, and stay the portable route if the
copper list arrives later.

## 2. Decisions so far

- **Names: WARPH and WARPV (Peter).** They are FRAMEBUFFER subcommands. The CMM2 also gets a PAGE form (section 3).
- **step (Peter):** each element of the offset array covers `step` rows (WARPH) or columns (WARPV). "With step = 2
  the array needs 120 elements on a 320x240 display mode."
- **phase (Peter):** the element where the table starts, so a precomputed table (a sine wave) can be animated with
  no per-frame loop.
- **The source and the destination must be different buffers (Peter).** In-place warping is not offered in the
  first version.
- **VGA and HDMI only (Peter).** The PicoMite's LCD builds do not get the command for now.
- **Integer arrays only (Peter).** MATH SCALE already turns a float array into an integer one.
- **Branch (Peter):** v8 on the PicoMite, and v8-dev on the CMM2.
- **Directions (Peter):** a positive offset moves a row right (WARPH) and a column up (WARPV), as SPRITE SCROLL and
  PAGE SCROLL do (5.1).
- **rows%() in a band (Peter):** the band shows consecutive source rows (5.3).
- **The CMM2's F in its 8, 12, 16 and 32-bit modes (Peter):** the world framebuffer, as FRAMEBUFFER WRITE F means
  there (7.2).
- **FRAMEBUFFER WAIT in every CMM2 mode (Peter)** (9.2).
- **No ",B" argument.** Call FRAMEBUFFER WAIT first instead. On the PicoMite's LCD builds ",B" means "copy in the
  background on core 1", while on VGA/HDMI and the CMM2 it means "wait for the frame". Leaving it out keeps that
  conflict out of a new command.

## 3. Syntax

```
FRAMEBUFFER WARPH from, to, xoff%() [, step] [, first] [, phase] [, edge] [, transparent] [, rows%()]
FRAMEBUFFER WARPV from, to, yoff%() [, step] [, first] [, phase] [, edge] [, transparent]
```

On the CMM2 only, the same commands also exist with page numbers, written as PAGE COPY is written:

```
PAGE WARPH from TO to, xoff%() [, step] [, first] [, phase] [, edge] [, transparent] [, rows%()]
PAGE WARPV from TO to, yoff%() [, step] [, first] [, phase] [, edge] [, transparent]
```

The two forms take the same arguments, and the arguments mean the same things in both. Only the way the buffers are
named differs. The argument order is Peter's: step comes first because it decides how long the array is. The PAGE
form is written with TO, as PAGE COPY is, and has no deferred D form **(Peter)**.

| Argument | Meaning | Default |
|---|---|---|
| `from`, `to` | The buffers: letters in the FRAMEBUFFER form (section 7), page numbers in the PAGE form. They must differ. | — |
| `xoff%()`, `yoff%()` | A 1D integer array of offsets, one element for each band of `step` rows or columns. | — |
| `step` | The number of rows (WARPH) or columns (WARPV) that each element covers: 1 up to the buffer's height or width. | 1 |
| `first` | The first row (WARPH) or column (WARPV) to process. | 0 |
| `phase` | The element used for the first band. Any integer, taken modulo the number of elements. | 0 |
| `edge` | What a destination pixel gets when no source pixel moves onto it: omitted means wrap; −1 leaves the pixel as it is; a colour fills it. | wrap |
| `transparent` | A source pixel of this value is not written. | none |
| `rows%()` | WARPH only: the source row for each band (see 5.3). | each row from itself |

## 4. Which rows and columns are processed

Let `n` be the number of elements in the offset array, whatever OPTION BASE is: `DIM off%(119)` under OPTION BASE 0,
or `DIM off%(120)` under OPTION BASE 1, holds 120 elements. Elements are counted from 0 below.

- There are `n` bands. Band `b` covers rows (WARPH) or columns (WARPV) `first + b*step` to `first + b*step + step - 1`.
- **The last band is cut at the edge of the buffer**, so `step` does not have to divide the height or width. For
  example, 150 rows with step 4 need 38 elements, and the last band covers 2 rows.
- **Every element must start on the buffer.** If `first + (n-1)*step` is past the last row or column, that is an
  error (section 6). This rule is exact and guesses nothing: the most elements allowed is
  `ceil((size - first) / step)`.
- An array with fewer elements processes fewer bands. Rows or columns after the last band are left as they are, and
  so are those before `first`, so a status line can be kept still at either end.
- **Band `b` uses element `(b + phase) MOD n`**, with the remainder made non-negative, so `phase` can be any integer
  and can simply be counted up for ever. A table whose length is a whole number of wave periods wraps without a seam.

## 5. What each pixel gets

The rules below are written per pixel so that the two firmwares give the same picture bit for bit. The
implementations work a row at a time (section 9).

### 5.1 Direction

Directions follow SPRITE SCROLL on the PicoMite and PAGE SCROLL on the CMM2, which both move the picture right for a
positive x and **up for a positive y** **(Peter)**:

- **WARPH:** destination pixel (x, y) is source pixel (x − off, sy). A positive offset moves the row right.
- **WARPV:** destination pixel (x, y) is source pixel (x, y + off). A positive offset moves the column up.

where `off` is the band's element, and `sy` is y itself, or comes from `rows%()` (5.3).

### 5.2 Wrapping and the edge

- **edge omitted (wrap):**
  - WARPH: x − off is taken modulo the source's width;
  - WARPV: y + off is taken modulo the source's height.
  - Any offset value is allowed, so a program can pass a camera position that keeps growing.
- **edge = −1:** where the source position is outside the source, the destination pixel is left as it is.
- **edge = a colour:** those pixels get the colour, the same one BOX would draw with it in that mode. Edge pixels are
  written whether or not they match `transparent`.
- When an offset's size is the width (or height) or more, the whole band is uncovered.

This is SPRITE SCROLL's convention for its fill colour. getColour (graphics/Draw.c:676) refuses anything below −1,
so WARPH and WARPV do not take −2. (The SPRITE manual and the game guide said that −2 selects wrap for SPRITE SCROLL;
they were corrected on 23 Nov 2026.)

### 5.3 rows%() (WARPH only)

- It must have the same number of elements as `xoff%()`. The same element `e = (b + phase) MOD n` is used from both.
- `rows%(e)` = −1 leaves the band's destination rows exactly as they are, so no edge fill happens there either.
- Otherwise `rows%(e)` is the source row shown on the band's first row. The band's next rows show the source rows
  after it, wrapping from the bottom of the source to its top **(Peter)**. Consecutive rows keep the picture's detail.
  Line doubling (the same source row on every row of a band) can still be had with step 1 and repeated values.
- The value range is −1, or 0 up to the source's height − 1.

Uses: hills and perspective on a racing road (which line of the road picture each screen line shows), kerb stripes
(pointing at a second copy of the road drawn in the other colour), split screens, and skipping rows.

### 5.4 transparent

A source pixel equal to `transparent` is not written, and the destination keeps its pixel. Layers with different
offsets can then be stacked into one buffer. The value is given as FRAMEBUFFER MERGE and LAYER take it:
- the 4-bit modes: 0 to 15;
- the 8-bit modes: 0 to 255;
- the 16-bit modes: an RGB colour converted to the buffer's format (the PicoMite's MODE 4: RGB555());
- the CMM2's 12-bit and 32-bit modes: section 9.2.

## 6. Errors

All of these are raised before any pixel is written.

| Condition | Message |
|---|---|
| `from` and `to` are the same buffer | `Same framebuffer` (PAGE form: `Same page`) |
| A buffer that was not created | the message FRAMEBUFFER WRITE gives for that letter |
| The 1-bit mode (PicoMite MODE 1, CMM2 MODE n,1) | `Not available in mode 1` (PicoMite); `Invalid for this screen mode` (CMM2, as its BLIT FRAMEBUFFER) |
| The offsets are not a 1D integer array | the message the MATH commands give (parseintegerarray) |
| `step` or `first` out of range | the usual out-of-bounds message, with the range |
| Too many elements | e.g. `xoff%() has 121 elements: from row 0 with step 2 the buffer takes 120` |
| `rows%()` of another length | e.g. `rows%() has 119 elements and xoff%() has 120` |
| A `rows%()` value out of range | e.g. `rows%(57) is 300: a source row is -1 or 0 to 239` (the index as the program numbers it) |
| `transparent` out of range | the usual out-of-bounds message |
| CMM2, buffers of different sizes: the band's columns (WARPV) or rows (WARPH without `rows%()`) are not all in the source | e.g. `Columns 0 to 799 are not all in the source (640 wide)` |

## 7. Where it works

### 7.1 Display modes

| Pixels | PicoMite (VGA, HDMI) | CMM2 |
|---|---|---|
| 1 bit | MODE 1: refused | MODE n,1: refused |
| 4 bits (RGB121, two to a byte) | MODE 2; MODE 3 (RP2350) | MODE n,4 |
| 8 bits | MODE 5 (HDMI): palette, or raw RGB332 at the RGB332 resolutions | MODE n,8 (palette) |
| 12 bits (ARGB4444) | — | MODE n,12 |
| 16 bits | MODE 4 (HDMI, 640x480 and 720x400): RGB555 | MODE n,16: RGB565 |
| 32 bits | — | MODE n,32 (G2) |

The PicoMite's LCD builds (SPI, parallel, VGA222, NEXTGEN panels) do not have the command, and give a syntax error
**(Peter)**. A message saying it is VGA and HDMI only would cost flash in every LCD build.

### 7.2 Buffers

- **PicoMite:** N, F, L, and on the RP2350 also 2 and T, as FRAMEBUFFER COPY takes them. All of them are the size of
  the screen.
- **CMM2, the 4 and 1-bit modes:** the same letters, which are pages 0 to 4 (Rgb121Letter, Src/Rgb121.c). A
  PicoMite MODE 2 or 3 program runs unchanged under OPTION PICOMITE.
- **CMM2, the 8, 12, 16 and 32-bit modes:** there FRAMEBUFFER is the CMM2's own, a "world" that can be larger than
  the screen. N is page 0 and F is the world (page WPN), as FRAMEBUFFER WRITE F means in those modes **(Peter)**.
  TILEMAP and the image slots still read F as page 2 in every mode, through Rgb121Letter, so WARPH and WARPV must
  not use Rgb121Letter in these modes. With the world as the source, WARPH works like a per-line FRAMEBUFFER WINDOW:
  the offsets wrap at the world's width.
- **CMM2, the PAGE form:** pages 0 to LastPage, in every mode where PAGE COPY works. In the 4 and 1-bit modes, PAGE
  allows only WRITE, COPY and SCROLL today (the check at the top of cmd_page). WARPH and WARPV join that list.

When `to` is the visible buffer, call FRAMEBUFFER WAIT first; the destination is written from the top down.
- **PicoMite:** the copy is then ahead of the beam whenever it finishes within the frame, as FRAMEBUFFER MERGE is.
- **CMM2:** WAIT returns at the end of a frame. In the 4-bit modes the next merge then shows the finished picture,
  provided the warp finishes within a frame.

## 8. Examples

All of these are MODE 2 (320x240) on the PicoMite, and MODE 7,4 on the CMM2 under OPTION PICOMITE, unless a
different mode is shown.

**Parallax, with a still score line and 2-row bands.** Rows 16 to 239 are 224 rows, which is 112 bands with step 2.
MATH SCALE makes the whole offset table in one statement.

```basic
MODE 2
FRAMEBUFFER CREATE
FRAMEBUFFER WRITE F
' ... draw the scenery into F once ...
FRAMEBUFFER WRITE N
DIM speed!(111), off%(111)
FOR i = 0 TO 111 : speed!(i) = -(0.1 + i / 112) : NEXT
DO
  camx = camx + 2
  MATH SCALE speed!(), camx, off%()
  FRAMEBUFFER WAIT
  FRAMEBUFFER WARPH F, N, off%(), 2, 16
LOOP
```

**Water that wobbles, using phase.** Rows 120 to 239 are 30 bands of 4 rows. The table holds two periods of a sine
wave, so stepping the phase never shows a seam. Rows 0 to 119 of N are never written, so the scene above the water is
drawn into N only once.

```basic
DIM wob%(29)
FOR i = 0 TO 29 : wob%(i) = 3 * SIN(i * 2 * PI / 15) : NEXT
DO
  ph = ph + 1
  FRAMEBUFFER WAIT
  FRAMEBUFFER WARPH F, N, wob%(), 4, 120, ph
LOOP
```

**A racing road.**
- F's rows 120 to 239 hold the road in perspective with light kerbs, and F's rows 0 to 119 hold the same road with
  dark kerbs.
- Buffer 2 holds the sky and the mountains in rows 0 to 119.
- The offsets bend the road. The row map picks the kerb colour and the hill for each screen row from tables made
  before the loop, one column for each kerb phase.

```basic
DIM far!(29), sky%(29)                 ' 30 bands of 4 rows: rows 0-119
DIM bend!(119), road%(119), map%(119)  ' a row each: rows 120-239
DIM rowtab%(119, 7)                    ' 8 kerb phases, made once
' ... fill far!(), bend!() and rowtab%() ...
DO
  ' ... game logic sets camx, curve and kphase ...
  MATH SCALE far!(), -camx, sky%()
  MATH SCALE bend!(), curve, road%()
  MATH SLICE rowtab%(), , kphase, map%()
  FRAMEBUFFER WAIT
  FRAMEBUFFER WARPH 2, N, sky%(), 4
  FRAMEBUFFER WARPH F, N, road%(), 1, 120, , , , map%()
LOOP
```

**A melt from one picture to the next, with WARPV.** F holds the old picture and 2 the new one. The columns move down
in bands of 4 at random speeds. Edge −1 leaves the uncovered pixels showing the new picture, which COPY has just put
there. A positive y moves a column up (5.1), so the speeds are negative.

```basic
DIM drop%(79), v%(79)                  ' 80 bands of 4 columns
FOR i = 0 TO 79 : v%(i) = -(2 + RND * 6) : NEXT
DO
  MATH C_ADD drop%(), v%(), drop%()
  FRAMEBUFFER WAIT
  FRAMEBUFFER COPY 2, N
  FRAMEBUFFER WARPV F, N, drop%(), 4, , , -1
LOOP UNTIL MATH(MAX drop%()) <= -MM.VRES
```

**The CMM2's PAGE form, in a 16-bit mode.**

```basic
MODE 7, 16                              ' 320x240, 16 bits
PAGE WRITE 2
LOAD PNG "hills.png"
PAGE WRITE 0
DIM speed!(59), off%(59)                ' 60 bands of 4 rows
FOR i = 0 TO 59 : speed!(i) = -(0.2 + i / 60) : NEXT
DO
  camx = camx + 1
  MATH SCALE speed!(), camx, off%()
  FRAMEBUFFER WAIT
  PAGE WARPH 2 TO 0, off%(), 4
LOOP
```

## 9. Implementation

Both firmwares build the two commands from one row primitive: copy `w` pixels from a source row at `sx` to a
destination row at `dx`, skipping the transparent value, plus a fill for the edge.

- **WARPH, one destination row:**
  - with wrap: at most two pieces, the parts either side of the wrap point;
  - with an edge: one piece and up to two fills.
- **WARPV:** the destination is walked a row at a time from the top. For each band, one piece of `step` pixels is
  copied from the band's source row (y + off), so the writes go down the screen, ahead of the beam or the merge.
- No pointers to the arrays are kept after the command. No extra buffers are needed: the source and destination
  differ, so nothing has to be saved first.
- Everything is checked (section 6) before the first pixel is written.

### 9.1 PicoMite

- **Code:** a new file, graphics/Warp.c, in OPTIMIZED_SOURCES (-Os). It is called from the VGA/HDMI cmd_framebuffer
  (graphics/FrameBuffer.c:1511) and reads the buffer letters as FRAMEBUFFER COPY does.
- **Row copies by depth (as built, 23 Nov):**
  - 4 bits, WARPH: an even shift is a memcpy; an odd shift makes each byte from two. With a transparent colour each
    byte is merged whole through a nibble mask.
  - 4 bits, WARPV: a band keeps its x, so it copies whole bytes between its ends. Step 1 has a loop of its own,
    which in the usual case (wrapping, no transparency) makes each byte from two columns at once.
  - 8 and 16 bits: memcpy, or a loop that skips the transparent value.
  - The WARPV loops are made once for each depth, with the pixel access inlined: a call for each pixel cost more
    than the pixel (WARPV with step 1 took 37 ms in MODE 2 before).
- **The GUI cursor:** CursorHide, and CursorSuspend around the writes, every time, as MERGE does.
- **Memory:** no SRAM or BSS. The band tables take 4 bytes a band (8 with rows%()) of temporary memory.
- **Flash, measured 23 Nov on v8 with private builds:** Warp.c is 3,515 to 3,592 bytes. Every VGA and HDMI variant
  fits (PC3 +7.3 KB, VGA +7.8 KB, VGAUSB +1.8 KB, VGARP2350 +9.7 KB, HDMI +10.6 KB, HDMIUSB +12.3 KB, HDMIBTH
  +12.2 KB, HDMIWEB +11.2 KB) **except VGAUSBRP2350: -3,292 bytes** (it had 223 bytes left without it). Peter
  chooses between a FLASH_TARGET_OFFSET change and leaving that build out.
- **Speed, measured on the PC3 at 315 MHz** (Bas/bench/warp_time.bas, ms for the whole 320x240 screen):

| | MODE 2 (4 bits, SRAM) | MODE 5 (8 bits, PSRAM) |
|---|---|---|
| FRAMEBUFFER COPY F, N, for comparison | 0.08 | 1.95 |
| WARPH, odd offsets | 0.92 | 2.40 |
| WARPH with a transparent colour | 2.93 | 5.66 |
| WARPV step 1 | 4.67 | 9.56 |
| WARPV step 2 | 3.57 | 8.62 |
| WARPV step 4 | 2.25 | 7.04 |
| WARPV step 8 | 1.58 | 6.23 |

  In MODE 5 the framebuffer is in PSRAM, which sets the time. A frame at 640x480 75 Hz is 13.3 ms: the demo
  (Bas/warp_demo.bas) runs every scene at 75 fps with two to four warps a frame.

### 9.2 CMM2

- **The 4 and 1-bit modes:** in Rgb121Framebuffer (Src/Rgb121.c). Its Rgb121Row is the row primitive already:
  "w pixels of row s from pixel sx to row d at pixel dx, a pixel of value t left alone".
- **The 8, 12, 16 and 32-bit modes:**
  - FRAMEBUFFER WARPH and WARPV go in the native cmd_framebuffer (Src/Draw.c:997), and the PAGE form in cmd_page
    (Src/Draw.c:2745).
  - The DMA2D could move each band's pieces as rectangles with strides, so the rows of a band go in one transfer.
    Transparency still needs the processor.
- **Doubled rows:** pages of the doubled modes store each row twice (PageTable.expand). The command reads the first
  copy and writes both.
- **Long copies:** large pages (800x600 at 32 bits is 1.9 MB) call routinechecks between bands, as mycopy sees to the
  audio and USB between blocks.
- **FRAMEBUFFER WAIT in every mode (Peter):** today it is only in the 4 and 1-bit modes' FRAMEBUFFER. Native
  programs need it to time a warp, since there is no ",B". The native cmd_framebuffer gets a WAIT that calls the
  same wait (pagesetdone, as WaitFrame in Src/Rgb121.c).
- **No deferred ("D") form,** unlike PAGE COPY's: the program could change or ERASE the arrays before the frame
  ended.
- **12-bit:** to be settled when it is built: whether a pixel whose alpha is 0 counts as transparent, and how the
  edge colour's alpha is set.

## 10. Testing

One test program serves both machines:
- on the PicoMite, warp.bas with its warp.expected in the goldens corpus (the mmb2c tests, run by
  Bas/bench/goldens.py);
- on the CMM2, the same file as tests/picomite/warp.bas, run under OPTION PICOMITE.

It draws known patterns, warps them, and checks the result with PIXEL() for:
- step 1, 2, 3 and a step that does not divide the size (the cut last band);
- first, and an array shorter than the screen (the rows left alone);
- phase: 0, positive, negative and larger than n;
- offsets: 0, ±1 (the 4-bit odd shift), ± the width, and very large values;
- edge: wrap, −1 and a colour;
- transparent, alone and with an edge;
- rows%(): −1, consecutive rows within a band, and wrapping at the bottom;
- WARPV with the same cases;
- every error in section 6, using MM.ERRNO and MM.ERRMSG$.

It runs in MODE 2 and 3, in MODE 4 and 5 on HDMI, and on the CMM2 also in the 8, 16 and 32-bit modes through the
PAGE form. Both machines must print the same results.

## 11. To confirm

Everything else was settled by Peter on 23 Nov: the branches, the directions, rows%() in a band, the CMM2's F,
FRAMEBUFFER WAIT in every CMM2 mode, the argument order, the PAGE form with TO and no D, and a plain syntax error on
the LCD builds. The SPRITE SCROLL −2 error in the manuals was fixed the same day. Still open:

1. **The CMM2's 12-bit transparency and edge alpha:** to be settled when it is built.
