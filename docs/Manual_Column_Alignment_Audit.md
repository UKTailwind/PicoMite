# Column alignment in the reference tables: an audit

Written 2026-09-21 against the V6.04.00RC0 manual (294 pages).

**Finding 1, pages 113 and 114, was fixed on the day of the audit** and the
manual and its PDF have been regenerated; the description below is kept as
the record of what was wrong. Findings 2 to 11, and everything under
*Sub-line drift*, are untouched and still stand against the current manual.

## What is being checked, and why it breaks

The four reference tables lay their two columns out **by hand**:

| table | python-docx index | entries |
|---|---|---|
| Predefined Read Only Variables | 14 | 23 |
| Options | 15 | 75 |
| Commands | 16 | 338 |
| Functions | 17 | 92 |

A row of one of these tables is not one entry. It is a block of many
entries sharing two cells, and each name is kept level with the first line
of its own description by padding the name column until it matches. Two
different mechanisms do that padding, sometimes in the same table:

- **blank spacer paragraphs** in the name cell, one per line of description;
- **Word's paragraph spacing**, space-before and space-after, which opens a
  gap without a paragraph to hold it.

Nothing checks either of them. When a description gains a line, from a new
option value or a new sentence, every name below it in that row keeps its
old position and the two columns part company. The reader then reads a name
against the previous entry's text, which is worse than no text at all
because it looks authoritative.

This is the defect already fixed on page 111, where adding **Brown Out** to
`MM.INFO(BOOT)` pushed `MM.INFO(BOOT COUNT)` and the five names below it two
lines above their descriptions.

## Method

Alignment is measured on the **rendered PDF**, not in the docx, for two
reasons. It is what the reader sees, and it makes the two padding
mechanisms equivalent: a blank paragraph and 12.6 pt of space-after occupy
the same place on the page. The docx supplies only the paragraph
boundaries, so that a description's *first* line can be told from its
continuations.

For every name the audit finds the description line it is level with and
asks whether that line begins a description. A text line is 12.65 pt.

The scripts are in this session's scratchpad, not yet in `tools/`:
`align_audit5.py` (pairs entries and measures the offset in points),
`align_audit7.py` (the direct symptom test), `align_audit8.py` (finds the
running-narrative class described under finding 3 onwards) and
`showrows.py` (prints a page as two columns, which is how every finding
below was confirmed).

Coverage: 135 multi-entry rows were read and measured; 21 rows could not be
located in the PDF and were not checked. Those are listed at the end.

**The audit has a blind spot, which finding 3 fell into.** It treats a row
whose two columns hold different numbers of entries as an intended layout,
because that is usually several names sharing one description. Where the
description cell is instead a single running narrative, as in the `GUI
CURSOR` block, the audit counts one or two entries against eight or nine
names and passes over it. Rows listed under *rows whose two columns hold a
different number of entries* therefore need reading, not trusting.

## Confirmed defects

### 1. Read-only variables, pages 113 and 114: sixteen names against the wrong description  [FIXED]

**This is the significant one.** From `MM.INFO(SPI SPEED` to the foot of
page 114, every name sits two lines above its own description and is
therefore level with the previous entry's text. Sixteen entries are
affected:

| page | name | what appears beside it | what it should say |
|---|---|---|---|
| 113 | `MM.INFO(SPI SPEED` | `BBC)` | Returns the actual speed of the SYSTEM SPI |
| 113 | `MM.INFO(STACK)` | Returns the actual speed of the SYSTEM SPI | Returns the C stack pointer |
| 114 | `MM.INFO(SYSTEM HEAP)` | the error. | Returns the free space on the System Heap |
| 114 | `MM.INFO(SYSTICK)` | Returns the free space on the System Heap | Returns the current value of the systick timer |
| 114 | `MM.INFO(TILE HEIGHT)` | nothing | VGA AND HDMI VERSIONS ONLY / the tile height |
| 114 | `MM.INFO(TRACK)` | Returns the current setting of the tile height | Returns the name of the file currently playing |
| 114 | `MM.INFO$(TOUCH)` | on the audio output. | Returns the status of the Touch controller |
| 114 | `MM.INFO(USB n)` | "Disabled", "Not calibrated", and "Ready". | Return the device code for channel n |
| 114 | `MM.INFO(USB VID n)` | devices will be allocated to the highest channel | Returns the VID of the USB device |
| 114 | `MM.INFO(USB PID n)` | Returns the VID of the USB device | Returns the PID of the USB device |
| 114 | `MM.INFO(VARCNT)` | Returns the PID of the USB device | Returns the number of variables in use |
| 114 | `MM.INFO$(LINE)` | Returns the number of variables in use | Returns the current line number as a string |
| 114 | `MM.INFO(UPTIME)` | unit testing. | Returns the time in seconds since booted |
| 114 | `MM.INFO(VALID CPUSPEED speed%)` | Returns the time in seconds since booted | Returns 1 if speed% is valid |
| 114 | `MM.INFO(VERSION)` | Returns 1 if speed% is valid | The version number of the firmware |
| 114 | `MM.INFO(WRITEBUFF)` | The version number of the firmware | Returns the address of the current draw buffer |

