/* Native C services common to the RP2040 ELF runner and RP2350 TinyCC.
 * This module is linked with PicoMite itself, never with the compiler library.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/pm_pio_shared.h"
#include "core/tcc/include/picomite/input.h"
#include "pico/time.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
extern char *DMAinterruptRX, *DMAinterruptTX;

/* Only physical keyboard paths call KeyboardEmit(). A resident owns the
 * installed callback and must restore its predecessor before unloading.
 * NULL is the identity mapping. The pointer is deliberately independent
 * of pm_resident_start/stop and survives BASIC RUN/END. */
static uint8_t pm_remap_identity(uint8_t code, uint16_t key,
                                uint8_t modifiers, uint8_t flags)
{
    (void)key; (void)modifiers; (void)flags;
    return code;
}
#ifdef rp2350
/* Keep the one-word hook out of the tightly packed main SRAM region. */
static pm_remap_fn volatile pm_remap
    __attribute__((section(".scratch_x.pm_remap")));
#else
static pm_remap_fn volatile pm_remap;
#endif

pm_remap_fn pm_set_remap_fn(pm_remap_fn fn)
{
    pm_remap_fn previous = pm_remap;
    pm_remap = fn; /* Aligned pointer store; NULL restores identity. */
    return previous ? previous : pm_remap_identity;
}

/* May be reached from the PS/2 interrupt: no runtime or BASIC calls here. */
uint8_t KeyboardEmit(uint8_t code, uint16_t key,
                     uint8_t modifiers, uint8_t flags)
{
    pm_remap_fn fn = pm_remap;
    return fn ? fn(code, key, modifiers, flags) : code;
}

/* Firmware-only printable-byte classification for console and editor.
 * The resident owns the callback; no implicit cleanup on RUN/END/STOP. */
static int pm_isprint_default(int code)
{
    return code >= 0 && code <= 255 && isprint((unsigned char)code) != 0;
}
#ifdef rp2350
static pm_isprint_fn volatile pm_isprint_handler
    __attribute__((section(".scratch_x.pm_isprint")));
#else
static pm_isprint_fn volatile pm_isprint_handler;
#endif

pm_isprint_fn pm_set_isprint_fn(pm_isprint_fn fn)
{
    pm_isprint_fn previous = pm_isprint_handler;
    pm_isprint_handler = fn; /* NULL selects the original classification. */
    return previous ? previous : pm_isprint_default;
}

int pm_isprint(int code)
{
    pm_isprint_fn fn = pm_isprint_handler;
    return fn ? fn(code) : pm_isprint_default(code);
}

/* The console-font override is transient, not Option.DefaultFont in flash.
 * Font availability changes during PrepareProgram() and LIBRARY removal;
 * resolve it at the prompt, after fonts have been registered. */
#ifdef rp2350
static volatile int pm_console_font_override
    __attribute__((section(".scratch_x.pm_console_font")));
static int pm_console_font_saved_prompt
    __attribute__((section(".scratch_x.pm_console_font_saved")));
#else
static volatile int pm_console_font_override;
static int pm_console_font_saved_prompt;
#endif

int pm_set_console_font(int font)
{
    int previous = pm_console_font_override;
    int selected;
    if (font < 0 || font > FONT_TABLE_SIZE)
        return -1;
    /* Remember the original prompt font before the first override. */
    if (!previous && font)
        pm_console_font_saved_prompt = PromptFont;
    else if (previous && !font) {
        int saved = pm_console_font_saved_prompt;
        PromptFont = saved > 0 && ((unsigned)saved >> 4) < FONT_TABLE_SIZE &&
                     FontTable[(unsigned)saved >> 4] ? saved : Option.DefaultFont;
    }
    pm_console_font_override = font; /* 0: no override; otherwise 1-based. */
    /* The prompt path applies this override only at the NEXT prompt.  A
     * resident can also be stopped from within a BASIC program: its next
     * PRINT must already use the restored font.  Do not change PromptFont
     * or the VGA/HDMI tile-colour geometry (cmd_font() does both). */
    selected = pm_console_font_for_prompt();
    if (Option.DISPLAY_CONSOLE && FontTable[selected >> 4])
        SetFont(selected);
    return previous;
}

int pm_console_font_for_prompt(void)
{
    int font = pm_console_font_override;
    if (font > 0 && font <= FONT_TABLE_SIZE && FontTable[font - 1]) {
        /* EDIT takes its entry/exit font from PromptFont, not gui_font. */
        PromptFont = ((font - 1) << 4) | 1;
        return PromptFont;
    }
    /* A removed LIBRARY may make the selected font unavailable. */
    if (font > 0) {
        int saved = pm_console_font_saved_prompt;
        PromptFont = saved > 0 && ((unsigned)saved >> 4) < FONT_TABLE_SIZE &&
                     FontTable[(unsigned)saved >> 4] ? saved : Option.DefaultFont;
    } else if (PromptFont <= 0 ||
               ((unsigned)PromptFont >> 4) >= FONT_TABLE_SIZE ||
               !FontTable[(unsigned)PromptFont >> 4]) {
        PromptFont = Option.DefaultFont;
    }
    return PromptFont;
}

/* Resident code is fixed in flash LIBRARY. Its data must survive InitHeap(),
 * which clears the BASIC heap on RUN/END. The descriptor is allocated from
 * the separate newlib heap only when a resident is installed; no resident
 * state is reserved in SRAM by the linker. Resident DMA may reference only
 * these state words or a separate, managed newlib allocation. */
