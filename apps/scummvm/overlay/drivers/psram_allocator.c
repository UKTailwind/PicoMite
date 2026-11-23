/*
 * FRANK Quest
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-quest
 *
 * Derived from Cabal (https://github.com/project-cabal/cabal).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * psram_allocator.c — PSRAM heap backed by dlmalloc's mspace API.
 *
 * CMM2: the "PSRAM" is a block of the loadable program's heap (cmm2_psram_region, from main.c), not the RP2350's
 * external PSRAM; its lock holds off the loader's sound refill instead of the I2S DMA interrupt; the crash log is
 * ordinary memory (a reset ends the program); interrupts are turned off by cpsid. Nothing else is changed.
 *
 * Ported from frank-msx / frank-blood. Earlier cabal revisions used a
 * bump allocator (OOM after fragmented workloads like ScummVM) and a
 * free-list allocator with in-PSRAM headers (header writes got dropped
 * once the binary grew past ~2.5 MB due to XIP cache pressure on
 * PSRAM-as-XIP). dlmalloc keeps its metadata interleaved with payload
 * but the metadata pattern is read-heavy, not the header-rewriting
 * pattern that tripped the XIP bug.
 *
 * Memory layout in the 8 MB PSRAM window:
 *   [0         ..  96 kB)  HDMI framebuffer 0 (persistent across
 *                          psram_reset; HDMI DMA scans this or fb 1
 *                          continuously)
 *   [96        .. 192 kB)  HDMI framebuffer 1 (back buffer)
 *   [192       .. 512 kB)  reserved (legacy scratch regions, unused)
 *   [512 kB    .. 8 MB)    dlmalloc mspace
 */

#include "psram_allocator.h"
#include "psram_dlmalloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmm2_port.h"

// When using --wrap=malloc, call the real libc functions to avoid recursion
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);
extern void __real_free(void *ptr);

static uint8_t *psram_start;                         /* CMM2: the region main.c gives */
static size_t psram_size;
#define PSRAM_BASE       ((uintptr_t)psram_start)
#define PSRAM_SIZE       psram_size
#define PSRAM_END        (PSRAM_BASE + PSRAM_SIZE)

void cmm2_psram_region(void *base, unsigned size) {
    psram_start = (uint8_t *)base;
    psram_size = size;
}

// Reserve 512KB for scratch buffers at the beginning
#define SCRATCH_SIZE     (512u * 1024u)


// ---- dlmalloc mspace glue ------------------------------------

typedef void* mspace;
extern mspace create_mspace_with_base(void *base, size_t capacity, int locked);
extern void  *mspace_malloc  (mspace msp, size_t bytes);
extern void  *mspace_realloc (mspace msp, void *ptr, size_t bytes);
extern void   mspace_free    (mspace msp, void *ptr);
extern size_t mspace_usable_size(const void *ptr);
extern void   mspace_inspect_all(mspace msp,
                                 void (*handler)(void *start, void *end,
                                                 size_t used_bytes, void *arg),
                                 void *arg);

static mspace g_msp = NULL;

// Flag set by main() after PSRAM is confirmed working
static int psram_ready = 0;
static int psram_sram_mode = 0;

static int is_psram(const void *p) {
    uintptr_t a = (uintptr_t)p;
    return a >= PSRAM_BASE && a < PSRAM_END;
}

static void ensure_init(void) {
    if (g_msp) return;
    void  *base = psram_start + SCRATCH_SIZE;
    size_t size = PSRAM_SIZE  - SCRATCH_SIZE;
    g_msp = create_mspace_with_base(base, size, 0);
    if (g_msp) {
        printf("PSRAM heap: %u kB at %p (dlmalloc)\n",
               (unsigned)(size / 1024), base);
    } else {
        printf("PSRAM heap: FAILED to init mspace\n");
    }
}

void psram_set_ready(int ready) {
    psram_ready = ready;
    if (ready) ensure_init();
}

void psram_set_sram_mode(int enable) {
    psram_sram_mode = enable;
}

// ---- canary-wrapped allocator --------------------------------
//
// Each PSRAM allocation is wrapped like:
//
//   [hdr: magic | user_size | padding] [user bytes ...] [tail magic]
//
// hdr is 16 bytes (keeps the user pointer 16-byte aligned), tail is 4 bytes.
// Every free checks both magics; any overrun past user_size (the #1 cause of
// hard-to-debug crashes in the dlmalloc tree) is caught at free time and
// attributed to the block instead of a later unrelated chunk.