**Cause, and it is the same as page 111's.** The shift begins immediately
below `MM.INFO$(SOUND)`, whose description lists what the function can
return. That list has gained values this development cycle, `BBC` from
`PLAY BBC SOUND` and `SAMPLE`, and now wraps to four lines where the name
column still allows two. The two lines it gained are exactly the two lines
everything below it is out by.

The damage stops at the end of that row: `MM.INFO(SCROLL)`, which begins
the next row, is level again.

**Fixed** by adding two blank paragraphs to the name cell immediately
before `MM.INFO(SPI SPEED` (the cell pads with blank spacer paragraphs,
`spacing before=0 after=0`, so two more match the neighbours). All sixteen
names are now level with their own descriptions, verified on the rendered
pages. The row did not grow: 294 pages, 42 top-level and 844 outline
entries, 543/543 command bookmarks and the same contents pages as before.

### 2. Read-only variables, page 112: MM.INFO(FREE SPACE) one line low

`MM.INFO(FREE SPACE)` sits level with `active drive.`, the second line of
its own description, rather than with `Returns the free space on the Flash
Filesystem or SD Card ...` a line above it. The entries on either side are
correct, so the name column has one line too many just above this name
rather than too few. Only this one entry is affected.

### 3. Commands, page 155: the GUI CURSOR and GUI CLICK forms

Reported by Peter, 2026-09-22. Both blocks on this page drift, and the
`GUI CURSOR` block is the worst case found so far: **eight of its nine
forms are level with another form's description.**

The two columns start together at `GUI CURSOR ON`, then the name column
falls behind, so each name ends up beside the text belonging to the form
above it:

| name | what appears beside it | how far from its own text |
|---|---|---|
| `GUI CURSOR ON [n [,x, y [,colour]]]` | GUI CONTROLS VERSIONS ONLY | level, correct |
| `GUI CURSOR x, y` | GUI CURSOR **ON** enables and displays the cursor | 91 pt, 7.2 lines |
| `GUI CURSOR OFF` | initial position (the centre of the screen ...) | 89 pt, 7.0 lines |
| `GUI CURSOR HIDE` | display controller must support reading back ... | 70 pt, 5.5 lines |
| `GUI CURSOR SHOW` | GUI CURSOR **x, y** moves the cursor to 'x', 'y' | 51 pt, 4.0 lines |
| `GUI CURSOR COLOUR colour` | GUI CURSOR **OFF** turns off the cursor | 36 pt, 2.8 lines |
| `GUI CURSOR LOAD fname$` | GUI CURSOR **COLOUR** changes the colour | 33 pt, 2.6 lines |
| `GUI CURSOR LINK MOUSE` | GUI CURSOR **LOAD** loads cursor number 2 | 93 pt, 7.3 lines |
| `GUI CURSOR UNLINK MOUSE` | layout and colour coding as a sprite ... | 90 pt, 7.1 lines |

The `GUI CLICK` block above it is mostly sound: `DOWN`, `UP` and
`PIN pin [,INV]` are level. Two are not:

- `GUI CLICK PIN OFF` sits beside `button. The pin is polled in the
  background ...`; its own sentence, *GUI CLICK PIN OFF releases the pin*,
  is 32 pt lower.
- `GUI CLICK x, y` sits beside the paragraph that opens *GUI CLICK
  generates a momentary click*. That paragraph does go on to describe the
  `x, y` form, so a reader is not misled, but the name is a line high.

