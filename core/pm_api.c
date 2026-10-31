/* Native C services common to the RP2040 ELF runner and RP2350 TinyCC.
 * This module is linked with PicoMite itself, never with the compiler library.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "pico/time.h"
#include <stdint.h>
#include <string.h>

/* RP2350 keeps the existing PM_CALL error/abort boundary in the C runtime.
 * RP2040's ELF runner uses the BASIC prompt's error boundary and performs
 * abandoned-image cleanup before resetting its heap. */
#ifdef rp2350
extern void tcc_picomite_api_call(void (*fn)(void *), void *arg);
#endif
static void pm_core_call(void (*fn)(void *), void *arg)
{
#ifdef rp2350
    tcc_picomite_api_call(fn, arg);
#else
    if (MMAbort) CheckAbort();
    fn(arg);
#endif
}

int pm_hres(void) { return HRes; }
int pm_vres(void) { return VRes; }
unsigned long long pm_timer_us(void) { return time_us_64(); }
int pm_abort_requested(void) { return MMAbort != 0; }

/* Keep these arguments on the caller's C stack. All callbacks are synchronous.
 * The trampoline adds no heap allocation or persistent callback. */
static void pm_cls_do(void *p) { ClearScreen(((int *)p)[0]); }
void pm_cls(int c) { int a[] = {c}; pm_core_call(pm_cls_do, a); }

static void pm_pixel_do(void *p) { int *a = p; DrawPixel(a[0], a[1], a[2]); }
void pm_pixel(int x, int y, int c)
{ int a[] = {x, y, c}; pm_core_call(pm_pixel_do, a); }

static void pm_line_do(void *p)
{ int *a = p; DrawLine(a[0], a[1], a[2], a[3], a[4], a[5]); }
void pm_core_line(int x1, int y1, int x2, int y2, int w, int c)
{ int a[] = {x1, y1, x2, y2, w, c}; pm_core_call(pm_line_do, a); }

static void pm_box_do(void *p)
{ int *a = p; DrawRectangle(a[0], a[1], a[2], a[3], a[4]); }
void pm_box(int x1, int y1, int x2, int y2, int c)
{ int a[] = {x1, y1, x2, y2, c}; pm_core_call(pm_box_do, a); }

static void pm_circle_do(void *p)
{ int *a = p; DrawCircle(a[0], a[1], a[2], a[3], a[4], a[5], 1.0); }
void pm_circle(int x, int y, int r, int w, int c, int fill)
{ int a[] = {x, y, r, w, c, fill}; pm_core_call(pm_circle_do, a); }

static void pm_triangle_do(void *p)
{ int *a = p; DrawTriangle(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); }
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int c, int fill)
{ int a[] = {x0, y0, x1, y1, x2, y2, c, fill}; pm_core_call(pm_triangle_do, a); }

static void pm_refresh_do(void *p) { (void)p; Display_Refresh(); }
void pm_refresh(void) { pm_core_call(pm_refresh_do, NULL); }

static void pm_inkey_do(void *p) { *(int *)p = MMInkey(); }
int pm_inkey(void)
{ int value = -1; pm_core_call(pm_inkey_do, &value); return value; }

static void pm_pin_mode_do(void *p)
{ int *a = p; ExtCfg(codemap(a[0]), a[1], 0); }
void pm_pin_mode(int gp, int mode)
{ int a[] = {gp, mode}; pm_core_call(pm_pin_mode_do, a); }

static void pm_pin_write_do(void *p)
{ int *a = p; ExtSet(codemap(a[0]), a[1]); }
void pm_pin_write(int gp, int value)
{ int a[] = {gp, value}; pm_core_call(pm_pin_write_do, a); }

static void pm_pin_read_do(void *p)
{ int *a = p; a[1] = (int)ExtInp(codemap(a[0])); }
int pm_pin_read(int gp)
{ int a[] = {gp, 0}; pm_core_call(pm_pin_read_do, a); return a[1]; }

extern const void *const CallTable[];
typedef struct { const char *name; const void *address; } pm_core_symbol;
#define PM_CORE_SYM(n) {#n, (const void *)&n},
static const pm_core_symbol pm_core_symbols[] = {
    {"__picomite_calltable", (const void *)CallTable},
    PM_CORE_SYM(pm_hres)
    PM_CORE_SYM(pm_vres)
    PM_CORE_SYM(pm_timer_us)
    PM_CORE_SYM(pm_abort_requested)
    PM_CORE_SYM(pm_cls)
    PM_CORE_SYM(pm_pixel)
    {"pm_line", (const void *)&pm_core_line},
    PM_CORE_SYM(pm_box)
    PM_CORE_SYM(pm_circle)
    PM_CORE_SYM(pm_triangle)
    PM_CORE_SYM(pm_refresh)
    PM_CORE_SYM(pm_inkey)
    PM_CORE_SYM(pm_pin_mode)
    PM_CORE_SYM(pm_pin_write)
    PM_CORE_SYM(pm_pin_read)
};
#undef PM_CORE_SYM

const void *pm_core_resolve_symbol(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_core_symbols) / sizeof(pm_core_symbols[0]); ++i)
        if (strcmp(name, pm_core_symbols[i].name) == 0)
            return pm_core_symbols[i].address;
    return NULL;
}

/* External ELF CSUBs are linked by absolute address. On RP2040 their
 * firmware stamp must cover this table, not just the legacy CallTable. */
unsigned pm_core_firmware_id(void)
{
    uint32_t hash = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_core_symbols) / sizeof(pm_core_symbols[0]); ++i) {
        uint32_t value = (uint32_t)(uintptr_t)pm_core_symbols[i].address;
        for (j = 0; j < 4; ++j, value >>= 8)
            hash = (hash ^ (value & 255u)) * 16777619u;
    }
    return hash ? hash : 1u;
}
