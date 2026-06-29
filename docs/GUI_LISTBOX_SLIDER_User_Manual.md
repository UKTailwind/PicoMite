# GUI LISTBOX and SLIDER Controls User Manual

## Overview

`LISTBOX` and `SLIDER` are two advanced GUI controls for the PicoMite family,
adding to the existing set (`BUTTON`, `SWITCH`, `RADIO`, `CHECKBOX`, `LED`,
`SPINBOX`, `FRAME`, `NUMBERBOX`, `TEXTBOX`, `FORMATBOX`, `DISPLAYBOX`,
`CAPTION`, `AREA`, `GAUGE`, `BARGAUGE`). They fill two gaps in that set:

- **`LISTBOX`** — selecting one item from a list that can be long and/or change
  at run time. A collapsed control shows the current selection; tapping it opens
  a modal pop-up that scrolls when there are more items than fit on screen.
- **`SLIDER`** — entering a continuous analog value by dragging a thumb along a
  track, the natural touch-screen counterpart of the read-only `GAUGE` and
  `BARGAUGE`.

Both are created with the `GUI` command and read or written with the `CtrlVal`
command and function, exactly like every other control. They require touch (a
resistive or capacitive panel, or a USB/Bluetooth mouse) and a display capable
of GUI controls, and `OPTION CONTROLS nn` must be set so the firmware reserves
storage for the controls.

> **Prerequisite:** set the maximum number of controls once (it is stored in
> flash and survives a reboot), e.g. `OPTION CONTROLS 20`.

---

## GUI LISTBOX

```
GUI LISTBOX #ref, array$(), x, y, w, h [, fc] [, bc] [, maxrows]
```

Creates a list-selection control. The control itself is drawn as a single
collapsed box (similar in appearance to a `DISPLAYBOX`) showing the currently
selected item, with a small down-pointing marker on the right. Touching the box
opens a pop-up list; touching an item selects it and closes the pop-up.

| Parameter   | Description |
|-------------|-------------|
| `#ref`      | Control reference number (1 … `OPTION CONTROLS` − 1) |
| `array$()`  | A one-dimensional string array holding the list items (see **Array binding** below) |
| `x, y`      | Top-left corner of the collapsed control |
| `w, h`      | Width and height of the collapsed control |
| `fc`        | Foreground (text and border) colour (default: last colour used) |
| `bc`        | Background colour (default: last colour used) |
| `maxrows`   | Optional. Maximum number of list rows the pop-up shows before it scrolls. Default: as many as fit in the available screen space. |

**Example:**
```basic
DIM fruit$(4) LENGTH 16
fruit$(0) = "Apple"  : fruit$(1) = "Banana" : fruit$(2) = "Cherry"
fruit$(3) = "Date"   : fruit$(4) = "Elderberry"

GUI LISTBOX #1, fruit$(), 40, 30, 200, 28, RGB(WHITE), RGB(BLUE)
```

### Array binding

The list contents are bound **live** to the BASIC string array: the control
reads the array's current contents each time it is drawn, so updating the array
elements and redrawing (for example with `CtrlVal`) updates what the list shows.
No copy is made.

Because the binding is live, **the array must remain in scope for the entire
life of the control**. Declare it at the program (module) level, or as a
`STATIC` array inside a subroutine — never as an ordinary local array that is
destroyed when a subroutine returns. If the bound array goes out of scope while
the control still exists, the behaviour is undefined.

The array may be of any length. The number of items is taken from the array's
dimensions (respecting `OPTION BASE`). Item text longer than the control or
pop-up width is clipped to fit.

### Selecting items and reading the result

The control stores the **0-based index** of the selected item, which you read
with the `CtrlVal` function:

```basic
i = CtrlVal(#1)              ' index of the selected item (-1 if the list is empty)
item$ = fruit$(CtrlVal(#1))  ' the selected text (your program owns the array)
```

