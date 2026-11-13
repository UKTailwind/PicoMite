/* Hardware implementation shared by PicoMite BASIC PIO commands and C. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_pio_shared.h"
#include "hardware/dma.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/pio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
extern bool PIO0, PIO1;
#ifdef rp2350
extern bool PIO2;
#endif
extern char *DMAinterruptRX, *DMAinterruptTX;
extern uint32_t dma_rx_chan, dma_tx_chan, dma_rx_chan2, dma_tx_chan2;
extern int dma_rx_pio, dma_tx_pio, dma_rx_sm, dma_tx_sm;
extern void startPIO(PIO pio, int sm);
extern void pio_init(int pior, int sm, uint32_t pinctrl, uint32_t execctrl,
                     uint32_t shiftctrl, int start, float clock,
                     bool sideout, bool setout, bool outout);
#define CLKMIN ((Option.CPU_Speed * 125) >> 13)
#define CLKMAX (Option.CPU_Speed * 1000)

/* BASIC's configuration and clock synchronisation code is authoritative.
 * C calls these same functions, not a duplicate register-level driver. */
extern void syncPIO(int pio, int my, int prev, int next);
extern void configurePIO(PIO pio, int sm, int clock,
                         int startaddress, int base,
                         int sidesetbase, int sidesetno, int sidesetout,
                         int setbase, int setno, int setout,
                         int outbase, int outno, int outout, int inbase,
                         int jmppin, int wraptarget, int wrap,
                         int sideenable, int sidepindir,
                         int pushthreshold, int pullthreshold,
                         int autopush, int autopull,
                         int inshiftdir, int outshiftdir,
                         int joinrxfifo, int jointxfifo
#ifdef rp2350
                         , int joinrxfifoget, int joinrxfifoput
#endif
);

/* 24 FIFO slots + 2 DMA slots already exist in BASIC. Keep only a 26-bit
 * ownership discriminator; function pointers use the same 32-bit slots as
 * BASIC code addresses on RP2040/RP2350. */