#define CABAL_CANARY_HEAD  0xA11C0DEDu   // ALLOCATED
#define CABAL_CANARY_TAIL  0xDEADBEA7u   // DEAD_BEAT
#define CABAL_CANARY_FREED 0xFEEDF4EEu   // FREED, for double-free detection

/* ---- Alloc/free ring buffer in .uninitialized_data ----------
 *
 * Every malloc/free records (user ptr, size, caller PC, op). Survives
 * the watchdog reboot so we can see what happened just before a crash.
 * 128 entries × 16 B = 2 KB, negligible. */
typedef struct {
    uint32_t user;
    uint32_t size;
    uint32_t caller;
    uint16_t seq;      // monotonic sequence for ordering
    uint8_t  op;       // 0=none, 1=alloc, 2=free, 3=realloc-in, 4=realloc-out
    uint8_t  pad;
} alloc_event_t;

#define CABAL_ALLOC_LOG_MAGIC  0xA110C0DEu
#define CABAL_ALLOC_LOG_SIZE   128

struct alloc_log {
    uint32_t magic;
    uint32_t head;
    uint32_t seq;
    uint32_t reserved;
    alloc_event_t ev[CABAL_ALLOC_LOG_SIZE];
};

/* Put the log in its own .uninitialized_data section so it survives reset. */
static struct alloc_log g_alloc_log __attribute__((used));

static inline void alloc_log_reset_if_stale(void) {
    if (g_alloc_log.magic != CABAL_ALLOC_LOG_MAGIC) {
        g_alloc_log.magic = CABAL_ALLOC_LOG_MAGIC;
        g_alloc_log.head = 0;
        g_alloc_log.seq = 0;
        for (uint32_t i = 0; i < CABAL_ALLOC_LOG_SIZE; i++) {
            g_alloc_log.ev[i].op = 0;
        }
    }
}

static void alloc_log_push(uint8_t op, void *user, size_t size, void *caller) {
    alloc_log_reset_if_stale();
    uint32_t idx = g_alloc_log.head;
    alloc_event_t *e = &g_alloc_log.ev[idx];
    e->user   = (uint32_t)(uintptr_t)user;
    e->size   = (uint32_t)size;
    e->caller = (uint32_t)(uintptr_t)caller;
    e->seq    = (uint16_t)(g_alloc_log.seq & 0xFFFF);
    e->op     = op;
    e->pad    = 0;
    g_alloc_log.head = (idx + 1) % CABAL_ALLOC_LOG_SIZE;
    g_alloc_log.seq++;
}

static const char *alloc_op_name(uint8_t op) {
    switch (op) {
        case 1: return "alloc  ";
        case 2: return "free   ";
        case 3: return "rfree  ";
        case 4: return "ralloc ";
        default:return "?      ";
    }
}

static void alloc_log_dump_tail(uint32_t count) {
    if (g_alloc_log.magic != CABAL_ALLOC_LOG_MAGIC) {
        printf("  [alloc log empty]\n");
        return;
    }
    if (count > CABAL_ALLOC_LOG_SIZE) count = CABAL_ALLOC_LOG_SIZE;
    /* walk backwards from head, most recent first */
    uint32_t idx = (g_alloc_log.head + CABAL_ALLOC_LOG_SIZE - 1) % CABAL_ALLOC_LOG_SIZE;
    for (uint32_t i = 0; i < count; i++) {
        alloc_event_t *e = &g_alloc_log.ev[idx];
        if (e->op == 0) break;
        printf("  [%4u] %s user=%08lx size=%-8lu caller=%08lx\n",
               (unsigned)e->seq, alloc_op_name(e->op),
               (unsigned long)e->user, (unsigned long)e->size,
               (unsigned long)e->caller);
        if (idx == 0) idx = CABAL_ALLOC_LOG_SIZE - 1;
        else idx--;
    }
}

/* Look for the most recent allocation that covered [addr, addr+1).
 * Used to name the stale block behind a head-canary corruption. */
