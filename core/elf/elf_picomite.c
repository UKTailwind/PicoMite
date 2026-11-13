/* PicoMite filesystem/CSUB/RUN adapter for the independent ELF linker. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "elf_loader.h"
#include "elf_picomite.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#ifdef rp2350
#include "core/tcc/tcc_picomite.h"
#endif

#define ELF_RUN_STACK  (24u * 1024u)
#define ELF_MAX_OBJECT (1024u * 1024u)

extern const void *const CallTable[];
static uint32_t elf_firmware_id(void) {
    uint32_t hash = 2166136261u;
    unsigned i, j;
    /* Through offset 0x1e0: append-only CallTable ABI in PicoMite.c. */
    for (i = 0; i < 0x1e4u / 4; i++) {
        uint32_t v = (uint32_t)(uintptr_t)CallTable[i];
        for (j = 0; j < 4; j++, v >>= 8)
            hash = (hash ^ (v & 255u)) * 16777619u;
    }
    /* Common named exports can be bound by absolute address in a CSUB. */
    {
        uint32_t common = pm_core_firmware_id();
        for (j = 0; j < 4; j++, common >>= 8)
            hash = (hash ^ (common & 255u)) * 16777619u;
    }
#ifdef rp2350
    /* Native CSUBs can also bind pm_* symbols, not just CallTable entries. */
    {
        uint32_t symbols = tcc_pm_firmware_id();
        for (j = 0; j < 4; j++, symbols >>= 8)
            hash = (hash ^ (symbols & 255u)) * 16777619u;
    }
#endif
    return hash ? hash : 1u;
}

uint32_t elf_picomite_firmware_id(void) { return elf_firmware_id(); }

static bool elf_resolve_symbol(const char *name, uintptr_t *value, void *ctx) {
    bool csub = (bool)(uintptr_t)ctx;
    if (csub && pm_runtime_csub_forbidden(name)) return false;
    {
        const void *addr = pm_core_resolve_symbol(name);
        if (addr) { *value = (uintptr_t)addr; return true; }
    }
#ifdef rp2350
    {
        void *addr = csub ? tcc_picomite_resolve_csub_symbol(name) : tcc_pm_resolve_symbol(name);
        if (addr) { *value = (uintptr_t)addr; return true; }
    }
#else
    (void)csub;
#endif
    return false;
}

static unsigned char *elf_read_file(const char *path, int *length, char *err) {
    int size = FileSize((char *)path);
    int fnbr;
    unsigned char *bytes;
    unsigned int got = 0;
    if (size <= 0) {
        snprintf(err, MAXERRMSG, "ELF: file missing or empty: %.35s", path);
        return NULL;
    }
    if (size > (int)ELF_MAX_OBJECT) {
        snprintf(err, MAXERRMSG, "ELF: object too large: %.40s", path);
        return NULL;
    }
    fnbr = FindFreeFileNbr();
    if (!BasicFileOpen((char *)path, fnbr, FA_READ)) {
        snprintf(err, MAXERRMSG, "ELF: cannot open object");
        return NULL;
    }
    bytes = GetMemoryNull(size);
    if (!bytes) {
        ForceFileClose(fnbr);
        snprintf(err, MAXERRMSG, "ELF: not enough memory");
        return NULL;
    }
    if (FileGetData(fnbr, bytes, size, &got) || got != (unsigned int)size) {
        ForceFileClose(fnbr);
        FreeMemory(bytes);
        snprintf(err, MAXERRMSG, "ELF: incomplete read");
        return NULL;
    }
    ForceFileClose(fnbr);
    *length = size;
    return bytes;
}

bool elf_picomite_is_elf(const char *filename) {
    unsigned char sig[4];
    int fnbr;
    unsigned int got = 0;
    if (FileSize((char *)filename) < 52) return false;
    fnbr = FindFreeFileNbr();
    if (!BasicFileOpen((char *)filename, fnbr, FA_READ)) return false;
    bool found = FileGetData(fnbr, sig, 4, &got) == 0 && got == 4 &&
                 memcmp(sig, "\177ELF", 4) == 0;
    ForceFileClose(fnbr);
    return found;
}