static uint32_t pm_c_event_bits;
#define PM_FIFO_TAG(p,s,t) (1u << (((p) * 4 + (s)) * 2 + (t)))
#define PM_DMA_TAG(t) (1u << (24 + (t)))
static char **pm_pio_fifo_slot(int p, int sm, int tx)
{
    if (p < 0 || p >= PIOMAX || sm < 0 || sm >= 4 || (tx != 0 && tx != 1))
        error("Invalid PIO FIFO selection");
    return tx ? &pioTXinterrupts[sm][p] : &pioRXinterrupts[sm][p];
}
static char **pm_pio_dma_slot(int tx)
{
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    return tx ? &DMAinterruptTX : &DMAinterruptRX;
}
static void pm_pio_rearm_basic_interrupts(void)
{
    int p, sm;
    piointerrupt = 0;
    for (p = 0; p < PIOMAX; ++p)
        for (sm = 0; sm < 4; ++sm) {
            if ((pioRXinterrupts[sm][p] &&
                 !(pm_c_event_bits & PM_FIFO_TAG(p,sm,0))) ||
                (pioTXinterrupts[sm][p] &&
                 !(pm_c_event_bits & PM_FIFO_TAG(p,sm,1))))
                piointerrupt = 1;
        }
    if (piointerrupt) IntReady.poll |= INT_POLL_SCAN;
}
int pm_pio_fifo_is_c_hw(int p, int sm, int tx)
{
    (void)pm_pio_fifo_slot(p,sm,tx);
    return (pm_c_event_bits & PM_FIFO_TAG(p,sm,tx)) != 0;
}
void pm_pio_register_basic_fifo_hw(int p, int sm, int tx, char *target)
{
    char **slot = pm_pio_fifo_slot(p,sm,tx);
    if (pm_c_event_bits & PM_FIFO_TAG(p,sm,tx))
        error("PIO FIFO callback owned by C program");
    *slot = target;
    pm_pio_rearm_basic_interrupts();
}
void pm_pio_register_c_fifo_hw(int p, int sm, int tx, void (*cb)(int,int))
{
    char **slot = pm_pio_fifo_slot(p,sm,tx);
    uint32_t bit = PM_FIFO_TAG(p,sm,tx);
    if (cb && *slot && !(pm_c_event_bits & bit))
        error("PIO FIFO callback already owned by BASIC");
    if (!(pm_c_event_bits & bit) && !cb) return;
    *slot = (char *)(uintptr_t)cb;
    if (cb) pm_c_event_bits |= bit;
    else pm_c_event_bits &= ~bit;
    if (tx) pioTXlast[sm][p] = 0;
    pm_pio_rearm_basic_interrupts();
}
void (*pm_pio_c_fifo_handler_hw(int p, int sm, int tx))(int,int)
{
    char **slot = pm_pio_fifo_slot(p,sm,tx);
    return (pm_c_event_bits & PM_FIFO_TAG(p,sm,tx)) ?
           (void (*)(int,int))(uintptr_t)*slot : NULL;
}
int pm_pio_dma_is_c_hw(int tx)
{
    (void)pm_pio_dma_slot(tx);
    return (pm_c_event_bits & PM_DMA_TAG(tx)) != 0;
}
void pm_pio_register_basic_dma_hw(int tx, char *target)
{
    char **slot = pm_pio_dma_slot(tx);
    if (pm_c_event_bits & PM_DMA_TAG(tx))
        error("PIO DMA callback owned by C program");
    *slot = target;
    if (target) IntReady.poll |= INT_POLL_SCAN;
}
void pm_pio_register_c_dma_hw(int tx, void (*cb)(int))
{
    char **slot = pm_pio_dma_slot(tx);
    uint32_t bit = PM_DMA_TAG(tx);
    if (cb && *slot && !(pm_c_event_bits & bit))
        error("PIO DMA callback already owned by BASIC");
    if (!(pm_c_event_bits & bit) && !cb) return;
    *slot = (char *)(uintptr_t)cb;
    if (cb) pm_c_event_bits |= bit;
    else pm_c_event_bits &= ~bit;
}
void (*pm_pio_c_dma_handler_hw(int tx))(int)
{
    char **slot = pm_pio_dma_slot(tx);
    return (pm_c_event_bits & PM_DMA_TAG(tx)) ?
           (void (*)(int))(uintptr_t)*slot : NULL;
}
void pm_pio_unregister_c_handlers_hw(void)
{
    int p, sm, tx;
    for (p = 0; p < PIOMAX; ++p)
        for (sm = 0; sm < 4; ++sm)
            for (tx = 0; tx < 2; ++tx)
                if (pm_c_event_bits & PM_FIFO_TAG(p,sm,tx)) {
                    *pm_pio_fifo_slot(p,sm,tx) = NULL;
                    if (tx) pioTXlast[sm][p] = 0;
                }
    for (tx = 0; tx < 2; ++tx)
        if (pm_c_event_bits & PM_DMA_TAG(tx)) *pm_pio_dma_slot(tx) = NULL;
    pm_c_event_bits = 0;
    pm_pio_rearm_basic_interrupts();
}

/* PIO hardware primitives are shared by BASIC's cmd_pio and native C.
 * Keep the resource restrictions exactly where BASIC enforces them. */
static PIO pm_pio_checked(int pior)
{
    if (pior < 0 || pior >= PIOMAX) error("Invalid PIO");
    if (pm_resident_pio_block_owned(pior) &&
        !pm_resident_pio_access_allowed(pior))
        error("PIO block owned by resident C");
    if (pior == 0 && !PIO0) StandardError(3);
    if (pior == 1 && !PIO1) StandardError(4);
#ifdef rp2350
    if (pior == 2 && !PIO2) StandardError(5);
    return pior == 0 ? pio0 : (pior == 1 ? pio1 : pio2);
#else
    return pior == 0 ? pio0 : pio1;
#endif
}
static PIO pm_pio_sm_checked(int pior,int sm)
{
    if (sm < 0 || sm >= 4) error("Invalid PIO state machine");
    return pm_pio_checked(pior);
}
/* Validation mirrors BASIC PIO CONFIGURE before entering the common
 * configurePIO routine. No additional persistent state is allocated. */
