/*
 * FRANK Quest - board configuration
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CMM2: the RP2350 boards' pins (HDMI, SD, PS/2, I2S, PSRAM) and clocks have no meaning on the Colour Maximite 2;
 * only the screen, the frame buffer and the game directory are kept, as frank-quest has them.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// SCI: 320x200 (SCI0/SCI1), 640x480 (SCI1.1+)
// We use 320x200 as base with letterboxing to 320x240 for HDMI
#ifndef CABAL_SCREEN_WIDTH
#define CABAL_SCREEN_WIDTH  320
#endif
#ifndef CABAL_SCREEN_HEIGHT
#define CABAL_SCREEN_HEIGHT 200
#endif
#ifndef CABAL_HDMI_HEIGHT
#define CABAL_HDMI_HEIGHT   240  // With letterboxing
#endif

// Double buffer: each buffer 320 * 240 = 76,800 bytes
#define CABAL_FRAMEBUFFER_SIZE (CABAL_SCREEN_WIDTH * CABAL_HDMI_HEIGHT)

// PSRAM total size (8MB)
#ifndef CABAL_PSRAM_SIZE_BYTES
#define CABAL_PSRAM_SIZE_BYTES (8u * 1024u * 1024u)
#endif

// Game data directory on SD card
#define CABAL_GAME_DIR "quest"

#endif // BOARD_CONFIG_H
