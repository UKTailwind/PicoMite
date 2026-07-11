# OV5640 Camera Support — Implementation Plan

Target scope (per Peter): **QVGA RGB565 capture + change detection + JPEG capture at
the largest resolution that fits the 153600-byte fast-SRAM buffer**, trading
compression quality as needed. **No autofocus.** RP2350-only, like the OV2640.

This mirrors the existing OV2640 co-existence work (`io/I2C.c`, `CameraType` enum,
table-driven init, `CAMERA OPEN OV5640 ...`). The OV5640 is *hardware* pin-compatible
with the OV2640 board but is a completely different sensor internally, so most of the
work is new init/SCCB code, not reuse of the OV2640 tables.

---

## 1. The four things that make the OV5640 different from the OV2640

These drive almost all the new code:

1. **16-bit register addresses.** OV7670/OV2640 use 8-bit register addresses
   (`sccb_write(reg8, val8)`). The OV5640 uses **16-bit** addresses
   (`reg16, val8`). This needs new SCCB helpers and a new register-table type — the
   single biggest change.
2. **SCCB address `0x3C`** (7-bit) — vs 0x21 (OV7670) / 0x30 (OV2640).
3. **Chip-ID check**: read `0x300A` (== `0x56`) and `0x300B` (== `0x40`).
4. **No register banks.** The OV2640 bank-switches via `0xFF`; the OV5640 has a flat
   16-bit register space (no `0xFF` bank writes, no `{0xFF,0xFF}` terminator trick —
   pick a different table terminator).

Everything else (bit-bang DVP capture with IRQs off, the 153600 fast-SRAM buffer, the
RGB565 preview → change detection → JPEG-to-file flow) reuses the existing machinery.

---

## 2. Code changes in `io/I2C.c`

### 2.1 Globals / sensor selection
```c
#define ov5640_address 0x3C
#define CAM_OV5640 2
```
Extend the `CAMERA OPEN` keyword parse (currently `OV2640`/none) with `OV5640`,
setting `CameraType = CAM_OV5640; camera_address = ov5640_address;`.

### 2.2 16-bit SCCB helpers (new, `#ifdef rp2350`)
```c
void sccb_write16(uint16_t reg, uint8_t val);   // send reg hi, reg lo, val
int  readregister16(uint16_t reg);              // write reg hi/lo, restart, read 1 byte
```
Model them on the existing `sccb_write`/`readregister` (same I2C0/I2C1 lock handling,
same `camera_address`), just a 2-byte register phase. These are the OV5640 equivalent
of the OV2640's `sccb_write`.

### 2.3 New register-table type + loader
```c
typedef struct { uint16_t reg; uint8_t value; } OV5640_command;   // 16-bit address
static void load_camera_regs16(const OV5640_command *t);          // sccb_write16 loop
```
Terminator: use `{0xFFFF, 0xFF}` (the OV5640 has no 0xFF bank register, so 0xFFFF is a
safe sentinel). Keep it separate from `load_camera_regs` (which stays 8-bit for
OV7670/OV2640).

### 2.4 Register tables (new, `#ifdef rp2350`)
Source them from a known-good OV5640 init (esp32-camera `ov5640_settings.h`, the Linux
`ov5640` driver, or ArduCAM `ov5640_regs.h`) and trim to what we need:
- `OV5640_init[]` — sysclk/PLL, format, timing, AWB/AEC/AGC, lens/gamma/colour-matrix
  (the big common base).
- `OV5640_QVGA_RGB565[]` — output = QVGA 320×240, RGB565 (preview / change detection).
- `OV5640_JPEG[]` + per-resolution size tables (see §4).

**Skip the autofocus firmware download entirely** — do NOT write the AF MCU firmware
blob (`0x8000+` / `0x3000`,`0x3004` MCU reset dance). Fixed default focus is accepted.
(If the module has a VCM lens it stays at its power-on position; verify focus is
acceptable when the hardware arrives.)

### 2.5 OPEN path
Add a `CameraType == CAM_OV5640` branch alongside the OV2640 one (~`io/I2C.c:3041`):
```c
// reset (0x3008 bit7), confirm chip id, load QVGA RGB565 preview
sccb_write16(0x3103, 0x11);          // sysclk from pad
sccb_write16(0x3008, 0x82);          // software reset
uSec(20000);
if ((readregister16(0x300A) != 0x56) || (readregister16(0x300B) != 0x40))
    error("Camera not found");
load_camera_regs16(OV5640_init);
load_camera_regs16(OV5640_QVGA_RGB565);
```
**Pin/clock setup:** the OV2640 board is self-clocked (pin1 repurposed as PWDN, no XCLK
PWM). Assume the pin-compatible OV5640 board is the same and reuse the OV2640 pin branch
(`CameraType != CAM_OV7670` skips `setpwm`, drives pin1 PWDN low). **VERIFY on hardware**
— if the OV5640 module needs an external XCLK, open it OV7670-style with a real PWM pin
instead (the OV5640 wants ~24 MHz XCLK, above the 12 MHz we feed the OV7670, so the PWM
frequency would change).

### 2.6 CAPTURE (RGB565 QVGA) — mostly reuse
The simple CAPTURE and its display path already branch on `CameraType != CAM_OV7670`
to skip the OV7670 RGB-format writes and the column-0 HREF-junk fix. Add `CAM_OV5640`
wherever `CAM_OV2640` is treated as "already RGB565 from its init". Confirm the OV5640's
RGB565 byte order matches `capture()`'s high-byte-first assumption (byte-swap in the
init via `0x4300`/`0x501F` format-control if reversed).

