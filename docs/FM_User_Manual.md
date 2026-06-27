# FM Command User Manual

## Overview

The `FM` command opens a full-screen, two-panel file manager for the active storage devices.

It is designed for keyboard-driven workflow on serial terminals, VGA, HDMI, and LCD console modes:

- Browse directories in two panels.
- Sort each panel by name, datetime, or type then name.
- Launch `.BAS` programs directly.
- Open files in the built-in editor.
- Copy, rename, delete, and create directories.
- Mark multiple items and run batch copy/delete operations.
- Play audio files and preview image files.
- List any file using the built-in paged viewer.
- Keep context between invocations (paths, panel selection, filters, and sort mode).

## Command

```basic
FM
```

`FM` takes no arguments.

## Display Requirements

FM requires at least **50 columns** and **8 rows**.

On VGA or HDMI displays with fewer than 64 characters per line, FM automatically switches to a wider screen mode and a compatible font for the duration of the session, then restores the original mode and font on exit.

On LCD console modes narrower than 50 columns, FM attempts to select a smaller font. If the display is still too narrow after the font change, FM reports an error and does not start.

On serial terminals, FM requests the terminal to resize if the configured `Option.Width` or `Option.Height` is larger than the terminal's current size.

## Drives

| Drive letter | Storage device |
|---|---|
| `A:` | Internal SD card (always available) |
| `B:` | Second storage device (SD, NOR flash, or similar) |
| `C:` | USB Mass Storage Class device (RP2350 USB builds only) |

Pressing `A`, `B`, or `C` switches the active panel to that drive. If the requested drive is not ready, FM reports the error on the status line and does not switch.

## Screen Layout

The screen is divided into four areas:

```
┌─────────────── title / header row ─────────────────┐
│ Left panel (files)       │ Right panel (files)      │
│ ...                      │ ...                      │
├─────────────────────────────────────────────────────┤
│ Status line                                         │
└─────────────────────────────────────────────────────┘
```

Each panel header shows the drive letter, current path, and active filter. File list rows show a leading marker column (`*` = marked, space = unmarked) followed by the filename, size, and modification date.

The active panel is highlighted. All file operations apply to the active panel.

The status line at the bottom shows the result of the last operation, the current selection name, or a progress message.

## Keyboard Shortcuts

Each command has a single mnemonic letter and a function key. Cursor movement and panel switching additionally accept WordStar-style Ctrl-key alternatives so FM remains usable on terminals without arrow or paging keys. Command letters are case-insensitive.