static int pm_pio_pin_value(int value)
{
    if (value < 0 || value > 31) error("Invalid PIO pin base");
    return value;
}
static void pm_pio_bool_value(int value)
{
    if (value != 0 && value != 1) error("Invalid PIO Boolean option");
}
void pm_pio_configure_hw(int pior, int sm, const pm_pio_config *c)
{
    PIO pio = pm_pio_sm_checked(pior, sm);
    int base = 0;
    if (!c) error("Invalid PIO configuration");
    if (c->clock_hz < CLKMIN || c->clock_hz > CLKMAX)
        error("PIO clock out of range");
    if (c->start_address < 0 || c->start_address > 31 ||
        c->wrap_target < 0 || c->wrap_target > 31 ||
        c->wrap < 0 || c->wrap > 31 || c->wrap_target > c->wrap)
        error("Invalid PIO wrap or start address");
    pm_pio_pin_value(c->sideset_base);
    pm_pio_pin_value(c->set_base);
    pm_pio_pin_value(c->out_base);
    pm_pio_pin_value(c->in_base);
    if (c->jmp_pin < -1 || c->jmp_pin > 31)
        error("Invalid PIO jump pin");
    if (c->sideset_count < 0 || c->sideset_count > 5 ||
        c->set_count < 0 || c->set_count > 5 ||
        c->out_count < 0 || c->out_count > 31 ||
        c->sideset_count + c->side_enable > 5 ||
        c->push_threshold < 0 || c->push_threshold > 31 ||
        c->pull_threshold < 0 || c->pull_threshold > 31)
        error("Invalid PIO count or threshold");
    pm_pio_bool_value(c->sideset_output);
    pm_pio_bool_value(c->set_output);
    pm_pio_bool_value(c->out_output);
    pm_pio_bool_value(c->side_enable);
    pm_pio_bool_value(c->side_pindir);
    pm_pio_bool_value(c->autopush);
    pm_pio_bool_value(c->autopull);
    pm_pio_bool_value(c->in_shift_right);
    pm_pio_bool_value(c->out_shift_right);
    pm_pio_bool_value(c->join_rx_fifo);
    pm_pio_bool_value(c->join_tx_fifo);
    pm_pio_bool_value(c->join_rx_get);
    pm_pio_bool_value(c->join_rx_put);
    if ((c->join_rx_fifo && c->join_tx_fifo) ||
        ((c->join_rx_fifo || c->join_tx_fifo) &&
         (c->join_rx_get || c->join_rx_put)))
        error("Invalid PIO FIFO configuration");
#ifndef rp2350
    if (c->join_rx_get || c->join_rx_put)
        error("PIO FIFO PUT/GET requires RP2350");
#else
    base = pio_get_gpio_base(pio);
#endif
    configurePIO(pio, sm, (int)c->clock_hz,
                 c->start_address, base,
                 c->sideset_base, c->sideset_count, c->sideset_output,
                 c->set_base, c->set_count, c->set_output,
                 c->out_base, c->out_count, c->out_output, c->in_base,
                 c->jmp_pin, c->wrap_target, c->wrap,
                 c->side_enable, c->side_pindir,
                 c->push_threshold, c->pull_threshold,
                 c->autopush, c->autopull,
                 c->in_shift_right, c->out_shift_right,
                 c->join_rx_fifo, c->join_tx_fifo
#ifdef rp2350
                 , c->join_rx_get, c->join_rx_put
#endif
    );
}
void pm_pio_sync_hw(int pior, unsigned my, unsigned prev, unsigned next)
{
    (void)pm_pio_checked(pior);
    if (my < 1u || my > 15u || prev > 15u || next > 15u)
        error("Invalid PIO synchronisation mask");
    syncPIO(pior, (int)my, (int)prev, (int)next);
}
int pm_pio_available_hw(int pior)
{
    if (pior == 0) return PIO0;
    if (pior == 1) return PIO1;
#ifdef rp2350
    if (pior == 2) return PIO2;
#endif
    return 0;
}
/* This is a non-destructive eligibility check for self-tests.  A PIO block
 * with disabled SMs can still contain PROGRAM LINE instructions because
 * direct register writes are not tracked by the SDK program allocator. */
