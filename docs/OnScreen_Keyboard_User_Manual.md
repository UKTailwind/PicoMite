# On-Screen Keyboard (OSK) User Manual

## Overview

The PicoMite VGA / HDMI firmware can reserve a strip at the bottom of the screen
and paint a touch-driven on-screen keyboard there. With a USB touch panel
attached the keyboard works exactly like a hardware USB keyboard — anything you
tap is delivered into the same console RX queue that `INKEY$`, `INPUT`, the
editor, and the file manager read from. There is no separate "OSK API" — the
keyboard simply types for you.

The feature is available on builds compiled with `USBKEYBOARD`, `GUICONTROLS`,
and `PICOMITEVGA` (e.g. the HDMIUSB and VGAUSB targets). It is **off by
default** and only activates after you set its size with
`OPTION SCREEN KEYBOARD`.

## Quickstart

```basic
' Reserve 30% of the screen height for the keyboard and use multi-touch
' for modifiers (hold Shift with one finger, tap a key with another).
OPTION SCREEN KEYBOARD 30, M
```

After this is set, the keyboard is automatically drawn whenever you are at
the command prompt or inside the editor / file manager. The reserved strip
is enforced by the system — the editor's row count and the prompt's scroll
boundary automatically shrink to leave room for the keyboard.

To remove the keyboard:

```basic
OPTION SCREEN KEYBOARD 0
```

## Commands

### `OPTION SCREEN KEYBOARD percent [, M]`

Sets the percentage of the vertical resolution to reserve for the on-screen
keyboard and (optionally) selects multi-touch modifier mode.

| Parameter | Range | Meaning |
|---|---|---|
| `percent` | 0..50 | Strip height as a percentage of `VRes`. `0` disables the keyboard entirely. Typical useful values are 25..40. |
| `M` | flag | (optional) Enable multi-touch modifiers. Shift / Ctrl / Sym / Fn behave like physical keys (hold + tap). Omit for traditional tap-to-latch behaviour. |

The setting is written to flash so it survives a reboot. Both the strip
height and the multi-touch flag are reported by `OPTION LIST`:

```
> OPTION LIST
...
OPTION Screen Keyboard 30, M
...
```

Setting `percent = 0` clears both the strip and the multi-touch flag.

### `KEYBOARD ON | OFF`

Explicit runtime control over the on-screen keyboard.

```basic
KEYBOARD ON         ' show the keyboard
KEYBOARD OFF        ' hide the keyboard
```

`KEYBOARD ON` requires that `OPTION SCREEN KEYBOARD` has been set to a
non-zero value first; otherwise an error is raised.

The command is valid both at the command prompt and inside a running
program, with slightly different semantics:

- **At the prompt:** `KEYBOARD OFF` *persistently* disables the on-screen
  keyboard until you explicitly issue `KEYBOARD ON`. The strip is released
  back to the editor / prompt so the full screen is usable. The setting
  survives errors, soft resets, and `RUN`. The setting **does not** survive
  a power cycle — the keyboard returns on the next boot.
- **Inside a program:** `KEYBOARD ON` and `KEYBOARD OFF` toggle the
  keyboard for the duration of the program. They do **not** override the
  prompt's persistent disable — if you had `KEYBOARD OFF` set at the prompt
  before `RUN`, that state is restored when the program exits.

The "if the keyboard was on when the program started, it must be on when
the program exits" rule is automatic. The program may freely toggle it
mid-run for its own input UI without affecting what the user sees back at
the prompt.

When the keyboard is shown for the first time after being absent, the
display content scrolls up by the strip's height (rounded to the nearest
text row), so prompt history or program output is preserved rather than
overwritten.

### `MM.INFO(KEYBOARD)`

Returns the current runtime state of the on-screen keyboard as a string:

| Value | Meaning |
|---|---|
| `"On"` | A program has called `KEYBOARD ON` and the keyboard is in program-controlled state. |
| `"Off"` | The keyboard is not in program-controlled state (either off entirely, or in the prompt's automatic system mode). |

```basic
IF MM.INFO(KEYBOARD) = "On" THEN PRINT "OSK is program-controlled"
```

## Keyboard Layout

The keyboard is a 12-column × 4-row grid. The bottom row holds modifier and
navigation keys; the upper three rows hold the printable characters of the
currently selected page.

```
+----+----+----+----+----+----+----+----+----+----+----+----+
| q  | w  | e  | r  | t  | y  | u  | i  | o  | p  | "  | BS |
+----+----+----+----+----+----+----+----+----+----+----+----+
| a  | s  | d  | f  | g  | h  | j  | k  | l  | ;  | '  | Ent|
+----+----+----+----+----+----+----+----+----+----+----+----+
| Sh | z  | x  | c  | v  | b  | n  | m  | ,  | .  | /  | Del|
+----+----+----+----+----+----+----+----+----+----+----+----+
|Sym | Fn | Ct |Esc |Tab |   Space   | ←  | ↓  | ↑  | →  |
+----+----+----+----+----+----+----+----+----+----+----+----+
```

### Modifier keys

| Key | Function |
|---|---|
| **Sh** (Shift) | Selects upper-case letters and shifted punctuation on the current page. |
| **Ctl** (Ctrl) | Folds the next letter to its Ctrl-equivalent (Ctrl-A = `&H01` etc.). Ctrl-C breaks a running program just as a hardware keyboard's Ctrl-C does. |
| **Sym** | Switches to the symbol/digit page (numbers and common punctuation). |
| **Fn** | Switches to the function-key page (F1–F12, Home, End, Page Up/Down, Insert, Del). |

Sym and Fn are mutually exclusive — engaging one releases the other.

### Special keys

| Key | Sends |
|---|---|
| `Ent` | `\r` (carriage return) — same as Enter on a hardware keyboard |
| `BS` | `\b` (backspace, `&H08`) |
| `Del` | `&H7F` (forward delete) |
| `Tab` | `\t` (`&H09`) |
| `Esc` | `\x1B` |
| `←` `↓` `↑` `→` | Arrow-key byte codes (`&H82`, `&H81`, `&H80`, `&H83`) |
| `Hm` `End` `PU` `PD` `Ins` `Del` (Fn page) | Home, End, Page Up, Page Down, Insert, Delete |
| `F1` … `F12` (Fn page) | Function-key byte codes |

## Modifier Behaviour: Tap-Toggle vs Multi-Touch

The four modifier keys can operate in two modes, selected by the `M` flag of
`OPTION SCREEN KEYBOARD`.

### Tap-Toggle (default)

A single tap on a modifier latches it on. The modifier stays engaged until
the next non-modifier tap, which both emits the modified character and
clears the latch. Sym and Fn page selectors are *sticky* — they stay until
tapped again.

This works one-finger at a time, suitable for a stylus or a single-touch
panel.

```
Tap Shift    →  Shift latches (key shown highlighted)
Tap 'a'      →  emits 'A', Shift releases
```

### Multi-Touch (`OPTION SCREEN KEYBOARD percent, M`)

Modifiers work like physical keyboard keys: pressed-and-held by one finger,
then chord-tapped by another. The modifier engages on the *down* edge of
finger 1 and releases on the *up* edge.

```
Press and hold Shift (finger 1)
Tap 'a' with finger 2   →  emits 'A'
Tap 'b' with finger 2   →  emits 'B'   (Shift still held)
Lift finger 1           →  Shift releases
```

A modifier tapped alone (down then up without a second finger) has no
effect — it is press-and-hold only.

Requires a multi-touch USB panel (the firmware uses contact 0 for finger 1
and contact 1 for finger 2). Single-touch panels work in tap-toggle mode
regardless of the `M` flag.

## Lifecycle

- **At the command prompt:** the keyboard is in *system mode*. It draws
  automatically, and tapping a key types into the prompt's input line.
- **Inside the editor or file manager:** the keyboard stays drawn (system
  mode), and taps reach the editor/FM exactly as if they came from a
  hardware keyboard. The editor's auto-scroll boundary and the FM's row
  count both already respect the reserved strip.
- **`RUN` (or any program start, including `CHAIN` and FM-launched
  programs):** the keyboard is erased and the strip is released. The
  program has the full screen and full control. The on-screen keyboard
  becomes invisible until the program either calls `KEYBOARD ON` or exits.
- **Program calls `KEYBOARD ON`:** the strip is re-reserved (display scrolls
  up first so existing program output is preserved) and the keyboard is
  drawn in *program mode*. Taps reach the program via `INKEY$`, `INPUT`,
  `LINE INPUT`, etc.
- **Program ends (`END`, error, Ctrl-C, FM relaunch):** the keyboard is
  restored to the state it had at `RUN` time. If the keyboard was on at
  the prompt before `RUN`, it is on when control returns; if you had
  explicitly issued `KEYBOARD OFF` before `RUN`, it stays off.

## Editor and File Manager Integration

When a USB touch panel is present and the firmware was built with
`USBKEYBOARD` + `GUICONTROLS`:

- **Editor**
  - **Tap** inside the text area moves the edit caret to the tapped cell.
  - **Swipe up / down** sends Page Up / Page Down.
  - **Swipe left / right** sends Home / End.
  - Taps inside the keyboard strip are consumed by the OSK and never reach
    the editor's tap-to-position handler.
- **File manager**
  - **Tap** a row in either panel selects that row and makes the panel
    active.
  - **Swipe up / down** scrolls through entries (Page Up / Down).
  - **Swipe left / right** switches the active panel.
  - Taps inside the keyboard strip are consumed by the OSK.

## Notes and Limitations

- Available only on `PICOMITEVGA` builds with `USBKEYBOARD` and `GUICONTROLS`
  (HDMIUSB, VGAUSB, and similar variants).
- The keyboard uses the current `gui_font` for its captions, so changing
  fonts at the prompt also changes how the keyboard reads.
- The strip is taken from `VRes` *as a percentage*. A value of `33` on a
  480-pixel-tall display reserves roughly 158 pixels (≈ 11 text rows at a
  14-pixel font).
- The strip percentage is shared with the editor's reserved-area concept;
  if you set a non-zero value but never want the OSK visible, use
  `KEYBOARD OFF` (the strip is released back to the editor / prompt for
  the duration of the session).
- `OPTION SCREEN KEYBOARD 0` is the only way to remove the strip
  permanently. `KEYBOARD OFF` removes it only until the next reboot or
  explicit `KEYBOARD ON`.
- Programs may freely query and toggle the OSK with `MM.INFO(KEYBOARD)` /
  `KEYBOARD ON` / `KEYBOARD OFF`. The flash-stored setting is not changed
  by these runtime calls.
