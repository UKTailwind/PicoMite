# TinyUSB 0.21 patches for PicoMite

PicoMite's USB **host** support — fast USB flash-drive transfers, and reliable
enumeration of several devices (keyboards, mouse, touch, flash drive) behind a
hub — is built on **TinyUSB 0.21.0** with the patches in this directory applied.

The build expects the patched tree in a **sibling directory** `../tinyusb-0.21`
(next to the PicoMite repo, *not* inside it), selected by `PICO_TINYUSB_PATH` in
[`../CMakeLists.txt`](../CMakeLists.txt). The Pico SDK's own bundled TinyUSB is
left untouched.

## Create the tree

Run once, from the PicoMite repo, before the first build:

```
tinyusb-patches/setup-tinyusb.sh      # Linux / macOS / Git Bash
tinyusb-patches\setup-tinyusb.bat     # Windows (runs the .sh via Git Bash)
```

It clones TinyUSB **0.21.0** into `../tinyusb-0.21` and applies the three
patches. To recreate the tree, delete `../tinyusb-0.21` and run it again.
(A shallow clone is used; TinyUSB's `lib/` submodules are not needed for the
PicoMite build.)

## The patches

Each is a one-file diff against **stock** TinyUSB 0.21.0 (`patch -p1`), so the
apply order does not matter.

| Patch | File | What it changes |
|-------|------|-----------------|
| `hcd_rp2040.patch` | `src/portable/raspberrypi/rp2040/hcd_rp2040.c` | RP2 host driver: clear the EPX buffer on an RX-timeout, plus a bounded EP0 RX-timeout **grace period** so a spurious shared-latch timeout does not abandon a device mid-enumeration. Also: `ERROR_DATA_SEQ` recovers instead of `panic()`; the RP2040 SOF round-robin never preempts a transfer that has already moved data; `PM_FORCE_EPX_SOF` build switch. |
| `rp2040_usb.patch` | `src/portable/raspberrypi/rp2040/rp2040_usb.c` | Single-buffer host **control** transfers (long HID report descriptors otherwise panic); the two `buf_ctrl already available` `panic()`s clear the stale arming and continue; the shared USB fault record; optional timing-neutral event ring (`PC3_USB_EVLOG`, off by default). |
| `usbh.patch` | `src/host/usbh.c` | 100 ms reset-recovery for slow devices behind a hub; **enumeration-exclusive** control dispatch (application control traffic waits while a device is enumerating); hub-port disable on a failed enumeration. |

## No `panic()` in the host path

Stock 0.21.0 answers three recoverable bus conditions with `panic()`:
`ERROR_DATA_SEQ` in `hcd_rp2040_irq()`, and a still-armed `AVAIL` in
`bufctrl_write32()` / `bufctrl_write16()`. On a PicoMite that is both fatal and
**invisible** - pico-stdio is not routed to the console, so the panic text is
discarded and the breakpoint in the SDK's `_exit()` surfaces only as MMBasic's
own `*** FAULT PC=...` line (on RP2040, with `CFSR`/`HFSR` printing as zero
because M0+ has no such registers). All three now recover and record into
`pm_usb_fault_code` / `pm_usb_fault_count`; `USB_fault_service()`
(`input/USBKeyboard.c`, called from the main loop) prints e.g. `[USB DATA_SEQ x3]`,
capped at five lines.

## Exercising the RP2040 EPX path on RP2350 hardware

`HAS_STOP_EPX_ON_NAK` is RP2350-only, so RP2040 takes the `#else` SOF
round-robin path - which preempts a live transfer with `STOP_TRANS` and then
reconstructs the data toggle by hand in `epx_save_context()`. That path never
runs on an RP2350, so none of the RP2350 testing covered it.

Configure with `-DFORCE_EPX_SOF=ON` to suppress `HAS_STOP_EPX_ON_NAK` and make
an RP2350 build take the RP2040 path (both registers it touches, `NAK_POLL` and
`SOF_RD`, exist on RP2350). Diagnostic only - never ship a release with it on.

All three touch only the RP2 USB **host** path; the device (CDC console) stack
is stock 0.21.0. The [`docs/usb-host-hardening.html`](../docs/usb-host-hardening.html)
guide explains the host-driver fixes and the underlying RP2 shared-handshake-latch
race ([TinyUSB #3533](https://github.com/hathach/tinyusb/issues/3533)) in depth.