static int alloc_log_find_prior(uint32_t addr, alloc_event_t *out) {
    if (g_alloc_log.magic != CABAL_ALLOC_LOG_MAGIC) return 0;
    uint32_t idx = (g_alloc_log.head + CABAL_ALLOC_LOG_SIZE - 1) % CABAL_ALLOC_LOG_SIZE;
    for (uint32_t i = 0; i < CABAL_ALLOC_LOG_SIZE; i++) {
        alloc_event_t *e = &g_alloc_log.ev[idx];
        if (e->op == 0) break;
        if ((e->op == 1 || e->op == 4) && e->user != 0 &&
            addr >= e->user && addr < e->user + e->size) {
            *out = *e;
            return 1;
        }
        if (idx == 0) idx = CABAL_ALLOC_LOG_SIZE - 1;
        else idx--;
    }
    return 0;
}

typedef struct {
    uint32_t magic;      // CABAL_CANARY_HEAD while live, _FREED after free
    uint32_t user_size;  // requested size (not including canaries)
    uint32_t pad0;
    uint32_t pad1;
} cabal_chunk_hdr_t;

static void cabal_canary_report(const char *what, void *user,
                                cabal_chunk_hdr_t *h,
                                void *caller) {
    printf("\n*** PSRAM CANARY %s ***\n", what);
    printf("  user=%p hdr=%p\n", user, (void *)h);
    printf("  caller=%p (run addr2line -e build/frank-quest.elf -fCp <addr>)\n",
           caller);
    if (h) {
        printf("  hdr.magic=0x%08lx size=%lu\n",
               (unsigned long)h->magic, (unsigned long)h->user_size);
        if (h->user_size > 0 && h->user_size < (4u << 20)) {
            uint32_t *tail = (uint32_t *)((uint8_t *)(h + 1) + h->user_size);
            printf("  tail=0x%08lx (expected 0x%08x) @ %p\n",
                   (unsigned long)*tail, CABAL_CANARY_TAIL, (void *)tail);
        }
        /* Dump 32 bytes before the header so we can see what the
         * preceding chunk looks like — its trailing bytes are the
         * most likely culprit for a head-canary smash. */
        uint32_t *prev = (uint32_t *)h - 8;
        printf("  preceding 32B @ %p:", (void *)prev);
        for (int i = 0; i < 8; i++) {
            printf(" %08lx", (unsigned long)prev[i]);
        }
        printf("\n");
    }

    /* If this was a head-canary corruption, try to name the prior
     * allocation that owned this memory range. A use-after-free write
     * from an old block will usually appear here. */
    if (user) {
        alloc_event_t prior;
        if (alloc_log_find_prior((uint32_t)(uintptr_t)user, &prior)) {
            printf("  prior owner: seq=%u size=%lu caller=%08lx op=%s\n",
                   prior.seq, (unsigned long)prior.size,
                   (unsigned long)prior.caller, alloc_op_name(prior.op));
        } else {
            printf("  prior owner: <not found in log>\n");
        }
    }

    printf("  --- last 32 alloc/free events (most recent first) ---\n");
    alloc_log_dump_tail(32);
    printf("  --- end log ---\n");

    /* Flush any buffered stdio before the trap. */
    fflush(stdout);

    /* Kill interrupts so no IRQ can steal the CPU before we trap. */
    __asm volatile ("cpsid i" ::: "memory");

    /* __builtin_trap emits a guaranteed undefined instruction (UDF #0)
     * which escalates to HardFault on Cortex-M33. This is deterministic
     * unlike writing to flash (which is silently absorbed by XIP) or
     * NULL (which on RP2350 points at the vector table in flash and is
     * also absorbed on writes). */
    __builtin_trap();
    for (;;) { }
}

static void *cabal_wrap_alloc(void *raw, size_t user_size) {
    if (!raw) return NULL;
    cabal_chunk_hdr_t *h = (cabal_chunk_hdr_t *)raw;
    h->magic     = CABAL_CANARY_HEAD;
    h->user_size = (uint32_t)user_size;
    h->pad0      = 0;
    h->pad1      = 0;
    uint8_t *user = (uint8_t *)(h + 1);
    uint32_t *tail = (uint32_t *)(user + user_size);
    *tail = CABAL_CANARY_TAIL;
    return user;
}