| Action | Letter | Function key | Other |
|---|---|---|---|
| Exit FM | — | — | `ESC` |
| Switch active panel | — | — | `TAB`, `LEFT`, `RIGHT` (`Ctrl-S` = left, `Ctrl-D` = right) |
| Move selection up / down | — | — | `UP`, `DOWN` (`Ctrl-E`, `Ctrl-X`) |
| Page up / page down | — | — | `PgUp`, `PgDn` (`Ctrl-P`, `Ctrl-L`) |
| Jump to top of drive / end of list | — | — | `HOME`, `END` (`Ctrl-U`, `Ctrl-K`) |
| Go to parent directory | — | — | `Backspace` |
| Open / run selected item | — | `F2` | `Enter` |
| Help screen | `H` | `F1` | `?` |
| List selected file (paged viewer) | `L` | `F3` | — |
| Edit selected file | `E` | `F4` | — |
| Copy selected / marked item(s) | `Y` | `F5` | — |
| Stop audio playback | `T` | `F6` | — |
| Volume down | — | `F7` | `-` |
| Volume up | — | `F8` | `+` or `=` |
| Set filter | `F` | `F9` | — |
| Clear filter | `W` | `F10` | — |
| Make directory | `K` | `F11` | — |
| Rename selected item | `R` | — | — |
| Delete selected / marked item(s) | — | — | `DEL` |
| Recursive delete selected / marked item(s) | `X` | — | — |
| Duplicate selected item | `D` | — | — |
| Move selected item to other panel | `M` | — | — |
| New file (create and edit) | `N` | — | — |
| Make / change drive | `A`, `B`, `C` | — | — |
| Go to path | `G` | — | — |
| Cycle sort mode | `S` | — | — |
| Mark / unmark current item | — | — | `Space` |
| Mark all items in panel | — | — | `*` |
| Clear all marks in panel | — | — | `\` |
| Type-to-select by filename prefix | — | — | `/` then type (`Enter` opens, `Esc` cancels, `Backspace` edits prefix) |

## Navigation

### Moving Within A Panel

`UP`/`DOWN` (or `Ctrl-E`/`Ctrl-X`) move the selection one item at a time. `PgUp`/`PgDn` (`Ctrl-P`/`Ctrl-L`) scroll by a full page. `HOME` (`Ctrl-U`) jumps to the root of the drive; `END` (`Ctrl-K`) jumps to the last item.

`Backspace` navigates to the parent directory. Pressing `Enter` on a `..` entry has the same effect.

### Switching Panels

`TAB`, `LEFT`, or `RIGHT` toggles the active panel. `Ctrl-S` makes the left panel active; `Ctrl-D` makes the right panel active.

### Changing Drives

Press `A`, `B`, or `C` to switch the active panel to that drive. If the drive is not available, FM stays on the current drive.

### Go To Path

Press `G` to enter a path directly. The path may be absolute (`/subdir/project`) or relative. The active panel navigates to the entered path.

## Sorting

Press `S` to cycle the sort mode for the active panel:

1. **Name** — alphabetical, directories first.
2. **Datetime** — newest item first.
3. **Type then name** — grouped by file extension, alphabetically within each group.

Sort mode is saved in FM context and restored on the next launch.

## Type-Select

Press `/` to enter type-select mode:

- The active panel is forced to **name sort** if it is not already sorted that way.
- Type characters to build a case-insensitive filename prefix. The selection jumps to the first match after a short debounce pause.
- `Backspace` removes the last character from the prefix.
- `Enter` confirms the selection and opens the item.
- `Esc` cancels type-select and returns to normal mode.

Any key that is not a printable character (other than the above) exits type-select and is processed normally.

## Open Behavior

Press `Enter` (or `F2`) on the selected item:

- **Directory** — enters the directory.
- **`.BAS` file** — saves FM context and launches the program. FM relaunches automatically when the program ends (see [Program Launch And Return](#program-launch-and-return)).
- **Audio file** (`.WAV`, `.FLAC`, `.MP3`, `.MID`/`.MIDI`, `.MOD`) — starts background playback (see [Audio Playback](#audio-playback)).
- **Image file** (`.BMP`, `.JPG`, `.PNG`) — displays a full-screen preview (see [Image Preview](#image-preview)).
- **Any other file** — no action; the filename is shown on the status line.

## Marking

Marks allow a batch operation to apply to several items at once.

- `Space` toggles the mark on the current item and moves the selection down one row.
- `*` marks all items in the active panel.
- `\` clears all marks in the active panel.

A `*` character in the leading column of a list row indicates the item is marked. The status line shows the current mark count after each mark change.

When one or more items are marked, copy, delete, and recursive-delete operations apply to the **marked set** instead of the single selected item.

## Filters

Each panel has an independent wildcard filter (default `*`, which matches all files).

- `F9` (`F`) prompts for a new filter string (e.g. `*.BAS`, `TEST*`). Wildcards `*` and `?` are supported. The filter is case-insensitive.
- `F10` (`W`) resets the filter to `*`.

Filters affect the file list only; directory entries are always shown.

## File Operations

### Copy — `F5` / `Y`

Copies the selected item (or all marked items) from the active panel to the other panel's current directory.

- File copy overwrites the destination silently.
- Directory copy is recursive and works across drives.

### Delete — `DEL`

Deletes the selected item (or all marked items) after a single confirmation prompt.

- Files are deleted immediately.
- Directories must be empty to be deleted with this key. Use `X` to remove non-empty directories.

### Recursive Delete — `X`

Removes the selected item (or all marked items) recursively. Two separate confirmation prompts are required before any data is deleted.

### Rename — `R`

Prompts for a new name for the selected item. Only the leaf name may be changed; the item stays in its current directory.

### Duplicate — `D`

Creates a copy of the selected item in the same panel directory. FM prompts for the new name.

### Move — `M`

Moves the selected item to the other panel's current directory. Same-drive moves are performed as a rename. Cross-drive moves are not supported directly; use copy (`F5`) then delete (`DEL`).

### Make Directory — `F11` / `K`

Prompts for a directory name and creates it in the active panel's current path.

### New File — `N`

Prompts for a filename, creates an empty file in the active panel's current path, and immediately opens it in the built-in editor.

### Go To Path — `G`

Prompts for an absolute or relative path and navigates the active panel there.

## List File — `F3` / `L`

Pressing `F3` or `L` on a selected file displays the file contents using the same paged `LIST` viewer used by the `LIST` command. The display is paginated; `Ctrl-C` exits the listing and returns to FM. Directories cannot be listed.

## Editor Integration

Press `F4` or `E` to open the selected file in the built-in editor. The current working directory is set to the active panel's drive and path before the editor starts.

### Exiting The Editor

- `ESC` or `F1` — saves (if modified) and returns directly to FM.
- `F2` inside the editor on a `.BAS` file — saves and runs the program. FM relaunches when the program ends.

### Error Location Seek

When a BASIC program launched from FM throws a run-time error, FM records the file path, line number, and column. If `F4` is then pressed on the same file, the editor opens with the cursor positioned at the reported error location. The status line shows the line and column before the editor opens.

## Audio Playback

Pressing `Enter` on a recognised audio file starts background playback:

| Extension | Format | Notes |
|---|---|---|
| `.WAV` | PCM wave | Requires audio output configured |
| `.FLAC` | Free Lossless Audio | Requires audio output configured |
| `.MP3` | MPEG Audio Layer III | RP2040: requires VS1053; RP2350: requires `OPTION CPUSPEED` ≥ 200000 or VS1053 |
| `.MID` / `.MIDI` | MIDI | Requires VS1053 audio |
| `.MOD` | ProTracker module | Requires `OPTION MOD` buffer or PSRAM (RP2350) |

Audio plays in the background while FM remains fully interactive.

- `F6` (`T`) — stops playback.
- `F7` or `-` — decreases volume.
- `F8`, `+`, or `=` — increases volume.

If audio output is not configured, FM shows "Audio not enabled" on the status line.

## Image Preview

Pressing `Enter` on a `.BMP`, `.JPG`, or `.PNG` file displays it full-screen. Press any key to dismiss the preview and return to FM.

Image preview requires a graphical display (VGA, HDMI, or LCD). On a serial-only console, FM reports that the image cannot be displayed.

## Program Launch And Return

Pressing `Enter` on a `.BAS` file saves FM context (paths, selection, sort, filters) and runs the program using the active panel's drive and directory as the current working directory.

When the program finishes (normally, with `END`, or with an error), FM relaunches automatically and restores the saved context. Any error message from the program is shown on the FM status line.

`Ctrl-C` is disabled while FM is active. It remains active during `LIST` (where it exits the paged viewer) and during program execution launched from FM.

## Context Persistence

FM saves its full state when exiting or launching a program:

- Both panel paths, drives, and scroll positions.
- Active panel selection.
- Per-panel sort mode.
- Per-panel filename filter.

On the next `FM` invocation the state is restored. If a saved path is no longer accessible (e.g. drive removed), FM reports a partial restore on the status line and defaults the affected panel to the drive root.

## Notes For Manual Integration

This section is intended for inclusion in the main PicoMite User Manual under command reference (for example, near `FILES`, `CHDIR`, and editor-related commands).
