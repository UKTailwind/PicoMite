/* Native C entry points to the hardware implementation shared with BASIC.
 * PIO operations live in core/pm_pio_shared.c; BASIC calls the same helpers. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/pm_pio_shared.h"
#include "core/tcc/include/picomite/pio.h"
#include <string.h>
#include <stdio.h>
#include "core/tcc/include/picomite/basic.h"
extern int dma_tx_pio, dma_tx_sm, dma_rx_pio, dma_rx_sm;

/* Only resources started by this RUN may be stopped during teardown. */
static unsigned pm_c_dma_owned;
static const void *pm_c_dma_buffer[2];
static unsigned pm_c_sm_owned[3];
/* BASIC's pioRXinterrupts/pioTXinterrupts/DMAinterruptRX/TX are the
 * common callback slots; pioTXlast is also common. No parallel tables. */
typedef void (*pm_pio_fifo_callback)(int pio, int sm);
typedef void (*pm_pio_dma_callback)(int tx);
static unsigned pm_c_dma_pending;
static bool pm_c_polling;
void pm_pio_runtime_begin(void)
{
    pm_c_dma_owned = 0;
    pm_c_dma_buffer[0] = pm_c_dma_buffer[1] = NULL;
    pm_c_sm_owned[0] = pm_c_sm_owned[1] = pm_c_sm_owned[2] = 0;
    pm_pio_unregister_c_handlers_hw();
    pm_c_dma_pending = 0;
    pm_c_polling = false;
}
void pm_pio_runtime_end(void)
{
    int pio, sm;
    if (pm_c_dma_owned & 1u) pm_pio_dma_stop_hw(0);
    if (pm_c_dma_owned & 2u) pm_pio_dma_stop_hw(1);
    for (pio = 0; pio < 3; ++pio)
        for (sm = 0; sm < 4; ++sm)
            if (pm_c_sm_owned[pio] & (1u << sm))
                if (pm_pio_available_hw(pio)) pm_pio_stop_hw(pio, sm);
    pm_pio_runtime_begin();
}
void pm_pio_runtime_before_free(const void *allocation, size_t size)
{
    uintptr_t start = (uintptr_t)allocation;
    unsigned tx;
    if (!allocation) return;
    for (tx = 0; tx < 2; ++tx) {
        uintptr_t p = (uintptr_t)pm_c_dma_buffer[tx];
        if ((pm_c_dma_owned & (1u << tx)) &&
            p >= start && p - start < size) {
            pm_pio_dma_stop_hw((int)tx);
            pm_c_dma_owned &= ~(1u << tx);
            pm_c_dma_pending &= ~(1u << tx);
            pm_c_dma_buffer[tx] = NULL;
        }
    }
}
static void pm_pio_require_run(void)
{
    if (!pm_runtime_is_running()) error("PIO requires a running C program");
}
static void pm_pio_exec_do(void *p)
{ int *a = p; pm_pio_require_run(); pm_pio_exec_hw(a[0], a[1], (uint16_t)a[2]); }
void pm_pio_exec(int pio, int sm, uint16_t code)
{ int a[] = {pio, sm, code}; pm_core_call(pm_pio_exec_do, a); }
static void pm_pio_write_do(void *p)
{ struct {int pio, sm; uint32_t word;} *a=p;
  pm_pio_require_run(); pm_pio_write_hw(a->pio, a->sm, a->word); }
void pm_pio_write(int pio, int sm, uint32_t word)
{ struct {int pio,sm;uint32_t word;} a={pio,sm,word}; pm_core_call(pm_pio_write_do,&a); }
static void pm_pio_read_do(void *p)
{ struct {int pio,sm; uint32_t word; int got;} *a=p;
  pm_pio_require_run(); a->got=pm_pio_read_hw(a->pio,a->sm,&a->word); }
