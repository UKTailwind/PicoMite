/* ============================================================================
 * HDMI.h - interface to the HDMI/DVI core1 scanout (graphics/HDMI.c)
 *
 * Declares the core1 entry point plus the core0<->core1 shared state that
 * crosses the PicoMite.c / HDMI.c boundary after the scanout split. Only the
 * symbols actually referenced across the boundary belong here; the list is
 * built up from the compiler/linker as the split is completed.
 * ============================================================================ */
#ifndef HDMI_H
#define HDMI_H

#if defined(PICOMITEVGA) && defined(HDMI)

/* core1 entry point, launched from PicoMite.c via multicore_launch_core1. */
void HDMICore(void);

/* How long HDMICore holds the HSTX output dark across a live RESOLUTION switch. */
#define HDMI_BLANK_US 30000

/* The two DMA channels HDMICore hardcodes for the ping/pong HSTX command lists;
   PicoMite.c's HDMICUTDOWN path claims them too. */
#define DMACH_PING 0
#define DMACH_PONG 1

/* Live-resolution-switch handshake: core0 (PicoMite.c) sets it, core1 (HDMICore
   scanout loops) sees it, returns to rebuild, and clears it. */
extern volatile bool hdmi_switch_pending;

/* Scanout loop counters, defined in PicoMite.c (PICOMITEVGA scope) and shared
   with the HDMI scanout loops in HDMI.c. */
extern int vgaloop1, vgaloop2, vgaloop4, vgaloop8, vgaloop16, vgaloop32;

#endif // PICOMITEVGA && HDMI
#endif // HDMI_H