static __attribute__((noinline))
cabal_chunk_hdr_t *cabal_check_ptr(void *user, const char *ctx,
                                   void *caller) {
    if (!user) return NULL;
    cabal_chunk_hdr_t *h = ((cabal_chunk_hdr_t *)user) - 1;
    if (h->magic == CABAL_CANARY_FREED) {
        printf("double-free / use-after-free (ctx=%s)\n", ctx);
        cabal_canary_report("DOUBLE-FREE", user, h, caller);
        return NULL;
    }
    if (h->magic != CABAL_CANARY_HEAD) {
        printf("head canary corrupt (ctx=%s) got 0x%08lx\n",
               ctx, (unsigned long)h->magic);
        cabal_canary_report("HEAD CORRUPT", user, h, caller);
        return NULL;
    }
    if (h->user_size > (8u << 20)) {
        printf("impossible user_size %lu (ctx=%s)\n",
               (unsigned long)h->user_size, ctx);
        cabal_canary_report("SIZE CORRUPT", user, h, caller);
        return NULL;
    }
    uint32_t *tail = (uint32_t *)((uint8_t *)user + h->user_size);
    if (*tail != CABAL_CANARY_TAIL) {
        printf("tail canary corrupt (ctx=%s) got 0x%08lx\n",
               ctx, (unsigned long)*tail);
        cabal_canary_report("TAIL OVERRUN", user, h, caller);
        return NULL;
    }
    return h;
}

// ---- public API ----------------------------------------------

/* ---- mspace IRQ-safety -----------------------------------------
 *
 * The I2S audio DMA IRQ calls Audio::Mixer::mixCallback, which walks
 * all active channels and for some of them (iMUSE Digital decompressed
 * blocks, BundleMgr compressed-table buffers, etc.) can allocate or
 * free PSRAM. If the main thread is mid-mspace-walk when the IRQ fires
 * and re-enters the allocator, the dlmalloc tree is in an inconsistent
 * state and the second caller trashes it. The crashes at tmalloc_large
 * / mspace_free / insert_large_chunk with wild pointers in BFAR all
 * match this pattern.
 *
 * Cheapest correct fix: serialize all mspace operations under a brief
 * critical section. Allocations here are measured in sub-µs on RP2350
 * and the audio DMA buffer is 23 ms deep, so there's no risk of
 * underrun. */
/* Mask only the audio DMA IRQ, not all interrupts. Masking everything
 * (save_and_disable_interrupts) also hides HDMI's scanline IRQ on
 * DMA_IRQ_0, which collapses the video signal during long allocations.
 * Audio is on DMA_IRQ_1 in this project; every other IRQ keeps running. */
#define MSPACE_LOCK()   cmm2_sound_lock()
#define MSPACE_UNLOCK() cmm2_sound_unlock()

__attribute__((noinline))
void *psram_malloc(size_t size) {
    if (!psram_ready || psram_sram_mode) {
        return __real_malloc(size);
    }
    ensure_init();
    if (!g_msp) return NULL;
    size_t total = sizeof(cabal_chunk_hdr_t) + size + sizeof(uint32_t);
    MSPACE_LOCK();
    void *raw = mspace_malloc(g_msp, total);
    MSPACE_UNLOCK();
    void *user = cabal_wrap_alloc(raw, size);
    alloc_log_push(1 /*alloc*/, user, size, __builtin_return_address(0));
    return user;
}

__attribute__((noinline))
void *psram_realloc(void *ptr, size_t size) {
    void *caller = __builtin_return_address(0);
    if (!psram_ready || psram_sram_mode) {
        return __real_realloc(ptr, size);
    }
    ensure_init();
    if (!g_msp) return NULL;
    if (!ptr)       return psram_malloc(size);
    if (size == 0)  { psram_free(ptr); return NULL; }
    if (!is_psram(ptr)) {
        // Came from the C library heap.
        return __real_realloc(ptr, size);
    }
    cabal_chunk_hdr_t *h = cabal_check_ptr(ptr, "realloc", caller);
    if (!h) return NULL;  // cabal_check_ptr doesn't return on failure.
    alloc_log_push(3 /*realloc-free*/, ptr, h->user_size, caller);
    size_t total = sizeof(cabal_chunk_hdr_t) + size + sizeof(uint32_t);
    MSPACE_LOCK();
    void *raw = mspace_realloc(g_msp, h, total);
    MSPACE_UNLOCK();
    void *user = cabal_wrap_alloc(raw, size);
    alloc_log_push(4 /*realloc-alloc*/, user, size, caller);
    return user;
}

