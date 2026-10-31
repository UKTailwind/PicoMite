/* Compiler-independent PicoMite C program services.
 * Both external ELF runners and the TinyCC run path own an independent
 * allocation scope.  The compiler keeps using its private arena.
 */
#include "MMBasic_Includes.h"
#include "core/pm_api.h"
#include "Hardware_Includes.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/tcc/include/picomite/audio.h"
#include <setjmp.h>
#include "pico/time.h"

#define PM_RUNTIME_VERSION 11u

typedef union pm_runtime_block {
    struct {
        union pm_runtime_block *next;
        size_t size;
    } s;
    long double alignment;
} pm_runtime_block;

static pm_runtime_block *pm_runtime_blocks;
static bool pm_runtime_running;
static jmp_buf pm_runtime_exit_stack;
static int pm_runtime_exit_value;
/* A C program runs on a private stack. exit() unwinds only to that
 * program's entry; the owner still performs its normal cleanup. */
__attribute__((noinline))
int pm_runtime_call_main(int (*entry)(int, char **), int argc, char **argv)
{
    volatile int result = 0;
    if (setjmp(pm_runtime_exit_stack) == 0)
        result = entry(argc, argv);
    else
        result = pm_runtime_exit_value;
    return result;
}
static void __attribute__((noreturn)) pm_runtime_exit(int code)
{
    if (pm_runtime_running) {
        pm_runtime_exit_value = code;
        longjmp(pm_runtime_exit_stack, 1);
    }
    error("exit outside C program");
    __builtin_unreachable();
}
static void __attribute__((noreturn)) pm_runtime_abort(void)
{
    pm_runtime_exit(EXIT_FAILURE);
}
bool pm_runtime_is_running(void) { return pm_runtime_running; }
void pm_runtime_files_begin(void);
void pm_runtime_files_end(void);
void pm_runtime_tick_reset(void);
void pm_runtime_periph_begin(void);
void pm_runtime_periph_end(void);
const void *pm_runtime_basic_resolve(const char *name);
unsigned pm_runtime_basic_firmware_id(void);
const void *pm_runtime_peripheral_resolve(const char *name);
unsigned pm_runtime_peripheral_firmware_id(void);
const void *pm_runtime_commands_resolve(const char *name);
unsigned pm_runtime_commands_firmware_id(void);

/* A C run is synchronous; the interpreter and CSUBs never start a second
 * run while this scope is live. No dynamic runtime-control allocation. */
void pm_runtime_begin(void)
{
    pm_runtime_running = true;
    pm_runtime_blocks = NULL;
    pm_runtime_files_begin();
    pm_runtime_tick_reset();
    pm_runtime_periph_begin();
}

void pm_runtime_end(void)
{
    pm_runtime_block *b;
    /* A streaming callback may point into a soon-to-be-freed ELF image.
     * Detach it before releasing the program's allocations. */
    PCMRunCleanup();
    pm_runtime_files_end();
    pm_runtime_periph_end();
    pm_runtime_tick_reset();
    b = pm_runtime_blocks;
    pm_runtime_running = false;
    pm_runtime_blocks = NULL;
    while (b) {
        pm_runtime_block *next = b->s.next;
        FreeMemory(b);
        b = next;
    }
}

void *pm_runtime_alloc(size_t size, int where)
{
    pm_runtime_block *b;
    size_t bytes;
    if (!pm_runtime_running || size > (size_t)INT_MAX - sizeof(*b))
        return NULL;
    if (size == 0) size = 1;
    bytes = size + sizeof(*b);
    switch (where) {
    case 1:
#ifdef rp2350
        b = GetSRAMMemoryNull((int)bytes);
#else
        /* RP2040 has only the SRAM heap. */
        b = GetMemoryNull((int)bytes);
#endif
        break;
    case 2:
#ifdef rp2350
        b = PSRAMsize ? GetPSMemoryNull((int)bytes) : NULL;
#else
        /* PSRAM allocation is unavailable on RP2040. */
        return NULL;
#endif
        break;
    case 0: b = GetMemoryNull((int)bytes); break;
    default: return NULL;
    }
    if (!b) return NULL;
    b->s.size = size;
    b->s.next = pm_runtime_blocks;
    pm_runtime_blocks = b;
    return b + 1;
}

