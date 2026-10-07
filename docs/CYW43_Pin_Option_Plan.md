# OPTION CYW43 PINS: plan

Status, 2026-10-07: the approach is agreed but not implemented. Two things must
be finished first:

- the Bluetooth background-interrupt storm on the BT-host builds (see "Blocking
  work");
- committing the PIO2 sharing work it builds on (see "Already done").

## Why

Waveshare's RP2350B Pico WiFi board wires the CYW43439 to non-standard pins.
The onboard LED is still on the CYW43's WL_GPIO0.

| Signal | Pico W / Pico 2 W | Waveshare RP2350B |
|---|---|---|
| WL_ON | GP23 | GP36 |
| WL_D | GP24 | GP37 |
| WL_CS | GP25 | GP38 |
| WL_CLK | GP29 | GP39 |

Other boards in the SDK's board list also differ. The SparkFun IoT RedBoard
RP2350 uses ON 24, D 38, CLK 37, CS 36. The SparkFun XRP controller uses ON 26,
D 29, CLK 28, CS 27.

The pins become a soft option, in line with MMBasic's usual approach, rather
than a compile-time change.

## Command

```
OPTION CYW43 PINS wl_on, wl_d, wl_cs, wl_clk
OPTION CYW43 PINS DEFAULT
```

- Works at the prompt only. Saves the options and restarts, like other
  pin-reserving options.
- Usage: a board in a PicoCalc (say) is set up with `OPTION RESET PICOCALC`,
  then `OPTION CYW43 PINS ...`. There is no Waveshare-specific OPTION RESET
  preset (to confirm with Peter). OPTION RESET returns the pins to the default.
- **Four parameters are needed.** No two CYW43 pins have to be next to each
  other, unlike I2S, where LRCLK is BCLK+1 because the PIO side-set pins must
  be consecutive.
  - WL_ON and WL_CS are plain GPIO, driven by software.
  - WL_D (data in both directions, and the chip's interrupt line) and WL_CLK
    are PIO pins. Each is mapped through its own state-machine pin register
    (OUT/SET/IN base for data, side-set base for the clock), so they can be
    anywhere relative to each other.
- The SDK's six-entry pin array (data out, data in and host wake listed
  separately) collapses to WL_D. All 15 CYW43 boards in SDK 2.3.1 use one pin
  for all three.

## Scope

- **RP2350 wireless builds only:** WEBRP2350, HDMIWEB, PICOBTRP2350,
  PICOBTHRP2350, HDMIBTH.
- **The RP2040 WebMite stays fixed at compile time.** Its PinDef has no entries
  for pins 41-44, and the build is short of flash and RAM.

## SDK mechanism (2.3.1, no SDK fork)

- Compile with `CYW43_PIN_WL_DYNAMIC=1`. It is added to the WEB, BT and BTH
  define blocks in CMakeLists.txt, which is how `CYW43_PIO_CLOCK_DIV_DYNAMIC`
  already reaches the SDK files.
- Call `cyw43_set_pins_wl(uint pins[6])` **before every `cyw43_arch_init*()`**
  (PicoMite.c: the BT path, the BTH/HDMIBTH path and the WEB path).
  - `cyw43_driver_init` attaches the host-wake GPIO interrupt to whatever pin
    is current at that moment.
  - The function refuses once the bus is up.
- **The PIO program needs no change.** It is pin-relative: `out pins`,
  `in pins`, `set pindirs` and side-set, with no `wait gpio`. The SDK programs
  the state machine's pin registers from the run-time array in
  `cyw43_spi_init`.
- **GPIOBASE is per PIO block, not global.**
  - Datasheet §11.1.1: "Each PIO block is still limited to 32 GPIOs at a time,
    but GPIOBASE selects which 32".
  - The register is at offset 0x168 of each block: 0x50200168, 0x50300168,
    0x50400168.
  - Using GP36-39 sets only PIO2's window to GP16-47.
  - `pio_set_gpio_base` refuses (PICO_ERROR_INVALID_STATE) once any program is
    loaded on that block.

## The PIO2 interlock with I2S

- In every CYW43 build, I2S audio shares PIO2 with the radio (radio on SM0, I2S
  on SM1). One GPIO window must therefore hold:
  - the radio's WL_D and WL_CLK;
  - I2S BCLK, BCLK+1 and DATA.
- Rule: all of those pins must be in GP0-31, or all in GP16-47.
- **OPTION AUDIO I2S** already checks this against `piomap[2]`. At boot,
  `InitCYW43PIO()` records the radio's pins there.
- **OPTION CYW43 PINS** must do the reverse check against the configured I2S
  pins.
- Example: the PicoComputer 3's I2S on GP10/GP11 (data GP22) cannot coexist
  with radio pins at GP32 or above. Give a precise error.

## Validation (exact rules, no guessing)

- The four pins are all different and each is a valid GPIO.
- Each pin passes `CheckPin` (not in use or reserved by another option).
- On an RP2350A, no pin is above GP29. Check the run-time `rp2350a` flag. The
  SDK's own check uses the build's NUM_BANK0_GPIOS, which is 48.
- WL_D and WL_CLK are both in GP0-31, or both in GP16-47.
- The PIO2 window also fits the configured I2S pins (above).

## Implementation steps

1. **CMakeLists.txt:** add `CYW43_PIN_WL_DYNAMIC=1` to the WEB (RP2350 only),
   BT and BTH blocks.
2. **misc/FileIO.h:** add four `uint8_t` fields after `Compile`, and take four
   bytes from **both** `extensions[]` arms (62 to 58, 70 to 66).
   - The struct stays 896 bytes; the static assert checks it.
   - 0 means the board default, so no MagicKey bump is needed.
   - At boot, any non-zero set that fails validation falls back to the default
     with a message.
3. **core/MM_Misc.c:**
   - parse the command;
   - add an OPTION LIST line, printed when not the default;
   - add four `OPT(..., OPT_U8)` entries in `OptionMap[]` for OPTION DISK
     SAVE/LOAD.
4. **PicoMite.c PinDef (~672-692):** in the RP2350 CYW43 builds, GP23/24/25/29
   get their normal capabilities instead of `UNUSED`.
5. **misc/SDCard.c InitReservedIO:** mark the four active pins
   `EXT_BOOT_RESERVED`.
   - `ExtCfg` with a reserved code only sets the flag; it doesn't touch the pad.
   - `ClearExternalIO` skips reserved pins.
   - Boot order: InitReservedIO, then ClearExternalIO, then InitCYW43PIO, then
     the radio.
6. **misc/Custom.c `InitCYW43PIO()`:**
   - use the option's WL_D and WL_CLK, not `CYW43_DEFAULT_PIN_WL_*`, for
     `piomap[2]` and PIO2's GPIOBASE;
   - then call `cyw43_set_pins_wl({ON, D, D, D, CLK, CS})`;
   - keep it before anything loads on PIO2.
7. **misc/External.c:**
   - `codemap()` (~372-397): replace the WEB-only compile-time ban on
     GP23/24/25/29 with a run-time ban on the configured four;
   - ADC channel 3 (GP29) guards (~5350-5458): rely on the reserved-pin check;
   - make sure the ADC close paths (`ExtCfg(44, EXT_NOT_CONFIG)`) can't reach a
     reserved pin.
8. **Left compiled out for wireless builds:** the Pico SMPS/VBUS/VSYS pin
   set-up (External.c ~5921, SDCard.c ~3216, MM.INFO SUPPLY, OPTION
   PICO/POWER). On these boards those pins are not the Pico's power pins.
9. **Docs:** a manual entry, then regenerate and lint the help file.

## Already done (PIO2 sharing; uncommitted on development as of 2026-10-07)

- **I2S placement:** I2S runs on the radio's PIO, SM1, in every CYW43 build
  (`start_i2s(2,1)`; `(1,1)` on the RP2040 WebMite). It is claimed with
  `pio_sm_claim` so the SDK never hands it to the radio.
- **`InitCYW43PIO()` (Custom.c):** called after ClearExternalIO. It sets PIO2's
  GPIOBASE from the radio's pins plus the I2S pins and fills `piomap[2]`.
- **`CPUSpeedRuntime`:** retunes only the radio's SM (claimed, not I2S's)
  instead of all four.
