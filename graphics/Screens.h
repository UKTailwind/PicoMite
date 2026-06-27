/* ============================================================================
 * Screens.h - VGA/HDMI display configuration
 *
 * Resolutions, per-mode framebuffer sizes, the cut-down framebuffer pool and
 * (added in a later step) the VGA/HDMI scanout timing tables and QVGA macros.
 * Extracted from configuration.h and PicoMite.c so all display config lives in
 * one place. Pulled in from configuration.h inside its PICOMITEVGA block, so it
 * is reachable wherever configuration.h is (e.g. core/Memory.c, which uses
 * MODE1SIZE_S / FRAMEBUFFER_POOL_SIZE). The PICOMITEVGA guard below keeps it
 * inert if included from a non-VGA translation unit.
 * ============================================================================ */
#ifndef SCREENS_H
#define SCREENS_H

#ifdef PICOMITEVGA

/* VGA display mode definitions - Standard (640x480) */
#define MODE_H_S_ACTIVE_PIXELS 640
#define MODE_V_S_ACTIVE_LINES 480
#define MODE1SIZE_S (MODE_H_S_ACTIVE_PIXELS * MODE_V_S_ACTIVE_LINES / 8)
#define MODE2SIZE_S ((MODE_H_S_ACTIVE_PIXELS / 2) * (MODE_V_S_ACTIVE_LINES / 2) / 2)
#define MODE3SIZE_S ((MODE_H_S_ACTIVE_PIXELS) * (MODE_V_S_ACTIVE_LINES) / 2)
#define MODE4SIZE_S ((MODE_H_S_ACTIVE_PIXELS / 2) * (MODE_V_S_ACTIVE_LINES / 2) * 2)
#define MODE5SIZE_S ((MODE_H_S_ACTIVE_PIXELS / 2) * (MODE_V_S_ACTIVE_LINES / 2))

/* VGA display mode definitions - 720x400 */
#define MODE_H_4_ACTIVE_PIXELS 720
#define MODE_V_4_ACTIVE_LINES 400
#define MODE1SIZE_4 (MODE_H_4_ACTIVE_PIXELS * MODE_V_4_ACTIVE_LINES / 8)
#define MODE2SIZE_4 ((MODE_H_4_ACTIVE_PIXELS / 2) * (MODE_V_4_ACTIVE_LINES / 2) / 2)
#define MODE3SIZE_4 ((MODE_H_4_ACTIVE_PIXELS) * (MODE_V_4_ACTIVE_LINES) / 2)
#define MODE4SIZE_4 ((MODE_H_4_ACTIVE_PIXELS / 2) * (MODE_V_4_ACTIVE_LINES / 2) * 2)
#define MODE5SIZE_4 ((MODE_H_4_ACTIVE_PIXELS / 2) * (MODE_V_4_ACTIVE_LINES / 2))

/* VGA display mode definitions - 1280x720 */
#define MODE_H_W_ACTIVE_PIXELS 1280
#define MODE_V_W_ACTIVE_LINES 720
#define MODE1SIZE_W (MODE_H_W_ACTIVE_PIXELS * MODE_V_W_ACTIVE_LINES / 8)
#define MODE2SIZE_W ((MODE_H_W_ACTIVE_PIXELS / 4) * (MODE_V_W_ACTIVE_LINES / 4) / 2)
#define MODE3SIZE_W ((MODE_H_W_ACTIVE_PIXELS / 2) * (MODE_V_W_ACTIVE_LINES / 2) / 2)
#define MODE5SIZE_W ((MODE_H_W_ACTIVE_PIXELS / 4) * (MODE_V_W_ACTIVE_LINES / 4))

/* VGA display mode definitions - 848x480 */
#define MODE_H_8_ACTIVE_PIXELS 848
#define MODE_V_8_ACTIVE_LINES 480
#define MODE1SIZE_8 (MODE_H_8_ACTIVE_PIXELS * MODE_V_8_ACTIVE_LINES / 8)
#define MODE2SIZE_8 ((MODE_H_8_ACTIVE_PIXELS / 2) * (MODE_V_8_ACTIVE_LINES / 2) / 2)
#define MODE3SIZE_8 ((MODE_H_8_ACTIVE_PIXELS) * (MODE_V_8_ACTIVE_LINES) / 2)
#define MODE5SIZE_8 ((MODE_H_8_ACTIVE_PIXELS / 2) * (MODE_V_8_ACTIVE_LINES / 2))

/* VGA display mode definitions - 1024x768 (XGA) */
#define MODE_H_L_ACTIVE_PIXELS 1024
#define MODE_V_L_ACTIVE_LINES 768
#define MODE1SIZE_L (MODE_H_L_ACTIVE_PIXELS * MODE_V_L_ACTIVE_LINES / 8)
#define MODE2SIZE_L ((MODE_H_L_ACTIVE_PIXELS / 4) * (MODE_V_L_ACTIVE_LINES / 4) / 2)
#define MODE3SIZE_L ((MODE_H_L_ACTIVE_PIXELS / 2) * (MODE_V_L_ACTIVE_LINES / 2) / 2)
#define MODE5SIZE_L ((MODE_H_L_ACTIVE_PIXELS / 4) * (MODE_V_L_ACTIVE_LINES / 4))