void pm_runtime_free(void *ptr)
{
    pm_runtime_block **it, *b;
    if (!ptr) return;
    b = (pm_runtime_block *)ptr - 1;
    for (it = &pm_runtime_blocks; *it; it = &(*it)->s.next) {
        if (*it != b) continue;
        *it = b->s.next;
        FreeMemory(b);
        return;
    }
}

void *pm_runtime_malloc(size_t size) { return pm_runtime_alloc(size, 0); }
void *pm_runtime_calloc(size_t n, size_t size)
{
    void *p;
    if (size && n > SIZE_MAX / size) return NULL;
    p = pm_runtime_malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}
void *pm_runtime_realloc(void *ptr, size_t size)
{
    pm_runtime_block *b;
    void *p;
    if (!ptr) return pm_runtime_malloc(size);
    if (!size) { pm_runtime_free(ptr); return NULL; }
    for (b = pm_runtime_blocks; b; b = b->s.next)
        if (b + 1 == ptr) break;
    if (!b) return NULL;
    if (size <= b->s.size) { b->s.size = size; return ptr; }
    p = pm_runtime_malloc(size);
    if (p) { memcpy(p, ptr, b->s.size); pm_runtime_free(ptr); }
    return p;
}
char *pm_runtime_strdup(const char *s)
{
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = pm_runtime_malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

int pm_runtime_mem_where(const void *p)
{
    const unsigned char *a = p;
#ifdef rp2350
    if (PSRAMsize && a >= (const unsigned char *)PSRAMbase &&
        a < (const unsigned char *)(PSRAMbase + PSRAMsize)) return 2;
#endif
    if (a >= MMHeap && a < MMHeap + heap_memory_size) return 1;
    return 0;
}

extern unsigned int SBitsGet(unsigned char *addr);
int pm_runtime_get_mem_info(void *out)
{
    struct { unsigned size, sram_total, sram_free, psram_total, psram_free; } m;
#ifdef rp2350
    unsigned char *a;
#endif
    unsigned free_all = (unsigned)FreeSpaceOnHeap(), ps = 0;
    unsigned want;
    if (!out) return -1;
    want = *(unsigned *)out;
    if (want < sizeof(unsigned)) return -1;
    memset(&m, 0, sizeof(m));
#ifdef rp2350
    if (PSRAMsize)
        for (a = (unsigned char *)PSRAMbase;
             a < (unsigned char *)(PSRAMbase + PSRAMsize); a += PAGESIZE)
            if (!(SBitsGet(a) & PUSED)) ps += PAGESIZE;
    m.psram_total = PSRAMsize;
#endif
    m.sram_total = heap_memory_size;
    m.sram_free = free_all - ps;
    m.psram_free = ps;
    memcpy((unsigned char *)out + sizeof(unsigned),
           (unsigned char *)&m + sizeof(unsigned),
           (want < sizeof(m) ? want : sizeof(m)) - sizeof(unsigned));
    return 0;
}

unsigned pm_runtime_api_version(void) { return PM_RUNTIME_VERSION; }

/* For ELF CSUBs only stateless functions may be resolved; allocating
 * functions require a live RUN scope and are rejected during CSUB linkage. */
bool pm_runtime_csub_forbidden(const char *name)
{
    static const char *const unsafe[] = {
        "malloc", "calloc", "realloc", "free", "strdup", "exit", "abort",
        "pm_alloc", "pm_free",
        "opendir", "readdir", "closedir",
        "fopen", "fclose", "fread", "fwrite", "fseek", "ftell",
        "rewind", "feof", "ferror", "fflush", "fgetc", "getc",
        "getchar", "fgets", "fputc", "putc", "fputs",
        "__tcc_pm_stdin", "pm_settick",
        "pm_adc_read", "pm_adc_read_raw", "pm_pwm_open",
        "pm_pwm_set_duty", "pm_pwm_set_frequency", "pm_pwm_close",
        "pm_serial_open", "pm_serial_read", "pm_serial_write",
        "pm_serial_available", "pm_serial_flush", "pm_serial_close",
        "pm_i2c_open", "pm_i2c_close", "pm_i2c_transfer",
        "pm_i2c_read", "pm_i2c_write", "pm_i2c_probe",
        "pm_spi_open", "pm_spi_close", "pm_spi_transfer",
        "pm_spi_write", "pm_spi_read", NULL
    };
    const char *const *p;
    for (p = unsafe; *p; ++p)
        if (!strcmp(*p, name)) return true;
    return false;
}

extern int pm_get_framebuffer(void *out);
int pm_runtime_get_info(void *out)
{
    struct {
        unsigned size, api_version;
        char firmware[16], device[32];
        unsigned cpu_khz, sram_heap, psram;
        int hres, vres;
        unsigned caps;
    } i;
    struct { unsigned size; void *pixels; int w, h, stride, format; } fb;
    unsigned char *saved_sret = sret, *d;
    int saved_targ = targ;
    unsigned want;
    if (!out) return -1;
    want = *(unsigned *)out;
    if (want < sizeof(unsigned)) return -1;
    memset(&i, 0, sizeof(i));
    i.api_version = PM_RUNTIME_VERSION;
    snprintf(i.firmware, sizeof(i.firmware), "%s", VERSION);
    fun_device();
    d = sret;
    sret = saved_sret;
    targ = saved_targ;
    memcpy(i.device, d + 1,
           d[0] < sizeof(i.device) - 1 ? d[0] : sizeof(i.device) - 1);
    ClearSpecificTempMemory(d);
    i.cpu_khz = (unsigned)Option.CPU_Speed;
    i.sram_heap = heap_memory_size;
#ifdef rp2350
    i.psram = PSRAMsize;
#endif
    i.hres = HRes;
    i.vres = VRes;
#ifdef PICOMITEVGA
    i.caps |= 0x0001;
#else
    if (Option.DISPLAY_TYPE) i.caps |= 0x0001;
#endif
    fb.size = sizeof(fb);
    if (pm_get_framebuffer(&fb) == 0) i.caps |= 0x0002;
#ifdef rp2350
    if (PSRAMsize) i.caps |= 0x0004;
#endif
    if (SDCardConfigured()) i.caps |= 0x0008;
#ifdef USBKEYBOARD
    i.caps |= 0x0010;
#endif
#ifdef PICOMITEWEB
    i.caps |= 0x0020;
#endif
#if defined(PICOMITEBT) || defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
    i.caps |= 0x0040;
#endif
#ifdef HDMI
    i.caps |= 0x0080;
#elif defined(PICOMITEVGA)
    i.caps |= 0x0100;
#endif
    memcpy((unsigned char *)out + sizeof(unsigned),
           (unsigned char *)&i + sizeof(unsigned),
           (want < sizeof(i) ? want : sizeof(i)) - sizeof(unsigned));
    return 0;
}

/* The common resolver deliberately returns the host's proven libc functions
 * for pure operations; the allocator alone belongs to the active RUN scope.
 * No newlib FILE * is exposed: PicoMite's file API requires a separate adapter.
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

/* Console output must use PicoMite's console, not the SDK's stdio backend. */
static void pm_runtime_console(const char *p, size_t n)
{
    char b[96];
    size_t i, k = 0;
    for (i = 0; i < n; ++i) {
        if (p[i] == '\n') b[k++] = '\r';
        b[k++] = p[i];
        if (k >= sizeof(b) - 2) {
            b[k] = 0;
            MMPrintString(b);
            k = 0;
        }
    }
    if (k) { b[k] = 0; MMPrintString(b); }
}
static int pm_runtime_vprintf(const char *fmt, va_list ap)
{
    va_list cp;
    char *buf;
    int n;
    va_copy(cp, ap);
    n = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (n < 0 || n >= INT_MAX) return -1;
    buf = GetMemoryNull(n + 1);
    if (!buf) return -1;
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    pm_runtime_console(buf, (size_t)n);
    FreeMemory(buf);
    return n;
}
/* Preserve the previous TinyCC vsprintf ABI without importing newlib's
 * independent _svfprintf_r engine.  snprintf/vsnprintf already use the
 * formatter linked by PicoMite. */
static int pm_runtime_vsprintf(char *dst, const char *fmt, va_list ap)
{
    return vsnprintf(dst, INT_MAX, fmt, ap);
}
static int pm_runtime_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = pm_runtime_vprintf(fmt, ap);
    va_end(ap);
    return n;
}
static int pm_runtime_puts(const char *s)
{
    pm_runtime_console(s, strlen(s));
    pm_runtime_console("\n", 1);
    return 0;
}
static int pm_runtime_putchar(int c)
{
    char ch = (char)c;
    pm_runtime_console(&ch, 1);
    return (unsigned char)ch;
}

