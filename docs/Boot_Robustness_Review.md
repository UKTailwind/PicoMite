# Boot robustness review: what can stop a PC3 coming up

Review of V6.03.02b12, 2026-09-21. No firmware was changed. The question
was: how can random data in a RAM or flash slot, or any other state that
survives a power-off, stop the board booting, given that the library is
now validated before `PrepareProgram` walks it.

## Summary

1. **On b12 no slot can hang the boot.** Nothing between reset and the
   prompt reads a RAM slot at all, flash slots 1-2 are read only when
   `OPTION AUTORUN n` names one, and the library is validated first.
   `PrepareProgramExt` bounds every walk to `MAX_PROG_SIZE`. A slot full
   of garbage can still fault a later `FLASH LIST` / `RAM LIST`, but the
   prompt is reached first.
2. **The board on COM16 tells us nothing about last night's failure.** It
   was reflashed today at 10:19 with a *different variant* (HDMIUSB,
   flash offset 1088 KB) from the one it ran yesterday (HDMIWEB,
   1504 KB). HDMIUSB's slot area covers HDMIWEB's option sector, so its
   `ResetAllFlash` erased the HDMIWEB options, and its A: drive window
   (above 1708 KB) sits inside HDMIWEB's slot and MODBUFF area, so it
   formatted *that* and showed an empty drive. HDMIWEB's own A: drive
   starts at 2612 KB and was never touched: after flashing HDMIWEB back
   this afternoon it mounted with the game data intact, a `player.bas`
   saved at 09:48 and a screenshot at 09:53 today, boot count 15. So the
   board was running the game under HDMIWEB this morning; if it is the
   board that failed overnight it had been recovered before 09:48, and
   its options (now factory PC3 at 315 MHz) were reset by the variant
   change, not by the failure. Either way the recovery cannot
   distinguish a persistent cause from a transient one.
3. **Four ways the firmware can still loop or sit dead before any input**,
   ranked below. Two of them are silent (nothing on any console), and
   both write the option sector on every cycle.
4. **One destructive policy**: any `lfs_mount` failure at boot reformats
   the A: drive unconditionally, with only a console message. That is
   loss of the converted game data, not a brick. (An option reset on a
   PC3 does *not* format: the auto-configure path takes the `noask`
   branch of `testMODBUFF`, which only resets options. COM16's drive
   survived exactly that today.)

## What runs before the first input

