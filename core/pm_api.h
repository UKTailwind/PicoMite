#ifndef PICOMITE_CORE_PM_API_H
#define PICOMITE_CORE_PM_API_H

#include <stdint.h>

/* These entry points have no dependency on the TinyCC compiler or arena.
 * Their error boundary is provided by the active firmware run context. */
int pm_hres(void);
int pm_vres(void);
unsigned long long pm_timer_us(void);
int pm_abort_requested(void);
void pm_cls(int colour);
void pm_pixel(int x, int y, int colour);
/* Internal C name avoids the pm_line console-buffer variable in TinyCC.
 * ELF clients see the public symbol pm_line via the resolver. */
void pm_core_line(int x1, int y1, int x2, int y2, int width, int colour);
void pm_box(int x1, int y1, int x2, int y2, int colour);
void pm_circle(int x, int y, int r, int width, int colour, int fill);
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                 int colour, int fill);
void pm_refresh(void);
int pm_inkey(void);
/* Physical keyboard only: a character to emit or a notification. */
uint8_t KeyboardEmit(uint8_t code, uint16_t key,
                     uint8_t modifiers, uint8_t flags);
void pm_pin_mode(int gp, int mode);
void pm_pin_write(int gp, int value);
int pm_pin_read(int gp);

/* Resident flash-library CSUB service: not part of a RUN allocation scope. */
void pm_resident_service(void);
void pm_resident_unwind(void);
void pm_resident_before_library_write(void);
/* Exclusive GPIO reservation, checked by CheckPin() and ClearExternalIO(). */
int pm_resident_gpio_pin_owned(int pin);
int pm_resident_gpio_access_allowed(int pin);
/* Resident block-level PIO ownership; no persistent DMA in this stage. */
int pm_resident_pio_block_owned(int pio);
int pm_resident_pio_access_allowed(int pio);
int pm_resident_pio_any_owned(void);
/* Global PIO DMA channel reservation, checked by BASIC and C RUN. */
int pm_resident_dma_owned(int tx);
int pm_resident_dma_access_allowed(int tx);

/* Invoke a firmware service within the active C program's error boundary.
 * Passing NULL performs the abort/poll check only. */
void pm_core_call(void (*fn)(void *), void *arg);

/* Common named-symbol exports for external ET_REL and TinyCC-compiled code.
 * NULL means the name is not supplied by the common firmware API. */
const void *pm_core_resolve_symbol(const char *name);
unsigned pm_core_firmware_id(void);

#endif
