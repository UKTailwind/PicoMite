# `gui-demo-320x240.bas` — Multi-firmware GUI demonstration

## Overview

`gui-demo-320x240.bas` is a single MMBasic program that builds a fake
"Pump Control" panel using every standard GUI widget and then accepts
operator input from whichever device the running firmware happens to
expose:

| Input device           | Mechanism                                                |
| ---------------------- | -------------------------------------------------------- |
| Touch screen           | `GUI INTERRUPT TouchDown, TouchUp`                       |
| Mouse (USB HID or PS/2)| `GUI CURSOR LINK MOUSE`                                  |
| Keyboard               | `INKEY$` loop driving `GUI CURSOR` and `GUI CLICK`       |

The same `TouchDown` / `TouchUp` subs handle every source, so the
application logic is written only once.

The program is laid out for a 320×240 canvas — small enough to run on
the cheapest ILI9341/ILI9488 LCDs and on `MODE 2` of the VGA and HDMI
builds.

## Prerequisite

`GUI` controls are an opt-in feature of MMBasic. Before running the
demo (and only once, from the command prompt — not inside the
program) issue:

```
OPTION GUI CONTROLS 50
```

The number is the upper bound on simultaneous controls. The demo
uses about 25 control reference numbers, so anything ≥ 25 is fine;
fifty was chosen as a safe default for the underlying widget tables.

## Supported firmware

The demo runs unmodified on every build that defines `GUICONTROLS`.
On builds that don't, `GUI` commands raise a syntax error and the
program will not start — they are listed for completeness.

| Build                                | GUI controls | Touch | Mouse                       | Keyboard      |
| ------------------------------------ | :----------: | :---: | --------------------------- | ------------- |
| PicoMite **PICO** (RP2040 LCD)       |     yes      |  yes  | PS/2                        | PS/2          |
| PicoMite **PICOUSB** (RP2040 LCD)    |     yes      |  yes  | USB HID                     | USB HID       |
| PicoMite **PICORP2350**              |     yes      |  yes  | PS/2                        | PS/2          |
| PicoMite **PICOUSBRP2350**           |     yes      |  yes  | USB HID                     | USB HID       |
| PicoMiteVGA **VGARP2350**            |     yes      |   —   | PS/2                        | PS/2          |
| PicoMiteVGA **VGAUSBRP2350**         |     yes      |   —   | USB HID                     | USB HID       |
| PicoMiteHDMI **HDMI**                |     yes      |   —   | PS/2                        | PS/2          |
| PicoMiteHDMI **HDMIUSB**             |     yes      |   —   | USB HID                     | USB HID       |
| WebMite **WEBRP2350** (with LCD)     |     yes      |  yes  | PS/2                        | PS/2          |
| PicoMiteBT **PICOBTRP2350**          |     yes      |  yes  | PS/2                        | PS/2 + BT HID |
| PicoMiteBT **PICOBTHRP2350**         |     yes      |  yes  | USB HID                     | USB HID + BT  |
| WebMite **WEB** (RP2040)             |    **no**    |   —   | —                           | —             |
| PicoMiteVGA **VGA** / **VGAUSB** (RP2040) | **no**  |   —   | —                           | —             |
| PicoMite **PICOMIN**                 |    **no**    |   —   | —                           | —             |

A few notes that aren't obvious from the table:

* "PS/2" rows are non-USB-host builds and pick up a mouse via
  [mouse.c](../mouse.c) on the configured `MOUSE_CLOCK` /
  `MOUSE_DATA` pins. "USB HID" rows are USB-host builds and pick up a
  mouse through [USBKeyboard.c](../USBKeyboard.c) (`HID[1].Device_type == 2`).