int pm_pio_idle_hw(int pior)
{
    PIO pio;
    int sm;
    /* SDK tracks instruction slots allocated by pio_add_program().
     * Dummy instructions are in FLASH; this only queries availability. */
    static const uint16_t blank = 0;
    struct pio_program probe = {0};
    unsigned offset;
    if (!pm_pio_available_hw(pior)) return 0;
    pio = pm_pio_checked(pior);
    probe.instructions = &blank;
    probe.length = 1;
    probe.origin = -1;
    for (offset = 0; offset < PIO_INSTRUCTION_COUNT; ++offset)
        if (!pio_can_add_program_at_offset(pio, &probe, offset)) return 0;
    if (pio->ctrl & (0xfu << PIO_CTRL_SM_ENABLE_LSB)) return 0;
    for (sm = 0; sm < 4; ++sm)
        if (pioRXinterrupts[sm][pior] || pioTXinterrupts[sm][pior])
            return 0;
    if ((dma_rx_pio == pior && pm_pio_dma_busy_hw(0)) ||
        (dma_tx_pio == pior && pm_pio_dma_busy_hw(1))) return 0;
    return 1;
}
/* RP2350 extensions of BASIC PIO WRITEFIFO and SET BASE.  No new state. */
void pm_pio_writefifo_hw(int pior,int sm,int fifo,uint32_t value)
{
#ifdef rp2350
    PIO pio = pm_pio_sm_checked(pior,sm);
    if (fifo < 0 || fifo > 3) error("Invalid PIO FIFO index");
    pio->rxf_putget[sm][fifo] = value;
#else
    (void)pior; (void)sm; (void)fifo; (void)value;
    error("PIO WRITEFIFO requires RP2350");
#endif
}
void pm_pio_set_base_hw(int pior,int base)
{
#ifdef rp2350
    PIO pio = pm_pio_checked(pior);
    if (base != 0 && base != 16) error("Valid base values are 0 and 16");
    if (rp2350a && base == 16) error("Invalid for 60-pin chip");
    pio_set_gpio_base(pio,base);
#else
    (void)pior; (void)base;
    error("PIO SET BASE requires RP2350");
#endif
}
void pm_pio_exec_hw(int pior,int sm,uint16_t instr)
{ pio_sm_exec(pm_pio_sm_checked(pior,sm),sm,instr); }
void pm_pio_write_hw(int pior,int sm,uint32_t value)
{ pio_sm_put_blocking(pm_pio_sm_checked(pior,sm),sm,value); }
int pm_pio_read_hw(int pior,int sm,uint32_t *value)
{
    PIO pio=pm_pio_sm_checked(pior,sm);
    uint32_t under=1u << (PIO_FDEBUG_RXUNDER_LSB+sm);
    if (!value) error("Invalid PIO buffer");
    pio->fdebug=under;
    *value=pio_sm_get(pio,sm);
    if (pio->fdebug & under) { pio->fdebug=under; return 0; }
    return 1;
}
void pm_pio_start_hw(int pior,int sm)
{ startPIO(pm_pio_sm_checked(pior,sm),sm); }
void pm_pio_stop_hw(int pior,int sm)
{ pio_sm_set_enabled(pm_pio_sm_checked(pior,sm),sm,false); }
void pm_pio_init_hw(int pior,int sm,uint32_t pinctrl,uint32_t execctrl,
                    uint32_t shiftctrl,int start,float clock,
                    int sideout,int setout,int outout)
{
    (void)pm_pio_sm_checked(pior,sm);
    if (clock < CLKMIN || clock > CLKMAX) error("Invalid PIO clock");
    if (start < 0 || start > 31) error("Invalid PIO start");
    pio_init(pior,sm,pinctrl,execctrl,shiftctrl,start,clock,
             sideout!=0,setout!=0,outout!=0);
}

/* PIO PROGRAM, PROGRAM LINE and CLEAR use the same implementation
 * for BASIC and native C. PIO PROGRAM deliberately replaces the whole
 * 32-word instruction memory, matching the BASIC operation. */
