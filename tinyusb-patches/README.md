# TinyUSB patches for PicoMite

PicoMite's USB **host** support — fast USB flash-drive transfers, and reliable
enumeration of several devices (keyboards, mouse, gamepad, flash drive) behind
a hub — is built on **TinyUSB master at e42fa9357** (2026-10-07) with the
patches in this directory applied. Every build variant uses the same tree,
including those that are only a USB device (the CDC console).

The build expects the patched tree in a **sibling directory** `../tinyusb-master`
(next to the PicoMite repo, *not* inside it), selected by `PICO_TINYUSB_PATH` in
[`../CMakeLists.txt`](../CMakeLists.txt). The Pico SDK's own bundled TinyUSB is
left untouched.

## Create the tree

Run once, from the PicoMite repo, before the first build:

```
tinyusb-patches/setup-tinyusb.sh      # Linux / macOS / Git Bash
tinyusb-patches\setup-tinyusb.bat     # Windows (runs the .sh via Git Bash)
```

It fetches TinyUSB master at e42fa9357 into `../tinyusb-master` and applies the
two patches. To recreate the tree, delete `../tinyusb-master` and run it again.
(Only that one commit is fetched; TinyUSB's `lib/` submodules are not needed
for the PicoMite build.) A `../tinyusb-0.21` tree from an earlier setup is no
longer used.

## Why master, not a release

TinyUSB 0.21.0 needed a third patch, to `src/host/usbh.c`, for devices behind
a hub that fail their first enumeration request: a 100 ms reset recovery
instead of 10 ms, the hub port disabled when an enumeration was abandoned,
and application control transfers held back while a device enumerated.

Master has [#3862](https://github.com/hathach/tinyusb/pull/3862) (430fcd222),
the answer to [#3876](https://github.com/hathach/tinyusb/issues/3876): a device
whose enumeration control transfer fails is torn down and enumerated again
from address 0 after a fresh port reset (`CFG_TUH_ENUM_ATTEMPT_MAX`, default
2), and a control transfer that never completes times out after
`CFG_TUH_CONTROL_TIMEOUT_MS` (5 s). That replaces the `usbh.c` patch, but not
the endpoint-0 RX-timeout grace in `hcd_rp2040.patch`. On a PicoComputer 3
with a keyboard, mouse, gamepad and flash drive behind its hub, master
without the grace enumerated only the gamepad and the flash drive; with it,
all four on every boot (PicoMiteHDMIUSB: 5 `CPU RESTART`s, 4 reset-button and
4 power-on resets, and hot-plugging; PicoMiteHDMIWEB, PicoMiteHDMIBTH and
PicoMiteRP2350VGAUSB: 4 of each).

The TinyUSB 0.21.0 patches, `usbh.patch` included, are in this directory's
git history.

## The patches

Each is a one-file diff against **stock** TinyUSB master e42fa9357
(`patch -p1`), so the apply order does not matter. None of these issues is
fixed upstream at that commit.

| Patch | File | What it changes |
|-------|------|-----------------|
| `hcd_rp2040.patch` | `src/portable/raspberrypi/rp2040/hcd_rp2040.c` | RP2 host driver: a 1 s endpoint-0 RX-timeout **grace period** (`PC3_CTRL_RX_TIMEOUT_GRACE_US`) in which the controller retries instead of failing the request, so a spurious shared-latch timeout does not fail an enumeration; on a real RX-timeout failure the EPX buffer control is cleared, so the retry does not find it still armed ([#3874](https://github.com/hathach/tinyusb/issues/3874)). Also: `ERROR_DATA_SEQ` is recorded instead of `panic()`; the RP2040 SOF round-robin never preempts a transfer that has already moved data (`PM_EPX_PREEMPT_UNGUARDED` restores the stock behaviour, for diagnosis); a freed or EPX endpoint slot drops its interrupt-endpoint number and completions are never delivered to a free slot (a stale number let a newly plugged mouse steal a working keyboard's completions); `PM_FORCE_EPX_SOF` build switch; `pm_epx_*` counters; `PC3_USB_EVLOG` hooks. |
| `rp2040_usb.patch` | `src/portable/raspberrypi/rp2040/rp2040_usb.c` | Host **control** transfers single-buffered on the RP2350 ([#3875](https://github.com/hathach/tinyusb/issues/3875): a 9-packet control IN double-buffered on EPX completed after its first packet), but not on the RP2040, where erratum E4 makes single-buffered multi-packet host transfers unsafe; the two `buf_ctrl already available` `panic()`s clear the stale arming and continue; the shared USB fault record; optional timing-neutral event ring (`PC3_USB_EVLOG`, off by default). |

## No `panic()` in the host path

Stock TinyUSB answers three recoverable bus conditions with `panic()`:
`ERROR_DATA_SEQ` in `hcd_rp2040_irq()`, and a still-armed `AVAIL` in
`bufctrl_write32()` / `bufctrl_write16()`. On a PicoMite that is both fatal and
**invisible** - pico-stdio is not routed to the console, so the panic text is
discarded and the breakpoint in the SDK's `_exit()` surfaces only as MMBasic's
own `*** FAULT PC=...` line (on RP2040, with `CFSR`/`HFSR` printing as zero
because M0+ has no such registers). All three now recover and record into
`pm_usb_fault_n[]`, `pm_usb_fault_ep[]` and `pm_usb_fault_total`;
`USB_fault_service()` (`input/USBKeyboard.c`, called from the main loop)
prints e.g. `[USB DATA_SEQ 3 BUFCTRL32 2]`, capped at five lines.

## Exercising the RP2040 EPX path on RP2350 hardware

`HAS_STOP_EPX_ON_NAK` is RP2350-only, so RP2040 takes the `#else` SOF
round-robin path - which preempts a live transfer with `STOP_TRANS` and then
reconstructs the data toggle by hand in `epx_save_context()`. That path never
runs on an RP2350, so none of the RP2350 testing covered it.

Configure with `-DFORCE_EPX_SOF=ON` to suppress `HAS_STOP_EPX_ON_NAK` and make
an RP2350 build take the RP2040 path (both registers it touches, `NAK_POLL` and
`SOF_RD`, exist on RP2350). Diagnostic only - never ship a release with it on.

Apart from the `buf_ctrl` panics in `rp2040_usb.c`, which the device driver
shares, the patches touch only the RP2 USB **host** path; the device (CDC
console) stack is stock master. The
[`docs/usb-host-hardening.html`](../docs/usb-host-hardening.html) guide explains
the host-driver fixes and the underlying RP2 shared-handshake-latch race
([TinyUSB #3533](https://github.com/hathach/tinyusb/issues/3533)) in depth.