typedef void (*pm_resident_callback)(void *);
typedef void (*pm_resident_gpio_callback)(int, unsigned, void *);
typedef struct {
    uint32_t words[16];
    uint64_t next_us;
    pm_resident_callback fn;
    pm_resident_gpio_callback gpio_fn;
    uint32_t period_us;
    int gpio_pin;  /* PicoMite pin number; zero means no subscription. */
    int gpio_gp;   /* Physical GPIO number passed to the C callback. */
    unsigned gpio_edges; /* PM_GPIO_RISE / PM_GPIO_FALL mask. */
    uint64_t pio_pins; /* Physical GPIOs, kept through ClearExternalIO(). */
    uint64_t gpio_claims; /* GPIOs claimed by the resident, no hardware reset. */
    unsigned char pio_owner; /* PIO block + 1, zero is unclaimed. */
    unsigned char dma_owned; /* bit 0: RX; bit 1: TX */
    unsigned char dma_halted; /* claimed channel explicitly stopped */
    unsigned char dma_buffer_mask; /* legacy managed-buffer API */
    unsigned dma_region[2]; /* bitset of words used in the 64-byte state */
    unsigned dma_buffer_offset[2], dma_buffer_count[2];
    uint32_t *dma_buffer; /* separately allocated, persistent system heap */
    unsigned dma_buffer_words;
    bool pio_access; /* Temporary privilege while using shared PIO helpers. */
    bool gpio_access; /* Scoped permission to configure a claimed GPIO. */
    bool busy;
    bool faulted; /* disable callbacks on abnormal longjmp, retain ownership */
} pm_resident_descriptor;
/* M2P2HDMI has no slack in the main RAM linker region. The single pointer
 * resides in the otherwise unused 4 KB SCRATCH_X bank on RP2350. */
#ifdef rp2350
static pm_resident_descriptor *pm_resident
    __attribute__((section(".scratch_x.pm_resident")));
#else
static pm_resident_descriptor *pm_resident;
#endif
extern const uint8_t *flash_libmemory;
extern unsigned char *LibMemory;
extern unsigned char *CFunctionLibrary;

/* Accept only Thumb code within a CSUB's binary record in flash LIBRARY.
 * The current BASIC program and RAM LIBRARY can be replaced at the next RUN. */
static bool pm_resident_is_flash_csub(const void *fn)
{
    uintptr_t ptr = (uintptr_t)fn;
    uintptr_t base = (uintptr_t)flash_libmemory;
    uintptr_t end = base + MAX_PROG_SIZE;
    const uint32_t *record = (const uint32_t *)CFunctionLibrary;
    if (!(ptr & 1u) || LibMemory != flash_libmemory || !record ||
        Option.LIBRARY_FLASH_SIZE != MAX_PROG_SIZE)
        return false;
    ptr &= ~(uintptr_t)1u;
    if (ptr < base || ptr >= end) return false;
    while ((uintptr_t)record >= base && (uintptr_t)record < end &&
           end - (uintptr_t)record >= 8u) {
        uintptr_t body = (uintptr_t)record + 8u;
        uint32_t count = record[1];
        if (record[0] == 0xffffffffu) break;
        if ((count & 3u) || count == 0u || count > end - body) break;
        if (!(record[0] & 0x80000000u) && ptr >= body && ptr < body + count)
            return true;
        record = (const uint32_t *)(body + count);
    }
    return false;
}

/* As for BASIC SETPIN INTH/INTL/INTB, use the IO bank's *raw* edge
 * latches. Do not install another IO_IRQ_BANK0 handler: PicoMite already
 * owns that interrupt for IR, PS/2, counters and other devices. */
#define PM_RESIDENT_GPIO_EDGE_BITS 0xCu
static uint32_t pm_resident_gpio_raw_mask(int gp)
{
    return PM_RESIDENT_GPIO_EDGE_BITS << (4u * ((unsigned)gp & 7u));
}

/* Called before the descriptor is reused/freed. Clearing ownership first
 * allows ExtCfg() to perform the normal PicoMite pin cleanup. */
static void pm_resident_gpio_edge_detach(pm_resident_descriptor *r)
{
    int pin = r->gpio_pin;
    if (!pin) return;
    r->gpio_fn = NULL;
    r->gpio_pin = 0;
    r->gpio_edges = 0;
    io_bank0_hw->intr[r->gpio_gp >> 3] = pm_resident_gpio_raw_mask(r->gpio_gp);
    r->gpio_access = true;
    ExtCfg(pin, EXT_NOT_CONFIG, 0);
    r->gpio_access = false;
}

int pm_resident_gpio_pin_owned(int pin)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn) return 0;
    return ((r->pio_pins | r->gpio_claims) &
            (1ull << PinDef[pin].GPno)) != 0;
}
/* Only the explicit native GPIO configuration path may pass CheckPin().
 * CP_NOABORT (used by ClearExternalIO) must always respect ownership. */
int pm_resident_gpio_access_allowed(int pin)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned gp = PinDef[pin].GPno;
    return r && r->gpio_access && gp < 64u &&
           (r->gpio_claims & (1ull << gp)) != 0;
}

/* A claim protects a physical GPIO from BASIC END/RUN and SETPIN, but
 * does not configure the pad, install an IRQ or reset hardware on release.
 * The resident owns both the setup and teardown. */
int pm_resident_gpio_claim(int gp)
{
    pm_resident_descriptor *r = pm_resident;
    int pin;
    uint64_t bit;
    if (!r || !r->fn || pm_runtime_is_running() ||
        gp < 0 || gp >= NUM_BANK0_GPIOS) return -1;
#ifdef rp2350
    if (rp2350a && gp > 29) return -1;
#endif
    bit = 1ull << (unsigned)gp;
    if (r->gpio_claims & bit) return 0;
    pin = codemap(gp);
    if (IsInvalidPin(pin) || ExtCurrentConfig[pin] != EXT_NOT_CONFIG ||
        !CheckPin(pin, CP_NOABORT)) return -1;
    r->gpio_claims |= bit;
    return 0;
}
int pm_resident_gpio_release(int gp)
{
    pm_resident_descriptor *r = pm_resident;
    uint64_t bit;
    if (!r || pm_runtime_is_running() ||
        gp < 0 || gp >= NUM_BANK0_GPIOS) return -1;
    bit = 1ull << (unsigned)gp;
    if (!(r->gpio_claims & bit)) return -1;
    if (r->gpio_pin && r->gpio_gp == gp) return -1;
    r->gpio_claims &= ~bit; /* The resident must restore the pad itself. */
    return 0;
}

/* These checks also protect the BASIC PIO commands because they use the
 * shared hardware helpers. The one exception is the assembler in Custom.c. */
int pm_resident_pio_block_owned(int pio)
{
    return pm_resident && pm_resident->pio_owner == pio + 1;
}
int pm_resident_pio_access_allowed(int pio)
{
    return pm_resident_pio_block_owned(pio) && pm_resident->pio_access;
}
int pm_resident_pio_any_owned(void)
{
    return pm_resident && pm_resident->pio_owner != 0;
}
/* A resident keeps ownership even when its main callback has been detached:
 * teardown clears these bits only AFTER both DMA channels have stopped. */