/* VGA display mode definitions - 800x600 (SVGA) */
#define MODE_H_V_ACTIVE_PIXELS 800
#define MODE_V_V_ACTIVE_LINES 600
#define MODE1SIZE_V (MODE_H_V_ACTIVE_PIXELS * MODE_V_V_ACTIVE_LINES / 8)
#define MODE2SIZE_V ((MODE_H_V_ACTIVE_PIXELS / 2) * (MODE_V_V_ACTIVE_LINES / 2) / 2)
#define MODE3SIZE_V ((MODE_H_V_ACTIVE_PIXELS) * (MODE_V_V_ACTIVE_LINES) / 2)
#define MODE5SIZE_V ((MODE_H_V_ACTIVE_PIXELS / 2) * (MODE_V_V_ACTIVE_LINES / 2))

/* VGA display mode definitions - 1024x600 */
#define MODE_H_X_ACTIVE_PIXELS 1024
#define MODE_V_X_ACTIVE_LINES 600
#define MODE1SIZE_X (MODE_H_X_ACTIVE_PIXELS * MODE_V_X_ACTIVE_LINES / 8)
#define MODE2SIZE_X ((MODE_H_X_ACTIVE_PIXELS / 4) * (MODE_V_X_ACTIVE_LINES / 4) / 2)
#define MODE3SIZE_X ((MODE_H_X_ACTIVE_PIXELS / 2) * (MODE_V_X_ACTIVE_LINES / 2) / 2)
#define MODE5SIZE_X ((MODE_H_X_ACTIVE_PIXELS / 4) * (MODE_V_X_ACTIVE_LINES / 4))

#ifdef HDMICUTDOWN
/* Permanent framebuffer pool extension inside AllMemory[]. Sized to
   the largest layout the build can produce in mode 1: the
   1024x600x1bpp bitmap (MODE1SIZE_X = 76800) plus the tilefcols_w
   and tilebcols_w arrays that settiles() writes immediately after
   the framebuffer (1 byte per 8x8 cell, two arrays = 2 * 128 * 75 =
   19200). Total = 96000 bytes vs. 153600 for the default HDMI pool.
   HDMIWEB reuses the same shrunk pool to claw back RAM for the WiFi /
   lwIP / TLS stack. */
#define FRAMEBUFFER_POOL_SIZE \
   (MODE1SIZE_X + 2 * ((MODE_H_X_ACTIVE_PIXELS / 8) * (MODE_V_X_ACTIVE_LINES / 8)))
#endif

/* VGA display mode definitions - 800x480 */
#define MODE_H_Y_ACTIVE_PIXELS 800
#define MODE_V_Y_ACTIVE_LINES 480
#define MODE1SIZE_Y (MODE_H_Y_ACTIVE_PIXELS * MODE_V_Y_ACTIVE_LINES / 8)
#define MODE2SIZE_Y ((MODE_H_Y_ACTIVE_PIXELS / 2) * (MODE_V_Y_ACTIVE_LINES / 2) / 2)
#define MODE3SIZE_Y ((MODE_H_Y_ACTIVE_PIXELS) * (MODE_V_Y_ACTIVE_LINES) / 2)
#define MODE5SIZE_Y ((MODE_H_Y_ACTIVE_PIXELS / 2) * (MODE_V_Y_ACTIVE_LINES / 2))

/* ---- QVGA scanout configuration (VGA, non-HDMI) - from PicoMite.c ----
   The QVGA_GPIO_* macros reference PinDef[] / Option (configuration.h globals);
   they expand only at their PicoMite.c use sites, where those are in scope. */
#ifndef HDMI
#define QVGA_GPIO_FIRST PinDef[Option.VGA_BLUE].GPno
#define QVGA_GPIO_NUM 4
#define QVGA_GPIO_LAST (QVGA_GPIO_FIRST + QVGA_GPIO_NUM - 1)
#define QVGA_GPIO_HSYNC PinDef[Option.VGA_HSYNC].GPno
#define QVGA_GPIO_VSYNC (QVGA_GPIO_HSYNC + 1)
// QVGA horizontal timing (126 MHz clock); HSYNC inverted (negative SYNC=LOW=0x80)
#define QVGA_TOTAL_F 4000
#define QVGA_HSYNC_F 480
#define QVGA_BP_F 240
#define QVGA_FP_F 80
// QVGA vertical timing
#define QVGA_VTOT_F 525
#define QVGA_VSYNC_F 2
#define QVGA_VBACK_F 33
#define QVGA_VACT_F 480
#define QVGA_VFRONT_F 10
#endif // !HDMI

#endif // PICOMITEVGA
#endif // SCREENS_H