void pm_pio_program_hw(int pior, const uint16_t *words, size_t count)
{
    PIO pio = pm_pio_checked(pior);
    struct pio_program program = {0};
    if (!words || count == 0 || count > 32)
        error("Invalid PIO program size");
    program.instructions = words;
    program.length = (uint)count;
    program.origin = 0;
    for (int sm = 0; sm < 4; ++sm)
        hw_clear_bits(&pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + sm));
    pio_clear_instruction_memory(pio);
    if (pio_add_program(pio, &program) < 0)
        error("PIO program load failed");
}
void pm_pio_program_line_hw(int pior, int slot, uint16_t instruction)
{
    PIO pio = pm_pio_checked(pior);
    if (slot < 0 || slot >= 32) error("Invalid PIO instruction slot");
    pio->instr_mem[slot] = instruction;
}
void pm_pio_clear_hw(int pior)
{
    PIO pio = pm_pio_checked(pior);
    for (int sm = 0; sm < 4; sm++) {
        hw_clear_bits(&pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + sm));
        pio->sm[sm].pinctrl = (5u << 26);
        pio->sm[sm].execctrl = (0x1fu << 12);
        pio->sm[sm].shiftctrl = (3u << 18);
        pio_sm_clear_fifos(pio, sm);
    }
    pio->irq = 255;
    pio_clear_instruction_memory(pio);
}
uint32_t pm_pio_flevel_hw(int pior, int sm, int tx)
{
    PIO pio = pm_pio_checked(pior);
    uint32_t value = pio->flevel;
    if (sm == -1) return value;
    if (sm < 0 || sm >= 4 || (tx != 0 && tx != 1))
        error("Invalid PIO FIFO selection");
    return (value >> (8 * sm + (tx ? 0 : 4))) & 0xfu;
}

/* FSTAT/FDEBUG and RP2350 RX FIFO PUT/GET are also used by BASIC's
 * PIO(...) functions. Read the hardware registers; no shadow state. */
uint32_t pm_pio_fstat_hw(int pior)
{ return pm_pio_checked(pior)->fstat; }
uint32_t pm_pio_fdebug_hw(int pior)
{ return pm_pio_checked(pior)->fdebug; }
uint32_t pm_pio_readfifo_hw(int pior, int sm, int fifo)
{
#ifdef rp2350
    PIO pio = pm_pio_sm_checked(pior, sm);
    if (fifo < 0 || fifo > 3) error("Invalid PIO RX FIFO index");
    return pio->rxf_putget[sm][fifo];
#else
    (void)pior; (void)sm; (void)fifo;
    error("PIO READFIFO requires RP2350");
    return 0;
#endif
}

/* BASIC and C use the same FIFO event rules. TX events are generated
 * on a transition from full to not-full (but not when empty). */
int pm_pio_fifo_event_hw(int pior, int sm, int tx, uint8_t *last)
{
    PIO pio;
    int level, full, triggered;
    if (pior < 0 || pior >= PIOMAX || sm < 0 || sm >= 4)
        error("Invalid PIO state machine");
#ifdef rp2350
    pio = pior == 0 ? pio0 : (pior == 1 ? pio1 : pio2);
#else
    pio = pior == 0 ? pio0 : pio1;
#endif
    if (!tx) return pio_sm_get_rx_fifo_level(pio, (uint)sm) != 0;
    if (!last) error("PIO TX event needs previous level");
    level = pio_sm_get_tx_fifo_level(pio, (uint)sm);
    full = (pio->sm[sm].shiftctrl & (1u << 30)) ? 8 : 4;
    triggered = level && level != full && *last == full;
    *last = (uint8_t)level;
    return triggered;
}
/* This is the same completion condition BASIC uses: TX waits for the
 * state machine's FIFO to drain after the last DMA word. */
int pm_pio_dma_complete_hw(int tx)
{
    PIO pio;
    int pior = tx ? dma_tx_pio : dma_rx_pio;
    int sm = tx ? dma_tx_sm : dma_rx_sm;
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    if (dma_channel_is_busy(tx ? dma_tx_chan : dma_rx_chan)) return 0;
    if (!tx) return 1;
    pio = pm_pio_sm_checked(pior, sm);
    return (pio->flevel & (0xfu << (8 * sm))) == 0;
}
uintptr_t pm_pio_dma_pointer_hw(int tx)
{
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    return tx ? dma_hw->ch[dma_tx_chan].read_addr
              : dma_hw->ch[dma_rx_chan].write_addr;
}

/* Cyclic DMA: one data channel plus BASIC's existing restart channel.
 * The restart word is persistent storage in misc/Custom.c, NOT a local
 * variable: DMA continues reading it after this function returns. */