**This one is different from findings 1 and 2 and needs more than padding.**
There the description column was a list of separate entries and the fix was
to add spacer lines. Here the description cell is a single running
narrative - one paragraph per form, in order, each wrapping to two or three
lines - while the name column is a list of one-line names. The two flows
were never going to stay together, and the gaps that hold the names apart
have been set by eye. Re-aligning means deciding, form by form, where each
name belongs against its sentence, and a later edit to any sentence will
undo it again. Splitting the block into one table row per form would fix it
permanently; that is a bigger change and Peter's call.

### 4. Commands, pages 162 and 163: the LIBRARY forms

Reported by Peter, 2026-09-22. The same defect as finding 3 and the most
pronounced instance found: **every one of the seven LIBRARY forms is level
with the wrong text.**

| name | what appears beside it | how far from its own sentence |
|---|---|---|
| `LIBRARY SAVE` | The library is a special segment of program memory | 135 pt, 10.7 lines |
| `LIBRARY DELETE` | Any code in the library not contained within a subroutine | 158 pt, 12.5 lines |
| `LIBRARY LIST` | for a full explanation. | 143 pt, 11.3 lines |
| `LIBRARY LIST ALL` | LIBRARY **SAVE** will take whatever is in program memory | 113 pt, 8.9 lines |
| `LIBRARY DISK SAVE fname$` | in LIST or EDIT and will not be deleted ... | 85 pt, 6.7 lines |
| `LIBRARY DISK LOAD fname$` | LIBRARY **LIST** will list the contents of the library | 42 pt, 3.3 lines |
| `LIBRARY LOAD fname$ [, fname$ ...] [, O] [, RAM]` | allowing a subsequent call to LIBRARY DISK LOAD ... | 64 pt, 5.1 lines |

`LIBRARY SAVE` at the top is a partial exception: the paragraph beside it
is the general explanation of what the library is, so it does not read as
wrong, even though the sentence describing `LIBRARY SAVE` itself is ten
lines lower.

The imbalance is extreme. All seven names fit in the top half of page 162,
while the description runs from there to two thirds of the way down page
163 - the `LIBRARY LOAD` narrative alone is some forty lines. The name
column simply has nowhere to go.

This is the same shape as finding 3 and wants the same decision: pad it by
eye, knowing the next edit to any of those paragraphs will undo it, or give
each form its own table row. The block ends cleanly - `LINE`, which starts
the next row on page 163, is level.

### 5. Commands, pages 170 and 171: the MATH block

Reported by Peter, 2026-09-22, as "minor but could be tidied" for page 170
and "bad" for page 171. Both are right: it is one block and it degrades as
it goes down.

**Page 170 starts mildly.** In the matrix section the names sit about two
lines *below* their descriptions, which is untidy but not misleading -
`MATH M_INVERSE`, `MATH M_PRINT`, `MATH M_TRANSPOSE` and `MATH M_MULT` are
each 23 to 24 pt low, so the right text is just above the name rather than
beside it. `MATH CLAMP`, `MATH SLICE` and `MATH INSERT` at the top of the
page are level.

**By the foot of page 170 it has become a whole entry.** `MATH V_PRINT` is
beside `Converts a vector inV() to unit scale`, which belongs to
`MATH V_NORMALISE`; `MATH V_NORMALISE` is beside the text for
`MATH V_MULT`.

**Page 171 is the worst of it.** Seven names carry another function's
description:

| name | what appears beside it | whose text that is |
|---|---|---|
| `MATH V_MULT matrix(), inV(), outV()` | Calculates the cross product of two three element vectors | `V_CROSS` |
| `MATH V_CROSS inV1(), inV2(), outV()` | This command rotates the coordinate pairs in 'xin()' and 'yin()' | `V_ROTATE` |
| `Quaternion arithmetic` (heading) | Invert the quaternion in inQ() | `Q_INVERT` |
| `MATH Q_INVERT inQ(), outQ()` | Converts a vector specified by x, y and z to a quaternion | `Q_VECTOR` |
| `MATH Q_VECTOR x, y, z, outVQ()` | Generates a rotation quaternion ... around axis x,y,z by theta | `Q_CREATE` |
| `MATH Q_CREATE theta, x, y, z, outRQ()` | Generates a rotation quaternion ... by yaw, pitch and roll | `Q_EULER` |
| `MATH Q_MULT`, `MATH Q_ROTATE` | their own text, 22 to 24 pt above | themselves, low |

Note the direction is the **opposite** of findings 1, 3 and 4. There the
name column lagged; here the description column is a whole entry ahead, so
`MATH V_MULT`'s own description sits at the bottom of page 170 while its
name is at the top of 171. A reader looking up `V_MULT` is told about the
cross product.

