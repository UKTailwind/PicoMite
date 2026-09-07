# PicoMite User Manual — applicability notation

One rule, adopted at V6.03.02 (phase 02 of the manual audit):

> **Underlining in a reference-entry description means applicability, and nothing else.**

Where an entry in **Commands**, **Functions**, **Options**, **Predefined Read Only
Variables** or **Obsolete Commands and Functions** is not available everywhere, the
restriction is written as an underlined label at the **start** of the description.
An entry with no label is available in all sixteen firmware images.

A label is one of two kinds, and an entry may carry one of each:

- a **version label** — which of the sixteen images provide the feature. Drawn from
  the closed list below; no other wording is used.
- a **condition** — a hardware or configuration requirement (PSRAM fitted, a touch
  panel configured, a buffered driver selected).

Both lists are reproduced in the manual itself as **Appendix K – Firmware Version
Applicability**, where every version label is spelled out as an explicit list of
firmware images. Appendix K is the definition; this file is the authoring rule.

## Version labels (closed list)

    RP2350 VERSIONS ONLY
    RP2350 VERSIONS EXCEPT WEBMITERP2350
    RP2350 PICOMITE VERSIONS ONLY
    RP2350 USB VERSIONS ONLY
    RP2350 NON-USB VERSIONS ONLY
    RP2350 VGA USB AND HDMI USB VERSIONS ONLY
    VGA VERSIONS ONLY
    HDMI VERSIONS ONLY
    VGA AND HDMI VERSIONS ONLY
    VGA AND ALL RP2350 VERSIONS ONLY
    VGA, HDMI AND RP2350 PICOMITE VERSIONS ONLY
    NOT VGA OR HDMI VERSIONS
    NOT VGA OR HDMIWEB VERSIONS
    WIFI VERSIONS ONLY
    NOT WIFI VERSIONS
    USB VERSIONS ONLY
    NON-USB VERSIONS ONLY
    USB AND BLUETOOTH KEYBOARD VERSIONS ONLY
    GUI CONTROLS VERSIONS ONLY
    PS2 KEYBOARD VERSIONS ONLY
    TRACE CACHE VERSIONS ONLY
    NOT PICOMITEMIN
    NOT PICOMITEMIN OR WEBMITE
    NOT RP2040 VGA
    NOT RP2040 VGA, RP2040 WEBMITE OR PICOMITEMIN

`NON-USB VERSIONS ONLY` and `PS2 KEYBOARD VERSIONS ONLY` are not the same set and
are easy to confuse. The PS/2 *pin* options (`OPTION KEYBOARD PINS`, `OPTION PS2
PINS`, `OPTION MOUSE`) are gated on `!USBKEYBOARD` and so are available on
PicoMiteRP2350BTH; the PS/2 *keyboard* options (`OPTION KEYBOARD <layout>`,
`OPTION KEYBOARD I2C`, `OPTION KEYBOARD DISABLE`) are gated on
`!USBKEYBOARD && !PICOMITEBTH` and are not.

## Conditions

    RP2350B VERSIONS ONLY
    RP2350 VERSIONS WITH PSRAM ENABLED ONLY
    WITH A BUFFERED DISPLAY DRIVER ONLY
    WITH TOUCH SUPPORT ONLY
    IN MODE 1 ONLY
    PICOCALC ONLY
    ONLY ON RGB332 (256 COLOUR) DISPLAYS

## Rules for authors

1. **Do not invent a label.** If a new restriction does not fit an existing label,
   add it to this file *and* to Appendix K in the same edit, with its explicit image
   list. The value of the notation is that the list is closed.
2. **Do not write applicability as ordinary prose.** "RP2350 only" typed into the
   middle of a sentence is invisible to both the reader scanning for it and to the
   checker. It goes at the head of the description, underlined.
3. **Never write "NOT x AND y".** It reads as the opposite of what it means. The
   manual previously carried both `NOT VGA OR HDMI VERSIONS` and
   `NOT VGA AND HDMI VERSIONS` for the same condition. Use `OR`.
4. **"WebMite" means the two WebMite images only** (WebMiteRP2040, WebMiteRP2350).
   PicoMiteHDMIWEB is a PicoMite. Where a feature follows the WiFi stack rather than
   the WebMite images — everything gated on `PICOMITEWEB` in the source — use
   `WIFI VERSIONS ONLY`, which includes PicoMiteHDMIWEB.
5. **Where one keyword behaves differently on different versions**, give it one
   labelled block per version family rather than one block with a compound label.
   `MAP` and `MODE` already do this correctly and are the model to follow.

## Why

Before this rule the manual expressed applicability in 46 distinct wordings across
~100 entries, including six spellings of "not the VGA or HDMI versions", while the
Options listing carried none at all. Nothing could check any of it, and several
labels had drifted out of agreement with the source. A closed list can be checked
mechanically against the per-variant token tables extracted from the firmware.