/* The compiler uses these ABI helpers for integer division and soft-float.
 * Signatures are immaterial in the table; only addresses are taken. */
#define PM_RUNTIME_AEABI(X)                                                  \
    X(__aeabi_memcpy) X(__aeabi_memcpy4) X(__aeabi_memcpy8)                  \
    X(__aeabi_memmove) X(__aeabi_memset) X(__aeabi_idiv) X(__aeabi_uidiv)    \
    X(__aeabi_idivmod) X(__aeabi_uidivmod) X(__aeabi_ldivmod)                \
    X(__aeabi_uldivmod) X(__aeabi_lasr) X(__aeabi_llsr) X(__aeabi_llsl)      \
    X(__aeabi_dadd) X(__aeabi_dsub) X(__aeabi_dmul) X(__aeabi_ddiv)          \
    X(__aeabi_dcmpeq) X(__aeabi_dcmplt) X(__aeabi_dcmple)                    \
    X(__aeabi_dcmpgt) X(__aeabi_dcmpge) X(__aeabi_i2d) X(__aeabi_ui2d)      \
    X(__aeabi_d2iz) X(__aeabi_d2uiz) X(__aeabi_f2d) X(__aeabi_d2f)          \
    X(__aeabi_l2f) X(__aeabi_l2d) X(__aeabi_f2lz) X(__aeabi_d2lz)           \
    X(__aeabi_ul2f) X(__aeabi_ul2d) X(__aeabi_f2ulz) X(__aeabi_d2ulz)