The block recovers by itself at `MATH C_ADD` on page 171, which is level
again.

### 6. Commands, pages 190 and 191: the PLAY block

Reported by Peter, 2026-09-22. The top of page 191 is a whole entry out,
in the same direction as finding 5: the description column runs one entry
ahead of the names, across the page break.

| name | what appears beside it | whose text that is |
|---|---|---|
| `PLAY MODSAMPLE samplenum, channel [,volume]` | Loads a 1024 element array comprising 4096 16-bit values | `PLAY LOAD SOUND` |
| `PLAY LOAD SOUND array%()` | Play a series of sounds simultaneously on the audio output | `PLAY SOUND` |
| `PLAY SOUND soundno, channelno, type ...` | speaker), B (both speakers) | its own, four lines in |

`PLAY MODSAMPLE`'s own description - *Plays a specific sample in the mod
file on the channel specified* - is stranded at the foot of page 190 with
no name beside it. So a reader looking up `PLAY MODSAMPLE` is told how to
load a waveform array.

The block rights itself half way down page 191: `PLAY PAUSE`,
`PLAY RESUME`, `PLAY STOP`, `PLAY VOLUME`, `PLAY NEXT` and
`PLAY PREVIOUS` are all level.

This row was in the automated sweep's list below as *partly false, matches
example lines*. That was half right and is corrected here: some of the
sweep's individual matches were the worked examples such as
`PLAY MP3 "B:/mp3/mymp3.mp3"`, but the row does carry a real defect at the
page break.

### 7. Commands, pages 193 and 194: the POKE block

Reported by Peter, 2026-09-22, and previously confirmed by the automated
sweep. Seven forms, and after the first two every one carries another
form's text. The name column lags, as in findings 1, 3 and 4.

| name | page | what appears beside it | whose text that is |
|---|---|---|---|
| `POKE BYTE addr%, byte` | 193 | Will set a byte or a word within the Pico's memory space | the block's preamble |
| `POKE SHORT addr%, short%` | 193 | bytes: 2, 4, or 8 otherwise an error will be reported | the preamble, still |
| `POKE WORD addr%, word%` | 193 | POKE **BYTE** will set the byte (i.e. 8 bits) | `POKE BYTE` |
| `POKE INTEGER addr%, int%` | 193 | 'addr%' to 'word%' ... should be integers | `POKE SHORT`, mid-sentence |
| `POKE FLOAT addr%, float!` | 193 | 'word%'. 'addr%' and 'word%' should be integers | `POKE WORD`, mid-sentence |
| `POKE VAR var, offset, byte` | 194 | POKE **INTEGER** will set the MMBasic integer | `POKE INTEGER` |
| `POKE VARTBL, offset, byte` | 194 | POKE **FLOAT** will set the word (i.e. 64 bits) | `POKE FLOAT` |

`POKE BYTE` and `POKE SHORT` at the top are the partial exception seen in
finding 4: the text beside them is the general preamble about writing to
the Pico's memory space, so they do not read as wrong even though neither
form's own sentence is anywhere near its name.

The offsets run from 3.5 lines at `POKE SHORT` to 8.8 lines at
`POKE INTEGER` and `POKE FLOAT`, whose sentences are over the page break on
194. `POKE DISPLAY`, further down 194, is a separate row and is level.

### 8. Commands, page 214: WEB TLS NOVERIFY

Reported by Peter, 2026-09-22, as minor, which it is: one entry, and the
right text is a few lines below rather than missing.

`WEB TLS NOVERIFY` sits beside `WEB NTP) as verification checks the
certificate expiry dates`, which is the middle of `WEB TLS CA`'s
description. Its own sentence - *WEB TLS NOVERIFY removes the loaded
certificates and returns to the default in which connections are encrypted
but the server is not verified* - is 48 pt lower, 3.8 lines.

`WEB TLS CA file$` above it is level, and `WEB TCP READ` below it is level,
so the drift is confined to this one name. It is the same running-narrative
shape as the larger findings, just short enough that only one name lands
badly.

This was row 372 in the sweep list below, now confirmed.

### 9. Functions, pages 227 and 228: the MATH() functions

Reported by Peter, 2026-09-22. Every name in this block floats above its
own description, consistently, for two pages.