/* Common quoted-file CSUB body scanner, available on both chips. */
unsigned char *elf_picomite_csub_source(unsigned char *p, unsigned short endtoken, char *line, int size) {
    int n = 0;
    skipspace(p);
    if (*p != '"') return NULL;
    *line = 0;
    for (;;) {
        while (*p && *p != '\'') {
            unsigned char *e = NULL;
            skipspace(p);
            if (*p == 0 || *p == '\'') break;
            if (*p == '"') e = (unsigned char *)strchr((char *)p + 1, '"');
            if (!e) error("CSUB: expected quoted file names and options");
            if (n + (e - p) + 2 >= size) error("CSUB: too many files and options");
            memcpy(line + n, p, (size_t)(e - p) + 1);
            n += (int)(e - p) + 1;
            line[n++] = ' ';
            line[n] = 0;
            p = e + 1;
            skipspace(p);
            if (*p == ',') p++;
        }
        while (*p) p++;
        p++;
        if (*p == 0) error("Missing END declaration");
        if (*p == T_NEWLINE) { CurrentLinePtr = p; p += T_NEWLINE_HDR; }
        if (*p == T_LINENBR) p += 3;
        skipspace(p);
        if (commandtbl_at(p) == endtoken) return p;
    }
}

/* One quoted object path (without compiler flags or other inputs). */
static bool single_path(const char *line, char *path, size_t cap) {
    const char *e;
    size_t n;
    while (*line == ' ' || *line == '\t') line++;
    if (*line++ != '"') return false;
    e = strchr(line, '"');
    if (!e) return false;
    n = (size_t)(e - line);
    if (!n || n >= cap) return false;
    memcpy(path, line, n);
    path[n] = 0;
    line = e + 1;
    while (*line == ' ' || *line == '\t') line++;
    return *line == 0;
}

static void resolve_program_path(const unsigned char *prog, char *path, size_t cap) {
    const char *start, *slash;
    size_t dir;
    if (path[0] == '/' || (path[0] && path[1] == ':') ||
        !prog || prog[0] != T_NEWLINE || prog[T_NEWLINE_HDR] != '\'' ||
        prog[T_NEWLINE_HDR + 1] != '#') return;
    start = (const char *)prog + T_NEWLINE_HDR + 2;
    slash = strrchr(start, '/');
    if (!slash) return;
    dir = (size_t)(slash - start) + 1;
    if (dir + strlen(path) >= cap) return;
    memmove(path + dir, path, strlen(path) + 1);
    memcpy(path, start, dir);
}

bool elf_picomite_csub_single_object(const char *line, const unsigned char *prog) {
    char path[STRINGSIZE];
    if (!single_path(line, path, sizeof(path))) return false;
    resolve_program_path(prog, path, sizeof(path));
    /* A missing or invalid .elf/.o is an ELF error, not a request to
       compile it as C. Keep signature detection for other file suffixes. */
    size_t n = strlen(path);
    if ((n >= 4 && !strcasecmp(path + n - 4, ".elf")) ||
        (n >= 2 && !strcasecmp(path + n - 2, ".o")))
        return true;
    return elf_picomite_is_elf(path);
}

uint32_t *elf_picomite_csub_compile(const unsigned char *decl, const char *line, uint32_t base,
                                    const unsigned char *prog, int *words, char *err) {
    char path[STRINGSIZE], name[MAXVARLEN + 1];
    size_t out_size;
    int n = 0, len;
    uint8_t *object, *image = NULL;
    void *ws = NULL;
    uintptr_t entry;
    if (!single_path(line, path, sizeof(path))) {
        snprintf(err, MAXERRMSG, "CSUB: expected one ELF object");
        return NULL;
    }
    resolve_program_path(prog, path, sizeof(path));
    object = elf_read_file(path, &len, err);
    if (!object) return NULL;
    decl += 2;
    skipspace(decl);
    while (isnamechar(*decl) && n < MAXVARLEN) name[n++] = (char)*decl++;
    name[n] = 0;
    if (!elf_abi_supported(object, len,
#ifdef rp2350
         true,
#else
         false,
#endif
         err, MAXERRMSG)) goto done;
    ws = GetMemoryNull(elf_workspace_size());
    if (!ws) { snprintf(err, MAXERRMSG, "ELF: not enough memory"); goto done; }
    if (!elf_image_size(ws, object, len, 24, false, &out_size, err, MAXERRMSG)) goto done;
    out_size = (out_size + 3u) & ~3u;
    if (out_size > INT_MAX || !(image = GetMemoryNull((int)out_size))) {
        snprintf(err, MAXERRMSG, "ELF: not enough memory"); goto done;
    }
    if (!elf_link_image(ws, object, len, image, out_size, base, 24, false,
                        name, elf_resolve_symbol, (void *)1, &entry, err, MAXERRMSG)) {
        /* Conventional external CSUB entry fallback. */
        if (strncmp(err, "ELF: missing entry", 18) ||
            !elf_link_image(ws, object, len, image, out_size, base, 24, false,
                            "main", elf_resolve_symbol, (void *)1, &entry, err, MAXERRMSG)) goto done;
    }
    ((uint32_t *)image)[0] = 3;
    ((uint32_t *)image)[1] = ELF_CSUB_MAGIC;
    ((uint32_t *)image)[2] = elf_firmware_id();
    ((uint32_t *)image)[3] = base;
    ((uint32_t *)image)[4] = 0x47184b00u; /* ldr r3,[pc,#0]; bx r3 */
    ((uint32_t *)image)[5] = (uint32_t)entry | 1u;
    *words = (int)(out_size / 4);
    if (ws) FreeMemory(ws);
    FreeMemory(object);
    return (uint32_t *)image;
done:
    if (image) FreeMemory(image);
    if (ws) FreeMemory(ws);
    FreeMemory(object);
    return NULL;
}

