#ifndef PICOMITE_PM_PIO_SHARED_H
#define PICOMITE_PM_PIO_SHARED_H
#include <stddef.h>
#include <stdint.h>
#include "core/tcc/include/picomite/pio.h"

/* Common reload-address storage for DMA channel chaining. Reuses the two
 * static pointer words previously local to BASIC cmd_pio(). */
extern uint32_t *pio_dma_rx_address;
extern uint32_t *pio_dma_tx_address;

/* Common hardware implementation used directly by both BASIC and C. */
int pm_pio_available_hw(int pio);
int pm_pio_idle_hw(int pio);
void pm_pio_writefifo_hw(int pio,int sm,int fifo,uint32_t value);
void pm_pio_set_base_hw(int pio,int base);
void pm_pio_configure_hw(int pio, int sm, const pm_pio_config *config);
void pm_pio_sync_hw(int pio, unsigned my, unsigned prev, unsigned next);
void pm_pio_exec_hw(int pio, int sm, uint16_t instruction);
void pm_pio_write_hw(int pio, int sm, uint32_t value);
int pm_pio_read_hw(int pio, int sm, uint32_t *value);
void pm_pio_start_hw(int pio, int sm);
void pm_pio_stop_hw(int pio, int sm);
void pm_pio_init_hw(int pio, int sm, uint32_t pinctrl, uint32_t execctrl,
                    uint32_t shiftctrl, int start, float clock,
                    int sideout, int setout, int outout);

/* Shared with BASIC PIO PROGRAM / PROGRAM LINE / CLEAR / PIO(FLEVEL). */
void pm_pio_program_hw(int pio, const uint16_t *words, size_t count);
void pm_pio_program_line_hw(int pio, int slot, uint16_t instruction);
void pm_pio_clear_hw(int pio);
uint32_t pm_pio_flevel_hw(int pio, int sm, int tx);
uint32_t pm_pio_fstat_hw(int pio);
uint32_t pm_pio_fdebug_hw(int pio);
uint32_t pm_pio_readfifo_hw(int pio, int sm, int fifo);

/* DMA transfer size in bits: 8, 16 or 32. count is number of samples.
 * Caller retains the buffer while DMA is active. 1 = TX, 0 = RX. */
void pm_pio_dma_transfer_hw(int tx, int pio, int sm, void *buffer,
                            size_t capacity_bytes, uint32_t count,
                            unsigned bits, int from_c);
/* BASIC and C share the existing PIO/DMA interrupt slots and TX history;
 * C registrations are tagged so the BASIC dispatcher never executes them. */
void pm_pio_register_basic_fifo_hw(int pio, int sm, int tx, char *target);
void pm_pio_register_c_fifo_hw(int pio, int sm, int tx,
                               void (*callback)(int, int));
void (*pm_pio_c_fifo_handler_hw(int pio, int sm, int tx))(int, int);
int pm_pio_fifo_is_c_hw(int pio, int sm, int tx);
void pm_pio_register_basic_dma_hw(int tx, char *target);
void pm_pio_register_c_dma_hw(int tx, void (*callback)(int));
void (*pm_pio_c_dma_handler_hw(int tx))(int);
int pm_pio_dma_is_c_hw(int tx);
void pm_pio_unregister_c_handlers_hw(void);
/* FIFO IRQ conditions and DMA completion/pointer shared with BASIC. */
int pm_pio_fifo_event_hw(int pio, int sm, int tx, uint8_t *last_level);
int pm_pio_dma_complete_hw(int tx);
uintptr_t pm_pio_dma_pointer_hw(int tx);
/* Cyclic DMA uses BASIC's RX/TX channel pairs and the shared persistent
 * pio_dma_{rx,tx}_address words. count is elements per repeated block.
 * ring=1: TX address wrapping; ring=0: replay by control-channel reload.
 * For RX, replay always rewinds the destination after each block. */
/* from_c=0 preserves BASIC's restart and callback ownership semantics. */
void pm_pio_dma_cyclic_hw(int tx,int pio,int sm,void *buffer,
                          size_t capacity_bytes,uint32_t count,
                          unsigned bits,int ring,int from_c);
/* BASIC finite RX with a size=1 fixed destination or power-of-two ring.
 * The DMA transfer itself has an explicit finite count. */
void pm_pio_dma_finite_rx_hw(int pio,int sm,void *buffer,size_t capacity,
                             uint32_t count,unsigned bits,uint32_t window,int from_c);
void pm_pio_dma_stop_hw(int tx);
int pm_pio_dma_busy_hw(int tx);
#endif
