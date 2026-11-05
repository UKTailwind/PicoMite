#ifndef PICOMITE_NATIVE_PIO_H
#define PICOMITE_NATIVE_PIO_H
#include <stddef.h>
#include <stdint.h>

int pm_pio_available(int);
int pm_pio_idle(int);
void pm_pio_writefifo(int,int,int,uint32_t);
void pm_pio_set_base(int,int);
void pm_pio_exec(int,int,uint16_t);
void pm_pio_write(int,int,uint32_t);
int pm_pio_read(int,int,uint32_t *);
void pm_pio_start(int,int);
void pm_pio_stop(int,int);
void pm_pio_init_machine(int,int,uint32_t,uint32_t,uint32_t,int,float,int,int,int);
void pm_pio_program(int,const uint16_t *,size_t);
void pm_pio_assemble(int,const char *);
void pm_pio_program_line(int,int,uint16_t);
void pm_pio_clear(int);
uint32_t pm_pio_flevel(int,int,int);
uint32_t pm_pio_fstat(int);
uint32_t pm_pio_fdebug(int);
uint32_t pm_pio_readfifo(int,int,int);

static inline uint32_t pm_pio_encode_pinctrl(unsigned s,unsigned t,unsigned o,
                                              unsigned i,unsigned b,unsigned c,unsigned d)
{
    return ((s&7u)<<29)|((t&7u)<<26)|((o&31u)<<20)|
           ((i&31u)<<15)|((b&31u)<<10)|((c&31u)<<5)|(d&31u);
}
static inline uint32_t pm_pio_encode_execctrl(unsigned j,unsigned t,unsigned w,
                                               unsigned d,unsigned e)
{
    return ((j&31u)<<24)|((t&31u)<<7)|((w&31u)<<12)|
           ((d&1u)<<29)|((e&1u)<<30);
}
static inline uint32_t pm_pio_encode_shiftctrl(unsigned p,unsigned l,unsigned a,
                                                unsigned b,unsigned i,unsigned o,
                                                unsigned r,unsigned t,unsigned g,unsigned u)
{
    return ((p&31u)<<20)|((l&31u)<<25)|((a&1u)<<16)|((b&1u)<<17)|
           ((i&1u)<<18)|((o&1u)<<19)|((r&1u)<<30)|((t&1u)<<31)|
           ((g&1u)<<14)|((u&1u)<<15);
}

void pm_pio_dma_rx(int,int,void *,size_t,uint32_t,unsigned);
void pm_pio_dma_tx(int,int,const void *,size_t,uint32_t,unsigned);
void pm_pio_dma_rx_ring(int,int,void *,size_t,uint32_t,unsigned,uint32_t);
void pm_pio_dma_cyclic_rx(int,int,void *,size_t,uint32_t,unsigned);
void pm_pio_dma_cyclic_tx(int,int,const void *,size_t,uint32_t,unsigned,int);
int pm_pio_dma_busy(int);
void pm_pio_dma_stop(int);
uintptr_t pm_pio_dma_pointer(int);
void pm_pio_on_fifo(int,int,int,void (*)(int,int));
void pm_pio_on_dma_done(int,void (*)(int));
void pm_pio_poll(void);

typedef struct pm_pio_config {
    unsigned clock_hz;
    int start_address;
    int sideset_base, sideset_count, sideset_output;
    int set_base, set_count, set_output;
    int out_base, out_count, out_output;
    int in_base, jmp_pin;
    int wrap_target, wrap;
    int side_enable, side_pindir;
    int push_threshold, pull_threshold;
    int autopush, autopull, in_shift_right, out_shift_right;
    int join_rx_fifo, join_tx_fifo;
    int join_rx_get, join_rx_put;
} pm_pio_config;
void pm_pio_configure(int,int,const pm_pio_config *);
void pm_pio_sync(int,unsigned,unsigned,unsigned);

#endif