int pm_resident_dma_owned(int tx)
{
    pm_resident_descriptor *r = pm_resident;
    return (tx == 0 || tx == 1) && r &&
           (r->dma_owned & (1u << tx)) != 0;
}
int pm_resident_dma_access_allowed(int tx)
{
    return pm_resident_dma_owned(tx) && pm_resident->pio_access;
}

int pm_resident_gpio_on_edge(int gp, unsigned edges, int pull,
                              pm_resident_gpio_callback callback)
{
    pm_resident_descriptor *r = pm_resident;
    int pin, cfg, option;
    if (!r || !r->fn || r->busy || pm_runtime_is_running() || !callback ||
        edges < PM_GPIO_RISE || edges > PM_GPIO_BOTH ||
        pull < PM_GPIO_NOPULL || pull > PM_GPIO_PULLDOWN ||
        !pm_resident_is_flash_csub((const void *)callback)) return -1;
    /* codemap() reports errors, so reject out-of-range numbers first. */
    if (gp < 0 || gp >= NUM_BANK0_GPIOS) return -1;
#ifdef rp2350
    if (rp2350a && gp > 29) return -1;
#endif
    pin = codemap(gp);
    if (IsInvalidPin(pin) || !(PinDef[pin].mode & DIGITAL_IN) ||
        !(r->gpio_claims & (1ull << (unsigned)gp))) return -1;
    /* A subscription never claims a GPIO: claim() must precede on_edge().
     * Switching the one callback leaves all GPIO claims in place. */
    if (r->gpio_pin) pm_resident_gpio_edge_detach(r);
    cfg = edges == PM_GPIO_RISE ? EXT_INT_HI :
          edges == PM_GPIO_FALL ? EXT_INT_LO : EXT_INT_BOTH;
    option = pull == PM_GPIO_PULLUP ? CNPUSET :
             pull == PM_GPIO_PULLDOWN ? CNPDSET : 0;
#ifdef rp2350
    if (pull == PM_GPIO_PULLDOWN) {
        PinSetBit(pin, TRISCLR);
        PinSetBit(pin, LATCLR);
        PinSetBit(pin, TRISSET);
    }
#endif
    r->gpio_access = true;
    ExtCfg(pin, cfg, option);
    r->gpio_access = false;
    io_bank0_hw->intr[gp >> 3] = pm_resident_gpio_raw_mask(gp);
    r->gpio_gp = gp;
    r->gpio_edges = edges;
    r->gpio_fn = callback;
    r->gpio_pin = pin;
    return 0;
}

void pm_resident_gpio_off(void)
{
    if (pm_resident) pm_resident_gpio_edge_detach(pm_resident);
}

/* The resident, not the kernel, must halt and release DMA before PIO.
 * Keeping the claim while idle is necessary: BASIC must not reuse it. */
static int pm_resident_pio_release_hw(pm_resident_descriptor *r)
{
    unsigned pio, gp;
    uint64_t pins;
    if (!r->pio_owner) return 0;
    if (r->dma_owned) return -1;
    pio = r->pio_owner - 1u;
    r->pio_access = true;
    pm_pio_clear_hw((int)pio); /* Disable all SMs before releasing GPIOs. */
    r->pio_access = false;
    r->pio_owner = 0;
    pins = r->pio_pins;
    r->pio_pins = 0;
    for (gp = 0; gp < NUM_BANK0_GPIOS; ++gp) {
        if (!(pins & (1ull << gp))) continue;
        ExtCfg(codemap((int)gp), EXT_NOT_CONFIG, 0);
    }
    return 0;
}

int pm_resident_pio_claim(int pio)
{
    pm_resident_descriptor *r = pm_resident;
    int pin, cfg;
    if (!r || !r->fn || r->busy || pm_runtime_is_running() ||
        r->pio_owner || pio < 0 || pio >= PIOMAX ||
        !pm_pio_available_hw(pio) || !pm_pio_idle_hw(pio)) return -1;
    cfg = pio == 0 ? EXT_PIO0_OUT : EXT_PIO1_OUT;
#ifdef rp2350
    if (pio == 2) cfg = EXT_PIO2_OUT;
#endif
    /* A disabled SM can still own pins configured by BASIC. Refuse it. */
    for (pin = 1; pin <= NBRPINS; ++pin)
        if (!IsInvalidPin(pin) && ExtCurrentConfig[pin] == cfg) return -1;
    r->pio_owner = (unsigned char)(pio + 1);
    return 0;
}
int pm_resident_pio_release(void)
{
    if (!pm_resident || pm_runtime_is_running()) return -1;
    return pm_resident_pio_release_hw(pm_resident);
}
int pm_resident_pio_pin(int gp)
{
    pm_resident_descriptor *r = pm_resident;
    int pin, cfg, pio;
    if (!r || !r->fn || !r->pio_owner || pm_runtime_is_running() ||
        gp < 0 || gp >= NUM_BANK0_GPIOS) return -1;
#ifdef rp2350
    if (rp2350a && gp > 29) return -1;
#endif
    pin = codemap(gp);
    if (IsInvalidPin(pin) || !CheckPin(pin, CP_NOABORT) ||
        ExtCurrentConfig[pin] != EXT_NOT_CONFIG) return -1;
    pio = r->pio_owner - 1;
    cfg = pio == 0 ? EXT_PIO0_OUT : EXT_PIO1_OUT;
#ifdef rp2350
    if (pio == 2) cfg = EXT_PIO2_OUT;
#endif
    ExtCfg(pin, cfg, 0);
    r->pio_pins |= 1ull << gp;
    return 0;
}
int pm_resident_pio_program(const uint16_t *words, unsigned count)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || !r->pio_owner ||
        pm_runtime_is_running() || !words || count == 0 || count > 32)
        return -1;
    r->pio_access = true;
    pm_pio_program_hw(r->pio_owner - 1, words, count);
    r->pio_access = false;
    return 0;
}
int pm_resident_pio_init_machine(int sm, uint32_t pinctrl,
                                 uint32_t execctrl, uint32_t shiftctrl,
                                 int start, unsigned clock_hz,
                                 int sideout, int setout, int outout)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || !r->pio_owner ||
        pm_runtime_is_running() || sm < 0 || sm >= 4 ||
        start < 0 || start > 31) return -1;
    r->pio_access = true;
    pm_pio_init_hw(r->pio_owner - 1, sm, pinctrl, execctrl, shiftctrl,
                   start, (float)clock_hz, sideout, setout, outout);
    r->pio_access = false;
    return 0;
}
int pm_resident_pio_start(int sm)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || !r->pio_owner ||
        pm_runtime_is_running() || sm < 0 || sm >= 4) return -1;
    r->pio_access = true;
    pm_pio_start_hw(r->pio_owner - 1, sm);
    r->pio_access = false;
    return 0;
}
int pm_resident_pio_stop(int sm)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || !r->pio_owner ||
        pm_runtime_is_running() || sm < 0 || sm >= 4) return -1;
    r->pio_access = true;
    pm_pio_stop_hw(r->pio_owner - 1, sm);
    r->pio_access = false;
    return 0;
}