#define PM_RUNTIME_DECLARE(n) extern void n(void);
PM_RUNTIME_AEABI(PM_RUNTIME_DECLARE)
#undef PM_RUNTIME_DECLARE

/* PCM streaming callbacks run from PCMPoll(), not from IRQ. The firmware
 * marks them owned by the current C run so abort cleanup detaches them. */
static void pm_runtime_pcm_start_do(void *ptr)
{
    struct {unsigned rate, channels; pm_pcm_callback callback; void *userdata;} *p = ptr;
    PCMStartOwned(p->rate, p->channels, p->callback, p->userdata,
                  pm_runtime_running ? 1 : 0);
}
static void pm_runtime_pcm_start(unsigned rate, unsigned channels,
                                 pm_pcm_callback callback, void *userdata)
{
    struct {unsigned rate, channels; pm_pcm_callback callback; void *userdata;} p =
        { rate, channels, callback, userdata };
    pm_core_call(pm_runtime_pcm_start_do, &p);
}
/* PicoMite time/clock ABI matches its native C headers (time_t is 64-bit). */
static long long pm_runtime_time(long long *out)
{
    long long t = (long long)((time_us_64() - TimeSubSecondUs) / 1000000
                               + TimeOffsetToUptime);
    if (t <= 0) t = -1;
    if (out) *out = t;
    return t;
}
static long pm_runtime_clock(void)
{
    return (long)(time_us_64() / 1000u);
}
typedef struct { const char *name; const void *address; } pm_runtime_symbol;
#define PM_RUNTIME_SYM(n) { #n, (const void *)&n },
#define PM_RUNTIME_AS(n, fn) { #n, (const void *)&fn },
static const pm_runtime_symbol pm_runtime_symbols[] = {
    PM_RUNTIME_AS(malloc, pm_runtime_malloc)
    PM_RUNTIME_AS(calloc, pm_runtime_calloc)
    PM_RUNTIME_AS(realloc, pm_runtime_realloc)
    PM_RUNTIME_AS(free, pm_runtime_free)
    PM_RUNTIME_AS(strdup, pm_runtime_strdup)
    PM_RUNTIME_AS(exit, pm_runtime_exit)
    PM_RUNTIME_AS(abort, pm_runtime_abort)
    PM_RUNTIME_AS(pm_pcm_start, pm_runtime_pcm_start)
    PM_RUNTIME_AS(time, pm_runtime_time)
    PM_RUNTIME_AS(clock, pm_runtime_clock)
    PM_RUNTIME_AS(pm_alloc, pm_runtime_alloc)
    PM_RUNTIME_AS(pm_free, pm_runtime_free)
    PM_RUNTIME_AS(pm_mem_where, pm_runtime_mem_where)
    PM_RUNTIME_AS(pm_get_mem_info, pm_runtime_get_mem_info)
    PM_RUNTIME_AS(pm_api_version, pm_runtime_api_version)
    PM_RUNTIME_AS(pm_get_info, pm_runtime_get_info)
    PM_RUNTIME_AS(printf, pm_runtime_printf)
    PM_RUNTIME_AS(vprintf, pm_runtime_vprintf)
    PM_RUNTIME_AS(puts, pm_runtime_puts)
    PM_RUNTIME_AS(putchar, pm_runtime_putchar)
    PM_RUNTIME_SYM(snprintf)
    PM_RUNTIME_SYM(vsnprintf)
    PM_RUNTIME_SYM(sprintf)
    PM_RUNTIME_AS(vsprintf, pm_runtime_vsprintf)
    PM_RUNTIME_SYM(memcpy)
    PM_RUNTIME_SYM(memmove)
    PM_RUNTIME_SYM(memset)
    PM_RUNTIME_SYM(memcmp)
    PM_RUNTIME_SYM(memchr)
    PM_RUNTIME_SYM(strlen)
    PM_RUNTIME_SYM(strcpy)
    PM_RUNTIME_SYM(strncpy)
    PM_RUNTIME_SYM(strcat)
    PM_RUNTIME_SYM(strncat)
    PM_RUNTIME_SYM(strcmp)
    PM_RUNTIME_SYM(strncmp)
    PM_RUNTIME_SYM(strchr)
    PM_RUNTIME_SYM(strrchr)
    PM_RUNTIME_SYM(strstr)
    PM_RUNTIME_SYM(strspn)
    PM_RUNTIME_SYM(strcspn)
    PM_RUNTIME_SYM(strpbrk)
    PM_RUNTIME_SYM(strtok)
    PM_RUNTIME_SYM(atoi)
    PM_RUNTIME_SYM(atol)
    PM_RUNTIME_SYM(atof)
    PM_RUNTIME_SYM(strtol)
    PM_RUNTIME_SYM(strtoul)
    PM_RUNTIME_SYM(strtoll)
    PM_RUNTIME_SYM(strtoull)
    PM_RUNTIME_SYM(strtod)
    PM_RUNTIME_SYM(strtof)
    PM_RUNTIME_SYM(abs)
    PM_RUNTIME_SYM(labs)
    PM_RUNTIME_SYM(llabs)
    PM_RUNTIME_SYM(rand)
    PM_RUNTIME_SYM(srand)
    PM_RUNTIME_SYM(qsort)
    PM_RUNTIME_SYM(bsearch)
    PM_RUNTIME_SYM(isalnum)
    PM_RUNTIME_SYM(isalpha)
    PM_RUNTIME_SYM(iscntrl)
    PM_RUNTIME_SYM(isdigit)
    PM_RUNTIME_SYM(isgraph)
    PM_RUNTIME_SYM(islower)
    PM_RUNTIME_SYM(isprint)
    PM_RUNTIME_SYM(ispunct)
    PM_RUNTIME_SYM(isspace)
    PM_RUNTIME_SYM(isupper)
    PM_RUNTIME_SYM(isxdigit)
    PM_RUNTIME_SYM(tolower)
    PM_RUNTIME_SYM(toupper)
    PM_RUNTIME_SYM(sin)
    PM_RUNTIME_SYM(cos)
    PM_RUNTIME_SYM(tan)
    PM_RUNTIME_SYM(asin)
    PM_RUNTIME_SYM(acos)
    PM_RUNTIME_SYM(atan)
    PM_RUNTIME_SYM(atan2)
    PM_RUNTIME_SYM(exp)
    PM_RUNTIME_SYM(log)
    PM_RUNTIME_SYM(log10)
    PM_RUNTIME_SYM(pow)
    PM_RUNTIME_SYM(sqrt)
    PM_RUNTIME_SYM(fabs)
    PM_RUNTIME_SYM(floor)
    PM_RUNTIME_SYM(ceil)
    PM_RUNTIME_SYM(sinh)
    PM_RUNTIME_SYM(cosh)
    PM_RUNTIME_SYM(tanh)
    PM_RUNTIME_SYM(log2)
    PM_RUNTIME_SYM(exp2)
    PM_RUNTIME_SYM(cbrt)
    PM_RUNTIME_SYM(hypot)
    PM_RUNTIME_SYM(round)
    PM_RUNTIME_SYM(trunc)
    PM_RUNTIME_SYM(fmod)
    PM_RUNTIME_SYM(ldexp)
    PM_RUNTIME_SYM(frexp)
    PM_RUNTIME_SYM(modf)
    PM_RUNTIME_SYM(sinf)
    PM_RUNTIME_SYM(cosf)
    PM_RUNTIME_SYM(tanf)
    PM_RUNTIME_SYM(atan2f)
    PM_RUNTIME_SYM(sqrtf)
    PM_RUNTIME_SYM(powf)
    PM_RUNTIME_SYM(expf)
    PM_RUNTIME_SYM(logf)
    PM_RUNTIME_SYM(fabsf)
    PM_RUNTIME_SYM(floorf)
    PM_RUNTIME_SYM(ceilf)
    PM_RUNTIME_AEABI(PM_RUNTIME_SYM)
};
#undef PM_RUNTIME_SYM
#undef PM_RUNTIME_AS
const void *pm_runtime_stdio_resolve(const char *name);
unsigned pm_runtime_stdio_firmware_id(void);
const void *pm_runtime_resolve_symbol(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_runtime_symbols)/sizeof(pm_runtime_symbols[0]); ++i)
        if (!strcmp(name, pm_runtime_symbols[i].name))
            return pm_runtime_symbols[i].address;
    {
        const void *found = pm_runtime_stdio_resolve(name);
        if (found) return found;
        found = pm_runtime_basic_resolve(name);
        if (found) return found;
        found = pm_runtime_peripheral_resolve(name);
        return found ? found : pm_runtime_commands_resolve(name);
    }
}
unsigned pm_runtime_firmware_id(void)
{
    uint32_t hash = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_runtime_symbols)/sizeof(pm_runtime_symbols[0]); ++i) {
        uint32_t v = (uint32_t)(uintptr_t)pm_runtime_symbols[i].address;
        for (j = 0; j < 4; ++j, v >>= 8)
            hash = (hash ^ (v & 255u)) * 16777619u;
    }
    {
        uint32_t extra = pm_runtime_stdio_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    {
        uint32_t extra = pm_runtime_basic_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    {
        uint32_t extra = pm_runtime_peripheral_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    {
        uint32_t extra = pm_runtime_commands_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    return hash ? hash : 1u;
}