- **PIO visibility:** PIO2 is hidden from user programs in all CYW43 builds,
  and PIO1 is now available in WEBRP2350, HDMIWEB and HDMIBTH.
- **Bug fix:** `piomap[NUM_PIOS]` replaces an out-of-bounds read of `piomap[2]`
  in OPTION AUDIO I2S.
- **Tested:**
  - RP2040 WebMite (COM24);
  - HDMIWEB PC3 (COM9): tones heard, network fine during audio, radio retuned
    on RESOLUTION clock changes;
  - HDMIBTH PC3 (COM16).

## Related issues found on the way

- **HDMIBTH radio retune.** CPUSpeedRuntime's radio retune is guarded by
  `PICOMITEBT || PICOMITEWEB || PICOMITEBTH`. HDMIBTH defines only
  `PICOMITEHDMIBTH`, so a live RESOLUTION clock change never retunes its radio.
  Booting at 252 MHz gives divider 3, so a switch to 378 MHz runs the SPI at
  63 MHz, above the 50 MHz limit. Awaiting Peter's OK to fix.
- **Blocking work: the background-interrupt storm on the BT builds.**
  - The radio and btstack's async-context background interrupt re-fired about
    44,000 times a second (about 80,000 with a keyboard connected). It took
    72-88% of the CPU, and I2S tones warbled because the main loop couldn't
    refill the swing buffers.
  - It is present with both SDK 2.3.0 and 2.3.1, and in the posted b7.
  - It disappeared when BTAudio's speaker-reconnect timer was no longer armed
    while audio isn't Bluetooth (1,200-1,500 runs a second, 7-8% CPU).
  - The mechanism by which one long-pending btstack timer causes the storm is
    still being traced.
- **Wrong pins can't hang boot.** The SDK gives up after 10 test-register reads
  (about 280 ms with the chip reset).
  - After that failure it leaves the PIO state machine and two DMA channels
    claimed, and a WebConnect retry claims more.
  - Claiming SM1 for I2S keeps a retry off the I2S state machine.
- **Board header.** RP2350 wireless builds must stay on an RP2350B board header
  (`pimoroni_pico_plus2_w_rp2350`). With NUM_BANK0_GPIOS of 30 or fewer,
  `gpio_get()` of GP32 and above always reads 0, which would break host-wake on
  GP37.