/* These are ordinary persistent system-heap allocations, not BASIC heap.
 * Multiple allocations are permitted. The caller owns their lifetimes and
 * must stop any DMA using an allocation BEFORE freeing it. No auto cleanup. */
void *pm_resident_alloc(size_t bytes)
{
    if (!pm_resident || !pm_resident->fn || pm_runtime_is_running() || !bytes)
        return NULL;
    return malloc(bytes);
}
void pm_resident_free(void *ptr)
{
    if (!pm_runtime_is_running()) free(ptr);
}
/* Explicit claim of PicoMite's shared RX or TX DMA channel. A claim does NOT
 * start hardware; halt() stops hardware but retains the claim, release()
 * returns the channel to BASIC without stopping it implicitly. */
int pm_resident_dma_claim(int tx)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned bit;
    if (!r || !r->fn || !r->pio_owner || pm_runtime_is_running() ||
        (tx != 0 && tx != 1)) return -1;
    bit = 1u << tx;
    if (r->dma_owned & bit) return 0;
    if (pm_pio_dma_busy_hw(tx) || (tx ? DMAinterruptTX : DMAinterruptRX))
        return -1;
    r->dma_owned |= bit;
    r->dma_halted |= bit;
    return 0;
}
int pm_resident_dma_halt(int tx)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned bit;
    if (!r || pm_runtime_is_running() || !pm_resident_dma_owned(tx))
        return -1;
    bit = 1u << tx;
    r->pio_access = true;
    pm_pio_dma_stop_hw(tx);
    r->pio_access = false;
    r->dma_halted |= bit;
    return 0;
}
int pm_resident_dma_release(int tx)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned bit;
    if (!r || pm_runtime_is_running() || !pm_resident_dma_owned(tx))
        return -1;
    bit = 1u << tx;
    if (!(r->dma_halted & bit)) return -1; /* must halt explicitly */
    r->dma_owned &= ~bit;
    r->dma_halted &= ~bit;
    r->dma_region[tx] = 0;
    r->dma_buffer_mask &= ~bit;
    r->dma_buffer_offset[tx] = r->dma_buffer_count[tx] = 0;
    return 0;
}
int pm_resident_pio_dma_transfer(int tx, int sm, void *buffer,
                                 size_t capacity_bytes, uint32_t count,
                                 unsigned bits, int cyclic, int tx_ring)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned bit;
    int pior;
    if (!r || !r->fn || !r->pio_owner || pm_runtime_is_running() ||
        (tx != 0 && tx != 1) || sm < 0 || sm > 3 || !buffer ||
        (cyclic != 0 && cyclic != 1) || (tx_ring != 0 && tx_ring != 1) ||
        (!tx && tx_ring) || !pm_resident_dma_owned(tx)) return -1;
    bit = 1u << tx;
    pior = r->pio_owner - 1;
    r->dma_halted &= ~bit; /* longjmp leaves channel claimed for manual recovery */
    r->pio_access = true;
    if (cyclic)
        pm_pio_dma_cyclic_hw(tx, pior, sm, buffer, capacity_bytes,
                             count, bits, tx_ring, 1);
    else
        pm_pio_dma_transfer_hw(tx, pior, sm, buffer, capacity_bytes,
                               count, bits, 1);
    r->pio_access = false;
    return 0;
}

/* Compatibility v28 single-buffer helpers; the direct API above does not
 * impose their buffer count or overlap policy. */
/* The additional buffer is the ONLY resident DMA allocation. It uses
 * newlib's heap, not PicoMite's resettable BASIC heap, and is never resized
 * while a DMA channel can reference it. The caller selects its word count. */
int pm_resident_dma_buffer_alloc(unsigned words)
{
    pm_resident_descriptor *r = pm_resident;
    uint32_t *buffer;
    if (!r || !r->fn || r->busy || pm_runtime_is_running() ||
        r->dma_buffer || !words || words > (size_t)-1 / sizeof(uint32_t))
        return -1;
    buffer = malloc((size_t)words * sizeof(uint32_t));
    if (!buffer) return -1;
    memset(buffer, 0, (size_t)words * sizeof(uint32_t));
    r->dma_buffer = buffer;
    r->dma_buffer_words = words;
    return 0;
}
uint32_t *pm_resident_dma_buffer(void)
{
    return pm_resident && pm_resident->fn && !pm_runtime_is_running()
           ? pm_resident->dma_buffer : NULL;
}
unsigned pm_resident_dma_buffer_words(void)
{
    return pm_resident && pm_resident->fn && !pm_runtime_is_running()
           ? pm_resident->dma_buffer_words : 0;
}
int pm_resident_dma_buffer_free(void)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || r->busy || pm_runtime_is_running() ||
        !r->dma_buffer || r->dma_buffer_mask) return -1;
    free(r->dma_buffer);
    r->dma_buffer = NULL;
    r->dma_buffer_words = 0;
    return 0;
}
/* The legacy v28 buffer is also freed only by an explicit call. */

/* Legacy v27 state-backed and v28 allocated-buffer transfers share the same
 * channel ownership and hardware path; different buffers never alias.
 * Cyclic RX requires a power-of-two count, as in the BASIC implementation. */