void pm_pio_dma_cyclic_hw(int tx,int pior,int sm,void *buffer,
                          size_t capacity,uint32_t count,unsigned bits,int ring,int from_c)
{
    PIO pio = pm_pio_sm_checked(pior,sm);
    unsigned ch = tx ? dma_tx_chan : dma_rx_chan;
    unsigned ctl = tx ? dma_tx_chan2 : dma_rx_chan2;
    unsigned shift = bits==8 ? 0u : bits==16 ? 1u : bits==32 ? 2u : 3u;
    size_t bytes;
    unsigned ring_bits=0;
    dma_channel_config c, c2;
    uint32_t **reload = tx ? &pio_dma_tx_address : &pio_dma_rx_address;
    if ((tx != 0 && tx != 1) || shift == 3 || !buffer || !count ||
        (ring != 0 && ring != 1) ||
        (!tx && (count & (count-1u))) ||
        (ring && (count & (count-1u))))
        error("Invalid cyclic PIO DMA size");
    if (count > capacity / (1u<<shift))
        error("PIO DMA buffer too small for cyclic block");
    bytes = (size_t)count << shift;
    if ((uintptr_t)buffer & ((1u<<shift)-1u))
        error("Invalid cyclic PIO DMA buffer");
    if (ring) {
        if (!tx) error("RX DMA uses address-reload, not TX ring mode");
        if (bytes > 32768u || ((uintptr_t)buffer & (bytes-1u)))
            error("PIO TX DMA ring requires aligned buffer <= 32768 bytes");
        while ((1u << ring_bits) < bytes) ++ring_bits;
    }
    if (from_c && *pm_pio_dma_slot(tx) && !pm_pio_dma_is_c_hw(tx))
        error("PIO DMA callback already owned by BASIC");
    pm_pio_dma_stop_hw(tx);
    if (tx) { dma_tx_pio=pior; dma_tx_sm=sm; }
    else    { dma_rx_pio=pior; dma_rx_sm=sm; }
    *reload=(uint32_t *)buffer;
    c=dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&c,(enum dma_channel_transfer_size)shift);
    channel_config_set_dreq(&c,pio_get_dreq(pio,(uint)sm,tx!=0));
    channel_config_set_read_increment(&c,tx!=0);
    channel_config_set_write_increment(&c,tx==0);
    channel_config_set_chain_to(&c,ctl);
    if (ring) channel_config_set_ring(&c,false,ring_bits);

    c2=dma_channel_get_default_config(ctl);
    channel_config_set_transfer_data_size(&c2,DMA_SIZE_32);
    channel_config_set_read_increment(&c2,false);
    channel_config_set_write_increment(&c2,false);
    channel_config_set_dreq(&c2,0x3f);
    dma_channel_configure(ctl,&c2,
        tx ? (volatile void *)&dma_hw->ch[ch].al3_read_addr_trig
           : (volatile void *)&dma_hw->ch[ch].al2_write_addr_trig,
        reload,1,false);
    dma_channel_configure(ch,&c,
        tx ? (volatile void *)&pio->txf[sm] : buffer,
        tx ? buffer : (const volatile void *)&pio->rxf[sm],
        count,false);
    dma_start_channel_mask(1u << ctl);
    /* The SM may already be processing the opposite DMA direction.
     * pio_sm_restart() clears ISR/shift counters WITHOUT resetting PC;
     * doing this mid-PULL/MOV/PUSH corrupts the loopback stream. */
    if (!from_c || !(pio->ctrl & (1u << (PIO_CTRL_SM_ENABLE_LSB + sm))))
        pio_sm_restart(pio,sm);
    pio_sm_set_enabled(pio,sm,true);
}

/* BASIC PIO DMA RX with an explicit transfer count and a destination window.
 * The window controls hardware address wrap, NOT the total transfer count.
 * window=1 means all words overwrite the same destination. */