__attribute__((noinline))
void psram_free(void *ptr) {
    void *caller = __builtin_return_address(0);
    if (!ptr) return;
    if (!is_psram(ptr)) {
        __real_free(ptr);
        return;
    }
    cabal_chunk_hdr_t *h = cabal_check_ptr(ptr, "free", caller);
    if (!h) return;
    alloc_log_push(2 /*free*/, ptr, h->user_size, caller);
    h->magic = CABAL_CANARY_FREED;
    if (g_msp) {
        MSPACE_LOCK();
        mspace_free(g_msp, h);
        MSPACE_UNLOCK();
    }
}

size_t psram_usable_size(void *ptr) {
    if (!ptr || !is_psram(ptr)) return 0;
    cabal_chunk_hdr_t *h = ((cabal_chunk_hdr_t *)ptr) - 1;
    if (h->magic != CABAL_CANARY_HEAD) return 0;
    return h->user_size;
}

void psram_reset(void) {
    // Throw the whole mspace away and start over. Any live blocks leak.
    g_msp = NULL;
    ensure_init();
}

/* Dedicated 96 kB slot at the very base of PSRAM for the HDMI
 * framebuffer. Fixed address so HDMI DMA can keep scanning it even
 * while the mspace is wiped and re-initialized on a return-to-selector
 * transition. 320x240 @ 8 bpp = 75 kB, rounded up to 96 kB for
 * alignment and headroom. Legacy scratch_1 (128 kB at the same base)
 * is no longer used — the framebuffer slot now owns the first 96 kB
 * and scratch_1 maps to the same address so accidental callers still
 * return a valid pointer. */
#define FRAMEBUFFER_SIZE (96u * 1024u)
void *psram_get_framebuffer(void) {
    return psram_start;
}
void *psram_get_framebuffer_back(void) {
    return psram_start + FRAMEBUFFER_SIZE;
}
size_t psram_framebuffer_size(void) {
    return FRAMEBUFFER_SIZE;
}

void *psram_get_scratch_1(size_t size) {
    if (size > 128u * 1024u) return NULL;
    return psram_start;
}
void *psram_get_scratch_2(size_t size) {
    if (size > 128u * 1024u) return NULL;
    return psram_start + 128u * 1024u;
}
void *psram_get_file_buffer(size_t size) {
    if (size > 256u * 1024u) return NULL;
    return psram_start + 256u * 1024u;
}

// ---- compat shims kept so existing code keeps building -------

void  psram_set_temp_mode(int enable) { (void)enable; }
void  psram_reset_temp(void)          { }
size_t psram_get_temp_offset(void)    { return 0; }
void  psram_set_temp_offset(size_t o) { (void)o; }

void psram_mark_session(void)    { }
void psram_restore_session(void) { }

void psram_print_status(void) {
    printf("PSRAM Status: dlmalloc mspace at %p\n", (void*)g_msp);
}

// ---- dlmalloc FOOTERS error hooks ----------------------------
//
// Wired in via CMake: USAGE_ERROR_ACTION=cabal_heap_error and
// CORRUPTION_ERROR_ACTION=cabal_heap_corruption. These fire the FIRST time
// dlmalloc's chunk-footer magic fails a check (bad free, double free, write
// past usable size of a chunk), not later when the free list has already
// been scrambled and the next malloc crashes on a stale pointer.
//
// We print a loud marker, dump the recent alloc/free ring and the walkable
// portion of the heap, then trap into HardFault with a deterministic UDF so
// the reset handler can persist context.

/* Heap walker used by the corruption dump and by frank_quest_heap_walk().
 * Prints each chunk dlmalloc will visit up to `limit_used_bytes` — once we
 * cross a corrupted footer mspace_inspect_all() itself can loop forever,
 * so bound the walk. */
typedef struct {
    uint32_t count;
    uint32_t free_count;
    size_t   used_total;
    size_t   free_total;
    size_t   biggest_free;
    uint32_t limit;
} heap_walk_ctx_t;