> The listbox reports the selection **index**, not the text. To obtain the text,
> index your own array as shown above. This keeps the array under your control.

When the control is created, the selection defaults to item 0 (or −1 if the
array is empty). Set the selection from your program with the `CtrlVal` command;
the value is clamped to the valid range:

```basic
CtrlVal(#1) = 2              ' select "Cherry" and redraw the collapsed control
```

### The pop-up list and scrolling

Touching the collapsed control opens a modal pop-up immediately below it (or
above it, if there is not enough room below). While the pop-up is open the other
controls are greyed and do not respond to touch.

- Touch an item to select it. The pop-up closes, the collapsed control updates,
  and the touch "up" GUI interrupt fires (see **Interrupts**).
- Touch anywhere outside the pop-up to dismiss it without changing the
  selection.

When the list has more items than the pop-up can show (limited by `maxrows`
and/or the screen height), an **up-arrow** and a **down-arrow** appear as the
first and last rows of the pop-up:

- Tap an arrow to scroll one row; hold it for auto-repeat (continuous scroll).
- An arrow is greyed when the list is already at that end.
- When the pop-up opens, it scrolls automatically so that the current selection
  is visible.

The pop-up always fits on screen; on small displays it may show only a few rows
at a time but remains fully scrollable.

### GUI LISTBOX CANCEL

```
GUI LISTBOX CANCEL
```

Closes an open listbox pop-up from your program without changing the selection.
This is the listbox equivalent of `GUI NUMBERBOX CANCEL` / `GUI TEXTBOX CANCEL`.
It has no effect if no listbox pop-up is open.

### Complete example

```basic
' Pick an item from a scrollable list
Option Explicit
Dim integer i, lastsel = -2

' Module-level array: stays in scope for the life of the control.
Dim items$(11) Length 20
For i = 0 To 11 : items$(i) = "Item " + Str$(i + 1) : Next i

CLS
GUI LISTBOX    #1, items$(), 40, 30, 200, 26, RGB(white), RGB(blue), 5
GUI DISPLAYBOX #2, 40, 70, 200, 26, RGB(green), RGB(black)
CtrlVal(#2) = items$(CtrlVal(#1))

Do
  If CtrlVal(#1) <> lastsel Then
    lastsel = CtrlVal(#1)
    If lastsel >= 0 Then CtrlVal(#2) = items$(lastsel)
  EndIf
Loop
```

---

## GUI SLIDER

```
GUI SLIDER #ref, x, y, w, h [, fc] [, bc] [, min] [, max] [, inc]
```

Creates a slider: a draggable thumb on a track for entering a continuous value
by touch.

| Parameter | Description |
|-----------|-------------|
| `#ref`    | Control reference number (1 … `OPTION CONTROLS` − 1) |
| `x, y`    | Top-left corner of the control |
| `w, h`    | Width and height of the control |
| `fc`      | Foreground colour (track fill and thumb) (default: last colour used) |
| `bc`      | Background colour (default: last colour used) |
| `min`     | Optional. Value at the start of the track. Default: 0 |
| `max`     | Optional. Value at the end of the track. Default: 100 |
| `inc`     | Optional. Step size to snap to. Default: 0 (continuous, no snapping) |

### Orientation

The orientation is taken automatically from the control's shape:

- **Wider than tall** → **horizontal**: `min` at the left, `max` at the right.
- **Taller than wide** → **vertical**: `min` at the bottom, `max` at the top.

```basic
GUI SLIDER #1, 40,  40, 220, 30, RGB(CYAN),  RGB(BLACK), 0, 100      ' horizontal
GUI SLIDER #2, 280, 40,  30, 180, RGB(GREEN), RGB(BLACK), 0, 255      ' vertical
```

### Setting and reading the value

Drag the thumb, or simply tap anywhere on the track, to set the value — the
thumb jumps to the touch position and then follows the finger while it is held.
The value tracks continuously during the drag.