The offsets are steady at 15 to 16 pt, a line and a quarter: `MATH(ATAN3)`
is 16.2 pt above *Returns ATAN3 of x and y*, `MATH(COSH)` 16.0 pt above
*Returns the hyperbolic cosine of a*, and so on through `LOG10`, `SINH`,
`TANH`, `CRCn` and `RAND`. On page 228 they widen to 1.7 lines -
`MATH(MIN)`, `MATH(SD)`, `MATH(SUM)`, `MATH(MAGNITUDE)` and
`MATH(DOTPRODUCT)` are each 19 to 22 pt above their text.

At the page break it tips over into a whole entry: `MATH(MIN a(),
[index%])` at the top of 228 has *Returns the median of all values in the
a() array* beside it, which belongs to `MATH(MEDIAN)`, the last name on
227.

Because the offset is a little over one line and always downward, a reader
can still pair them up - the text is directly below the name rather than
beside it - but every entry on two pages looks wrong, and the one at the
page break is genuinely misleading.

### 10. Functions, page 230: the PEEK forms

Reported by Peter, 2026-09-22. The PEEK block is level on page 229, where
`PEEK(BYTE)`, `PEEK(SHORT)`, `PEEK(WORD)`, `PEEK(INTEGER)`, `PEEK(FLOAT)`,
`PEEK(VARADDR)` and `PEEK(VARHEADER)` all read correctly. It goes out of
step at the page break and stays out for the whole of 230, by about two
lines:

