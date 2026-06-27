# FM Manual Regression Checklist

Use this sheet during manual validation. Mark each item Pass/Fail and add notes.

## Test Metadata

- Build/commit:
- Date:
- Hardware/console mode:
- Drives tested:
- Tester:

## 1) Launch And UI

- [ ] FM launches with two panels and status line.
- [ ] Active panel highlight is correct.
- [ ] Marker column is visible and updates.

## 2) Navigation

- [ ] Up/Down, PgUp/PgDn, Home/End work.
- [ ] Tab/Left/Right switches active panel correctly.
- [ ] Backspace goes to parent directory.
- [ ] Enter on directory opens directory.

## 3) Open Behavior

- [ ] Enter on BAS launches program.
- [ ] Enter on audio starts playback.
- [ ] Enter on BMP/JPG/PNG shows preview.

## 4) Sorting

- [ ] S cycles Name -> Datetime -> Type then name.
- [ ] Name sort order is correct.
- [ ] Datetime sort is newest first.
- [ ] Type then name groups by type, then name.

## 5) Sort Persistence

- [ ] Set different sort mode per panel.
- [ ] Exit FM and relaunch.
- [ ] Paths, filters, active panel, and per-panel sort restore.

## 6) Type-Select

- [ ] Press / enters type-select mode.
- [ ] Entering type-select forces Name sort.
- [ ] Fast typing does not drop characters.
- [ ] Match updates after brief debounce.
- [ ] Backspace edits prefix.
- [ ] Enter exits and opens selected item; Esc cancels.

## 7) Filtering

- [ ] F3 sets filter and list updates.
- [ ] F4 clears filter and full list returns.

## 8) Multi-Select Marks

- [ ] Space toggles current mark.
- [ ] * marks all items.
- [ ] \ clears all marks.
- [ ] Marks render correctly after scroll/redraw.

## 9) Copy/Delete Behavior

- [ ] With marks: F5 copies all marked items.
- [ ] With marks: DEL deletes all marked items (single confirm).
- [ ] With marks: X recursively deletes marked items (double confirm).
- [ ] Without marks: operations apply to selected item only.

## 10) Move Semantics

- [ ] Same-drive file move with M succeeds.
- [ ] Same-drive directory move with M succeeds.
- [ ] Cross-drive file move with M succeeds (copy + delete).
- [ ] Cross-drive directory move with M is rejected.
- [ ] Cross-drive directory transfer works via copy then delete.

## 11) Other File Operations

- [ ] F9 renames selected item correctly.
- [ ] F10 creates directory in active panel path.
- [ ] D duplicates selected file/directory.
- [ ] N creates empty file and opens editor.
- [ ] G jumps to valid absolute/relative paths.

## 12) Safety And Edge Cases

- [ ] Dot entries are protected from destructive operations.
- [ ] Destination-inside-source protection works.
- [ ] Name collision handling is correct.
- [ ] Error messages are clear and non-destructive.

## 13) Cache/Refresh Integrity

- [ ] After copy/move/delete/rename, lists refresh correctly.
- [ ] Selection remains in valid range.
- [ ] Marks do not drift to wrong entries after sort/filter/path changes.

## 14) Editor Integration

- [ ] F2/Ctrl-W opens selected file in editor.
- [ ] BASIC error round-trip returns to expected editor location.

## Results Summary

- Total cases:
- Passed:
- Failed:
- Known limitations confirmed:
- Follow-up issues:
