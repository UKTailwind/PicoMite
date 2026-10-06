# Bluetooth speaker output for the BT host builds — implementation plan

Branch `bt-audio` (worktree `../pm_bta2dp`), based on `development` at dd6a312.
Builds affected: PICOBTHRP2350 (`PICOMITEBTH`) and HDMIBTH (`PICOMITEHDMIBTH`).
Test board: RP2350B (Pimoroni Pico Plus 2 W class, 16 MB flash) on COM21, USB serial 34E6C028FA17500D.

## Goal

Play MMBasic audio (PLAY TONE, SOUND, WAV, FLAC, MP3, MOD, ARRAY, BBC SOUND) through a Bluetooth speaker or headphones, while a BLE keyboard/mouse keeps working exactly as it does now.

Bluetooth speakers use A2DP, a Bluetooth Classic profile. Both BTH builds are BLE-only today, so the work starts by making BTstack dual-mode. The reference is BTstack's `example/a2dp_source_demo.c`, which pico-examples builds as `pico_w/bt/a2dp_source_demo`.

## Budget (measured before starting)

| | PICOBTHRP2350 | HDMIBTH |
|---|---|---|
| Flash margin, development dd6a312 | +25.3 KB | +25.5 KB (5 Oct build) |
| … after removing VS1053 (d122987) | **+59.8 KB** | not built yet |
| RAM margin | +4.1 KB → +4.9 KB | +4.6 KB |

Costs measured by compiling BTstack in isolation with gc-sections:
- Classic + L2CAP + SDP + AVDTP + A2DP source + SBC encoder: about +53 KB flash.
- AVRCP (optional): about +16 KB flash.
- Static RAM: about +9 KB. The largest items are the HCI connection pool (1×768 → 2×2348 B), the CYW43 receive buffer, the SDP response buffer and the A2DP endpoint-discovery table.
- SBC encoder working state (about 4 KB): taken from the MMBasic heap when a speaker connects, not static.

Consequences:
- RAM: `HEAP_MEMORY_SIZE` will have to drop by about 8 KB on each build.
- Flash: removing VS1053 makes room for the Classic stack on PICOBTH without moving `FLASH_TARGET_OFFSET`. HDMIBTH has the same VS1053 saving.

## Proposed BASIC syntax (provisional — Peter to approve)

```
BLUETOOTH SCAN [seconds]        ' list nearby Classic devices; audio devices marked
BLUETOOTH CONNECT addr$         ' pair/connect to a speaker ("AA:BB:CC:DD:EE:FF")
BLUETOOTH CONNECT               ' reconnect the last speaker used
BLUETOOTH DISCONNECT
BLUETOOTH STATUS                ' keyboard and speaker state, stream format, encoder load
BLUETOOTH FORGET                ' delete all stored pairings (keyboard and speaker)
OPTION AUDIO BLUETOOTH          ' PLAY output goes to the connected speaker (phase 2)
MM.INFO(BLUETOOTH ...)          ' state for programs (phase 3)
```

`BLUETOOTH` is a new command; it is appended at the end of the command table, so no existing token number changes. No new function tokens are used.

## Phases

### Phase 1 — dual-mode stack and a test tone (no PLAY integration)