int pm_resident_pio_dma_stop(int tx);
static int pm_resident_pio_dma_start_common(int tx, int sm,
                                           unsigned offset_words,
                                           unsigned count, int cyclic,
                                           bool external)
{
    pm_resident_descriptor *r = pm_resident;
    uint32_t *base;
    unsigned capacity, bit, other, region = 0;
    int pior;
    if (!r || !r->fn || !r->pio_owner || pm_runtime_is_running() ||
        (tx != 0 && tx != 1) || sm < 0 || sm >= 4 ||
        (cyclic != 0 && cyclic != 1) || !count ||
        (cyclic && !tx && (count & (count - 1u)))) return -1;
    base = external ? r->dma_buffer : r->words;
    capacity = external ? r->dma_buffer_words : 16u;
    if (!base || offset_words >= capacity ||
        count > capacity - offset_words) return -1;
    bit = 1u << tx;
    other = 1u << (tx ^ 1);
    if (external) {
        if ((r->dma_owned & r->dma_buffer_mask & other) &&
            offset_words < r->dma_buffer_offset[tx ^ 1] +
                           r->dma_buffer_count[tx ^ 1] &&
            r->dma_buffer_offset[tx ^ 1] < offset_words + count)
            return -1;
    } else {
        region = ((1u << count) - 1u) << offset_words;
        if ((r->dma_owned & other) && !(r->dma_buffer_mask & other) &&
            (r->dma_region[tx ^ 1] & region)) return -1;
    }
    if (!(r->dma_owned & bit)) {
        if (pm_pio_dma_busy_hw(tx) ||
            (tx ? DMAinterruptTX : DMAinterruptRX)) return -1;
    } else {
        if (pm_resident_pio_dma_stop(tx) != 0) return -1;
    }
    pior = r->pio_owner - 1;
    /* Claim BEFORE arming: error unwind must see an owned channel. */
    r->dma_owned |= bit;
    r->dma_halted &= ~bit;
    r->dma_region[tx] = region;
    r->dma_buffer_offset[tx] = external ? offset_words : 0;
    r->dma_buffer_count[tx] = external ? count : 0;
    if (external) r->dma_buffer_mask |= bit;
    else r->dma_buffer_mask &= ~bit;
    r->pio_access = true;
    if (cyclic)
        pm_pio_dma_cyclic_hw(tx, pior, sm, base + offset_words,
                             (size_t)count * sizeof(uint32_t),
                             count, 32, 0, 1);
    else
        pm_pio_dma_transfer_hw(tx, pior, sm, base + offset_words,
                               (size_t)count * sizeof(uint32_t),
                               count, 32, 1);
    r->pio_access = false;
    return 0;
}
int pm_resident_pio_dma_start(int tx, int sm, unsigned offset_words,
                              unsigned count, int cyclic)
{
    return pm_resident_pio_dma_start_common(tx, sm, offset_words, count,
                                            cyclic, false);
}
int pm_resident_pio_dma_start_buffer(int tx, int sm, unsigned offset_words,
                                     unsigned count, int cyclic)
{
    return pm_resident_pio_dma_start_common(tx, sm, offset_words, count,
                                            cyclic, true);
}
int pm_resident_pio_dma_busy(int tx)
{
    if (!pm_resident_dma_owned(tx)) return -1;
    return pm_pio_dma_busy_hw(tx);
}
int pm_resident_pio_dma_stop(int tx)
{
    pm_resident_descriptor *r = pm_resident;
    unsigned bit;
    if (!pm_resident_dma_owned(tx) || pm_runtime_is_running()) return -1;
    bit = 1u << tx;
    r->pio_access = true;
    pm_pio_dma_stop_hw(tx);
    r->dma_owned &= ~bit;
    r->dma_halted &= ~bit;
    r->dma_region[tx] = 0;
    r->dma_buffer_mask &= ~bit;
    r->dma_buffer_offset[tx] = r->dma_buffer_count[tx] = 0;
    r->pio_access = false;
    return 0;
}

int pm_resident_start(pm_resident_callback fn, unsigned interval_ms)
{
    pm_resident_descriptor *r = pm_resident;
    if ((r && r->busy) || pm_runtime_is_running() ||
        interval_ms < 10u || interval_ms > 60000u ||
        !pm_resident_is_flash_csub((const void *)fn))
        return -1;
    if (!r) {
        r = malloc(sizeof(*r));
        if (!r) return -1;
        pm_resident = r;
    } else {
        /* Restart is not permission to steal/free live resident resources. */
        if (r->gpio_pin || r->gpio_claims || r->pio_owner ||
            r->dma_owned || r->dma_buffer) return -1;
    }
    memset(r, 0, sizeof(*r));
    r->period_us = interval_ms * 1000u;
    r->next_us = time_us_64() + r->period_us;
    r->fn = fn;
    return 0;
}
int pm_resident_stop(void)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r) return 0;
    if (r->pio_owner || r->gpio_pin || r->gpio_claims ||
        r->dma_owned || r->dma_buffer) return -1;
    r->fn = NULL;
    if (r->busy) return 0; /* defer descriptor free until callback returns */
    pm_resident = NULL;
    free(r);
    return 0;
}
int pm_resident_faulted(void)
{
    return pm_resident && pm_resident->faulted;
}
int pm_resident_active(void)
{
    return pm_resident && pm_resident->fn != NULL;
}
uint32_t *pm_resident_state(void)
{
    return pm_resident ? pm_resident->words : NULL;
}