int pm_pio_read(int pio, int sm, uint32_t *out)
{
    struct {int pio,sm;uint32_t word;int got;} a={pio,sm,0,0};
    if (!out) return 0;
    pm_core_call(pm_pio_read_do, &a);
    if (a.got) *out = a.word;
    return a.got;
}
static void pm_pio_sm_do(void *p)
{
    int *a = p;
    pm_pio_require_run();
    if (a[0] < 0 || a[0] >= 3 || a[1] < 0 || a[1] >= 4)
        error("Invalid PIO state machine");
    if (a[2]) {
        pm_pio_start_hw(a[0],a[1]);
        pm_c_sm_owned[a[0]] |= 1u << a[1];
    } else {
        pm_pio_stop_hw(a[0],a[1]);
        pm_c_sm_owned[a[0]] &= ~(1u << a[1]);
    }
}
void pm_pio_start(int pio, int sm)
{ int a[]={pio,sm,1}; pm_core_call(pm_pio_sm_do,a); }
void pm_pio_stop(int pio, int sm)
{ int a[]={pio,sm,0}; pm_core_call(pm_pio_sm_do,a); }
static void pm_pio_init_do(void *p)
{ struct {int pio,sm;uint32_t pinctrl,execctrl,shiftctrl;
          int start;float clock;int sideout,setout,outout;} *a=p;
  pm_pio_require_run();
  pm_pio_init_hw(a->pio,a->sm,a->pinctrl,a->execctrl,a->shiftctrl,
                 a->start,a->clock,a->sideout,a->setout,a->outout); }
void pm_pio_init_machine(int pio,int sm,uint32_t pinctrl,uint32_t execctrl,
                          uint32_t shiftctrl,int start,float clock,
                          int sideout,int setout,int outout)
{
    struct {int pio,sm;uint32_t pinctrl,execctrl,shiftctrl;
            int start;float clock;int sideout,setout,outout;} a=
        {pio,sm,pinctrl,execctrl,shiftctrl,start,clock,sideout,setout,outout};
    pm_core_call(pm_pio_init_do,&a);
}
typedef struct {
    int pio;
    const uint16_t *words;
    size_t count;
} pm_pio_program_args;
/* Replacing instruction memory stops all four SMs; a running DMA
 * must first be stopped by its owner to avoid stranding a transfer. */
static void pm_pio_require_idle_dma(void)
{
    if ((pm_c_dma_owned & 1u) && pm_pio_dma_busy_hw(0))
        error("Stop C DMA RX before replacing PIO program");
    if ((pm_c_dma_owned & 2u) && pm_pio_dma_busy_hw(1))
        error("Stop C DMA TX before replacing PIO program");
}
static void pm_pio_program_do(void *arg)
{
    const pm_pio_program_args *a = arg;
    pm_pio_require_run();
    pm_pio_require_idle_dma();
    pm_pio_program_hw(a->pio, a->words, a->count);
    pm_c_sm_owned[a->pio] = 0; /* PROGRAM stops all SMs on the block */
}
void pm_pio_program(int pio, const uint16_t *words, size_t count)
{
    pm_pio_program_args args = {pio, words, count};
    pm_core_call(pm_pio_program_do, &args);
}
/* PIO ASSEMBLE owns parsing, labels and directives.  Quote one line only. */
void pm_pio_assemble(int pio,const char *line)
{
    char cmd[STRINGSIZE];
    int n;
    pm_pio_require_run();
    if (!line || strpbrk(line, "\"\r\n"))
        error("Invalid PIO assembler line");
    if (!pm_pio_available(pio))
        error("PIO unavailable");
    pm_pio_require_idle_dma();
    n=snprintf(cmd,sizeof cmd,"PIO ASSEMBLE %d,\"%s\"",pio,line);
    if (n<0 || (size_t)n>=sizeof cmd)
        error("PIO assembler line too long");
    pm_execute(cmd);
}
static void pm_pio_program_line_do(void *arg)
{
    int *a = arg;
    pm_pio_require_run();
    pm_pio_program_line_hw(a[0], a[1], (uint16_t)a[2]);
}
void pm_pio_program_line(int pio, int slot, uint16_t instruction)
{
    int args[] = {pio, slot, instruction};
    pm_core_call(pm_pio_program_line_do, args);
}
static void pm_pio_clear_do(void *arg)
{
    int pio = *(int *)arg;
    pm_pio_require_run();
    pm_pio_require_idle_dma();
    pm_pio_clear_hw(pio);
    pm_c_sm_owned[pio] = 0; /* CLEAR stops all SMs on the block */
}
void pm_pio_clear(int pio)
{ pm_core_call(pm_pio_clear_do, &pio); }
static void pm_pio_flevel_do(void *arg)
{
    struct {int pio, sm, tx; uint32_t level;} *a = arg;
    pm_pio_require_run();
    a->level = pm_pio_flevel_hw(a->pio, a->sm, a->tx);
}
uint32_t pm_pio_flevel(int pio, int sm, int tx)
{
    struct {int pio, sm, tx; uint32_t level;} args = {pio, sm, tx, 0};
    pm_core_call(pm_pio_flevel_do, &args);
    return args.level;
}

