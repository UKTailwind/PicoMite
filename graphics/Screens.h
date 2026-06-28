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
/* CPU frequency definitions */
#define Freq720P 372000
#define Freq480P 315000
#define Freq252P 252000
#define Freq378P 378000
#define FreqXGA 375000
#define FreqSVGA 360000
#define Freq848 336000
#define Freq400 283200
#define FreqY 333000
#define FreqX 252000
#define FreqDefault 200000
typedef enum
{
    R0 = 0,
    R1280x720 = 1,
    R640x480f315 = 2,
    R640x480f252 = 3,
    R640x480f378 = 4,
    R1024x768 = 5,
    R800x600 = 6,
    R848x480 = 7,
    R720x400 = 8,
    R800x480 = 9,
    R1024x600 = 10,
#ifdef HDMICUTDOWN
    /* HDMIBTH/HDMIWEB-only: a "special" 640x480 that is RGB332 (8-bit)
       rather than the normal full-colour RGB555 640x480. It shares the
       1024x600 8-bit tile pipeline so it costs no extra framebuffer
       and is deliberately kept OUT of the FullColour macro. Selected
       at runtime by the RESOLUTION command (no reboot). */
    R640x480x8 = 11,
    /* HDMIBTH/HDMIWEB-only: the RGB332 (8-bit) 720x400, the sibling of
       R640x480x8. Kept as its own enum (rather than reusing the full
       build's RGB555 R720x400 = 8) so it stays OUT of FullColour and is
       driven by the same 8-bit tile pipeline / HDMIloopBTH640. Runs at
       283.2 MHz (Freq400); selected by OPTION/RESOLUTION. */
    R720x400x8 = 12,
#endif
} Resolution_TypeDef;
static const int CPUFreqs[] = {FreqDefault, Freq720P, Freq480P, Freq252P, Freq378P, FreqXGA, FreqSVGA, Freq848, Freq400, FreqY, FreqX};
/* Display capability macros */
#define FullColour (Option.Resolution == R640x480f252 || Option.Resolution == R640x480f378 || \
                    Option.Resolution == R640x480f315 || Option.Resolution == R720x400)
#ifdef HDMICUTDOWN
/* R640x480x8 behaves like 1024x600 (8-bit/RGB332, tile-based) so it is
   treated as a MediumRes mode for font/tile-height selection. */
#define MediumRes (Option.Resolution == R800x600 || Option.Resolution == R848x480 ||  \
                   Option.Resolution == R800x480 || Option.Resolution == R1024x600 || \
                   Option.Resolution == R640x480x8 || Option.Resolution == R720x400x8)
#else
#define MediumRes (Option.Resolution == R800x600 || Option.Resolution == R848x480 || \
                   Option.Resolution == R800x480 || Option.Resolution == R1024x600)
#endif

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

/* ---- HDMI/DVI scanout timing (HDMI only) - from PicoMite.c ----
   TMDS control symbols and per-mode sync / porch / active / total timing.
   The *_TOTAL_* macros reference the MODE_*_ACTIVE_* defines above and the
   *_PORCH (Option.CPU_Speed) macros; all expand at their PicoMite.c use sites. */
#ifdef HDMI
#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define MODE_H_S_SYNC_POLARITY 0
#define MODE_H_S_FRONT_PORCH (Option.CPU_Speed % 126000 == 0 ? 16 : 16)
#define MODE_H_S_SYNC_WIDTH (Option.CPU_Speed % 126000 == 0 ? 96 : 64)
#define MODE_H_S_BACK_PORCH (Option.CPU_Speed % 126000 == 0 ? 48 : 120)

#define MODE_V_S_SYNC_POLARITY 0
#define MODE_V_S_FRONT_PORCH (Option.CPU_Speed % 126000 == 0 ? 10 : 1)
#define MODE_V_S_SYNC_WIDTH (Option.CPU_Speed % 126000 == 0 ? 2 : 3)
#define MODE_V_S_BACK_PORCH (Option.CPU_Speed % 126000 == 0 ? 33 : 16)

#define MODE_H_F_SYNC_POLARITY 0
#define MODE_H_F_ACTIVE_PIXELS 640
#define MODE_H_F_FRONT_PORCH 16
#define MODE_H_F_SYNC_WIDTH 96
#define MODE_H_F_BACK_PORCH 48