/* Called from routinechecks() on the foreground core, never the 1 ms IRQ. */
void pm_resident_service(void)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r || !r->fn || r->busy || r->faulted ||
        pm_runtime_is_running() || MMAbort)
        return;
    if (r->gpio_pin && r->gpio_fn) {
        const int gp = r->gpio_gp;
        const unsigned shift = 4u * ((unsigned)gp & 7u);
        uint32_t raw = io_bank0_hw->intr[gp >> 3] &
                       pm_resident_gpio_raw_mask(gp);
        if (raw) {
            unsigned events = 0;
            io_bank0_hw->intr[gp >> 3] = raw;
            if (raw & ((uint32_t)GPIO_IRQ_EDGE_RISE << shift))
                events |= PM_GPIO_RISE;
            if (raw & ((uint32_t)GPIO_IRQ_EDGE_FALL << shift))
                events |= PM_GPIO_FALL;
            events &= r->gpio_edges;
            if (events) {
                pm_resident_gpio_callback cb = r->gpio_fn;
                r->busy = true;
                cb(gp, events, r->words);
                r->busy = false;
                if (!r->fn) { pm_resident_stop(); return; }
            }
        }
    }
    uint64_t now = time_us_64();
    if (now < r->next_us) return;
    r->next_us = now + r->period_us;
    r->busy = true;
    r->fn(r->words);
    r->busy = false;
    if (!r->fn) pm_resident_stop();
}
/* An abnormal callback unwind must never free hardware or resident code.
 * Suspend callbacks; the resident's foreground CSUB performs recovery. */
void pm_resident_unwind(void)
{
    pm_resident_descriptor *r = pm_resident;
    if (!r) return;
    r->pio_access = false;
    r->gpio_access = false;
    if (r->busy) {
        r->busy = false;
        r->faulted = true;
    }
}
void pm_resident_before_library_write(void)
{
    if (pm_resident)
        error("Stop resident C explicitly before modifying LIBRARY");
}

/* RP2350 keeps the existing PM_CALL error/abort boundary in the C runtime.
 * RP2040's ELF runner uses the BASIC prompt's error boundary and performs
 * abandoned-image cleanup before resetting its heap. */