Order in `main()` ([PicoMite.c](../PicoMite.c#L3577)), with the persistent
or uninitialised state each step consumes:

| Step | Reads | Can it stop the boot? |
| --- | --- | --- |
| `_excep_code` tests | uninitialised SRAM, no magic | decides autorun skip, clock reset, A: format (see 5) |
| `LoadOptions` + sanity list ([:3612](../PicoMite.c#L3612)) | option sector | invalid -> `ResetAllFlash` + reboot, never loops |
| 1 s watchdog window ([:3690](../PicoMite.c#L3690)-[:3861](../PicoMite.c#L3861)) | `CPU_Speed`, `PSRAM_CS_PIN`, PSRAM chip | **yes: silent loop (1)** |
| `TestPicoComputer3` / `configure` | GP27, `Option.platform` | ends in `RESET_FLASHSTORAGE` -> A: formatted |
| `updatebootcount` ([:3126](../PicoMite.c#L3126)) | lfs superblock, `FlashSize`, `modbuff` | mount failure -> **format** |
| HDMI core1 launch | `Option.Resolution` | unknown value -> no picture (4) |
| USB host init, then `tuh_task` in the loop | attached devices | a hcd `panic()` -> fault -> reboot loop (2) |
| WEB only: `cyw43_arch_init` + `WebConnect` | SSID | blocks up to 30 s, not a loop |
| `LibraryImageValid` -> erase on failure | library slot | no (fixed in cee311a) |
| `PrepareProgram(true)` | program flash + library | no, bounded |
| `MM.STARTUP` if present | program/library text | a valid program that waits: Ctrl-C breaks it |
| `OPTION AUTORUN` | program flash or slot 1-3 | same as MM.STARTUP; a fault clears autorun |

Saved variables are never restored at boot. RAM slots and the PSRAM
context area are never read at boot.

## Ranked findings

### 1. Silent boot loop inside the clock-speed watchdog window

[PicoMite.c:3668-3692](../PicoMite.c#L3668): every boot arms a 1000 ms
hardware watchdog with `_excep_code = RESET_CLOCKSPEED`, then sets the
voltage and PLL, brings up PSRAM, and clears the watchdog at
[:3861](../PicoMite.c#L3861). If that window ever overruns:

- reboot with `RESET_CLOCKSPEED` -> `CPU_Speed = Freq480P` (315 MHz),
  `SaveOptions()`, `SoftReset(INVALID_CLOCKSPEED)`
- the next boot is not `RESET_CLOCKSPEED`, so it arms the same window
  again; if the overrun is not a clock problem it repeats for ever.

Each cycle erases and programs the option sector once. Nothing is printed
because no console exists yet. What can overrun the window:

- `csr_busy_wait()` and `csr_txempty_wait()` in
  [io/psram.c:91-107](../io/psram.c#L91) have no timeout. A PSRAM or QMI
  that does not answer the read-ID command spins for ever.
- `psram_init` also `memset`s the whole 6 MB heap and touches every slot;
  fine at 315 MHz, but it is inside the window.
- A marginal chip at the saved speed. The recovery only ever changes the
  clock, so any cause that is not the clock loops indefinitely.

This is the only path found that persists across a same-variant reflash
AND shows nothing on any console. A firmware reflash "fixes" it only
because the BOOTSEL sequence power-cycles the PSRAM.

**Fixed 2026-09-21 (uncommitted at the time of writing):** both waits in
`io/psram.c` are bounded by a loop count (1<<20 iterations, about 20 ms
at 315 MHz; a timer cannot be used because XIP is off in direct mode).
On a timeout the command sequence still runs through to
`csr_disable_direct_mode()`, `psram_init` returns -5, `psram_size()` is
0, and `main()` keeps `OPTION PSRAM PIN` instead of clearing it (a chip
that did not answer is not a wrong option). Once the console is up the
boot prints `PSRAM not responding: disabled until the next restart`.
Built as HDMIWEB and running on the COM16 PC3; PSRAM detects normally
(heap 6438144).

### 2. Deterministic fault before the prompt = reboot loop

`sigbus_c` ([PicoMite.c:2802](../PicoMite.c#L2802)) prints the dump on the
serial console, does `Option.Autorun = 0; SaveOptions()`, then
`SoftReset(SOFT_RESET)`. Anything that faults on every boot before the
prompt therefore loops, and writes the option sector each time. The dump
is visible only with the serial console attached; on the HDMI screen the
board looks dead. Sources found:

- `panic("hcd_clear_stall")` still in
  [tinyusb-0.21/.../hcd_rp2040.c:832](../../tinyusb-0.21/src/portable/raspberrypi/rp2040/hcd_rp2040.c#L832);
  the other host panics were already downgraded. A device that needs a
  stall cleared during enumeration hits it at every boot.
- `PICO_MALLOC_PANIC=1` on non-network builds
  ([CMakeLists.txt:677](../CMakeLists.txt#L677)): a newlib heap
  exhaustion during boot panics. HDMIUSB is a non-network build.
- Any option field that passes the magic check but is out of range and is
  used as a table index (`PinDef[Option.SerialTX]` etc). Only reachable by
  a corrupt-but-magic-valid sector, which needs a code bug to produce.

A fault while XIP is disabled (inside a flash erase/program) double-faults
because `MMPrintString`/`SaveOptions` live in flash: that is a hard lock
with no watchdog, until power-cycled. Not persistent.

### 3. A mount failure at boot formats the A: drive

[PicoMite.c:3138](../PicoMite.c#L3138): if `lfs_mount` fails in
`updatebootcount`, the drive is formatted at once, with a console message
only. Every boot also writes the `bootcount` file, so every boot is an lfs
write and a power-off during boot lands inside it. lfs is designed for
that, but the recovery policy is the harshest available and the game's
converted data cannot be regenerated on the board.

What does *not* trigger it, checked today: an option reset on a PC3. The
chain is option sector invalid -> `ResetAllFlash` -> reboot ->
`Option.platform` empty -> `TestPicoComputer3` -> `configure(..., noask)`
-> `testMODBUFF(true, 512, noask)`, and with `noask` that only calls
`ResetOptions` and returns false, so `doreset` is a plain `SOFT_RESET`.
The lfs geometry (`FlashSize`, `modbuff`) is back to the PC3 values before
`updatebootcount` runs. COM16 went through this at 10:19 (HDMIUSB) and
again this afternoon (HDMIWEB) and its HDMIWEB drive is intact.

What does: a manual `OPTION MODBUFF` / platform change that answers Y to
"This erases everything in flash", and any real lfs corruption.

**Changed 2026-09-21 at Peter's request (uncommitted):** a failed magic
key check is now a full clean. The boot sanity path sets
`RESET_FLASHSTORAGE` after `ResetAllFlash` instead of clearing the code,
and `doreset()` (MM_Misc.c) keeps that code across the soft reset the PC3
/ PicoCalc auto-configure does, so `updatebootcount()` formats the A:
drive on the following boot. The magic key is per variant, so a variant
change wipes everything; a firmware VERSION change keeps the same key and
still resets nothing. Note a power cut inside `SaveOptions` leaves an
unreadable sector, which the check cannot tell from a variant change,
so that now costs the A: drive too. Not exercised on hardware: it needs
a build with a different key.

Ways to get an invalid option sector on a working board, for the record:
power-off inside `SaveOptions` (erase then program, about 10 ms; 118 call
sites in MM_Misc.c plus the boot-time ones in items 1, 2, the
RTC-not-found and PSRAM-not-found paths), or flashing a different variant
whose slot area overlaps the option sector.

### 4. HDMI: an unknown resolution gives no picture and is not repaired

[PicoMite.c:3653](../PicoMite.c#L3653) (full HDMI builds): if
`Option.Resolution` is not one of the known values, the code resets
`Option.CPU_Speed` and saves, but leaves `Option.Resolution` unchanged.
`HDMICore` selects the mode with an if/else chain over the same values
and matches nothing, so core1 never scans out. The serial prompt is alive,
the monitor is blank, and the state survives a same-variant reflash. The
cut-down builds validate the pair and repair both fields, the full build
should do the same.

### 5. `_excep_code` is uninitialised RAM with no companion magic

[PicoMite.c:364](../PicoMite.c#L364). At cold power-up its value is
whatever the SRAM cells settle to. The codes are 9988-9998; 9989
(`RESET_FLASHSTORAGE`) formats the A: drive, 9990 resets the clock,
9998/9992 suppress autorun. The odds are tiny for a uniform value, but a
given board's SRAM power-up pattern is largely the same every time, so a
board that hits it hits it on every cold boot and never on a warm one.
Pairing the word with its complement (or a magic in `_persistent`'s
style) closes it for four bytes.

### 6. RAM slots keep power-up garbage that starts with 0x01

[io/psram.c:358](../io/psram.c#L358): at every boot a slot is zeroed
unless its first byte is `T_NEWLINE` (0x01). That is deliberate so slots
survive `CPU RESTART`, but at cold power-up the first byte is whatever the
PSRAM powered up with. A slot kept this way is never touched by the boot,
so it cannot brick, but it feeds:

- `RAM LIST` ([misc/FileIO.c:771](../misc/FileIO.c#L771)) walks it with
  `llist`, unbounded, the same way `FLASH LIST` faults over a garbage
  flash slot.
- `RAM LOAD n` ([:867](../misc/FileIO.c#L867)) only checks that first
  byte, then copies the slot over program flash. Program memory is then
  garbage; boot survives (bounded walk) but `LIST`/`EDIT` will not.
- `RAM RUN n` executes it.

Prince of Pico is not exposed: it writes slots 4-8 with
`Flash Load Image ..., O`, which overwrites. Clearing all five slots when
`restart_reason` shows `HAD_POR` would remove the class.

### 7. PSRAM detection failure is made permanent

[PicoMite.c:3750](../PicoMite.c#L3750): if `psram_size()` is 0 at boot
the firmware sets `Option.PSRAM_CS_PIN = 0` and saves. `Option.platform`
stays "PICO COMPUTER 3", so `TestPicoComputer3` never re-enables it. One
cold-boot glitch on the PSRAM read-ID turns the PC3 into a 144 KB machine
until someone re-issues the option; the game then fails at its first
`Flash Load Image 4`. Not a brick, but it looks like one to a player.

### 8. Flash slots 1-3 with garbage

Only consumed at boot through `OPTION AUTORUN n`. Then
`PrepareProgram(true)` is bounded, and if the first byte is 0x01 the
garbage is RUN; the resulting error or fault returns to the prompt with
autorun cleared. `FLASH LIST` over such a slot still faults (known since the library
guard work, deliberately left). The library check only runs
when `LIBRARY_FLASH_SIZE == MAX_PROG_SIZE`; a garbage slot 3 with the
flag clear is an ordinary garbage slot.

### 9. Program memory partially written

`RUN "file"` and `AUTOSAVE` erase `PROGSTART` and program it in 4 KB
pages. A power-off in between leaves a valid prefix followed by 0xFF; the
boot walk stops at the 0xFF, prints any pre-program error, and reaches the
prompt. `NEW` repairs it.

### 10. Things that look like a brick for a while

- HDMIWEB: `WebConnect` blocks up to 30 s at boot when the access point
  is not reachable, before the library check and the prompt.
- `OPTION AUTORUN` or an `MM.STARTUP` that waits for something: Ctrl-C
  from the serial console or the USB keyboard breaks it, but only once the
  keyboard has enumerated.
- `Option Console Serial` in the game is runtime only, it is not saved,
  so it cannot blank the HDMI console on the next boot.

## What the recent library guard does and does not cover

It covers exactly the class it was written for: an image in slot 3 with
the library flag set that `PrepareProgramExt` used to walk off the end
of. It does not cover items 1, 2, 4 or 5 above, none of which involve a
slot. If the failing board had a valid, empty library (COM16 has), the
guard was never involved.

## Next time it happens: capture before reflashing

1. Attach the serial console (PC3: COM2 on GP8/GP9, 115200) and power
   cycle. Read what appears:
   - `*** FAULT PC=...` repeating: item 2. The PC identifies the code.
   - `Formatting the A: drive`: item 3, the option sector was reset.
   - `Library was corrupt`: the guard did its job.
   - nothing at all, for ever: item 1 (or a dead clock). `MM.INFO(BOOT)`
     cannot help because the prompt never comes.
2. In BOOTSEL mode, save the persistent region before writing anything:
   ```
   picotool save -r 0x10178000 0x1020D000 pc3_state.bin
   ```
   (HDMIWEB: `FLASH_TARGET_OFFSET` 1504 KB, slots 144 KB; the range
   covers options, saved vars, three slots and program memory. For
   HDMIUSB use 0x10110000 to 0x101AD000, slots are 152 KB.) The options,
   library flag and slots can then be read offline.
3. Reflash with the same variant. A different variant, or
   `Clear_flash.uf2`, discards the evidence; whether the A: drive
   survives a variant change depends on where the other variant's lfs
   window lands, so do not count on it.

## Suggested fixes, smallest first (not implemented)

- Bound `csr_busy_wait`/`csr_txempty_wait` and treat a timeout as
  "no PSRAM" for this boot only, without saving the option (items 1, 7).
- Count consecutive `RESET_CLOCKSPEED` cycles in `_persistent` (it is
  uninitialised RAM that survives the watchdog) and after two, boot with
  PSRAM off and the default clock instead of looping (item 1).
- Store `_excep_code` with its complement and treat a mismatch as 0
  (item 5).
- On the full HDMI build, repair `Option.Resolution` together with
  `CPU_Speed` (item 4).
- On `HAD_POR`, zero all RAM slots (item 6).
- Do not format the A: drive on a mount failure at boot; leave it
  unmounted, print the error, and let `OPTION RESET`/`FORMAT` do it
  explicitly (item 3).
- Replace the remaining `panic("hcd_clear_stall")` with a recoverable
  return, as the other host panics were (item 2).