1. **Done (d122987): remove VS1053 from the BTH builds.** New `NOVS1053` define, set from CMake for PICOMIN and both BTH builds. VS1053-only gates move from `PICOMITEMIN` to `NOVS1053`; the SPI DAC stays. Flashed to COM21 and checked: `OPTION AUDIO VS1053` gives Invalid syntax, and `MM.INFO(OPTION AUDIO)` still works.
2. **Done (0d77864): fix where BTstack keeps its pairing data.** The SDK's `btstack_cyw43_init()` sets up a TLV store at 16 MB − 12 KB, which is inside the A: drive. When the bank header isn't valid it erases a 4 KB sector there on boot (confirmed on COM21: the "BTstack" magic is at 0x10FFD000). The fix wraps `pico_flash_bank_instance()` so the SDK uses the existing Option-backed `bt_tlv` store. BTKeyboard then uses that single TLV instance for the LE device DB and the Classic link keys. (Released BT and BTH builds on development have the same overlap — reported separately.)
3. **Done (484e747): dual-mode BTstack.** PICOBTH flash margin +5.0 KB; BSS +12.8 KB, so `HEAP_MEMORY_SIZE` 336 → 324 KB (RAM margin +4.0 KB).
   - `ENABLE_CLASSIC`, plus only the Classic sources A2DP needs (not the whole `pico_btstack_classic`), plus `pico_btstack_sbc_encoder`.
   - In `btstack_config.h`: 2 HCI connections; 1021-byte ACL payload; L2CAP/AVDTP pools; 4 link keys; controller ACL buffers capped at 3 and controller-to-host flow control (the SDK example's settings to avoid CYW43 shared-bus overrun).
   - Check: the build still pairs and uses a BLE keyboard.
4. **Done (484e747): changes to the keyboard path.**
   - Filter `HCI_EVENT_DISCONNECTION_COMPLETE` by the keyboard's connection handle. Without this, a speaker dropping out would tear down the keyboard session.
   - Register the Classic services before `hci_power_control(ON)`.
   - Lower the LE scan duty cycle (currently 100%) while audio is streaming.
5. **Done (484e747): `bluetooth/BTAudio.c`.** On COM21, `BLUETOOTH SCAN` finds a nearby audio device; connect and stream not yet tried.
   - A2DP source endpoint, SDP record, Classic name and pairing (SSP, Just Works).
   - `BLUETOOTH SCAN / CONNECT / DISCONNECT / STATUS`, plus a temporary `BLUETOOTH TEST freq` that streams an internal sine wave through the SBC encoder on BTstack's 10 ms timer, as the demo does.
6. **Next — measurements on COM21, with a speaker and a BLE keyboard near the board:**
   - Does the tone play cleanly while the keyboard types?
   - Encoder load: time spent in the timer, reported by STATUS.
   - Interpreter slowdown during streaming (a timed BASIC loop).
   - Behaviour across a speaker power-off and back on.

**Exit criterion for phase 1:** the stream is clean and the keyboard is unaffected. If the CYW43439 can't do both at once, stop here.

### Phase 2 — PLAY through the speaker

- Abstract the audio output in `Audio.c`. Today the PWM wrap interrupt clocks every output: PWM directly, I2S by keeping the FIFO topped up. The new output kinds are PWM / I2S / SPI DAC / BLUETOOTH.
- **Bluetooth output**: the A2DP 10 ms timer emulates the PWM sample tick at `AudioCurrentRate` (honouring `audiorepeat`). It pulls stereo pairs through `advance_swing_buffer()`, the same path the I2S output uses, with linear interpolation to a fixed 44.1 kHz SBC stream.
- Samples arrive in the signed-16-bit I2S format (`i2sconvert`), so volume and ramping behave as on I2S.
- `OPTION AUDIO BLUETOOTH`. It needs no pins and no PWM slice, so the "Audio not enabled" checks and the ~30 `pwm_set_irq0_enabled(AUDIO_SLICE, …)` start/stop sites are routed through the output abstraction.
- Silence while idle, and the stream suspends after N seconds idle so speakers can auto-off. PAUSE/STOP behave as on wired audio.
- Not available over Bluetooth: PLAY STREAM and MIDI. These were VS1053-only and are already removed.

### Phase 3 — usability and robustness

- Reconnect to the last speaker at boot, and accept the speaker reconnecting to us.
- Implement `BLUETOOTH FORGET` and `MM.INFO(BLUETOOTH …)`.
- Error messages for: speaker not found, pairing refused, no stream.
- Keyboard reconnect behaviour while streaming (scan duty; possibly controller auto-connect for the bonded keyboard).
- AVRCP (optional, about +16 KB): the speaker's volume and play/pause buttons.

### Phase 4 — HDMIBTH, documentation, release checks

- Build and test HDMIBTH (RAM and flash budget). Its scanout on core1 is unaffected; BTstack runs in the lowest-priority interrupt on core0.
- User manual, `docs/help.txt` (regenerate), OPTION LIST.
- Goldens on PICOBTH.

## Known limits to state in the manual

- **Latency:** 150–250 ms, set by the speaker's buffering and the codec. Music is fine; game sound effects will lag.
- **Gaps during flash writes:** audio stops while MMBasic writes flash (RUN from file, SAVE, AUTOSAVE, LIBRARY, OPTION changes), as wired audio does.
- **Keyboard wake-up:** a sleeping BLE keyboard reconnects a little more slowly while audio streams (lower scan duty).
- **CPU:** SBC encoding costs an estimated 4–8% of core0 at 252 MHz. Phase 1 replaces this with a measurement.
