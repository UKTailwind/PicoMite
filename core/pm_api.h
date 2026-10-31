#ifndef PICOMITE_CORE_PM_API_H
#define PICOMITE_CORE_PM_API_H

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
void pm_pin_mode(int gp, int mode);
void pm_pin_write(int gp, int value);
int pm_pin_read(int gp);

/* Invoke a firmware service within the active C program's error boundary.
 * Passing NULL performs the abort/poll check only. */
void pm_core_call(void (*fn)(void *), void *arg);

/* Common named-symbol exports for external ET_REL and TinyCC-compiled code.
 * NULL means the name is not supplied by the common firmware API. */
const void *pm_core_resolve_symbol(const char *name);
unsigned pm_core_firmware_id(void);

#endif