Read or set the value with `CtrlVal`, exactly like a `SPINBOX` or `GAUGE`:

```basic
v = CtrlVal(#1)              ' current value (between min and max)
CtrlVal(#1) = 50             ' move the thumb to 50 and redraw
```

Assigned values are clamped to the `min … max` range.

### Snapping with inc

If `inc` is greater than 0, the value snaps to the nearest multiple of `inc`
measured from `min`. For example, `min 0, max 10, inc 1` yields only whole
numbers 0–10; `inc 0` (the default) gives a smooth continuous value.

```basic
GUI SLIDER #3, 40, 110, 220, 30, RGB(YELLOW), RGB(BLACK), 0, 10, 1   ' whole steps 0..10
```

### Complete example

```basic
' Three sliders feeding a display box
Option Explicit
Dim string shown = "", s

CLS
GUI SLIDER     #1, 40,  40, 220, 30, RGB(cyan),   RGB(black), 0, 100
GUI SLIDER     #2, 40, 110, 220, 30, RGB(yellow), RGB(black), 0, 10, 1
GUI SLIDER     #3, 280, 40,  30, 180, RGB(green),  RGB(black), 0, 255
GUI DISPLAYBOX #4, 40, 170, 220, 26, RGB(white),  RGB(black)

Do
  s = "A=" + Str$(CtrlVal(#1),0,0) + " B=" + Str$(CtrlVal(#2),0,0) + " C=" + Str$(CtrlVal(#3),0,0)
  If s <> shown Then shown = s : CtrlVal(#4) = s
Loop
```

---

## Interrupts

Both controls work with the standard GUI touch interrupt set up with:

```
GUI INTERRUPT downsub [, upsub]
```

- **Slider:** the *down* interrupt fires repeatedly while the thumb is being
  dragged, giving live updates; the *up* interrupt fires when the finger is
  lifted.
- **Listbox:** the *up* interrupt fires when an item is selected (the pop-up
  closes). `Touch(LASTREF)` (or testing `CtrlVal` of your controls) identifies
  which control changed.

If no `GUI INTERRUPT` is set, simply poll `CtrlVal(#ref)` in your main loop, as
the examples above do.

---

## Reading and writing controls

| Operation | Listbox | Slider |
|-----------|---------|--------|
| `CtrlVal(#ref)` (function) | Selected item index (0-based, −1 if empty) | Current value (`min … max`) |
| `CtrlVal(#ref) = n` (command) | Set selection (clamped to valid range) | Set value (clamped to `min … max`) |

Both controls also respond to the usual control-management commands:
`GUI DISABLE`, `GUI ENABLE`, `GUI HIDE`, `GUI SHOW`, `GUI DELETE`,
`GUI FCOLOUR`, `GUI BCOLOUR`, and page selection via `GUI PAGE` / `PAGE`.

---

## Notes and limitations

- `OPTION CONTROLS nn` must be set before any GUI control is created.
- A listbox holds a **live pointer** into its bound array; keep the array in
  scope (module-level or `STATIC`) for the life of the control.
- A listbox reports the selection **index**; map it to text via your own array.
- Slider orientation is fixed by the width/height you give at creation; there is
  no separate orientation keyword.
- The slider's *down* interrupt firing continuously during a drag is by design
  (it mirrors the `SPINBOX` auto-repeat). If you only need the final value, act
  on the *up* interrupt or poll after the drag.
- Both controls are sub-keywords of the `GUI` command and add no reserved words
  to MMBasic.

---

## Error messages

| Error | Cause |
|-------|-------|
| `Maximum number of controls not set` | A GUI control command was used before `OPTION CONTROLS nn` was set |
| `GUI reference number #n is in use` | The `#ref` already belongs to another control |
| `Argument n must be a string array` | The second argument to `GUI LISTBOX` is not a 1D string array |
| `Syntax error` | Missing or malformed parameters |