static void pm_pio_status_do(void *arg)
{
    struct {int pio, sm, fifo, kind; uint32_t value;} *a = arg;
    pm_pio_require_run();
    if (a->kind == 0) a->value = pm_pio_fstat_hw(a->pio);
    else if (a->kind == 1) a->value = pm_pio_fdebug_hw(a->pio);
    else a->value = pm_pio_readfifo_hw(a->pio, a->sm, a->fifo);
}
static uint32_t pm_pio_status(int pio, int sm, int fifo, int kind)
{
    struct {int pio, sm, fifo, kind; uint32_t value;} a={pio,sm,fifo,kind,0};
    pm_core_call(pm_pio_status_do,&a);
    return a.value;
}
uint32_t pm_pio_fstat(int pio) { return pm_pio_status(pio,0,0,0); }
uint32_t pm_pio_fdebug(int pio) { return pm_pio_status(pio,0,0,1); }
uint32_t pm_pio_readfifo(int pio,int sm,int fifo)
{ return pm_pio_status(pio,sm,fifo,2); }

static void pm_pio_configure_do(void *p)
{
    struct {int pio,sm; const pm_pio_config *config;} *a = p;
    pm_pio_require_run();
    pm_pio_configure_hw(a->pio, a->sm, a->config);
}
void pm_pio_configure(int pio, int sm, const pm_pio_config *config)
{
    struct {int pio,sm; const pm_pio_config *config;} a = {pio,sm,config};
    pm_core_call(pm_pio_configure_do, &a);
}
static void pm_pio_sync_do(void *p)
{
    struct {int pio; unsigned my,prev,next;} *a = p;
    pm_pio_require_run();
    pm_pio_sync_hw(a->pio, a->my, a->prev, a->next);
}
void pm_pio_sync(int pio, unsigned my, unsigned prev, unsigned next)
{
    struct {int pio; unsigned my,prev,next;} a = {pio,my,prev,next};
    pm_core_call(pm_pio_sync_do, &a);
}
static void pm_pio_idle_do(void *p)
{ int *a=p; a[1]=pm_pio_idle_hw(a[0]); }
int pm_pio_idle(int pio)
{ int a[]={pio,0};pm_core_call(pm_pio_idle_do,a);return a[1]; }
static void pm_pio_writefifo_do(void *p)
{ struct {int pio,sm,fifo;uint32_t value;} *a=p;
  pm_pio_require_run();pm_pio_writefifo_hw(a->pio,a->sm,a->fifo,a->value); }
void pm_pio_writefifo(int pio,int sm,int fifo,uint32_t value)
{ struct {int pio,sm,fifo;uint32_t value;} a={pio,sm,fifo,value};
  pm_core_call(pm_pio_writefifo_do,&a); }