void pm_pio_dma_finite_rx_hw(int pior,int sm,void *buffer,size_t capacity,
                             uint32_t count,unsigned bits,uint32_t window,int from_c)
{
    PIO pio=pm_pio_sm_checked(pior,sm);
    unsigned shift=bits==8?0u:bits==16?1u:bits==32?2u:3u;
    unsigned ring_bits=0;
    size_t window_bytes;
    dma_channel_config cfg;
    if (shift==3 || !buffer || !count || !window ||
        window>32768u || (window & (window-1u)))
        error("Invalid finite PIO RX DMA parameters");
    window_bytes=(size_t)window << shift;
    if (window_bytes>capacity ||
        ((uintptr_t)buffer & ((1u<<shift)-1u)))
        error("Invalid finite PIO RX DMA buffer");
    if (window>1) {
        while ((1u<<ring_bits) < window_bytes) ++ring_bits;
        if (ring_bits>15u || ((uintptr_t)buffer & (window_bytes-1u)))
            error("Invalid finite PIO RX DMA ring alignment or size");
    }
    if (from_c && *pm_pio_dma_slot(0) && !pm_pio_dma_is_c_hw(0))
        error("PIO DMA callback already owned by BASIC");
    pm_pio_dma_stop_hw(0);
    dma_rx_pio=pior;
    dma_rx_sm=sm;
    cfg=dma_channel_get_default_config(dma_rx_chan);
    channel_config_set_transfer_data_size(&cfg,(enum dma_channel_transfer_size)shift);
    channel_config_set_dreq(&cfg,pio_get_dreq(pio,(uint)sm,false));
    channel_config_set_read_increment(&cfg,false);
    channel_config_set_write_increment(&cfg,window!=1);
    if (window>1) channel_config_set_ring(&cfg,true,ring_bits);
    dma_channel_configure(dma_rx_chan,&cfg,buffer,
                          (const volatile void *)&pio->rxf[sm],count,true);
    /* BASIC historically restarts the SM; C may attach RX to an SM that
     * already has TX DMA in flight, so preserve its shift state and PC. */
    if (!from_c || !(pio->ctrl & (1u << (PIO_CTRL_SM_ENABLE_LSB + sm))))
        pio_sm_restart(pio,sm);
    pio_sm_set_enabled(pio,sm,true);
}

void pm_pio_dma_stop_hw(int tx)
{
    unsigned ch,c2;
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    if (pm_resident_dma_owned(tx) && !pm_resident_dma_access_allowed(tx))
        error("PIO DMA channel owned by resident C");
    ch=tx ? dma_tx_chan : dma_rx_chan;
    c2=tx ? dma_tx_chan2 : dma_rx_chan2;
    dma_hw->abort=(1u<<ch)|(1u<<c2);
    if (dma_channel_is_busy(ch)) dma_channel_abort(ch);
    if (dma_channel_is_busy(c2)) dma_channel_abort(c2);
    /* DMA may no longer use the restart word after both channels abort. */
    if (tx) pio_dma_tx_address = NULL;
    else pio_dma_rx_address = NULL;
}
int pm_pio_dma_busy_hw(int tx)
{
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    return dma_channel_is_busy(tx ? dma_tx_chan : dma_rx_chan) ||
           dma_channel_is_busy(tx ? dma_tx_chan2 : dma_rx_chan2);
}
void pm_pio_dma_transfer_hw(int tx,int pior,int sm,void *buffer,
                            size_t capacity,uint32_t count,unsigned bits,int from_c)
{
    PIO pio=pm_pio_sm_checked(pior,sm);
    unsigned chan=tx ? dma_tx_chan : dma_rx_chan;
    unsigned shift=bits==8?0:bits==16?1:bits==32?2:3;
    dma_channel_config cfg;
    if (!buffer || shift==3 || !count ||
        count > capacity / (1u<<shift))
        error("Invalid PIO DMA buffer or transfer size");
    if ((uintptr_t)buffer & ((1u<<shift)-1u))
        error("PIO DMA buffer alignment error");
    if (from_c && *pm_pio_dma_slot(tx) && !pm_pio_dma_is_c_hw(tx))
        error("PIO DMA callback already owned by BASIC");
    pm_pio_dma_stop_hw(tx);
    if (tx) { dma_tx_pio=pior;dma_tx_sm=sm; }
    else { dma_rx_pio=pior;dma_rx_sm=sm; }
    cfg=dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&cfg,(enum dma_channel_transfer_size)shift);
    channel_config_set_dreq(&cfg,pio_get_dreq(pio,sm,tx!=0));
    channel_config_set_read_increment(&cfg,tx!=0);
    channel_config_set_write_increment(&cfg,tx==0);
    dma_channel_configure(chan,&cfg,
        tx ? (volatile void *)&pio->txf[sm] : buffer,
        tx ? buffer : (const volatile void *)&pio->rxf[sm],
        count,true);
    /* Preserve BASIC's original restart semantics. Native C can start
     * RX and TX DMA on the same running SM, and must not restart it for
     * the second direction while instructions are in flight. */
    if (!from_c || !(pio->ctrl & (1u << (PIO_CTRL_SM_ENABLE_LSB + sm))))
        pio_sm_restart(pio,sm);
    pio_sm_set_enabled(pio,sm,true);
}