| name | what appears beside it |
|---|---|
| `PEEK(VAR var, ~offset)` | memory. This address can be passed to another CFunction (`CFUNADDR`'s text) |
| `PEEK(VARTBL, ~offset)` | as var(). (the tail of `VAR`'s text) |
| `PEEK(PROGMEM, ~offset)` | VARTBL. (the tail of `VARTBL`'s text) |
| `PEEK(BP n%)` | nothing |
| `PEEK(SP n%)` | the next byte. (the tail of `BP`'s text) |
| `PEEK(WP n%)` | the next short. (the tail of `SP`'s text) |

Each name is 19 to 28 pt above its own sentence. `PI`, which follows, is
level again.

### 11. Functions, pages 234 and 235: the TOUCH gestures

Reported by Peter, 2026-09-22. The worst mismatch in the functions table:
**the name column runs a whole group ahead of the description column.**

On page 234 the single-finger gesture names - `TOUCH(TAP)`, `TOUCH(DTAP)`,
`TOUCH(HOLD)`, `TOUCH(SWIPE)`, `TOUCH(SWL)`, `TOUCH(SWR)`, `TOUCH(SWU)`,
`TOUCH(SWD)` - are listed against the block's general preamble about how
gestures are latched and read.

On page 235 the multi-finger names are then listed against the
single-finger descriptions:

| name | what appears beside it |
|---|---|
| `TOUCH(PINCH)` | TOUCH(**TAP**) - a quick tap |
| `TOUCH(EXPAND)` | without moving). |
| `TOUCH(CONTRACT)` | TOUCH(**DTAP**) - a double tap |
| `TOUCH(ROTATE)` | second). |
| `TOUCH(CW)` | TOUCH(**HOLD**) - a long press |
| `TOUCH(CCW)` | This is recognised while the touch is still down. |
| `TOUCH(TTAP)` | TOUCH(**SWL**), TOUCH(SWR) ... - a swipe |

So a reader looking up `TOUCH(PINCH)` is told about a quick tap, and
`TOUCH(CW)` is told about a long press. Unlike findings 9 and 10 the right
text is nowhere near the name, so this one actively misinforms.

## The automated sweep for this class

Findings 3, 4 and 5 share a shape the first sweep could not see: a
description cell written as a running narrative that names each form as it
describes it. That naming gives an exact test, with no pairing guesswork -
find the description paragraph that opens with a form's name and check
whether the name is level with it. `align_audit8.py` does that.

It reports **23 blocks**. Three are confirmed above; `POKE` on page 193 was
also checked and is genuine, six forms between 3.5 and 8.8 lines from their
own sentences. The rest are listed here unverified, worst first, for
working through:

| page | block | worst | forms |
|---|---|---|---|
| 201 | `SELECT CASE` | 22.3 ln | 3 - probably false, matches the syntax example |
| 189 to 193 | `PLAY` | 19.7 ln | 5 - **confirmed, see finding 6**; some matches are example lines |
| 210 | row 349 | 18.7 ln | 1 |
| 205 | row 326 | 16.9 ln | 1 |
| 161 | row 157 | 13.4 ln | 1 |
| 146 | `EDIT` | 12.0 ln | 1 |
| 200 | row 303 | 11.6 ln | 1 |
| 121 | options row 38 | 9.2 ln | 2 |
| 193 | `POKE` | 8.8 ln | 6 - **confirmed, see finding 7** |
| 182 | rows 240 and 243 | 6.6 ln | 5 |
| 206 | row 330 | 6.5 ln | 1 |
| 210 | row 348 | 5.2 ln | 2 |
| 160 | row 153 | 5.2 ln | 1 |
| 214 | `WEB TLS NOVERIFY` | 3.8 ln | 1 - **confirmed, see finding 8** |
| 167 | `MANDELBROT` | 3.8 ln | 3 |
| 215 | row 381 | 3.0 ln | 2 |
| 110 | `MM.ERRNO` group | 2.8 ln | 1 |
| 125 | `OPTION RESET` | 1.7 ln | 2 |
| 209 | row 342 | 1.7 ln | 1 |

**The false positives in that list are easy to spot and worth naming.** The
test matches any description paragraph that opens with the form's name,
and a worked example does exactly that: the `DIM` row on page 144 scores
57 lines because the description contains the line
`DIM INTEGER nbr(4) = (22, 44, 55, 66, 88)`. Before acting on a row,
confirm the matched paragraph is a sentence describing the form and not a
code example.

## Sub-line drift: the space above and below paragraphs

Separately from the defects above, the two columns very often run at
slightly different vertical rhythms, because the description cell opens
15.6 pt before a new paragraph where the name cell uses a plain 12.65 pt
line, or the reverse. The offsets are fractions of a line and they
accumulate down a row until something re-anchors them.

Measured examples, none of which misinform the reader but all of which
show the layout drifting:

| page | name | offset |
|---|---|---|
| 194 | `POKE DISPLAY command` | 11.4 pt, 0.90 line |
| 123 | `OPTION LOGGING ON` | 9.9 pt, 0.78 line |
| 111 | `MM.HPOS` | 9.6 pt, 0.76 line |
| 130 | `/*` | 9.6 pt, 0.76 line |
| 151 | `FRAMEBUFFER SYNC` | 7.6 pt, 0.60 line |
| 112, 114, 115 | many, e.g. `MM.INFO(FCOLOUR)`, `MM.INFO(TRACK)` | 1 to 3 pt |

**Pages 186 and 187, the PIO block, are the clearest worked example** and
were reported by Peter as "very minor", which is the right reading. The
names sit below their descriptions:

| name | offset |
|---|---|
| `PIO DMA RX pio, sm, nbr, data%() ...` | 8.5 pt, 0.67 line |
| `PIO DMA RX OFF` | 6.0 pt, 0.47 line |
| `PIO INTERRUPT pio, sm ...` | 6.2 pt, 0.49 line |
| `PIO INIT MACHINE pio%, ...` (187) | 21.8 pt, 1.72 lines |

Everything else on page 187 is level or within 3 pt: `PIO EXECUTE`,
`PIO WRITE`, `PIO WRITEFIFO`, `PIO READ`, `PIO START`, `PIO STOP`,
`PIO CLEAR` and both `PIO PROGRAM` forms all read correctly.
`PIO INIT MACHINE` is the one case here that exceeds a line - its name
block begins about two lines into its own five-line description - but the
text above the name is still its own, so nothing is misread.

The mechanism is visible in the line spacing. Down the description column
the gaps are 15.6 to 15.8 pt, because each parameter is its own paragraph
and carries space before it. Down the name column, where the wrapped name
is one paragraph, they are 12.6 pt. The two columns start 8.5 pt apart at
`PIO DMA RX` and have converged to 0.6 pt four lines later, then open up
again at the next entry. Nothing is misread; the names just sit low.

Correcting this one would mean changing paragraph spacing in a cell whose
description runs for a page and a half, which moves everything below it.
It is not worth the risk for half a line.

A one-off offset of 1 to 3 pt reads as level and is not worth chasing. The
9 to 12 pt cases are visible: the name looks like it belongs to the line
above or below. More importantly this drift is **the mechanism that makes
the whole layout fragile**, because it means a row's two columns cannot be
kept in step by counting paragraphs; only the rendered page tells the
truth. That is why the audit measures the PDF.

## Not defects, though the detector flags them

Two layouts are intentional and account for most of the raw hits:

- **Several names sharing one description**, such as `MM.INFO(FONTHEIGHT)`
  and `MM.INFO(FONTWIDTH)` on page 112, or the `MM.INFO(HEAPEND)` group on
  page 113. Only the first name is level with the description.
- **Long name lists with one description block**, such as the `MATH`
  functions on pages 226 to 228 and `MODE` on pages 178 to 181, where the
  name column deliberately runs on past the end of the description.

## Rows the audit could not read

Twenty-one rows could not be anchored in the PDF, usually because the first
name contains characters that the PDF extraction renders differently, and a
handful more were skipped where the text match fell below 85 per cent.
These have **not** been checked and may hide further instances. Re-running
`align_audit5.py` after any fix will list them by table and row.

## Suggested order of work

1. ~~Pages 113 and 114, sixteen entries, the only place a reader is
   actively misled.~~ Done.
2. Pages 162 and 163, the `LIBRARY` block, all seven forms against the
   wrong text and by the widest margins in the manual.
3. Pages 170 and 171, the `MATH` block, seven names carrying another
   function's description.
4. Pages 193 and 194, the `POKE` block, five of seven forms.
5. Pages 190 and 191, the `PLAY` block, three names at the page break.
6. Page 155, the `GUI CURSOR` block, eight entries reading against the
   wrong form, plus `GUI CLICK PIN OFF`.
7. Pages 234 and 235, the `TOUCH` gestures, seven names a whole group out.
8. Pages 227 and 228, the `MATH()` functions, every name just over a line
   high across two pages.
9. Page 230, the `PEEK` forms, six names about two lines high.
10. Page 214, `WEB TLS NOVERIFY`, one name, minor.
11. Decide 2 to 10 together: all are a running narrative against a list of
   names, and all want either padding by eye or one table row per form.
12. Page 112, `MM.INFO(FREE SPACE)`, one entry.
13. Work through the unverified rows in *The automated sweep for this
   class*, discarding the ones that matched a code example.
14. Leave the sub-line drift alone unless a particular entry looks wrong on
   the page; correcting it means touching paragraph spacing, which moves
   everything below it and risks introducing the very defect being fixed.
15. Consider putting the audit in `tools/` and running it before a release,
   the way `release_preflight.py` runs. It takes about three minutes over
   the whole manual and needs only the docx and the PDF.


## What was done, and what could not be

The name columns were re-padded on 2026-09-22 so that each name sits level
with the first line of its own description. Of the 60 name-and-description
pairs targeted, **53 now sit within 8 pt of level, which reads as aligned,
and 57 are within one line.** The worst residual is 1.5 lines. Nothing else
in the document moved: 294 pages, 42 top-level and 844 outline entries,
543/543 command bookmarks and the same contents pages as before.

Fixed: `MM.INFO(FREE SPACE)` (112), the `LIBRARY` forms (162, 163), the
`PLAY` forms at the page break (190, 191), the `POKE` forms (193, 194),
`WEB TLS NOVERIFY` (214), the `MATH()` functions (227, 228), the `PEEK`
forms (230) and the `TOUCH` gestures (234, 235).

**The `TOUCH` names were also reordered** to follow the narrative: `SWIPE`
now comes after the four swipe directions, `PINCH` after `EXPAND` and
`CONTRACT`, and `ROTATE` after `CW` and `CCW`. Without that the names could
not be aligned at all, because the list and the text disagreed on order.

**Two blocks resisted and were put back as they were.**

- **Page 155, `GUI CURSOR`** (finding 3). Re-padding aligned the names but
  cost three entries in the generated help files, so it was put back and
  then split into rows instead, below.
- **Pages 170 and 171, `MATH`** (finding 5) resisted padding for the reason
  given under *One row per command*, below, and was split too.

## One row per command: the permanent fix, used for MATH

Padding could not hold the `MATH` block. That row runs over three pages and
the two columns break at different points, because the description column
carries paragraph spacing and the name column does not; padding the name
column moves the break, which moves the names again. Three passes converged
with the names still 2.8 lines high on page 171, and a fourth made it worse.

So on 2026-09-22, at Peter's suggestion, **the block was split into one
table row per command** - thirty-one rows - and the rule between them was
turned off: every internal row carries `top` and `bottom` borders of `nil`,
and zero top and bottom cell margins, so the section reads as one
continuous block exactly as before. Only the first row keeps its top rule
and the last its bottom rule, joining it to the rows either side.

Word now does the alignment, and it cannot drift again however the text is
edited. Every one of the thirty-one entries is level with its own
description, within 3 pt. Three things came free with it:

- the page count did not change, because the row heights still add up to
  the same total;
- each `MATH` command now has **its own PDF bookmark** - the outline went
  from 844 entries to 859 - where previously the whole block had one;
- the help files are unaffected, because each row now holds exactly one
  name group and one description group.

**`GUI CURSOR` on page 155 was then split the same way**, into six rows:
`ON` (carrying the version banner and the preamble), `x, y`, the
`OFF`/`HIDE`/`SHOW` trio that share one sentence, `COLOUR`, `LOAD`, and the
`LINK MOUSE`/`UNLINK MOUSE` pair. Every form is now level with its own
text, the page count is unchanged and five more PDF bookmarks appear.

**The one thing to get right when splitting is the help files.**
`gen_help.py` compares the number of blank-separated groups in the two
cells: equal counts pair one to one, unequal counts make a primary topic
under the common prefix plus a stub for every other name. That is where
`GUI CURSOR HIDE` and `GUI CURSOR SHOW` come from. So **keep the paragraph
grouping inside each new row exactly as it was** - the three names that
share a sentence stay in one row, still separated by their blank
paragraphs - and the topic list comes through untouched. It did here: 1163
topics, the eleven gained earlier still present, none lost.

**Every hand-aligned block named in this audit has since been split the
same way.** On 2026-09-22, after `MATH` and `GUI CURSOR` proved the
technique, the remaining six went too:

| block | pages | rows |
|---|---|---|
| `GUI CLICK` | 155 | 5 |
| `LIBRARY` | 162, 163 | 6 |
| `PLAY` | 189 to 193 | 5 |
| `POKE` | 193, 194 | 10 |
| `PEEK` | 229, 230 | 15 |
| `TOUCH` gestures | 234, 235 | 12 |

Nothing about the printed page changed except the alignment: still 294
pages, the same contents pages, and no rule anywhere inside a block. The
PDF outline grew from 844 entries to 882, because each form is now its own
cell and earns its own bookmark.

**The help files gained 24 topics and lost none.** Commands that had never
had an entry now do, among them `LIBRARY DELETE`, `LIBRARY LIST`,
`LIBRARY LOAD`, `POKE SHORT`, `POKE WORD`, `POKE INTEGER`, `POKE FLOAT`,
`POKE VAR`, `PEEK(SHORT)`, `PEEK(WORD)`, `PEEK(INTEGER)`, `PEEK(FLOAT)`,
`PEEK(VARADDR)`, `PEEK(VARTBL)` and nine of the `TOUCH` gestures. That is
the real measure of how badly the old layout was confusing the generator as
well as the reader.

Only one hand-aligned block is left, the `MATH()` functions on pages 227
and 228. It was corrected by padding and currently reads within half a
line, so it was left alone; it is the last candidate if it ever drifts.

**A coupling worth knowing about.** `tools/gen_help.py` pairs the two
columns by splitting each cell into blank-line separated groups and zipping
them, so the blank spacer paragraphs that hold the visual alignment are
also what decide which name gets a help topic. Re-padding therefore changes
the help files. Here it was a net gain: **eleven commands that had no help
topic now have one** - `LIBRARY DELETE`, `PEEK(FLOAT)`, `PEEK(VARTBL)`,
`PEEK(WORD)`, `POKE INTEGER`, `POKE SHORT`, `TOUCH(DTAP)`, `TOUCH(EXPAND)`,
`TOUCH(HOLD)`, `TOUCH(SWL)` and `TOUCH(TTAP)` - and none were lost, after
`GUI CURSOR` was put back. Check the topic list after any future re-padding.

**The case for one row per form is now stronger.** Both blocks that
resisted, and the residual line or so elsewhere, come from the same thing:
a compact list of names cannot be made to track a running narrative when
the two columns flow independently and break across pages separately.
Giving each form its own table row would make the alignment automatic,
permanent, and immune to the next edit.

## Why this keeps happening

Every one of these defects was introduced by a correct edit to a
description: Brown Out added to `MM.INFO(BOOT)`, BBC and SAMPLE added to
`MM.INFO$(SOUND)`. The editor has no way to see that a description has
grown past the space the name column reserved for it, because the two
columns are independent paragraph flows that only meet on the printed
page. Any entry added to a list inside one of these tables should be
followed by a look at the rendered page, or by a run of this audit.