### 2.7 CHANGE (motion) — reuse unchanged
CHANGE already stays in RGB565 preview for the non-OV7670 path and uses the high RGB565
byte as the brightness proxy. It should work for the OV5640 with no change beyond
including `CAM_OV5640` in the "stay in RGB565" branch. No display needed for the core
measurement (recent OPEN/CAPTURE decoupling already covers this).

---

## 3. JPEG capture — the 153600-byte problem

The OV5640 has a hardware JPEG encoder. The flow mirrors `CAMERA CAPTURE JPEG` for the
OV2640: switch to JPEG mode at the chosen resolution, bit-bang the variable-length byte
stream into fast SRAM (`WriteBuf` on HDMI mode-4 = 153600, else `GetSystemMemory`),
stop at EOI (`FF D9`) / frame-end / `maxlen`, trim SOI..EOI, write to file, then reload
the QVGA preview.

### 3.1 Format / quality registers (OV5640)
- `0x4300` / `0x501F` — output format select (set JPEG mode).
- `0x4407[5:0]` — **JPEG quantization scale (QS)** — the quality knob. Lower = higher
  quality / bigger file (same polarity as the OV2640's QS at 0x44).
- `0x460B`, `0x460C`, `0x4837`, `0x3035/0x3036` (PLL) — DVP/PCLK timing; must be set so
  the bit-banged read keeps up (see §3.3).

### 3.2 Resolution vs the 153600 cap
The JPEG must fit **153600 bytes** of fast SRAM. JPEG size ≈ pixels × detail ÷
compression, so higher resolution needs a higher QS (more compression) to fit. Plan a
per-resolution `qsbase` (best quality that reliably fits) × `qsmult` (HIGH/MEDIUM/LOW),
exactly like the OV2640 command, and **enforce the cap**: if `capture_jpeg` hits
`maxlen` without seeing EOI, the frame overflowed — free/close and
`error("Image too detailed for the buffer - lower the resolution or quality")` rather
than writing a truncated file.

Candidate resolution ladder (to calibrate empirically on hardware — these are starting
guesses for what fits 153600):

| Keyword | Pixels | Likely status in 153600 | Default quality |
|---------|--------|--------------------------|-----------------|
| SVGA 800×600 | 0.5 MP | fits easily | high |
| XGA 1024×768 | 0.8 MP | fits | high/med |
| SXGA 1280×960 | 1.2 MP | fits | medium |
| UXGA 1600×1200 | 1.9 MP | fits at medium | medium |
| 1080P 1920×1080 | 2.1 MP | fits at medium/low | medium |
| QXGA 2048×1536 | 3.1 MP | borderline — low quality | low |
| QSXGA 2592×1944 | 5.0 MP | only at aggressive compression | low |

"Maximum possible size given 153600 + judicious quality" ⇒ default to the **highest
resolution that reliably fits on a normal scene** (likely UXGA/1080P at medium), expose
up to QSXGA for users who accept low quality / risk of the overflow error, and always
enforce the 153600 cap. Calibrate the table with real captures (like we did for the
OV2640: XGA≈50 KB, UXGA≈2⁄3 of the buffer).

### 3.3 Bit-bang timing (the recurring trap)
JPEG is byte-fragile: one dropped bit-bang byte corrupts the whole stream. The OV5640's
default PCLK is far faster than the OV2640's, so the DVP PCLK **must** be slowed via the
PLL/PCLK-divider registers until the IRQ-disabled poll loop keeps up at the target CPU
speed (same lesson as the OV2640's `R_DVP_SP`=0xD3 and the OV7670 CLKRC work). This is
the #1 thing to tune on real hardware; start slow and speed up.

---

## 4. Phasing

1. **Phase 1 — plumbing + preview:** `CAM_OV5640`, 16-bit SCCB helpers, table type +
   loader, `CAMERA OPEN OV5640`, reset + chip-ID, `OV5640_init` + `OV5640_QVGA_RGB565`.
   Success = a live QVGA RGB565 preview via `CAMERA CAPTURE`.
2. **Phase 2 — change detection:** confirm `CAMERA CHANGE` works (should be free once
   preview works). Fix RGB565 byte order / colour if needed.
3. **Phase 3 — JPEG:** JPEG-mode tables + resolution ladder + QS quality + 153600 cap +
   overflow error. Calibrate the resolution/quality table on real scenes.
4. **Phase 4 — tuning:** slow the PCLK for byte-perfect JPEG; colour/saturation/exposure
   tuning (reuse the OV2640 tuning-guide approach, but OV5640 registers).

---

## 5. RP2040 / flash-budget gating

Gate everything OV5640 behind `#ifdef rp2350` exactly like the OV2640 (tables, SCCB16
helpers, loader, OPEN branch, JPEG branch), keeping only the `OV5640` keyword detection +
an `#ifndef rp2350 error("OV5640 is only supported on RP2350")` ungated so the command
gives a clean message on RP2040. **Flash note:** the OV5640 base init is large (hundreds
of 16-bit entries); watch the RP2350 flash budget and trim the init to the registers we
actually need (drop AF firmware, unused test/colour-bar/scaling blocks).

---

## 6. Open questions to resolve when the module arrives

1. **Self-clocked or needs XCLK?** (Determines pin setup — reuse OV2640 PWDN branch vs
   OV7670 PWM branch. OV5640 typically wants ~24 MHz XCLK.)
2. **Fixed-focus or VCM lens?** (We skip AF firmware; is default focus usable?)
3. **RGB565 byte order** vs `capture()` (byte-swap in format-control if reversed).
4. **Which init source** (esp32-camera / Linux driver / ArduCAM) gives correct colour
   with the least register count.
5. **Slowest-necessary PCLK** for byte-perfect JPEG bit-bang at the build's CPU speed.
6. **Real resolution/quality-vs-153600 calibration** for the ladder in §3.2.
