/* ============================================================================
 * VGA.h - interface to the PIO VGA (QVGA) core1 scanout (graphics/VGA.c)
 *
 * Declares the core1 entry point plus the core0<->core1 shared state that
 * crosses the PicoMite.c / VGA.c boundary after the scanout split. Most shared
 * globals are already extern-declared in Hardware_Includes.h; only the extras
 * referenced across this boundary live here.
 * ============================================================================ */
#ifndef VGA_H
#define VGA_H

#if defined(PICOMITEVGA) && !defined(HDMI)

/* core1 entry point, launched from PicoMite.c via multicore_launch_core1. */
void QVgaCore(void);

/* Scanout loop counters, defined in PicoMite.c (PICOMITEVGA scope) and shared
   with the QVGA scanout loops in VGA.c. */
extern int vgaloop1, vgaloop2, vgaloop4, vgaloop8, vgaloop16, vgaloop32;

/* Runtime QVGA timing, computed in PicoMite.c from the QVGA_*_F defines
   (Screens.h) and consumed by the scanout in VGA.c. */
extern int QVGA_TOTAL, QVGA_HSYNC, QVGA_BP, QVGA_FP;
extern int QVGA_HACT, QVGA_VACT, QVGA_VFRONT, QVGA_VSYNC, QVGA_VBACK, QVGA_VTOT;

/* I2S PIO program offset, set in PicoMite.c. */
extern uint I2SOff;

#endif // PICOMITEVGA && !HDMI
#endif // VGA_H
