# OV2640 Camera — Colour, Saturation & Contrast Tuning Guide

This guide describes the OV2640 registers that control **colour, saturation,
contrast, brightness, white balance and special effects**, and how to adjust
them from MMBasic with the `CAMERA REGISTER` command so you can tune the picture
to your liking.

It applies to `CAMERA OPEN OV2640 ...` on RP2350 builds (HDMI 640×480 mode 4, or
an ILI9341-class SPI panel).

---

## 1. How `CAMERA REGISTER` works

```basic
CAMERA REGISTER addr, value
```

writes one byte `value` to sensor register `addr` over the SCCB (I²C-like) bus.
Both arguments are normal MMBasic numbers — hexadecimal (`&Hxx`) is easiest.

Two things you **must** understand before tuning:

### 1.1 Register banks

The OV2640 has two register banks, selected by register `0xFF`:

| Write | Selects | Holds |
|-------|---------|-------|
| `CAMERA REGISTER &HFF, &H01` | **Sensor bank** | exposure, gain, auto-exposure target, sensor clock |
| `CAMERA REGISTER &HFF, &H00` | **DSP bank** | colour, saturation, contrast, brightness, white balance, effects |

**Always set the bank first**, then write the registers in that bank. Colour
tuning lives almost entirely in the **DSP bank (`&H00`)**.

### 1.2 Live vs permanent

- `CAMERA REGISTER` changes the **running preview only**. They are great for
  experimenting because you see the effect immediately on screen.
- They are **lost on the next `CAMERA OPEN`**, and **`CAMERA CAPTURE JPEG`
  reloads its own init table**, so a live poke does *not* affect the saved JPEG.
- To make a setting **permanent** (and to affect JPEG captures), bake it into the
  firmware register tables `OV2640_QVGA` (preview) and `OV2640_JPEG_INIT` (JPEG)
  in `I2C.c`. Once you find values you like with `CAMERA REGISTER`, put them
  there and rebuild.

---

## 2. The SDE block (saturation / contrast / brightness / effects)

Saturation, contrast, brightness and the colour special-effects all live in the
OV2640's **Special Digital Effects (SDE)** block, which is accessed *indirectly*
through two DSP-bank registers:

- **`0x7C`** — sets the SDE **sub-address** (which SDE parameter you're pointing at)
- **`0x7D`** — writes a **value** to the current sub-address, then **auto-increments**
  the sub-address by one

So a sequence like "point at sub-address 3, write two values" is:

```basic
CAMERA REGISTER &HFF, &H00   ' DSP bank
CAMERA REGISTER &H7C, &H03   ' point at SDE sub-address 0x03
CAMERA REGISTER &H7D, &H68   ' -> writes sub 0x03, pointer now 0x04
CAMERA REGISTER &H7D, &H68   ' -> writes sub 0x04
```

### 2.1 The SDE control byte (sub-address 0x00) — THE MASTER SWITCH

Sub-address `0x00` is an **enable mask**. None of the saturation/contrast/
brightness values do anything unless the matching bit is set here:

| Bit | Value | Enables |
|-----|-------|---------|
| 1 | `0x02` | **Saturation** (UV adjust) |
| 2 | `0x04` | **Contrast & brightness** |
| 3-4 | `0x18` | **Fixed hue** (used by B&W / sepia / colour-tint effects) |
| 6 | `0x40` | **Negative** |

Combine bits by OR-ing them. The most common "normal colour, fully tunable"
value is **`0x06`** (saturation + contrast/brightness both on):

```basic
CAMERA REGISTER &HFF, &H00
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H06   ' enable saturation AND contrast/brightness
```

> If you only enable saturation (`0x02`) the contrast/brightness values are
> ignored, and vice-versa. Writing `0x00` here disables all SDE adjustment
> (the picture falls back to the raw, low-saturation default).

---

## 3. Saturation  *(fixes "reds look pink")*

Saturation is SDE **sub-address `0x03`** (two bytes: U gain, then V gain). Higher
= more vivid colour. This is the main control for washed-out / pink colours.

| Level | U / V value |
|-------|-------------|
| −2 (palest) | `0x28` |
| −1 | `0x38` |
| 0 (default) | `0x48` |
| +1 | `0x58` |
| **+2 (most vivid)** | **`0x68`** |

```basic
CAMERA REGISTER &HFF, &H00   ' DSP bank
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H02   ' enable saturation
CAMERA REGISTER &H7C, &H03
CAMERA REGISTER &H7D, &H68   ' U gain  (+2)
CAMERA REGISTER &H7D, &H68   ' V gain  (+2)
```

For *even more* punch than +2 you can go beyond the table — `0x78` or `0x80` —
at the risk of looking garish or clipping. **The firmware default is now `0x68`
(+2).**

---

## 4. Brightness

Brightness is SDE **sub-address `0x09`** (one value, plus a trailing `0x00`).
Requires the contrast/brightness enable bit (`0x04`) in the control byte.

| Level | Value |
|-------|-------|
| −2 (darkest) | `0x00` |
| −1 | `0x10` |
| 0 (default) | `0x20` |
| +1 | `0x30` |
| +2 (brightest) | `0x40` |

```basic
CAMERA REGISTER &HFF, &H00
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H04   ' enable contrast/brightness
CAMERA REGISTER &H7C, &H09
CAMERA REGISTER &H7D, &H10   ' brightness -1 (a bit darker)
CAMERA REGISTER &H7D, &H00   ' trailing byte (always 0x00)
```

> This is a *post-processing* brightness offset. For overall exposure it is
> usually better to use the auto-exposure target (Section 7), which actually
> changes how much light the sensor collects.

---

## 5. Contrast

Contrast is SDE **sub-address `0x07`** (four bytes: `0x20`, then two level bytes,
then `0x06`). Requires the contrast/brightness enable bit (`0x04`).

| Level | Bytes after 0x20 |
|-------|------------------|
| −2 (flattest) | `0x18, 0x34` |
| −1 | `0x1C, 0x2A` |
| 0 (default) | `0x20, 0x20` |
| +1 | `0x24, 0x16` |
| +2 (punchiest) | `0x28, 0x0C` |

```basic
CAMERA REGISTER &HFF, &H00
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H04   ' enable contrast/brightness
CAMERA REGISTER &H7C, &H07
CAMERA REGISTER &H7D, &H20   ' fixed
CAMERA REGISTER &H7D, &H24   ' \ contrast +1
CAMERA REGISTER &H7D, &H16   ' /
CAMERA REGISTER &H7D, &H06   ' fixed
```

Raising contrast also makes colours look more saturated and cuts a hazy /
washed-out look.

---

## 6. Special effects (B&W, sepia, colour tints, negative)

Special effects are SDE **sub-address `0x05`** (two bytes: fixed U, fixed V),
combined with the matching control byte. They override normal colour.

| Effect | Control byte (sub 0x00) | U / V (sub 0x05) |
|--------|--------------------------|------------------|
| Normal colour | `0x00` | `0x80, 0x80` |
| Black & white | `0x18` | `0x80, 0x80` |
| Sepia / reddish | `0x18` | `0x40, 0xC0` |
| Greenish | `0x18` | `0x40, 0x40` |
| Bluish | `0x18` | `0xA0, 0x40` |
| Retro | `0x18` | `0x40, 0xA6` |
| Negative | `0x40` | `0x80, 0x80` |

Example — black & white:

```basic
CAMERA REGISTER &HFF, &H00
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H18   ' fixed-hue enable
CAMERA REGISTER &H7C, &H05
CAMERA REGISTER &H7D, &H80   ' U
CAMERA REGISTER &H7D, &H80   ' V
```

> To go back to normal colour, set the control byte back to `0x06` (saturation +
> contrast) — *not* `0x00`, or you lose your saturation/contrast settings too.

---

## 7. Exposure / "too bright"  (sensor bank)

If the whole picture is too bright or too dark, adjust the **auto-exposure
target** — these are in the **sensor bank (`&H01`)**, not the DSP bank:

| Reg | Name | Meaning |
|-----|------|---------|
| `0x24` | AEW | auto-exposure target, upper threshold |
| `0x25` | AEB | auto-exposure target, lower threshold |
| `0x26` | VV | fast-mode region (leave as is) |

Lower `0x24`/`0x25` → darker image; raise → brighter. Keep `0x24` a little above
`0x25`. The firmware default is `0x28` / `0x20`.

```basic
CAMERA REGISTER &HFF, &H01   ' SENSOR bank
CAMERA REGISTER &H24, &H20   ' darker target
CAMERA REGISTER &H25, &H18
```

Related sensor-bank controls:

| Reg | Name | Notes |
|-----|------|-------|
| `0x13` | COM8 | bit0 = auto-exposure on, bit2 = auto-gain on. `0xE5` = both on. Clear bit2 to cap gain noise. |
| `0x14` | COM9 | bits 7:5 = auto-gain ceiling (how far gain may rise in dim light). |

---

## 8. White balance / colour matrix  (DSP bank)

The colour matrix (CMX) and auto white balance (AWB) are enabled in **CTRL1
(`0xC3`)** in the DSP bank:

| Bit | Value | Enables |
|-----|-------|---------|
| 7 | `0x80` | Colour matrix (CMX) |
| 6 | `0x40` | Auto white balance |
| 5 | `0x20` | AWB gain |

`0xED` turns the whole colour pipeline on (this is the firmware default in the
JPEG init). If you ever see a strong, uniform colour cast, check this is set:

```basic
CAMERA REGISTER &HFF, &H00
CAMERA REGISTER &HC3, &HED
```

---

## 9. Worked example — "reds look pink, picture a touch bright"

This is the exact recipe used to cure pink reds, run live on the preview:

```basic
CAMERA REGISTER &HFF, &H00   ' DSP bank
CAMERA REGISTER &HC3, &HED   ' colour matrix + AWB on
CAMERA REGISTER &H7C, &H00
CAMERA REGISTER &H7D, &H06   ' enable saturation + contrast
CAMERA REGISTER &H7C, &H03
CAMERA REGISTER &H7D, &H68   ' saturation U  (+2, vivid)
CAMERA REGISTER &H7D, &H68   ' saturation V  (+2)
CAMERA REGISTER &HFF, &H01   ' SENSOR bank
CAMERA REGISTER &H24, &H28   ' \ slightly darker exposure
CAMERA REGISTER &H25, &H20   ' /
```

If reds are still pink, raise the two saturation bytes (`0x68` → `0x78`). If the
image is too dark, raise `0x24`/`0x25` back toward `0x30`/`0x28`.

When you're happy, copy the final values into `OV2640_QVGA` and
`OV2640_JPEG_INIT` in `I2C.c` and rebuild so they apply to every capture
(including JPEGs).

---

## 10. Quick reference

| Want to change | Bank | Register(s) |
|----------------|------|-------------|
| Master enable (sat/contrast) | DSP `&H00` | `0x7C`=`0x00`, `0x7D`=`0x06` |
| Saturation | DSP `&H00` | `0x7C`=`0x03`, `0x7D`=U, `0x7D`=V (`0x28`…`0x68`) |
| Brightness | DSP `&H00` | `0x7C`=`0x09`, `0x7D`=val (`0x00`…`0x40`), `0x7D`=`0x00` |
| Contrast | DSP `&H00` | `0x7C`=`0x07`, `0x7D`=`0x20`,a,b,`0x06` |
| Special effect | DSP `&H00` | ctrl `0x18`/`0x40`, `0x7C`=`0x05`, `0x7D`=U,V |
| White balance / matrix | DSP `&H00` | `0xC3`=`0xED` |
| Exposure (bright/dark) | Sensor `&H01` | `0x24`, `0x25` |
| Auto exp/gain on-off | Sensor `&H01` | `0x13` |

All values are written most-significant first with `CAMERA REGISTER addr, value`;
set the bank with `CAMERA REGISTER &HFF, &H00` (DSP) or `&H01` (sensor) before
each group.