#define MODE_V_F_SYNC_POLARITY 0
#define MODE_V_F_ACTIVE_LINES 480
#define MODE_V_F_FRONT_PORCH 10
#define MODE_V_F_SYNC_WIDTH 2
#define MODE_V_F_BACK_PORCH 33

#define MODE_H_8_SYNC_POLARITY 1
#define MODE_H_8_FRONT_PORCH 16
#define MODE_H_8_SYNC_WIDTH 112
#define MODE_H_8_BACK_PORCH 112

#define MODE_V_8_SYNC_POLARITY 1
#define MODE_V_8_FRONT_PORCH 8
#define MODE_V_8_SYNC_WIDTH 6
#define MODE_V_8_BACK_PORCH 23

#define MODE_H_4_SYNC_POLARITY 0
#define MODE_H_4_FRONT_PORCH 18
#define MODE_H_4_SYNC_WIDTH 108
#define MODE_H_4_BACK_PORCH 54

#define MODE_V_4_SYNC_POLARITY 0
#define MODE_V_4_FRONT_PORCH 12
#define MODE_V_4_SYNC_WIDTH 2
#define MODE_V_4_BACK_PORCH 35

#define MODE_H_W_SYNC_POLARITY 1
#define MODE_H_W_FRONT_PORCH 110
#define MODE_H_W_SYNC_WIDTH 40
#define MODE_H_W_BACK_PORCH 220

#define MODE_V_W_SYNC_POLARITY 1
#define MODE_V_W_FRONT_PORCH 5
#define MODE_V_W_SYNC_WIDTH 5
#define MODE_V_W_BACK_PORCH 20

#define MODE_H_L_SYNC_POLARITY 0
#define MODE_H_L_FRONT_PORCH 24
#define MODE_H_L_SYNC_WIDTH 136
#define MODE_H_L_BACK_PORCH 144

#define MODE_V_L_SYNC_POLARITY 0
#define MODE_V_L_FRONT_PORCH 3
#define MODE_V_L_SYNC_WIDTH 6
#define MODE_V_L_BACK_PORCH 29

#define MODE_H_V_SYNC_POLARITY 1
#define MODE_H_V_FRONT_PORCH 24
#define MODE_H_V_SYNC_WIDTH 72
#define MODE_H_V_BACK_PORCH 128

#define MODE_V_V_SYNC_POLARITY 1
#define MODE_V_V_FRONT_PORCH 1
#define MODE_V_V_SYNC_WIDTH 2
#define MODE_V_V_BACK_PORCH 22

#define MODE_H_X_SYNC_POLARITY 1
#define MODE_H_X_FRONT_PORCH 24
#define MODE_H_X_SYNC_WIDTH 136
#define MODE_H_X_BACK_PORCH 160

#define MODE_V_X_SYNC_POLARITY 0
#define MODE_V_X_FRONT_PORCH 1
#define MODE_V_X_SYNC_WIDTH 4
#define MODE_V_X_BACK_PORCH 23

#define MODE_H_Y_SYNC_POLARITY 0
#define MODE_H_Y_FRONT_PORCH 32
#define MODE_H_Y_SYNC_WIDTH 80
#define MODE_H_Y_BACK_PORCH 112

#define MODE_V_Y_SYNC_POLARITY 1
#define MODE_V_Y_FRONT_PORCH 3
#define MODE_V_Y_SYNC_WIDTH 10
#define MODE_V_Y_BACK_PORCH 7

#define MODE_H_S_TOTAL_PIXELS (                  \
    MODE_H_S_FRONT_PORCH + MODE_H_S_SYNC_WIDTH + \
    MODE_H_S_BACK_PORCH + MODE_H_S_ACTIVE_PIXELS)
#define MODE_V_S_TOTAL_LINES (                   \
    MODE_V_S_FRONT_PORCH + MODE_V_S_SYNC_WIDTH + \
    MODE_V_S_BACK_PORCH + MODE_V_S_ACTIVE_LINES)