static void pm_pio_set_base_do(void *p)
{ int *a=p;pm_pio_require_run();pm_pio_set_base_hw(a[0],a[1]); }
void pm_pio_set_base(int pio,int base)
{ int a[]={pio,base};pm_core_call(pm_pio_set_base_do,a); }
static void pm_pio_available_do(void *p)
{ int *a=p; a[1]=pm_pio_available_hw(a[0]); }
int pm_pio_available(int pio)
{ int a[]={pio,0};pm_core_call(pm_pio_available_do,a);return a[1]; }

typedef struct {
    int tx,pio,sm;
    void *ptr;
    size_t capacity;
    uint32_t count;
    unsigned bits;
    int mode, ring; /* 0=one-shot, 1=cyclic, 2=finite RX window */
    uint32_t window;
} pm_pio_dma_args;
static void pm_pio_dma_do(void *arg)
{
    pm_pio_dma_args *a=arg;
    unsigned mask=1u << a->tx;
    pm_pio_require_run();
    if (pm_resident_dma_owned(a->tx))
        error("PIO DMA channel owned by resident C");
    if (a->pio < 0 || a->pio >= 3 || a->sm < 0 || a->sm >= 4)
        error("Invalid PIO state machine");
    if (!a->count || !a->capacity ||
        !pm_runtime_owns_range(a->ptr,a->capacity))
        error("PIO DMA requires a live C allocation");
    /* Set ownership BEFORE touching hardware: errors must still clean up. */
    if (pm_pio_dma_busy_hw(a->tx))
        error("PIO DMA channel in use");
    if ((a->tx ? DMAinterruptTX : DMAinterruptRX) &&
        !pm_pio_dma_is_c_hw(a->tx))
        error("PIO DMA callback already owned by BASIC");
    /* Record before configuring hardware so teardown remains safe when an
     * MMBasic error longjmps out of the shared hardware implementation. */
    pm_c_dma_owned |= mask;
    pm_c_dma_buffer[a->tx] = a->ptr;
    if (a->mode == 1) {
        /* Cyclic channels never finish; do not synthesize a completion
         * callback on a momentary DMA channel-chain gap. */
        pm_c_dma_pending &= ~mask;
        pm_pio_dma_cyclic_hw(a->tx,a->pio,a->sm,a->ptr,a->capacity,
                             a->count,a->bits,a->ring,1);
    } else {
        pm_c_dma_pending |= mask;
        if (a->mode == 2) {
            pm_pio_dma_finite_rx_hw(a->pio,a->sm,a->ptr,a->capacity,
                                    a->count,a->bits,a->window,1);
        } else {
            pm_pio_dma_transfer_hw(a->tx,a->pio,a->sm,a->ptr,a->capacity,
                                   a->count,a->bits,1);
        }
    }
    pm_c_sm_owned[a->pio] |= 1u << a->sm;
}
void pm_pio_dma_rx(int pio,int sm,void *buf,size_t capacity,uint32_t count,unsigned bits)
{ pm_pio_dma_args a={0,pio,sm,buf,capacity,count,bits,0,0,0};pm_core_call(pm_pio_dma_do,&a); }
void pm_pio_dma_tx(int pio,int sm,const void *buf,size_t capacity,uint32_t count,unsigned bits)
{ pm_pio_dma_args a={1,pio,sm,(void *)buf,capacity,count,bits,0,0,0};
  pm_core_call(pm_pio_dma_do,&a); }
/* Finite RX with a 1-element overwrite target or an address-wrapping ring.
 * This is the same hardware path used by BASIC PIO DMA RX count,...,size. */
void pm_pio_dma_rx_ring(int pio,int sm,void *buf,size_t capacity,
                        uint32_t count,unsigned bits,uint32_t window)
{ pm_pio_dma_args a={0,pio,sm,buf,capacity,count,bits,2,0,window};
  pm_core_call(pm_pio_dma_do,&a); }