* RP2040 VGA / WebMite firmware compiles the mouse driver but does
  **not** define `GUICONTROLS` — the `Ctrl[]` heap reservation is too
  expensive for the available RAM. See [CMakeLists.txt:534-538](../CMakeLists.txt#L534-L538).
* The WEBRP2350 build *does* enable `GUICONTROLS` and *does* drive an
  attached LCD ([CMakeLists.txt:577](../CMakeLists.txt#L577)); the
  plain RP2040 WEB build does not.

## How "one program runs everywhere" actually works

The program uses four small techniques to absorb firmware
differences. Each is explained below.

### 1. Force a known display mode on VGA / HDMI

LCD builds boot with `MM.HRES = 320`, `MM.VRES = 240` already, so all
the hard-coded pixel coordinates land in the right place. VGA and
HDMI builds boot at a higher resolution, which would push the layout
off the visible area.

```basic
If InStr(MM.DEVICE$, "VGA") Or InStr(MM.DEVICE$, "HDMI") Then
  MODE 2          ' selects 320x240 on both VGA and HDMI builds
EndIf
```

`MM.DEVICE$` is the only reliable cross-firmware identifier; the
demo uses it as the single source of truth for "do I need to switch
resolution?".

### 2. Turn the soft cursor on, then sync it to whatever the user is using

`GUI CURSOR ON` enables a *soft* cursor — a sprite that the firmware
overlays on the screen by saving and restoring the pixels beneath it.
Mouse and keyboard input naturally move that cursor; the touch path
does not, so the demo updates it explicitly inside `TouchDown`:

```basic
Sub TouchDown
  Local Integer tx, ty
  tx = Touch(x) : ty = Touch(y)
  If Not (tx = -1 Or ty = -1) Then
    GUI Cursor tx, ty        ' make the cursor follow the finger
    mx = tx : my = ty        ' keep the keyboard's idea in step
  EndIf
  ...
```

The result is that, no matter which input device the user picks up
next — finger, mouse, or arrow keys — the cursor is already at the
last interaction point.

### 3. Try to link the cursor to a real mouse

```basic
On Error Skip
GUI Cursor Link Mouse
```

`GUI CURSOR LINK MOUSE` exists on every build that defines
`GUICONTROLS`, regardless of mouse type — it is the same command for
USB HID and PS/2. What it raises at run time is **"No mouse
connected"** if `cursor_have_mouse()` cannot find a live mouse
([Draw.c:1046-1059](../Draw.c#L1046-L1059)). That is the case the
`On Error Skip` is there for: the demo wants to keep running and use
touch / keyboard, not abort because the user didn't plug in a mouse.

The probe also gracefully handles any historical firmware that
predates the `LINK MOUSE` subcommand and reports it as a syntax
error.

Once linked, the soft cursor follows the physical mouse and the
mouse's left button is dispatched through the same touch machinery
described next.

### 4. Unify all three input paths through the touch interrupt

```basic
GUI Interrupt TouchDown, TouchUp
```

`TouchDown` and `TouchUp` are fired for:

* A real touch from the LCD panel.
* A real click from a linked mouse (USB HID or PS/2 — `GUI CURSOR
  LINK MOUSE` re-routes mouse-button events into the touch
  dispatcher).
* A synthetic `GUI CLICK` issued from the keyboard fallback loop —
  the engine dispatches `GUI CLICK` through the *same* code path as a
  touch, so the interrupts fire there too.

That last point is what removes all per-input-source branching from
the program. There is no "if touch then... else if mouse then..." —
every input shows up at `Touch(REF)` in `TouchDown` as the control
the cursor was over when the click happened.

### 5. Keyboard fallback loop

For builds where neither touch nor mouse is available at the moment
(a bare HDMI build with no mouse plugged in, for example), the main
`Do … Loop` provides a pointer the user can drive from any PS/2 or
USB keyboard:

| Key          | Action                                                   |
| ------------ | -------------------------------------------------------- |
| ← → ↑ ↓      | Move the soft cursor by `stepx` pixels (4 px)            |
| Space        | `GUI Click mx, my` — momentary press-release             |
| `D` / `d`    | `GUI Click Down` — start a held press                    |
| `U` / `u`    | `GUI Click Up` — release the held press                  |
| `Esc`/`Q`/`q`| Exit the program cleanly                                 |

After each move the cursor coordinates are clamped to the visible
canvas so the cursor cannot run off-screen and lose its background.

The held-press support exists so this same demo can be reused as a
drag-test harness: press `D`, walk the cursor across the screen with
the arrow keys, then `U`. MMBasic binds the click to whichever
control was hit on `Down`, so dragging across other controls does
not retrigger them — matching real touch and real mouse behaviour.

On the way out, the loop calls `GUI Click Up` if a key chord was
left half-finished, then `GUI Cursor Off`, so the system is left in
a clean state for the next program.

## The control set

The "Pump Control" panel uses one of every widget supported by GUI
CONTROLS:

| Widget       | Purpose in this demo                                  |
| ------------ | ----------------------------------------------------- |
| `Caption`    | Static labels (heading, "Pump", "Flow Rate", "Hi:" …) |
| `Switch`     | Pump ON / OFF                                         |
| `Displaybox` | Read-only flow-rate value                             |
| `Frame`      | Visual grouping (Power, Alarm, Log File)              |
| `Radio`      | Power mode: Economy / Normal / High                   |
| `Numberbox`  | Editable Hi and Lo alarm thresholds                   |
| `Button`     | Momentary "TEST" alarm                                |
| `LED`        | Status indicators ("Running", "Alarm")                |
| `Checkbox`   | Logging enable + per-event filters                    |
| `Textbox`    | Log file name                                         |
| `Spinbox`    | Back-light percentage                                 |

Every control declares a reference number (a small integer) via
`Const`. Those constants are passed to `GUI` commands when the
control is built, and matched against `Touch(REF)` in the interrupt
handler.

## What each control actually does

The application logic lives entirely inside `TouchDown` / `TouchUp`:

* `sw_pmp` (the pump switch) — copies its state to the green
  "Running" LED, recomputes the flow-rate display, and forces the
  power-mode radio group back to Normal.
* `r_econ` / `r_norm` / `r_hi` — recompute the displayed flow rate
  using a different multiplier (18.3, 20.1, 23.7). When the pump is
  off, multiplying by `CtrlVal(sw_pmp) = 0` yields zero.
* `pb_test` (the "TEST" button) — lights the red "Alarm" LED on
  `TouchDown`, extinguishes it on `TouchUp`. This is the only control
  that uses both interrupts; everything else is a momentary action.
* `cb_enabled` — enables or disables the four logging-related widgets
  with `GUI Restore` / `GUI Disable`. Disabled widgets are drawn dim
  and ignore further input.
* `sb_bright` — feeds its value (10–100 in steps of 10) straight to
  the firmware `Backlight` command. On firmware without a controllable
  backlight, `Backlight` is a no-op.

Numberboxes (`nbr_hi`, `nbr_lo`) and the file-name textbox
(`tb_fname`) do not appear in the `Select Case` at all — they are
edited in place by the firmware's built-in input handler, and the
program would simply read their current `CtrlVal(...)` if it were
doing real work.

## Layout

The screen is divided into three vertical columns:

```
+-----------+--------+--------------+
| Pump      | LEDs   | Log File     |
| Switch    | Run    | [x] Log On   |
|           | Alarm  | File Name    |
| Flow Rate |        | LOGFILE.TXT  |
|  20.1     | Alarm  |              |
|           | Hi: 35 | Record:      |
| Power     | Lo: 16 | [x] Flow     |
|  o Econ   | [TEST] | [x] Alarms   |
|  * Normal |        | [x] Warnings |
|  o High   |        |              |
|           |        | Back Light   |
|           |        | [- 100  +]   |
+-----------+--------+--------------+
```

All x/y coordinates are absolute pixel positions inside the 320×240
canvas. The numbers chosen by the original author have been preserved
unchanged so the demo continues to render identically on every
firmware build it has historically been tested on.

## Extending the demo

* **Add a new control**: pick an unused reference-number constant,
  declare it at the top of the file, create the widget in the build
  section, and add a `Case` in `TouchDown` (and optionally
  `TouchUp`).
* **Add a new input source**: do nothing. As long as the new device
  ultimately drives `GUI CURSOR` + `GUI CLICK` or fires the touch
  interrupt, the existing handlers will pick it up.
* **Run on a different resolution**: change the four section anchors
  (the column-x positions 5, 115, 210 and the LED column at 125), or
  move the `MODE 2` guard to select a larger mode and re-flow the
  layout. The compatibility scaffolding is independent of the
  layout numbers.