#define MODE_H_F_TOTAL_PIXELS (                  \
    MODE_H_F_FRONT_PORCH + MODE_H_F_SYNC_WIDTH + \
    MODE_H_F_BACK_PORCH + MODE_H_F_ACTIVE_PIXELS)
#define MODE_V_F_TOTAL_LINES (                   \
    MODE_V_F_FRONT_PORCH + MODE_V_F_SYNC_WIDTH + \
    MODE_V_F_BACK_PORCH + MODE_V_F_ACTIVE_LINES)
#define MODE_H_W_TOTAL_PIXELS (                  \
    MODE_H_W_FRONT_PORCH + MODE_H_W_SYNC_WIDTH + \
    MODE_H_W_BACK_PORCH + MODE_H_W_ACTIVE_PIXELS)
#define MODE_V_W_TOTAL_LINES (                   \
    MODE_V_W_FRONT_PORCH + MODE_V_W_SYNC_WIDTH + \
    MODE_V_W_BACK_PORCH + MODE_V_W_ACTIVE_LINES)
#define MODE_H_L_TOTAL_PIXELS (                  \
    MODE_H_L_FRONT_PORCH + MODE_H_L_SYNC_WIDTH + \
    MODE_H_L_BACK_PORCH + MODE_H_L_ACTIVE_PIXELS)
#define MODE_V_L_TOTAL_LINES (                   \
    MODE_V_L_FRONT_PORCH + MODE_V_L_SYNC_WIDTH + \
    MODE_V_L_BACK_PORCH + MODE_V_L_ACTIVE_LINES)
#define MODE_H_V_TOTAL_PIXELS (                  \
    MODE_H_V_FRONT_PORCH + MODE_H_V_SYNC_WIDTH + \
    MODE_H_V_BACK_PORCH + MODE_H_V_ACTIVE_PIXELS)
#define MODE_V_V_TOTAL_LINES (                   \
    MODE_V_V_FRONT_PORCH + MODE_V_V_SYNC_WIDTH + \
    MODE_V_V_BACK_PORCH + MODE_V_V_ACTIVE_LINES)
#define MODE_H_8_TOTAL_PIXELS (                  \
    MODE_H_8_FRONT_PORCH + MODE_H_8_SYNC_WIDTH + \
    MODE_H_8_BACK_PORCH + MODE_H_8_ACTIVE_PIXELS)
#define MODE_V_8_TOTAL_LINES (                   \
    MODE_V_8_FRONT_PORCH + MODE_V_8_SYNC_WIDTH + \
    MODE_V_8_BACK_PORCH + MODE_V_8_ACTIVE_LINES)
#define MODE_H_4_TOTAL_PIXELS (                  \
    MODE_H_4_FRONT_PORCH + MODE_H_4_SYNC_WIDTH + \
    MODE_H_4_BACK_PORCH + MODE_H_4_ACTIVE_PIXELS)
#define MODE_V_4_TOTAL_LINES (                   \
    MODE_V_4_FRONT_PORCH + MODE_V_4_SYNC_WIDTH + \
    MODE_V_4_BACK_PORCH + MODE_V_4_ACTIVE_LINES)
#define MODE_H_X_TOTAL_PIXELS (                  \
    MODE_H_X_FRONT_PORCH + MODE_H_X_SYNC_WIDTH + \
    MODE_H_X_BACK_PORCH + MODE_H_X_ACTIVE_PIXELS)
#define MODE_V_X_TOTAL_LINES (                   \
    MODE_V_X_FRONT_PORCH + MODE_V_X_SYNC_WIDTH + \
    MODE_V_X_BACK_PORCH + MODE_V_X_ACTIVE_LINES)
#define MODE_H_Y_TOTAL_PIXELS (                  \
    MODE_H_Y_FRONT_PORCH + MODE_H_Y_SYNC_WIDTH + \
    MODE_H_Y_BACK_PORCH + MODE_H_Y_ACTIVE_PIXELS)
#define MODE_V_Y_TOTAL_LINES (                   \
    MODE_V_Y_FRONT_PORCH + MODE_V_Y_SYNC_WIDTH + \
    MODE_V_Y_BACK_PORCH + MODE_V_Y_ACTIVE_LINES)
#endif // HDMI

#endif // PICOMITEVGA
#endif // SCREENS_H