/* Cyclic RX reloads the same destination block; cyclic TX can either
 * replay via address reload or wrap in a power-of-two TX ring. */
void pm_pio_dma_cyclic_rx(int pio,int sm,void *buf,size_t capacity,uint32_t count,unsigned bits)
{ pm_pio_dma_args a={0,pio,sm,buf,capacity,count,bits,1,0,0};
  pm_core_call(pm_pio_dma_do,&a); }
void pm_pio_dma_cyclic_tx(int pio,int sm,const void *buf,size_t capacity,
                          uint32_t count,unsigned bits,int ring)
{ pm_pio_dma_args a={1,pio,sm,(void *)buf,capacity,count,bits,1,ring,0};
  pm_core_call(pm_pio_dma_do,&a); }
static void pm_pio_dma_busy_do(void *p)
{ int *a=p; pm_pio_require_run();a[1]=pm_pio_dma_busy_hw(a[0]); }
int pm_pio_dma_busy(int tx)
{ int a[]={tx,0};pm_core_call(pm_pio_dma_busy_do,a);return a[1]; }
static void pm_pio_dma_stop_do(void *p)
{ int tx=*(int *)p;
  pm_pio_require_run();
  if (tx < 0 || tx > 1 || !(pm_c_dma_owned & (1u << tx)))
      error("PIO DMA not owned by C program");
  pm_pio_dma_stop_hw(tx);
  pm_c_dma_owned &= ~(1u << tx);
  pm_c_dma_pending &= ~(1u << tx);
  pm_c_dma_buffer[tx] = NULL; }
void pm_pio_dma_stop(int tx)
{ pm_core_call(pm_pio_dma_stop_do,&tx); }

/* Callbacks execute only at C API / explicit poll boundaries, never in an
 * IRQ. A callback may call other C API services; guard nested polling. */
void pm_pio_runtime_poll(void)
{
    int pior, sm, tx;
    if (!pm_runtime_is_running() || pm_c_polling) return;
    pm_c_polling = true;
    for (pior = 0; pior < 3; ++pior) {
        if (!pm_pio_available_hw(pior)) continue;
        for (sm = 0; sm < 4; ++sm) {
            pm_pio_fifo_callback rx = pm_pio_c_fifo_handler_hw(pior, sm, 0);
            pm_pio_fifo_callback txcb = pm_pio_c_fifo_handler_hw(pior, sm, 1);
            if (rx && pm_pio_fifo_event_hw(pior, sm, 0, NULL))
                rx(pior, sm);
            if (txcb && pm_pio_fifo_event_hw(pior, sm, 1,
                                             &pioTXlast[sm][pior]))
                txcb(pior, sm);
        }
    }
    for (tx = 0; tx < 2; ++tx) {
        unsigned mask = 1u << tx;
        if (!(pm_c_dma_pending & mask) ||
            !pm_pio_dma_complete_hw(tx)) continue;
        pm_c_dma_pending &= ~mask; /* notify at most once per transfer */
        pm_pio_dma_callback cb = pm_pio_c_dma_handler_hw(tx);
        if (cb) {
            int pior = tx ? dma_tx_pio : dma_rx_pio;
            int sm = tx ? dma_tx_sm : dma_rx_sm;
            pm_pio_stop_hw(pior, sm);
            cb(tx);
        }
    }
    pm_c_polling = false;
}
void pm_pio_poll(void)
{
    pm_pio_require_run();
    pm_pio_runtime_poll();
}
static void pm_pio_fifo_callback_do(void *arg)
{
    struct {int pio, sm, tx; pm_pio_fifo_callback fn;} *a = arg;
    pm_pio_require_run();
    if (a->pio < 0 || a->pio >= 3 || a->sm < 0 || a->sm >= 4 ||
        (a->tx != 0 && a->tx != 1) || !pm_pio_available_hw(a->pio))
        error("Invalid PIO FIFO interrupt selection");
    pm_pio_register_c_fifo_hw(a->pio, a->sm, a->tx, a->fn);
}
void pm_pio_on_fifo(int pio, int sm, int tx, pm_pio_fifo_callback callback)
{
    struct {int pio, sm, tx; pm_pio_fifo_callback fn;} a =
        {pio, sm, tx, callback};
    pm_core_call(pm_pio_fifo_callback_do, &a);
}
static void pm_pio_dma_callback_do(void *arg)
{
    struct {int tx; pm_pio_dma_callback fn;} *a = arg;
    pm_pio_require_run();
    if (a->tx != 0 && a->tx != 1)
        error("Invalid PIO DMA direction");
    pm_pio_register_c_dma_hw(a->tx, a->fn);
}
void pm_pio_on_dma_done(int tx, pm_pio_dma_callback callback)
{
    struct {int tx; pm_pio_dma_callback fn;} a = {tx, callback};
    pm_core_call(pm_pio_dma_callback_do, &a);
}
static void pm_pio_dma_pointer_do(void *arg)
{
    struct {int tx; uintptr_t value;} *a = arg;
    pm_pio_require_run();
    if ((a->tx != 0 && a->tx != 1) ||
        !(pm_c_dma_owned & (1u << a->tx)))
        error("PIO DMA not owned by C program");
    a->value = pm_pio_dma_pointer_hw(a->tx);
}
uintptr_t pm_pio_dma_pointer(int tx)
{
    struct {int tx; uintptr_t value;} a = {tx, 0};
    pm_core_call(pm_pio_dma_pointer_do, &a);
    return a.value;
}