void elf_picomite_csub_check(const uint32_t *code) {
    if (code[0] == ELF_CSUB_MAGIC &&
        (code[1] != elf_firmware_id() || code[2] != (uint32_t)(code - 1)))
        error("ELF CSUB built for another firmware or place: LOAD again");
}

bool elf_picomite_csub_stale(void) {
    const uint32_t *p = (const uint32_t *)CFunctionFlash;
    if (!p) return false;
    while (*p != 0xffffffffu) {
        uint32_t decl = *p++;
        if (!(decl & 0x80000000u) && *p >= 24 && p[1] == 3 &&
            p[2] == ELF_CSUB_MAGIC &&
            (p[3] != elf_firmware_id() || p[4] != (uint32_t)(p + 1))) return true;
        p += (*p + 4u) / 4u;
    }
    return false;
}

void elf_picomite_csub_rebuild(void) {
    unsigned char *buf, *q, *p, line[STRINGSIZE];
    if (ProgMemory != flash_progmemory)
        error("ELF CSUB built for another firmware: LOAD again");
    MMPrintString("Relinking ELF CSUBs for this firmware\r\n");
    q = buf = GetTempMemory(EDIT_BUFFER_SIZE - 2048);
    p = ProgMemory;
    while (!(*p == 0 || *p == 0xff)) {
        size_t n;
        p = llist(line, p);
        n = strlen((char *)line);
        if ((size_t)(q - buf) + n + 2 >= EDIT_BUFFER_SIZE - 2048)
            error("Not enough memory");
        memcpy(q, line, n);
        q += n;
        *q++ = '\n';
        if (p[0] == 0 && p[1] == 0) break;
    }
    *q = 0;
    SaveProgramToFlash(buf, false, PROGRAM_FLASH);
}

#ifndef rp2350
static void *running_image;
#endif

/* Entry is resolved only once the image has reached its final RAM address. */
static int elf_exec_argv_impl(int argc, char **argv, bool tcc_runtime) {
    int len, result = -1;
#ifndef rp2350
    (void)tcc_runtime;
#endif
    char err[MAXERRMSG];
    size_t sz;
    uint8_t *object = NULL, *image = NULL;
    void *ws = NULL;
    uintptr_t entry;
    if (argc < 1) return -1;
    object = elf_read_file(argv[0], &len, err);
    if (!object) goto fail;
    if (!elf_abi_supported(object, len,
#ifdef rp2350
                           true,
#else
                           false,
#endif
                           err, sizeof(err))) goto fail;
    ws = GetMemoryNull(elf_workspace_size());
    if (!ws) { snprintf(err, sizeof(err), "ELF: not enough memory"); goto fail; }
    if (!elf_image_size(ws, object, len, 0, true, &sz, err, sizeof(err))) goto fail;
#ifdef rp2350
    image = tcc_runtime ? tcc_pm_malloc(sz) : GetMemoryNull((int)sz);
#else
    image = GetMemoryNull((int)sz);
#endif
    if (!image) { snprintf(err, sizeof(err), "ELF: not enough memory"); goto fail; }
    if (!elf_link_image(ws, object, len, image, sz, (uintptr_t)image, 0, true,
                        "main", elf_resolve_symbol, NULL, &entry, err, sizeof(err))) goto fail;
    FreeMemory(ws); ws = NULL;
    FreeMemory(object); object = NULL;
    __asm volatile("dsb\n\tisb" ::: "memory");
#ifdef rp2350
    if (tcc_runtime) result = tcc_pm_call_main((int (*)(int, char **))entry, argc, argv);
    else result = ((int (*)(int, char **))entry)(argc, argv);
    if (tcc_runtime) tcc_pm_free(image);
    else FreeMemory(image);
#else
    running_image = image;
    pm_runtime_begin();
    result = pm_runtime_call_main((int (*)(int, char **))entry, argc, argv);
    pm_runtime_end();
    running_image = NULL;
    FreeMemory(image);
#endif
    return result;
fail:
    if (ws) FreeMemory(ws);
    if (object) FreeMemory(object);
    if (image) {
#ifdef rp2350
        if (tcc_runtime) tcc_pm_free(image);
        else FreeMemory(image);
#else
        FreeMemory(image);
#endif
    }
    error("$", err);
    return -1;
}

