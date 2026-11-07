/* Shared configuration of the vga-nextgen eight-bit PIO scanout. */
#ifndef PICO_MM_VGA_NEXTGEN_H
#define PICO_MM_VGA_NEXTGEN_H

/* Samples in a whole line, sync included: 848x480 has 1088. The RP2040 has
 * no 800x600 or 848x480 (OPTION RESOLUTION); its widest line is 720x400's
 * 900, and the five line buffers must fit in its SRAM. */
#ifdef rp2350
#define VGA_NEXTGEN_MAX_LINE 1088
#else
#define VGA_NEXTGEN_MAX_LINE 900
#endif

#endif