typedef struct {const char *name;const void *address;} pm_pio_symbol;
#define PM_PIO_SYM(n) {#n,(const void *)&n},
static const pm_pio_symbol pm_pio_syms[]={
    PM_PIO_SYM(pm_pio_available)
    PM_PIO_SYM(pm_pio_idle)
    PM_PIO_SYM(pm_pio_writefifo)
    PM_PIO_SYM(pm_pio_set_base)
    PM_PIO_SYM(pm_pio_exec)
    PM_PIO_SYM(pm_pio_write)
    PM_PIO_SYM(pm_pio_read)
    PM_PIO_SYM(pm_pio_start)
    PM_PIO_SYM(pm_pio_stop)
    PM_PIO_SYM(pm_pio_init_machine)
    PM_PIO_SYM(pm_pio_program)
    PM_PIO_SYM(pm_pio_assemble)
    PM_PIO_SYM(pm_pio_program_line)
    PM_PIO_SYM(pm_pio_clear)
    PM_PIO_SYM(pm_pio_flevel)
    PM_PIO_SYM(pm_pio_fstat)
    PM_PIO_SYM(pm_pio_fdebug)
    PM_PIO_SYM(pm_pio_readfifo)
    PM_PIO_SYM(pm_pio_configure)
    PM_PIO_SYM(pm_pio_sync)
    PM_PIO_SYM(pm_pio_dma_rx)
    PM_PIO_SYM(pm_pio_dma_tx)
    PM_PIO_SYM(pm_pio_dma_rx_ring)
    PM_PIO_SYM(pm_pio_dma_cyclic_rx)
    PM_PIO_SYM(pm_pio_dma_cyclic_tx)
    PM_PIO_SYM(pm_pio_dma_busy)
    PM_PIO_SYM(pm_pio_dma_stop)
    PM_PIO_SYM(pm_pio_dma_pointer)
    PM_PIO_SYM(pm_pio_on_fifo)
    PM_PIO_SYM(pm_pio_on_dma_done)
    PM_PIO_SYM(pm_pio_poll)
};
const void *pm_pio_resolve_symbol(const char *name)
{
    unsigned i;
    for (i=0;i<sizeof(pm_pio_syms)/sizeof(pm_pio_syms[0]);i++)
        if (!strcmp(pm_pio_syms[i].name,name)) return pm_pio_syms[i].address;
    return NULL;
}