int elf_picomite_run_argv(int argc, char **argv) {
#ifdef rp2350
    return elf_exec_argv_impl(argc, argv, true);
#else
    return elf_exec_argv_impl(argc, argv, false);
#endif
}

#ifndef rp2350
/* RP2040 has no TinyCC state; use a private stack for main().  Longjmps to
   PicoMite's prompt run elf_picomite_abandoned() before heap reinitialisation. */
static void *running_image_stack;
static bool running;
/* MMBasic tests the active stack against heapend/stackwarn.  While RUN uses
   its private heap-allocated stack those bounds must follow that stack. */
extern uint32_t heapend, stackwarn, StackLow;
extern int StackLowDepth;
static uint32_t saved_heapend, saved_stackwarn, saved_stacklow;
static int saved_stacklowdepth;
#define ELF_STACK_MARK_WORDS 64u
static void elf_stack_restore(void) {
    heapend = saved_heapend;
    stackwarn = saved_stackwarn;
    StackLow = saved_stacklow;
    StackLowDepth = saved_stacklowdepth;
}
static bool elf_stack_intact(const void *stack) {
    const uint32_t *p = (const uint32_t *)stack;
    unsigned i;
    for (i = 0; i < ELF_STACK_MARK_WORDS; ++i)
        if (p[i] != (0x5ac0ffeeu ^ i)) return false;
    return true;
}
static int __attribute__((naked,noinline)) elf_stack_call(int (*fn)(void *), void *arg, void *top) {
    __asm volatile("push {r4, lr}\n\tmov r4, sp\n\tmov sp, r2\n\tmov r3, r0\n\tmov r0, r1\n\tblx r3\n\tmov sp, r4\n\tpop {r4, pc}\n\t");
}
static int elf_run_trampoline(void *v) {
    char **a = (char **)v;
    return elf_exec_argv_impl((int)(uintptr_t)a[0], a + 1, false);
}
void elf_picomite_run_file(const char *filename, const char *args) {
    char line[STRINGSIZE * 2], *words[33], *p;
    int n = 1;
    snprintf(line, sizeof(line), "%s", args ? args : "");
    words[0] = (char *)filename;
    p = line;
    while (*p && n < 32) {
        char *start, *out;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        words[n++] = start = out = p;
        while (*p && *p != ' ' && *p != '\t') {
            if (*p == '"') { p++; while (*p && *p != '"') *out++ = *p++; if (*p) p++; }
            else *out++ = *p++;
        }
        if (*p) p++;
        *out = 0;
    }
    words[n] = 0;
    /* Trampoline receives [argc-as-pointer, argv...] */
    char *params[34];
    params[0] = (char *)(uintptr_t)n;
    memcpy(params + 1, words, (n + 1) * sizeof(char *));
    running_image_stack = GetMemoryNull(ELF_RUN_STACK);
    if (!running_image_stack) error("Not enough memory");
    {
        uint32_t *p = (uint32_t *)running_image_stack;
        unsigned i;
        for (i = 0; i < ELF_STACK_MARK_WORDS; ++i)
            p[i] = 0x5ac0ffeeu ^ i;
    }
    saved_heapend = heapend;
    saved_stackwarn = stackwarn;
    saved_stacklow = StackLow;
    saved_stacklowdepth = StackLowDepth;
    heapend = (uint32_t)running_image_stack + ELF_STACK_MARK_WORDS * 4u + 256u;
    stackwarn = heapend + 1024u;
    running = true;
    elf_stack_call(elf_run_trampoline, params,
                   (char *)running_image_stack + ELF_RUN_STACK);
    running = false;
    elf_stack_restore();
    if (!elf_stack_intact(running_image_stack)) {
        FreeMemory(running_image_stack);
        running_image_stack = NULL;
        error("ELF: C stack overflow");
    }
    FreeMemory(running_image_stack);
    running_image_stack = NULL;
}
void elf_picomite_abandoned(void) {
    if (!running) return;
    running = false;
    /* C allocations must be released before the abandoned ELF image. */
    pm_runtime_end();
    elf_stack_restore();
    if (running_image) FreeMemory(running_image);
    running_image = NULL;
    if (running_image_stack) FreeMemory(running_image_stack);
    running_image_stack = NULL;
}
#else
void elf_picomite_run_file(const char *filename, const char *args) {
    (void)filename; (void)args;
    error("Internal ELF runtime error");
}
void elf_picomite_abandoned(void) { }
#endif
