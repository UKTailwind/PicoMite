# TinyUSB patches for PicoMite

PicoMite's USB **host** support — fast USB flash-drive transfers, and reliable
enumeration of several devices (keyboards, mouse, gamepad, flash drive) behind
a hub — is built on **TinyUSB master at efab607b8** (2026-10-11) with the
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

It fetches TinyUSB master at efab607b8 into `../tinyusb-master` and applies the
three patches. To recreate the tree, delete `../tinyusb-master` and run it again.
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

Master e20482387 ([#4166](https://github.com/hathach/tinyusb/pull/4166)) is
the answer to [#3874](https://github.com/hathach/tinyusb/issues/3874). An EPX
transfer that ends in a STALL or an RX timeout now releases its buffer and any
pending `BUFF_STATUS`, and `hcd_edpt_abort_xfer()` (a stub until then) stops
the transfer, so #3862's control watchdog really cancels it. Our own fix for
#3874, which cleared the buffer on the RX-timeout path only, was dropped from
`hcd_rp2040.patch` there. That commit changes nothing else in the build: the
other source changes since e42fa9357 are in the dwc2 host driver and the audio
device class, neither of which PicoMite compiles. Tested on PicoMiteHDMIBTH
(PicoComputer 3, same four devices, 2026-10-08): all four on every one of 5
`CPU RESTART`s, 4 reset-button and 4 power-on resets, and flash-drive
hot-plug 5 of 5 with nothing else disturbed. Plugging in the Raspberry Pi
keyboard (it has its own hub) makes a bus-powered device on the PC3's hub drop
and re-enumerate: the gamepad, or the flash drive once the gamepad was swapped
for a battery-powered PS4 controller. b8 (e42fa9357) does the same, 5 of 5, so
it is not this change; it looks like the keyboard's inrush current.

Master 3174e7603 ([#4177](https://github.com/hathach/tinyusb/pull/4177)) is
the answer to [#4173](https://github.com/hathach/tinyusb/issues/4173).
`hcd_device_close()` now clears a freed interrupt slot's `interrupt_num`, so a
completion can no longer reach a slot reused by another endpoint (a stale
number let a newly plugged mouse steal a working keyboard's completions), and
`hcd_edpt_abort_xfer()` now aborts interrupt endpoints too. Our own fix, which
cleared the number on close and when a slot was reopened for EPX and skipped
free slots in `handle_buf_status_isr()`, was dropped from `hcd_rp2040.patch`
there, and the RP2040 preempt guard moved into the new `epx_sof_round_robin()`.
The other source changes since e20482387 are in the nrf5x and Renesas drivers,
which PicoMite does not compile. Tested on PicoMiteHDMIBTH (PicoComputer 3,
same four devices, 2026-10-11): all four on every one of 5 `CPU RESTART`s,
4 reset-button and 4 power-on resets; flash-drive hot-plug and unplugging and
replugging the mouse with the keyboard in use, nothing else disturbed. The
Raspberry Pi keyboard still drops the gamepad, as above.

Master efab607b8 ([#3776](https://github.com/hathach/tinyusb/pull/3776)) adds
a change to `src/host/hub.c`:
- **Port status:** the hub's port status now stays in its `ctrl_buf`, and the
  completion callback travels with the transfer. A hub closed while its
  requests are failing can no longer lose a later attach.
- **Enumeration:** enumeration fails, and is retried, when a port status read
  fails, instead of decoding stale data.
- **Stuck status poll:** after three failed status-change polls the hub reads
  its port statuses directly. This cannot happen on the RP2 host, where an
  interrupt endpoint only ever completes successfully (a timeout or STALL is
  charged to the transfer on EPX).

No patch changes there. Tested on PicoMiteHDMIBTH (PicoComputer 3, same four
devices, 2026-10-11): all four on every one of 5 `CPU RESTART`s, and
reset-button and power-on resets, flash-drive hot-plug and the mouse replug
all clean.

The TinyUSB 0.21.0 patches, `usbh.patch` included, are in this directory's
git history.

## The patches

Each is a diff against **stock** TinyUSB master efab607b8 (`patch -p1`) of the
files in its row, so the apply order does not matter. None of these issues is
fixed upstream at that commit.

| Patch | File | What it changes |
|-------|------|-----------------|
| `hcd_rp2040.patch` | `src/portable/raspberrypi/rp2040/hcd_rp2040.c` | RP2 host driver: a 1 s endpoint-0 RX-timeout **grace period** (`PC3_CTRL_RX_TIMEOUT_GRACE_US`) in which the controller retries instead of failing the request, so a spurious shared-latch timeout does not fail an enumeration; the window closes when the control transfer ends, whether it completes or is aborted or closed (`epx_retire()`). An EPX transfer that **fails** (RX timeout, STALL) gives back the data toggle of each buffer that never completed (`epx_unwind_buffers()`, the undo `epx_save_context()` already did for preemption); left advanced, the next transfer on that endpoint expected the other toggle and its first packet was dropped as a repeat (`DATA_SEQ`) - seen as COM3 echoes lost while typing on a low-speed USB keyboard, whose transactions raise the RX timeout charged to the CDC receive. The **endpoint pool** has 32 entries instead of `USB_MAX_ENDPOINTS` (16, of which 15 are usable): control and bulk endpoints are software state sharing EPX and only interrupt endpoints need one of the controller's 15 slots, but every endpoint takes an entry, and a hub, a CDC adapter, a keyboard with a media-keys interface, a two-interface mouse and a flash drive took all 15, leaving the next device's driver without one (the Colour Maximite 2's DWC2 host ran out the same way). An endpoint that still cannot be opened is recorded as `EP_FULL` instead of failing an `assert`. A **thread-context `hcd_int_handler()`** call (`tuh_int_handler(0, false)`, which MMBasic's C: drive code makes while it waits for a transfer) runs with interrupts masked: the USB interrupt could run the same handler in the middle of it, and both switched EPX on one stop-on-NAK, re-arming EPX for another endpoint while the controller was already running the first switch's transaction. With a USB serial adapter receiving, a flash drive's data packet landed in the adapter's buffer (`DATA_SEQ`, corrupt reads), or a write lost a packet and the drive then NAKed its status for good ("A hard error occurred in the low level disk I/O layer"). 0.20 never switched EPX in mid-transfer, so the same polling did no harm there. Also: `ERROR_DATA_SEQ` is recorded instead of `panic()`; the RP2040 SOF round-robin never preempts a transfer that has already moved data (`PM_EPX_PREEMPT_UNGUARDED` restores the stock behaviour, for diagnosis); `PM_FORCE_EPX_SOF` build switch; `pm_epx_*` counters; `PC3_USB_EVLOG` hooks. |
| `rp2040_usb.patch` | `src/portable/raspberrypi/rp2040/rp2040_usb.c` | Host **control** transfers single-buffered on the RP2350 ([#3875](https://github.com/hathach/tinyusb/issues/3875): a 9-packet control IN double-buffered on EPX completed after its first packet), but not on the RP2040, where erratum E4 makes single-buffered multi-packet host transfers unsafe; the two `buf_ctrl already available` `panic()`s clear the stale arming and continue; the shared USB fault record; optional timing-neutral event ring (`PC3_USB_EVLOG`, off by default). |
| `cdc_host.patch` | `src/class/cdc/cdc_host.c`, `src/class/cdc/cdc_host.h` | CDC host: a bulk IN that ends `XFER_RESULT_FAILED` is **re-armed**. Stock returns without re-arming, so one failed IN stops a COM3-COM6 port receiving until the device re-enumerates. On the RP2 host the failure need not be the CDC endpoint's: `SIE_STATUS.RX_TIMEOUT` is shared with the interrupt endpoints the controller polls in the background (a keyboard behind the hub), and `hcd_rp2040` charges a timeout to whatever transfer holds EPX, nearly always the CDC IN waiting on an idle device. Found by reading the code while investigating a COM3 report (2026-10-10); not yet seen to fire. A STALL is not re-armed. **Receive on request:** stock arms each adapter's bulk IN at mount and keeps it armed, so an adapter that nothing reads holds a transfer pending on EPX for as long as it is plugged in, and takes a turn after every NAK of another transfer (stop-on-NAK switches EPX while anything else is pending). The new `tuh_cdc_rx_enable(idx, on)` (declared in `cdc_host.h`) arms the IN only while the application receives - MMBasic turns it on at `OPEN "COMn:"` and off at `CLOSE` - and turning it off aborts the IN, which gives back its data toggle, and empties the receive FIFO. |

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
prints e.g. `[USB DATA_SEQ 3 BUFCTRL32 2]`, capped at five lines. The same record
has `EP_FULL`: an endpoint `hcd_edpt_open()` could not open, for want of a pool
entry or an interrupt slot.

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