#ifdef rp2350
extern void tcc_picomite_api_call(void (*fn)(void *), void *arg);
#endif
void pm_core_call(void (*fn)(void *), void *arg)
{
    pm_runtime_tick_service();
#ifdef rp2350
    tcc_picomite_api_call(fn, arg);
#else
    if (MMAbort) CheckAbort();
    if (fn) fn(arg);
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
{
    int *a = p;
    pm_resident_descriptor *r = pm_resident;
    bool allow = r && r->fn && !pm_runtime_is_running() &&
                 a[0] >= 0 && a[0] < NUM_BANK0_GPIOS &&
                 (r->gpio_claims & (1ull << (unsigned)a[0])) != 0;
    /* BASIC SETPIN never goes through this native API. The foreground
     * CSUB may configure a claimed pin; the BASIC reset must not. */
    if (allow) r->gpio_access = true;
    ExtCfg(codemap(a[0]), a[1], 0);
    if (allow) r->gpio_access = false;
}
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

extern void pm_gpio_subscribe_hw(int,int,int,void (*)(int,int));
static void pm_gpio_on_do(void *p)
{
    struct {int gp,edge,pull;void (*fn)(int,int);} *a=p;
    pm_gpio_subscribe_hw(a->gp,a->edge,a->pull,a->fn);
}
void pm_gpio_on_edge(int gp,int edge,int pull,void (*fn)(int,int))
{
    struct {int gp,edge,pull;void (*fn)(int,int);} a={gp,edge,pull,fn};
    pm_core_call(pm_gpio_on_do,&a);
}
void pm_gpio_poll(void) { pm_core_call(NULL,NULL); }

/* USB/BLE gamepads already decode into BASIC's nunstruct[1..4]. Reuse
 * nunfoundc[] without altering BASIC's x1 mask or nunInterruptc[] address.
 * Never pass a C function pointer to the BASIC dispatcher. */
typedef void (*pm_gamepad_callback)(int, unsigned);
static pm_gamepad_callback pm_c_gamepad_cb[5];
static unsigned pm_c_gamepad_owned;
static unsigned pm_c_gamepad_masks[5], pm_c_gamepad_last[5];
static bool pm_c_gamepad_polling;

static bool pm_gamepad_valid_type(unsigned type)
{
    return type == PM_GAMEPAD_PS4 || type == PM_GAMEPAD_PS3 ||
           type == PM_GAMEPAD_GENERIC || type == PM_GAMEPAD_XBOX ||
           type == PM_GAMEPAD_SWITCHPRO || type == PM_GAMEPAD_BLE;
}
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
extern bool bt_keyboard_has_gamepad(void);
#endif
static bool pm_gamepad_live_type(unsigned type)
{
    if (!pm_gamepad_valid_type(type)) return false;
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
    if (type == PM_GAMEPAD_BLE && !bt_keyboard_has_gamepad()) return false;
#endif
    return true;
}

int pm_gamepad_c_owns(int channel)
{
    return channel >= 1 && channel <= 4 &&
           (pm_c_gamepad_owned & (1u << channel)) != 0;
}
/* Called only by the existing USB HID gamepad report decoders. BASIC
 * keeps its own x1 change mask; these are ORed for event detection. */
unsigned pm_gamepad_c_mask(int channel)
{
    return pm_gamepad_c_owns(channel) ? pm_c_gamepad_masks[channel] : 0;
}
void pm_gamepad_runtime_begin(void)
{
    memset(pm_c_gamepad_cb, 0, sizeof(pm_c_gamepad_cb));
    memset(pm_c_gamepad_masks, 0, sizeof(pm_c_gamepad_masks));
    memset(pm_c_gamepad_last, 0, sizeof(pm_c_gamepad_last));
    pm_c_gamepad_owned = 0;
    pm_c_gamepad_polling = false;
}
void pm_gamepad_runtime_end(void)
{
    for (int n = 1; n <= 4; ++n) {
        if (!pm_gamepad_c_owns(n)) continue;
        pm_c_gamepad_cb[n] = NULL;
        pm_c_gamepad_masks[n] = 0;
        nunfoundc[n] = false;
    }
    pm_c_gamepad_owned = 0;
    pm_c_gamepad_polling = false;
}
void pm_gamepad_runtime_poll(void)
{
    if (!pm_runtime_is_running() || pm_c_gamepad_polling ||
        !pm_c_gamepad_owned) return;
    pm_c_gamepad_polling = true;
    for (int n = 1; n <= 4; ++n) {
        if (!pm_gamepad_c_owns(n) || !nunfoundc[n]) continue;
        nunfoundc[n] = false;
        pm_gamepad_callback cb = pm_c_gamepad_cb[n];
        unsigned buttons = nunstruct[n].x0;
        unsigned change = (buttons ^ pm_c_gamepad_last[n]) & pm_c_gamepad_masks[n];
        pm_c_gamepad_last[n] = buttons;
        /* BLE also sets this BASIC latch for axis movement. Mouse and
         * other HID channels can set it too; only button changes on a
         * recognised gamepad are delivered to the C callback. */
        if (cb && change && pm_gamepad_live_type((unsigned)nunstruct[n].type))
            cb(n, buttons);
    }
    pm_c_gamepad_polling = false;
}

static void pm_gamepad_get_do(void *arg)
{
    struct {int channel, result; pm_gamepad_state *out;} *a = arg;
    pm_gamepad_state value = {0};
    unsigned want;
    int n = a->channel;
    if (n < 1 || n > 4) error("Invalid gamepad channel");
    if (!a->out || (want = a->out->size) < sizeof(unsigned)) {
        a->result = -1;
        return;
    }
    a->result = 0;
    /* Only gamepad types, not keyboards, mice or other HID functions. */
    unsigned type = (unsigned)nunstruct[n].type;
    if (pm_gamepad_live_type(type)) {
        a->result = 1;
        value.type = type;
        value.buttons = nunstruct[n].x0;
        value.hat = (uint8_t)nunstruct[n].y0;
        value.lx = nunstruct[n].ax;
        value.ly = nunstruct[n].ay;
        value.rx = nunstruct[n].Z;
        value.ry = nunstruct[n].C;
        value.l = nunstruct[n].L;
        value.r = nunstruct[n].R;
        for (int i = 0; i < 3; ++i) {
            value.gyro[i] = nunstruct[n].gyro[i];
            value.accel[i] = nunstruct[n].accs[i];
        }
#ifdef USBKEYBOARD
        if (HID[n - 1].active && HID[n - 1].Device_type == type) {
            value.vid = HID[n - 1].vid;
            value.pid = HID[n - 1].pid;
        }
#endif
    }
    /* Match pm_get_info: preserve the caller's size field and copy only
     * the portion understood by both sides of the ABI. */
    size_t copy = want < sizeof(value) ? want : sizeof(value);
    memcpy((unsigned char *)a->out + sizeof(unsigned),
           (const unsigned char *)&value + sizeof(unsigned),
           copy - sizeof(unsigned));
}
int pm_gamepad_get(int channel, pm_gamepad_state *out)
{
    struct {int channel, result; pm_gamepad_state *out;} a = {channel, -1, out};
    pm_core_call(pm_gamepad_get_do, &a);
    return a.result;
}

static void pm_gamepad_subscribe_do(void *arg)
{
    struct {int n; unsigned mask; pm_gamepad_callback cb;} *a = arg;
    int n = a->n;
    if (!pm_runtime_is_running()) error("Gamepad events require a running C program");
    if (n < 1 || n > 4) error("Invalid gamepad channel");
    if (!a->cb) {
        if (pm_gamepad_c_owns(n)) {
            pm_c_gamepad_cb[n] = NULL;
            pm_c_gamepad_owned &= ~(1u << n);
            pm_c_gamepad_masks[n] = 0;
            nunfoundc[n] = false;
        }
        return;
    }
    if (a->mask > 0xFFFFu || !a->mask) error("Invalid gamepad button mask");
#if !defined(USBKEYBOARD) && !defined(PICOMITEBTH) && !defined(PICOMITEHDMIBTH)
    error("Gamepad events unavailable in this firmware");
#elif !defined(USBKEYBOARD)
    if (n != 3) error("BLE gamepad uses channel 3");
#endif
#ifdef USBKEYBOARD
    if (HID[n - 1].active &&
        !pm_gamepad_valid_type(HID[n - 1].Device_type))
        error("HID channel is not a gamepad");
#endif
    if (!pm_gamepad_c_owns(n) && nunInterruptc[n])
        error("Gamepad interrupt already configured by BASIC");
    /* BASIC retains nunInterruptc[] and x1. The USB decoders OR this
     * C mask into their existing edge detection; BLE uses the same latch. */
    pm_c_gamepad_last[n] = nunstruct[n].x0;
    pm_c_gamepad_masks[n] = a->mask;
    pm_c_gamepad_cb[n] = a->cb;
    pm_c_gamepad_owned |= 1u << n;
    nunfoundc[n] = false;
}
void pm_gamepad_on_change(int channel, unsigned mask, pm_gamepad_callback cb)
{
    struct {int n; unsigned mask; pm_gamepad_callback cb;} a = {channel,mask,cb};
    pm_core_call(pm_gamepad_subscribe_do, &a);
}
void pm_gamepad_poll(void) { pm_core_call(NULL,NULL); }

/* Compiler-independent native functions; local declarations avoid clashing
 * with the legacy static TinyCC adapter implementations during migration. */
extern void pm_print(const char *);
extern void pm_yield(void);
extern void pm_delay_us(unsigned int);
extern void pm_pause(unsigned int);
extern int pm_keydown(int);
extern int pm_mouse(int, int);
extern int pm_usb_device(int);
extern int pm_errno(void);
extern const char *pm_errmsg(void);
extern void pm_error_clear(void);
extern int pm_get_framebuffer(void *);
extern unsigned pm_fb_colour(int);
extern int pm_font_width(int, int);
extern int pm_font_height(int, int);
extern void pm_text(int, int, const char *, const char *, int, int, int, int);
extern void pm_draw_box(int, int, int, int, int, int, int);
extern void pm_draw_rbox(int, int, int, int, int, int, int);
extern void pm_blit_framebuffer(int, int, int, int, int, int, int, int, int);
extern void pm_sprite_show(int, int, int, int, int);
extern void pm_sprite_hide(int);
extern void pm_sprite_write(int, int, int, int);
extern void pm_sprite_close_all(void);
extern void pm_framebuffer_copy(int, int, int);
extern void pm_pcm_play(const int16_t *, unsigned, unsigned, unsigned);
extern void pm_pcm_stop(void);
extern int pm_pcm_playing(void);
extern void pm_pcm_poll(void);
extern void pm_colour(int, int);
extern void pm_mode(int);
extern int pm_touch(const char *);
extern int pm_click(const char *);
extern void pm_gui(const char *);
extern int pm_ctrlval_get_number(int, double *);
extern int pm_ctrlval_set_number(int, double);
extern int pm_ctrlval_get_text(int, char *, unsigned);
extern int pm_ctrlval_set_text(int, const char *);
extern void pm_gui_on_event(void (*)(int, int, int, int));
extern void pm_gui_poll(void);

extern const void *const CallTable[];
typedef struct { const char *name; const void *address; } pm_core_symbol;
#define PM_CORE_SYM(n) {#n, (const void *)&n},
static const pm_core_symbol pm_core_symbols[] = {
    {"__picomite_calltable", (const void *)CallTable},
    PM_CORE_SYM(pm_hres)
    PM_CORE_SYM(pm_vres)
    PM_CORE_SYM(pm_timer_us)
    PM_CORE_SYM(pm_abort_requested)
    PM_CORE_SYM(pm_resident_start)
    PM_CORE_SYM(pm_resident_stop)
    PM_CORE_SYM(pm_resident_active)
    PM_CORE_SYM(pm_resident_state)
    PM_CORE_SYM(pm_resident_gpio_on_edge)
    PM_CORE_SYM(pm_resident_gpio_off)
    PM_CORE_SYM(pm_resident_gpio_claim)
    PM_CORE_SYM(pm_resident_gpio_release)
    PM_CORE_SYM(pm_resident_pio_claim)
    PM_CORE_SYM(pm_resident_pio_release)
    PM_CORE_SYM(pm_resident_pio_pin)
    PM_CORE_SYM(pm_resident_pio_program)
    PM_CORE_SYM(pm_resident_pio_init_machine)
    PM_CORE_SYM(pm_resident_pio_start)
    PM_CORE_SYM(pm_resident_pio_stop)
    PM_CORE_SYM(pm_resident_pio_dma_start)
    PM_CORE_SYM(pm_resident_pio_dma_busy)
    PM_CORE_SYM(pm_resident_pio_dma_stop)
    PM_CORE_SYM(pm_resident_dma_buffer_alloc)
    PM_CORE_SYM(pm_resident_dma_buffer)
    PM_CORE_SYM(pm_resident_dma_buffer_words)
    PM_CORE_SYM(pm_resident_dma_buffer_free)
    PM_CORE_SYM(pm_resident_pio_dma_start_buffer)
    PM_CORE_SYM(pm_resident_alloc)
    PM_CORE_SYM(pm_resident_free)
    PM_CORE_SYM(pm_resident_dma_claim)
    PM_CORE_SYM(pm_resident_dma_halt)
    PM_CORE_SYM(pm_resident_dma_release)
    PM_CORE_SYM(pm_resident_pio_dma_transfer)
    PM_CORE_SYM(pm_resident_faulted)
    PM_CORE_SYM(pm_cls)
    PM_CORE_SYM(pm_pixel)
    {"pm_line", (const void *)&pm_core_line},
    PM_CORE_SYM(pm_box)
    PM_CORE_SYM(pm_circle)
    PM_CORE_SYM(pm_triangle)
    PM_CORE_SYM(pm_refresh)
    PM_CORE_SYM(pm_inkey)
    PM_CORE_SYM(pm_set_remap_fn)
    PM_CORE_SYM(pm_set_isprint_fn)
    PM_CORE_SYM(pm_isprint)
    PM_CORE_SYM(pm_set_console_font)
    PM_CORE_SYM(pm_pin_mode)
    PM_CORE_SYM(pm_pin_write)
    PM_CORE_SYM(pm_pin_read)
    PM_CORE_SYM(pm_gpio_on_edge)
    PM_CORE_SYM(pm_gpio_poll)
    PM_CORE_SYM(pm_gamepad_get)
    PM_CORE_SYM(pm_gamepad_on_change)
    PM_CORE_SYM(pm_gamepad_poll)
    PM_CORE_SYM(pm_touch)
    PM_CORE_SYM(pm_click)
    PM_CORE_SYM(pm_gui)
    PM_CORE_SYM(pm_ctrlval_get_number)
    PM_CORE_SYM(pm_ctrlval_set_number)
    PM_CORE_SYM(pm_ctrlval_get_text)
    PM_CORE_SYM(pm_ctrlval_set_text)
    PM_CORE_SYM(pm_gui_on_event)
    PM_CORE_SYM(pm_gui_poll)
    PM_CORE_SYM(pm_print)
    PM_CORE_SYM(pm_yield)
    PM_CORE_SYM(pm_delay_us)
    PM_CORE_SYM(pm_pause)
    PM_CORE_SYM(pm_keydown)
    PM_CORE_SYM(pm_mouse)
    PM_CORE_SYM(pm_usb_device)
    PM_CORE_SYM(pm_errno)
    PM_CORE_SYM(pm_errmsg)
    PM_CORE_SYM(pm_error_clear)
    PM_CORE_SYM(pm_get_framebuffer)
    PM_CORE_SYM(pm_fb_colour)
    PM_CORE_SYM(pm_font_width)
    PM_CORE_SYM(pm_font_height)
    PM_CORE_SYM(pm_text)
    PM_CORE_SYM(pm_draw_box)
    PM_CORE_SYM(pm_draw_rbox)
    PM_CORE_SYM(pm_blit_framebuffer)
    PM_CORE_SYM(pm_sprite_show)
    PM_CORE_SYM(pm_sprite_hide)
    PM_CORE_SYM(pm_sprite_write)
    PM_CORE_SYM(pm_sprite_close_all)
    PM_CORE_SYM(pm_framebuffer_copy)
    PM_CORE_SYM(pm_pcm_play)
    PM_CORE_SYM(pm_pcm_stop)
    PM_CORE_SYM(pm_pcm_playing)
    PM_CORE_SYM(pm_pcm_poll)
    PM_CORE_SYM(pm_colour)
    PM_CORE_SYM(pm_mode)
};
#undef PM_CORE_SYM

const void *pm_core_resolve_symbol(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_core_symbols) / sizeof(pm_core_symbols[0]); ++i)
        if (strcmp(name, pm_core_symbols[i].name) == 0)
            return pm_core_symbols[i].address;
    return pm_runtime_resolve_symbol(name);
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
    {
        uint32_t extra = pm_runtime_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    return hash ? hash : 1u;
}