static void heap_walk_cb(void *start, void *end, size_t used, void *arg) {
    heap_walk_ctx_t *c = (heap_walk_ctx_t *)arg;
    if (c->count >= c->limit) return;
    size_t total = (size_t)((uint8_t *)end - (uint8_t *)start);
    size_t freeb = total > used ? total - used : 0;
    if (used) {
        c->used_total += used;
    } else {
        c->free_count++;
        c->free_total += freeb;
        if (freeb > c->biggest_free) c->biggest_free = freeb;
    }
    /* Print first N chunks to keep log manageable. */
    if (c->count < 32) {
        printf("    [%3u] %p..%p total=%-8u used=%-8u  %s\n",
               (unsigned)c->count, start, end,
               (unsigned)total, (unsigned)used,
               used ? "USED" : "free");
    } else if (c->count == 32) {
        printf("    ... (further chunks suppressed) ...\n");
    }
    c->count++;
}

static void heap_walk_and_dump(uint32_t limit) {
    if (!g_msp) {
        printf("  [heap walk: mspace not initialized]\n");
        return;
    }
    heap_walk_ctx_t c = { 0 };
    c.limit = limit;
    printf("  --- heap walk (first %u chunks, bounded at %u) ---\n",
           (unsigned)(limit > 32 ? 32 : limit), (unsigned)limit);
    mspace_inspect_all(g_msp, heap_walk_cb, &c);
    printf("  heap: chunks=%u used=%u free_chunks=%u free_total=%u biggest_free=%u\n",
           (unsigned)c.count, (unsigned)c.used_total,
           (unsigned)c.free_count, (unsigned)c.free_total,
           (unsigned)c.biggest_free);
}

/* Public: walk the heap on demand — safe to call when audio/HDMI IRQs
 * are running because we take MSPACE_LOCK to keep dlmalloc quiescent. */
void frank_quest_heap_walk(void) {
    if (!psram_ready || !g_msp) {
        printf("frank_quest_heap_walk: heap not ready\n");
        return;
    }
    MSPACE_LOCK();
    heap_walk_and_dump(1024);
    MSPACE_UNLOCK();
}

/* dlmalloc calls the error hooks via an `fm->magic = 0; ACTION(fm)` macro
 * expansion, so `__builtin_return_address(0)` inside a nested helper returns
 * a PC inside our own trap routine — useless. Instead, capture the caller
 * PC directly at the hook entry and pass it through. Marked noinline so the
 * address captured here is the one dlmalloc itself returned into.
 *
 * Re-entry guard: if the heap walk below trips another footer check, we'd
 * recurse into this function forever. Skip the walk on re-entry. */
static volatile int g_in_heap_trap = 0;

static __attribute__((noinline))
void cabal_heap_trap(const char *kind, void *m, const void *p, void *caller) {
    int first_entry = (g_in_heap_trap == 0);
    g_in_heap_trap = 1;

    printf("\n*** PSRAM HEAP %s ***\n", kind);
    printf("  mstate=%p chunk=%p\n", m, p);
    printf("  caller=%p (run: arm-none-eabi-addr2line -e build/frank-quest.elf -fCp %p)\n",
           caller, caller);

    if (first_entry) {
        /* Print ring of recent alloc/free events — the last entry is almost
         * always the allocation that triggered the footer check. */
        printf("  --- last 32 alloc/free events (most recent first) ---\n");
        alloc_log_dump_tail(32);
        printf("  --- end log ---\n");

        /* Walk the heap. This is best-effort: once dlmalloc's invariants
         * are broken the walker may abort early, but the chunks printed
         * BEFORE it gives up tend to identify the scribbled-over block. */
        heap_walk_and_dump(1024);
    } else {
        printf("  [re-entry — skipping heap walk and log dump]\n");
    }

    fflush(stdout);

    /* Kill interrupts so no IRQ can mutate state before the trap. */
    __asm volatile ("cpsid i" ::: "memory");

    /* UDF #0 → HardFault. Deterministic on Cortex-M33, unlike writing to
     * NULL (which on RP2350 hits the flash XIP vector table and is
     * silently absorbed on writes). */
    __builtin_trap();
    for (;;) { }
}

void cabal_heap_error(void *m, void *p) {
    cabal_heap_trap("USAGE ERROR (bad free / double free / bad ptr)",
                    m, p, __builtin_return_address(0));
}

void cabal_heap_corruption(void *m) {
    cabal_heap_trap("CORRUPTION (footer magic failed)",
                    m, NULL, __builtin_return_address(0));
}
