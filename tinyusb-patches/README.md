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
| `hcd_rp2040.patch` | `src/portable/raspberrypi/rp2040/hcd_rp2040.c` | RP2 host driver: clear the EPX buffer on an RX-timeout, plus a bounded EP0 RX-timeout **grace period** so a spurious shared-latch timeout does not abandon a device mid-enumeration. |
| `rp2040_usb.patch` | `src/portable/raspberrypi/rp2040/rp2040_usb.c` | Single-buffer host **control** transfers (long HID report descriptors otherwise panic); optional timing-neutral event ring (`PC3_USB_EVLOG`, off by default). |
| `usbh.patch` | `src/host/usbh.c` | 100 ms reset-recovery for slow devices behind a hub; **enumeration-exclusive** control dispatch (application control traffic waits while a device is enumerating); hub-port disable on a failed enumeration. |

All three touch only the RP2 USB **host** path; the device (CDC console) stack
is stock 0.21.0. The [`docs/usb-host-hardening.html`](../docs/usb-host-hardening.html)
guide explains the host-driver fixes and the underlying RP2 shared-handshake-latch
race ([TinyUSB #3533](https://github.com/hathach/tinyusb/issues/3533)) in depth.
