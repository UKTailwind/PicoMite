/* Cooperative flash-LIBRARY C CSUB resident service (API v31). */
#ifndef _PICOMITE_RESIDENT_H
#define _PICOMITE_RESIDENT_H
#include <stdint.h>
#include <stddef.h>

/* Register only from a CSUB linked in FLASH LIBRARY. No RUN-scoped pointers.
 * Period is 10..60000 ms. Callback runs on the main thread, not in IRQ.
 * It must not call BASIC or use RUN-scoped I/O/IRQ/DMA. Resident-owned
 * system-heap allocations are explicit and survive BASIC END/RUN.
 * Returns 0 on success, -1 if registration is unsafe or unsupported. */
int pm_resident_start(void (*callback)(void *state), unsigned interval_ms);
/* Returns -1 while GPIO/PIO/DMA/legacy buffer remain owned. No auto cleanup. */
int pm_resident_stop(void);
int pm_resident_active(void);
int pm_resident_faulted(void); /* abnormal callback suspended until restart */
/* One optional GPIO edge subscription per installed resident, attached
 * ONLY to an already claimed GPIO. Valid edges: RISE/FALL/BOTH; pull: NONE/
 * UP/DOWN. Off removes the subscription but does not release the GPIO claim.
 * The callback is also required to live in flash LIBRARY CSUB code.
 * It runs from routinechecks() on the foreground core, NOT in GPIO IRQ.
 * 'events' is a mask of latched edges, not a reliable edge count: multiple
 * changes can coalesce, especially during RUN .c/.elf or long BASIC work.
 * The claim survives END/RUN and is released only by the resident itself.
 * A failed subscription returns -1 without acquiring a pin. */
#define PM_RESIDENT_GPIO_RISE 1u
#define PM_RESIDENT_GPIO_FALL 2u
#define PM_RESIDENT_GPIO_BOTH 3u
#define PM_RESIDENT_GPIO_NOPULL 0
#define PM_RESIDENT_GPIO_PULLUP 1
#define PM_RESIDENT_GPIO_PULLDOWN 2
int pm_resident_gpio_on_edge(int gp, unsigned edges, int pull,
    void (*callback)(int gp, unsigned events, void *state));
void pm_resident_gpio_off(void);
/* API v31: reserve any number of available GPIOs, independent of their
 * operation. The resident configures a claimed pin with pm_pin_mode() (and
 * uses pm_pin_write/read()), optionally subscribes to edges, then explicitly
 * detaches and releases it. Claim changes no hardware state; release never
 * resets hardware. Release refuses a pin with an active edge subscription.
 * Claims survive END/RUN and prevent stop/restart until released. */
int pm_resident_gpio_claim(int gp);
int pm_resident_gpio_release(int gp);
/* A PIO claim owns one *whole block* (all four SMs and instruction memory),
 * never just an SM. Claims and hardware teardown are explicit.
 * Existing hardware must be idle. Claim alone does not change PIO memory.
 * Explicit PROGRAM replaces instruction memory; only explicit PIO release
 * clears the block and returns its GPIOs. No RUN-owned buffers survive.
 * Calls return 0 on success and -1 when invalid/unavailable. */
int pm_resident_pio_claim(int pio);
int pm_resident_pio_release(void); /* -1 until DMA claims released */
int pm_resident_pio_pin(int gp); /* Configure/reserve GPIO for claimed PIO. */
int pm_resident_pio_program(const uint16_t *words, unsigned count);
int pm_resident_pio_init_machine(int sm, uint32_t pinctrl,
                                 uint32_t execctrl, uint32_t shiftctrl,
                                 int start, unsigned clock_hz,
                                 int sideout, int setout, int outout);
int pm_resident_pio_start(int sm);
int pm_resident_pio_stop(int sm);
/* Managed DMA on a claimed resident PIO block; TX=1, RX=0.
 * v27 uses the 64-byte resident state (16 aligned words).
 * offset_words/count select a contiguous subrange. Concurrent RX/TX must
 * not overlap; do not modify a DMA-owned region until stopped. 32-bit data
 * transfers only. cyclic=1 is address-reload RX/TX (RX count power of two).
 * 0 on success, -1 for invalid/unavailable resources. No IRQ callbacks.
 * Channel ownership survives END/RUN and releases before PIO/CSUB teardown. */
int pm_resident_pio_dma_start(int tx, int sm, unsigned offset_words,
                               unsigned count, int cyclic);
int pm_resident_pio_dma_busy(int tx); /* -1 when not owned */
int pm_resident_pio_dma_stop(int tx); /* legacy: halt and release at once */
/* API v29, explicit ownership.  No forced buffer location or size.
 * Claim is independent of starting DMA; halt retains ownership and release
 * requires an explicit halt. Programmer owns all allocation lifetimes.
 * The transfer uses BASIC's shared PIO DMA engine (8/16/32-bit, one-shot or
 * cyclic). tx_ring applies only to cyclic TX; 0 uses address-reload.
 * The PIO block must already be claimed by the resident. */
void *pm_resident_alloc(size_t bytes);
void pm_resident_free(void *ptr);
int pm_resident_dma_claim(int tx);
int pm_resident_pio_dma_transfer(int tx, int sm, void *buffer,
                                 size_t capacity_bytes, uint32_t count,
                                 unsigned bits, int cyclic, int tx_ring);
int pm_resident_dma_halt(int tx);
int pm_resident_dma_release(int tx);
/* API v28: an additional persistent buffer in the firmware's system heap.
 * One buffer per resident. Word count is limited only by available system
 * memory and overflow checking. Allocation zeroes its contents; NULL means
 * no buffer. Call alloc/free only from foreground CSUB installation code,
 * not from callbacks or a native RUN. Never modify an RX DMA-owned range.
 * Getters are unavailable during a native RUN. Free refuses a buffer
 * referenced by either DMA direction. Stop/restart/LIBRARY do NOT free it. */
int pm_resident_dma_buffer_alloc(unsigned words);
uint32_t *pm_resident_dma_buffer(void);
unsigned pm_resident_dma_buffer_words(void);
int pm_resident_dma_buffer_free(void);
/* Like pm_resident_pio_dma_start(), but uses the allocated buffer instead of
 * state[16]. Offset and count are in 32-bit words; cyclic RX count must be a
 * power of two. Overlapping RX/TX regions of the same buffer are rejected. */
int pm_resident_pio_dma_start_buffer(int tx, int sm,
                                     unsigned offset_words,
                                     unsigned count, int cyclic);
/* 64 bytes allocated from the system heap on installation, not BASIC's
 * resettable heap. Survives RUN/END, zeroed on start, freed on stop/unload.
 * Returns NULL when no resident is installed. */
uint32_t *pm_resident_state(void);
#endif
