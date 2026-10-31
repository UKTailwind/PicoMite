/*
 * TinyCC <-> PicoMite adapter (see tcc_picomite.h).
 *
 * Compiled with the firmware, so it sees all of PicoMite.  It gives TinyCC:
 *
 *  - memory: an arena of chunks from the PicoMite heap (GetMemoryNull: SRAM,
 *    then PSRAM), sub-allocated with a first-fit free list and released as a
 *    whole at the end of the run - the compiler, the code it generates and the
 *    compiled program's own malloc() all live in it;
 *  - files: TinyCC's open/read/lseek/close and fopen/fwrite/... go through
 *    MMBasic's file layer (BasicFileOpen and friends), so A:, B:, C: and the
 *    current directory work as they do for BASIC; the built-in headers are
 *    read from the firmware ("rom:/include");
 *  - the console: stdout/stderr go to MMPrintString, stdin reads a line
 *    with MMInkey;
 *  - the run-time symbol table for "-run" (libc, the ARM EABI helpers and a
 *    small PicoMite API, see include/picomite.h);
 *  - a private stack: TinyCC's recursive descent and the compiled program
 *    need more than the 8 KB main stack of most builds.
 *
 * MMBasic errors raised by PicoMite calls are trapped (ErrNext with
 * OptionErrorSkip, as ON ERROR SKIP does) and turned into C error returns, or,
 * in the compiled program, into the end of the program; TinyCC then unwinds
 * normally and the error is raised again at the prompt.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "pico/printf.h"
#include "pico/time.h"
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "tcc_picomite.h"
#include "include/picomite/serial.h"
#include "include/picomite/i2c.h"
#include "include/picomite/spi.h"
#include "include/picomite/adc.h"
#include "include/picomite/pwm.h"

#ifndef TCC_PM_STACK_SIZE
#define TCC_PM_STACK_SIZE (32 * 1024) /* compiler + compiled program */
#endif
#define TCC_PM_CHUNK_SIZE (16 * 1024)  /* arena growth step */
#define TCC_PM_MAX_FILES 8             /* adapter files open at once */
#define TCC_PM_FD_BASE 3               /* 0..2 are stdin/out/err */

/* ------------------------------------------------------------------------ */
/* MMBasic error trap                                                        */
/* ------------------------------------------------------------------------ */

/* error() goes to ErrNext while OptionErrorSkip is between 1 and 100000.  The
   trap saves what it changes and puts it back, so traps nest and the BASIC
   state after a trapped error is as it was before. */
typedef struct
{
    jmp_buf errnext;
    int skip;
    int mmerrno;
    unsigned char *cmdline;
    char errmsg[MAXERRMSG];
} pm_trap;

/* getfullfilename() takes a drive letter off a file name only while the
   command line is not empty: make sure it is not while files are opened. */
static unsigned char pm_cmdline[] = " ";

static void pm_trap_save(pm_trap *t)
{
    memcpy(t->errnext, ErrNext, sizeof(jmp_buf));
    t->skip = OptionErrorSkip;
    t->mmerrno = MMerrno;
    t->cmdline = cmdline;
    memcpy(t->errmsg, MMErrMsg, MAXERRMSG);
    OptionErrorSkip = 1;
    cmdline = pm_cmdline;
}

static void pm_trap_restore(pm_trap *t)
{
    memcpy(ErrNext, t->errnext, sizeof(jmp_buf));
    OptionErrorSkip = t->skip;
    MMerrno = t->mmerrno;
    cmdline = t->cmdline;
    memcpy(MMErrMsg, t->errmsg, MAXERRMSG);
}

/* if (PM_TRY(trap)) { ...may call error()... } else { ...it did... }
   pm_trap_restore(&trap);  - always, on both paths */
#define PM_TRY(t) (pm_trap_save(&(t)), setjmp(ErrNext) == 0)

/* ------------------------------------------------------------------------ */
/* Run state                                                                 */
/* ------------------------------------------------------------------------ */

enum
{
    PM_END_NONE = 0, /* the program ran to its end, or exit() */
    PM_END_ABORT,    /* CTRL-C */
    PM_END_ERROR     /* an MMBasic error in a PicoMite call */
};

static volatile bool pm_active;       /* between entry and cleanup of a run */
volatile bool tcc_pm_running;         /* the compiled program is running (ProgramRunning) */
static volatile bool pm_prog_started; /* it was compiled and started */
static volatile int pm_prog_end;      /* PM_END_* */
static volatile int pm_exit_code;

/* Everything else a run needs lives in the arena (pm_state, set up by
   tcc_picomite_run): until RUN "file.c" TinyCC takes no RAM but a few
   pointers and its own globals. */
typedef struct pm_state pm_state;
static pm_state *pm;

static void __attribute__((noreturn)) pm_end_program(int how, int code);

/* CTRL-C ends the program; the compiler itself is not interrupted (it
   finishes quickly, and the program is not started if CTRL-C is pending). */
static bool pm_tick_any; /* a SETTICK is set (pm_settick) */
static void pm_tick_service(void);

static inline void pm_poll(void)
{
    if (MMAbort && tcc_pm_running)
        pm_end_program(PM_END_ABORT, 1);
    if (pm_tick_any)
        pm_tick_service();
}

/* ------------------------------------------------------------------------ */
/* Memory                                                                    */
/* ------------------------------------------------------------------------ */

typedef union pm_hdr
{
    struct
    {
        union pm_hdr *next; /* next free block (address order) */
        size_t units;       /* size in units, header included */
    } s;
    long long align;
} pm_hdr;

typedef union pm_chunk
{
    union pm_chunk *next; /* next chunk from the PicoMite heap */
    long long align;
} pm_chunk;

static pm_hdr pm_base;
static pm_hdr *pm_freep;
static pm_chunk *pm_chunks;

static void pm_free_block(pm_hdr *bp)
{
    pm_hdr *p;
    for (p = pm_freep; !(bp > p && bp < p->s.next); p = p->s.next)
        if (p >= p->s.next && (bp > p || bp < p->s.next))
            break; /* at one end of the arena */
    if (bp + bp->s.units == p->s.next)
    { /* join the block above */
        bp->s.units += p->s.next->s.units;
        bp->s.next = p->s.next->s.next;
    }
    else
        bp->s.next = p->s.next;
    if (p + p->s.units == bp)
    { /* join the block below */
        p->s.units += bp->s.units;
        p->s.next = bp->s.next;
    }
    else
        p->s.next = bp;
    pm_freep = p;
}

static pm_hdr *pm_morecore(size_t units)
{
    size_t bytes = units * sizeof(pm_hdr) + sizeof(pm_chunk);
    pm_chunk *c;
    pm_hdr *b;
    if (bytes < TCC_PM_CHUNK_SIZE)
        bytes = TCC_PM_CHUNK_SIZE;
    bytes = (bytes + PAGESIZE - 1) & ~(size_t)(PAGESIZE - 1);
    if (bytes > INT_MAX)
        return NULL;
    c = (pm_chunk *)GetMemoryNull((int)bytes);
    if (c == NULL)
        return NULL;
    c->next = pm_chunks;
    pm_chunks = c;
    b = (pm_hdr *)(c + 1);
    b->s.units = (bytes - sizeof(pm_chunk)) / sizeof(pm_hdr);
    pm_free_block(b);
    return pm_freep;
}

void *tcc_pm_malloc(size_t size)
{
    pm_hdr *p, *prev;
    size_t units;
    if (size == 0)
        size = 1;
    if (size > INT_MAX / 2)
        return NULL;
    units = (size + sizeof(pm_hdr) - 1) / sizeof(pm_hdr) + 1;
    if ((prev = pm_freep) == NULL)
    {
        pm_base.s.next = pm_freep = prev = &pm_base;
        pm_base.s.units = 0;
    }
    for (p = prev->s.next;; prev = p, p = p->s.next)
    {
        if (p->s.units >= units)
        {
            if (p->s.units == units)
                prev->s.next = p->s.next;
            else
            { /* hand out the top of the block */
                p->s.units -= units;
                p += p->s.units;
                p->s.units = units;
            }
            pm_freep = prev;
            return (void *)(p + 1);
        }
        if (p == pm_freep && (p = pm_morecore(units)) == NULL)
            return NULL;
    }
}

void tcc_pm_free(void *ptr)
{
    if (ptr != NULL)
        pm_free_block((pm_hdr *)ptr - 1);
}

void *tcc_pm_realloc(void *ptr, size_t size)
{
    size_t have;
    void *n;
    if (ptr == NULL)
        return tcc_pm_malloc(size);
    if (size == 0)
    {
        tcc_pm_free(ptr);
        return NULL;
    }
    have = (((pm_hdr *)ptr - 1)->s.units - 1) * sizeof(pm_hdr);
    if (size <= have)
        return ptr;
    n = tcc_pm_malloc(size);
    if (n == NULL)
        return NULL;
    memcpy(n, ptr, have);
    tcc_pm_free(ptr);
    return n;
}

static void *pm_calloc(size_t nmemb, size_t size)
{
    void *p;
    if (size && nmemb > (size_t)-1 / size)
        return NULL;
    p = tcc_pm_malloc(nmemb * size);
    if (p)
        memset(p, 0, nmemb * size);
    return p;
}

static char *pm_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = tcc_pm_malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

/* Give every chunk back to the PicoMite heap. */
/* pm_alloc's blocks (<picomite/memory.h>): straight from the MMBasic heap,
   listed here so they go back with the arena */
typedef union pm_block
{
    union pm_block *next;
    long long align;
} pm_block;
static pm_block *pm_blocks;

static void pm_heap_release(void)
{
    pm_chunk *c = pm_chunks;
    while (pm_blocks)
    {
        pm_block *next = pm_blocks->next;
        FreeMemory(pm_blocks);
        pm_blocks = next;
    }
    while (c)
    {
        pm_chunk *next = c->next;
        FreeMemory(c);
        c = next;
    }
    pm_chunks = NULL;
    pm_freep = NULL;
}

/* ------------------------------------------------------------------------ */
/* Files                                                                     */
/* ------------------------------------------------------------------------ */

enum
{
    PM_FILE_FREE = 0,
    PM_FILE_ROM,
    PM_FILE_DISK,
    PM_FILE_CONIN,
    PM_FILE_CONOUT
};

typedef struct pm_file
{
    int kind;
    int fnbr;                 /* PM_FILE_DISK: MMBasic file number */
    bool write;               /* opened for writing */
    bool read;                /* opened for reading */
    bool append;              /* "a", "a+": every write at the end */
    bool eof, err;            /* feof() / ferror() */
    const unsigned char *rom; /* PM_FILE_ROM */
    unsigned int rom_size, rom_pos;
    int wlen;   /* bytes waiting in wbuf */
    char *wbuf; /* (writing: TCC_PM_WBUF bytes from the arena) */
} pm_file;
#define TCC_PM_WBUF 128

typedef struct
{
    int argc;
    char **argv;
    int ret;
    /* a C CSUB (tcc_picomite_csub_compile) */
    const char *csub_entry;   /* its name: the function to enter */
    uint32_t csub_base;       /* the address its image is linked for */
    const char *csub_dir;     /* relative names are relative to this, or NULL */
    void *csub_image;         /* out: the image, from the MMBasic heap */
    unsigned csub_size;       /* out: its size in bytes */
    char *csub_err;           /* out: the error (MAXERRMSG) */
} pm_run_args;

/* SETTICK in a C program (pm_settick) */
#define TCC_PM_TICKS 4
typedef struct
{
    void (*fn)(void);
    uint64_t period_us, next_us;
} pm_tick;

struct pm_state
{
    pm_tick ticks[TCC_PM_TICKS];
    bool in_tick; /* a tick function is running: not again until it returns */
    unsigned serial_owned; /* bit 0: COM1, bit 1: COM2 */
    unsigned serial_epoch[2];
    unsigned i2c_owned; /* bit 0: I2C0, bit 1: I2C1 */
    unsigned i2c_epoch[2];
    unsigned i2c_timeout[2];
    unsigned spi_owned;
    unsigned spi_epoch[2];
    unsigned spi_bits[2];
    pm_file files[TCC_PM_MAX_FILES];
    pm_file con_in, con_out, con_err;
    jmp_buf prog_jmp; /* tcc_pm_call_main: exit() and errors of the program */
    jmp_buf run_jmp;  /* tcc_picomite_run: exit() of the compiler */
    char prog_errmsg[MAXERRMSG];
    pm_run_args args;
    void *stack;
    uint32_t msplim;
    uint32_t heapend, stackwarn, stacklow; /* MMBasic's stack check, while on our stack */
    int stacklowdepth;
};
#define pm_files (pm->files)

/* pm_try (ON ERROR SKIP): the innermost one in progress.  Its frame is on
   the stack of the code that called it - a C program's, or a C CSUB's. */
typedef struct pm_tryframe
{
    jmp_buf jmp;
    struct pm_tryframe *prev;
    int err;               /* the error caught: MM.ERRNO */
    char msg[MAXERRMSG];   /* and MM.ERRMSG$ */
} pm_tryframe;
static pm_tryframe *pm_try_top;

/* End the compiled program now (exit(), CTRL-C, an MMBasic error). */
static void __attribute__((noreturn)) pm_end_program(int how, int code)
{
    pm_try_top = NULL; /* (its frames are left behind) */
    pm_prog_end = how;
    pm_exit_code = code;
    longjmp(pm->prog_jmp, 1);
}

/* An MMBasic error in a PicoMite call made by the compiled program. */
static void __attribute__((noreturn)) pm_program_error(void)
{
    memcpy(pm->prog_errmsg, MMErrMsg, MAXERRMSG);
    pm->prog_errmsg[MAXERRMSG - 1] = 0;
    pm_end_program(PM_END_ERROR, 1);
}


/* stdin, stdout and stderr of compiled programs (include/stdio.h): set to
   pm->con_* for each run */
/* The console outside a run: a C CSUB printing (stdout and stderr).  In
   flash: nothing writes to a console pm_file. */
static const pm_file pm_con_rom = {.kind = PM_FILE_CONOUT};
FILE *__tcc_pm_stdin;
FILE *__tcc_pm_stdout = (FILE *)&pm_con_rom, *__tcc_pm_stderr = (FILE *)&pm_con_rom;

/* a C CSUB is being linked: refuse what needs a run (tcc_pm_resolve_symbol) */
static bool pm_csub_linking;

static pm_file *pm_file_of(FILE *f)
{
    pm_file *p = (pm_file *)f;
    if (pm == NULL) /* a C CSUB: only the console */
        return (p == &pm_con_rom || f == stdout || f == stderr) ? (pm_file *)&pm_con_rom : NULL;
    if (p == &pm_con_rom)
        return &pm->con_out;
    if (p >= pm_files && p < pm_files + TCC_PM_MAX_FILES && p->kind != PM_FILE_FREE)
        return p;
    if (p == &pm->con_in || f == stdin)
        return &pm->con_in;
    if (p == &pm->con_out || p == &pm->con_err || f == stdout || f == stderr)
        return &pm->con_out;
    return NULL;
}

static pm_file *pm_file_of_fd(int fd)
{
    if (fd >= TCC_PM_FD_BASE && fd < TCC_PM_FD_BASE + TCC_PM_MAX_FILES &&
        pm_files[fd - TCC_PM_FD_BASE].kind != PM_FILE_FREE)
        return &pm_files[fd - TCC_PM_FD_BASE];
    return NULL;
}

static int pm_free_fnbr(void)
{
    int i;
    for (i = MAXOPENFILES; i >= 1; i--)
        if (FileTable[i].com == 0)
            return i;
    return -1;
}

static const tcc_pm_rom_file *pm_rom_find(const char *path)
{
    static const char prefix[] = TCC_PM_ROM_INCLUDE "/";
    unsigned int i;
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
        return NULL;
    path += sizeof(prefix) - 1;
    for (i = 0; i < tcc_pm_rom_count; i++)
        if (strcmp(tcc_pm_rom_files[i].name, path) == 0)
            return &tcc_pm_rom_files[i];
    return NULL;
}

static bool pm_is_rom_path(const char *path)
{
    return strncmp(path, TCC_PM_ROM_INCLUDE, sizeof(TCC_PM_ROM_INCLUDE) - 1) == 0;
}

/* How fopen()'s mode opens the file: one of MMBasic's OPEN modes (fa: INPUT
   FA_READ, OUTPUT, APPEND, RANDOM), and what C asks on top of it */
typedef struct
{
    int fa;
    bool must_exist; /* "r", "r+" */
    bool truncate;   /* "w+": emptied first (RANDOM keeps the contents) */
    bool rewind;     /* "r+", "w+", "a+": reading starts at the beginning */
    bool read, write, append;
} pm_fmode;
#define PM_FA_OUTPUT (FA_WRITE | FA_CREATE_ALWAYS)
#define PM_FA_APPEND (FA_WRITE | FA_OPEN_APPEND)
#define PM_FA_RANDOM (FA_WRITE | FA_OPEN_APPEND | FA_READ)

static bool pm_fmode_parse(const char *m, pm_fmode *o)
{
    bool plus = false;
    const char *q;
    memset(o, 0, sizeof(*o));
    if (m == NULL || (*m != 'r' && *m != 'w' && *m != 'a'))
        return false;
    for (q = m + 1; *q; q++)
        if (*q == '+')
            plus = true;
        else if (*q != 'b' && *q != 't')
            return false;
    if (*m == 'r')
        o->fa = plus ? PM_FA_RANDOM : FA_READ, o->must_exist = true, o->read = true, o->write = plus;
    else if (*m == 'w')
        o->fa = plus ? PM_FA_RANDOM : PM_FA_OUTPUT, o->truncate = plus, o->write = true, o->read = plus;
    else
        o->fa = plus ? PM_FA_RANDOM : PM_FA_APPEND, o->append = true, o->write = true, o->read = plus;
    o->rewind = plus;
    return true;
}

/* BasicFileOpen without the MMBasic error: false if the file cannot be opened
   (a missing file is not an error for TinyCC, which tries each include
   directory in turn).  fa is an MMBasic OPEN mode (pm_fmode). */
static bool pm_disk_open(const char *path, int fnbr, int fa, bool must_exist)
{
    char name[FF_MAX_LFN + 4];
    volatile bool ok = false;
    pm_trap t;
    if (strlen(path) >= sizeof(name))
        return false;
    strcpy(name, path);
    if (PM_TRY(t))
    {
        FatFSFileSystemSave = FatFSFileSystem;
        /* checked first: a failed FatFS open is closed as a LittleFS one */
        if (!must_exist || ExistsFile(name) == 1)
            ok = BasicFileOpen(name, fnbr, fa);
    }
    else
    {
        ok = false;
        FatFSFileSystem = FatFSFileSystemSave;
        if (filesource[fnbr] != NONEFILE && FileTable[fnbr].com != 0)
            ForceFileClose(fnbr);
    }
    pm_trap_restore(&t);
    return ok;
}

static int pm_close(pm_file *f);

static pm_file *pm_open(const char *path, const pm_fmode *m)
{
    pm_file *f = NULL;
    bool write = m->write;
    int i;
    for (i = 0; i < TCC_PM_MAX_FILES; i++)
        if (pm_files[i].kind == PM_FILE_FREE)
        {
            f = &pm_files[i];
            break;
        }
    if (f == NULL || path == NULL || *path == 0)
        return NULL;
    memset(f, 0, sizeof(*f));
    if (pm_is_rom_path(path))
    {
        const tcc_pm_rom_file *r = pm_rom_find(path);
        if (r == NULL || write)
            return NULL;
        f->kind = PM_FILE_ROM;
        f->rom = r->data;
        f->rom_size = r->size;
        return f;
    }
    f->fnbr = pm_free_fnbr();
    if (f->fnbr < 0)
        return NULL;
    if (m->truncate)
    { /* "w+": OUTPUT empties it, then RANDOM reads and writes it */
        if (!pm_disk_open(path, f->fnbr, PM_FA_OUTPUT, false))
            return NULL;
        ForceFileClose(f->fnbr);
    }
    if (!pm_disk_open(path, f->fnbr, m->fa, m->must_exist))
        return NULL;
    if (write && (f->wbuf = tcc_pm_malloc(TCC_PM_WBUF)) == NULL)
    {
        ForceFileClose(f->fnbr);
        return NULL;
    }
    f->kind = PM_FILE_DISK;
    f->write = write;
    f->read = m->read;
    f->append = m->append;
    if (m->rewind)
    { /* (RANDOM opens at the end) */
        volatile bool ok = true;
        pm_trap t;
        if (PM_TRY(t))
            positionfile(f->fnbr, 0, true);
        else
            ok = false;
        pm_trap_restore(&t);
        if (!ok)
        {
            pm_close(f);
            return NULL;
        }
    }
    return f;
}

static long pm_disk_size(pm_file *f);

static bool pm_disk_flush(pm_file *f)
{
    volatile bool ok = true;
    pm_trap t;
    if (f->kind != PM_FILE_DISK || f->wlen == 0)
        return true;
    if (PM_TRY(t))
    {
        if (f->append) /* (C's "a": wherever an fseek left it) */
            positionfile(f->fnbr, (int)pm_disk_size(f), true);
        FilePutData(f->wbuf, f->fnbr, f->wlen);
    }
    else
        ok = false;
    pm_trap_restore(&t);
    f->wlen = 0;
    if (!ok)
        f->err = true;
    return ok;
}

static int pm_close(pm_file *f)
{
    int r = 0;
    if (f->kind == PM_FILE_DISK)
    {
        if (!pm_disk_flush(f))
            r = -1;
        ForceFileClose(f->fnbr); /* never raises an error */
        if (FSerror)
            r = -1;
        tcc_pm_free(f->wbuf);
        f->wbuf = NULL;
    }
    f->kind = PM_FILE_FREE;
    return r;
}

static void pm_close_all(void)
{
    int i;
    for (i = 0; i < TCC_PM_MAX_FILES; i++)
        if (pm_files[i].kind != PM_FILE_FREE)
            pm_close(&pm_files[i]);
}

static int pm_disk_read(pm_file *f, void *buf, unsigned int count)
{
    volatile int r = -1;
    unsigned int got = 0;
    pm_trap t;
    if (!pm_disk_flush(f))
        return -1;
    if (PM_TRY(t))
    {
        if (FileGetData(f->fnbr, buf, (int)count, &got) == 0)
            r = (int)got;
    }
    pm_trap_restore(&t);
    return r;
}

static int pm_read(pm_file *f, void *buf, unsigned int count)
{
    int r;
    if (f->kind == PM_FILE_ROM)
    {
        unsigned int left = f->rom_size - f->rom_pos;
        if (count > left)
            count = left;
        memcpy(buf, f->rom + f->rom_pos, count);
        f->rom_pos += count;
        r = (int)count;
    }
    else if (f->kind == PM_FILE_DISK)
        r = f->read ? pm_disk_read(f, buf, count) : -1;
    else
        r = -1;
    if (r < 0)
        f->err = true;
    else if (r == 0 && count)
        f->eof = true;
    return r;
}

static long pm_disk_size(pm_file *f)
{
    if (filesource[f->fnbr] == FLASHFILE)
        return (long)lfs_file_size(&lfs, FileTable[f->fnbr].lfsptr);
    return (long)f_size(FileTable[f->fnbr].fptr);
}

static long pm_tell(pm_file *f)
{
    volatile long r = -1;
    pm_trap t;
    if (f->kind == PM_FILE_ROM)
        return f->rom_pos;
    if (f->kind != PM_FILE_DISK)
        return -1;
    if (PM_TRY(t))
        r = filegetpos(f->fnbr) + f->wlen;
    pm_trap_restore(&t);
    return r;
}

static long pm_seek(pm_file *f, long offset, int whence)
{
    volatile long pos;
    pm_trap t;
    if (f->kind == PM_FILE_ROM)
    {
        pos = whence == SEEK_SET ? offset : whence == SEEK_CUR ? (long)f->rom_pos + offset : (long)f->rom_size + offset;
        if (pos < 0 || pos > (long)f->rom_size)
            return -1;
        f->rom_pos = (unsigned int)pos;
        f->eof = false;
        return pos;
    }
    if (f->kind != PM_FILE_DISK || !pm_disk_flush(f))
        return -1;
    if (PM_TRY(t))
    {
        pos = whence == SEEK_SET ? offset : whence == SEEK_CUR ? filegetpos(f->fnbr) + offset : pm_disk_size(f) + offset;
        if (pos >= 0)
            positionfile(f->fnbr, (int)pos, true);
    }
    else
        pos = -1;
    pm_trap_restore(&t);
    if (pos >= 0)
        f->eof = false;
    return pos;
}

/* descriptor interface (TinyCC's sources and headers) */

int tcc_pm_open(const char *path, int flags, ...)
{
    pm_file *f;
    if ((flags & O_ACCMODE) != O_RDONLY)
        return -1; /* TinyCC writes through fopen() */
    {
        pm_fmode m;
        pm_fmode_parse("r", &m);
        f = pm_open(path, &m);
    }
    return f ? (int)(f - pm_files) + TCC_PM_FD_BASE : -1;
}

int tcc_pm_read(int fd, void *buf, unsigned int count)
{
    pm_file *f = pm_file_of_fd(fd);
    return f ? pm_read(f, buf, count) : -1;
}

long tcc_pm_lseek(int fd, long offset, int whence)
{
    pm_file *f = pm_file_of_fd(fd);
    return f ? pm_seek(f, offset, whence) : -1;
}

int tcc_pm_close(int fd)
{
    pm_file *f = pm_file_of_fd(fd);
    return f ? pm_close(f) : -1;
}

int tcc_pm_unlink(const char *path)
{
    (void)path; /* TinyCC only removes its own temporary files */
    return -1;
}

/* ------------------------------------------------------------------------ */
/* Console                                                                   */
/* ------------------------------------------------------------------------ */

/* Write to the MMBasic console, a C newline becoming CR LF. */
static void pm_con_write(const char *s, size_t n)
{
    char buf[96];
    size_t k = 0;
    while (n--)
    {
        char c = *s++;
        if (c == 0)
            continue;
        if (c == '\n')
            buf[k++] = '\r';
        buf[k++] = c;
        if (k >= sizeof(buf) - 2)
        {
            buf[k] = 0;
            MMPrintString(buf);
            k = 0;
            pm_poll();
        }
    }
    if (k)
    {
        buf[k] = 0;
        MMPrintString(buf);
    }
    pm_poll();
}

/* stdin: one line at a time, echoed and editable with backspace (the line
   buffer comes from the arena when the program first reads) */
static char *pm_line;
static int pm_line_len, pm_line_pos;

static int pm_con_getc(void)
{
    if (pm_line_pos >= pm_line_len)
    {
        pm_line_len = pm_line_pos = 0;
        if (pm_line == NULL && (pm_line = tcc_pm_malloc(STRINGSIZE)) == NULL)
            return EOF;
        for (;;)
        {
            int c;
            pm_poll();
            c = MMInkey();
            if (c == -1)
                continue;
            if (c == '\r' || c == '\n')
            {
                MMPrintString("\r\n");
                pm_line[pm_line_len++] = '\n';
                break;
            }
            if ((c == '\b' || c == 0x7f) && pm_line_len)
            {
                pm_line_len--;
                MMPrintString("\b \b");
            }
            else if (c >= ' ' && c < 0x7f && pm_line_len < STRINGSIZE - 1)
            {
                char e[2] = {(char)c, 0};
                pm_line[pm_line_len++] = (char)c;
                MMPrintString(e);
            }
        }
    }
    return (unsigned char)pm_line[pm_line_pos++];
}

/* ------------------------------------------------------------------------ */
/* stdio                                                                     */
/* ------------------------------------------------------------------------ */

static size_t pm_write(pm_file *f, const char *p, size_t n)
{
    size_t done = 0;
    if (f->kind == PM_FILE_CONOUT)
    {
        pm_con_write(p, n);
        return n;
    }
    if (f->kind != PM_FILE_DISK || !f->write)
    {
        f->err = true;
        return 0;
    }
    while (done < n)
    {
        size_t k = TCC_PM_WBUF - (size_t)f->wlen;
        if (k > n - done)
            k = n - done;
        memcpy(f->wbuf + f->wlen, p + done, k);
        f->wlen += (int)k;
        done += k;
        if (f->wlen == TCC_PM_WBUF && !pm_disk_flush(f))
            return done - k;
    }
    return done;
}

typedef struct
{
    pm_file *f;
    int n;
    char buf[64];
} pm_out;

static void pm_out_flush(pm_out *o)
{
    if (o->n)
        pm_write(o->f, o->buf, (size_t)o->n);
    o->n = 0;
}

static void pm_out_char(char c, void *arg)
{
    pm_out *o = (pm_out *)arg;
    o->buf[o->n++] = c;
    if (o->n == (int)sizeof(o->buf))
        pm_out_flush(o);
}

int tcc_pm_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    pm_out o;
    int r;
    o.f = pm_file_of(stream);
    o.n = 0;
    if (o.f == NULL)
        return -1;
    r = vfctprintf(pm_out_char, &o, fmt, ap);
    pm_out_flush(&o);
    return o.f->err ? -1 : r;
}

int tcc_pm_fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = tcc_pm_vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int tcc_pm_printf(const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = tcc_pm_vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

static int pm_vprintf(const char *fmt, va_list ap)
{
    return tcc_pm_vfprintf(stdout, fmt, ap);
}

static int pm_vsprintf(char *buf, const char *fmt, va_list ap)
{
    return vsnprintf(buf, INT_MAX, fmt, ap);
}

static int pm_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, INT_MAX, fmt, ap);
    va_end(ap);
    return r;
}

/* fopen: "r" "w" "a" and "r+" "w+" "a+" ("b", "t" ignored), as MMBasic's
   OPEN FOR INPUT, OUTPUT, APPEND and RANDOM */
FILE *tcc_pm_fopen(const char *path, const char *mode)
{
    pm_fmode m;
    if (!pm_fmode_parse(mode, &m))
        return NULL;
    return (FILE *)pm_open(path, &m);
}

int tcc_pm_fclose(FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    if (f == NULL)
        return EOF;
    if (f->kind == PM_FILE_CONIN || f->kind == PM_FILE_CONOUT)
        return 0;
    return pm_close(f) ? EOF : 0;
}

size_t tcc_pm_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    if (f == NULL || size == 0 || nmemb == 0)
        return 0;
    return pm_write(f, (const char *)ptr, size * nmemb) / size;
}

size_t tcc_pm_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    size_t want, got = 0;
    if (f == NULL || size == 0 || nmemb == 0)
        return 0;
    want = size * nmemb;
    if (f->kind == PM_FILE_CONIN)
    {
        char *p = (char *)ptr;
        while (got < want)
        {
            int c = pm_con_getc();
            if (c == EOF)
                break;
            p[got++] = (char)c;
            if (c == '\n')
                break;
        }
        return got / size;
    }
    while (got < want)
    {
        int r = pm_read(f, (char *)ptr + got, (unsigned int)(want - got));
        if (r <= 0)
            break;
        got += (size_t)r;
    }
    return got / size;
}

int tcc_pm_fseek(FILE *stream, long offset, int whence)
{
    pm_file *f = pm_file_of(stream);
    return f && pm_seek(f, offset, whence) >= 0 ? 0 : -1;
}

long tcc_pm_ftell(FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    return f ? pm_tell(f) : -1;
}

int tcc_pm_fputc(int c, FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    char ch = (char)c;
    return f && pm_write(f, &ch, 1) == 1 ? (unsigned char)c : EOF;
}

int tcc_pm_fputs(const char *s, FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    size_t n = strlen(s);
    return f && pm_write(f, s, n) == n ? 0 : EOF;
}

int tcc_pm_fflush(FILE *stream)
{
    pm_file *f;
    if (stream == NULL)
        return 0;
    f = pm_file_of(stream);
    if (f == NULL)
        return EOF;
    return pm_disk_flush(f) ? 0 : EOF;
}

int tcc_pm_puts(const char *s)
{
    pm_con_write(s, strlen(s));
    pm_con_write("\n", 1);
    return 0;
}

int tcc_pm_putchar(int c)
{
    char ch = (char)c;
    pm_con_write(&ch, 1);
    return (unsigned char)c;
}

char *tcc_pm_getenv(const char *name)
{
    (void)name;
    return NULL;
}

static int pm_fgetc(FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    unsigned char c;
    if (f == NULL)
        return EOF;
    if (f->kind == PM_FILE_CONIN)
        return pm_con_getc();
    return pm_read(f, &c, 1) == 1 ? c : EOF;
}

static int pm_getchar(void)
{
    return pm_con_getc();
}

static char *pm_fgets(char *s, int size, FILE *stream)
{
    int n = 0;
    if (size <= 0)
        return NULL;
    while (n < size - 1)
    {
        int c = pm_fgetc(stream);
        if (c == EOF)
            break;
        s[n++] = (char)c;
        if (c == '\n')
            break;
    }
    if (n == 0)
        return NULL;
    s[n] = 0;
    return s;
}

static int pm_feof(FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    return f ? f->eof : 0;
}

static int pm_ferror(FILE *stream)
{
    pm_file *f = pm_file_of(stream);
    return f ? f->err : 1;
}

static void pm_rewind(FILE *stream)
{
    tcc_pm_fseek(stream, 0, SEEK_SET);
}

/* ------------------------------------------------------------------------ */
/* exit, time                                                                */
/* ------------------------------------------------------------------------ */

void tcc_pm_exit(int code)
{
    if (tcc_pm_running)
        pm_end_program(PM_END_NONE, code);
    pm_exit_code = code;
    if (pm_active)
        longjmp(pm->run_jmp, 1);
    error("TinyCC exit");
    __builtin_unreachable();
}

static void __attribute__((noreturn)) pm_abort(void)
{
    MMPrintString("abort()\r\n");
    tcc_pm_exit(134);
}

unsigned int tcc_pm_clock_ms(void)
{
    return (unsigned int)(time_us_64() / 1000);
}

time_t tcc_pm_time(void)
{
    time_t t = (time_t)((time_us_64() - TimeSubSecondUs) / 1000000 + TimeOffsetToUptime);
    return t > 0 ? t : (time_t)-1;
}

static time_t pm_c_time(time_t *t)
{
    time_t now = tcc_pm_time();
    if (t)
        *t = now;
    return now;
}

static long pm_clock(void)
{
    return (long)tcc_pm_clock_ms(); /* CLOCKS_PER_SEC is 1000 in include/time.h */
}

/* ------------------------------------------------------------------------ */
/* PicoMite API for compiled programs (include/picomite.h)                   */
/* ------------------------------------------------------------------------ */

/* PM_CALL(statement): run a PicoMite call; an MMBasic error ends the program. */
#define PM_CALL(stmt)                \
    do                               \
    {                                \
        pm_trap t_;                  \
        pm_poll();                   \
        if (PM_TRY(t_))              \
        {                            \
            stmt;                    \
            pm_trap_restore(&t_);    \
        }                            \
        else                         \
        {                            \
            pm_program_error_tr(&t_); \
        }                            \
    } while (0)


/* An MMBasic error (already untrapped) inside a pm_try: back to it */
static void pm_try_catch(const char *msg, int err)
{
    pm_tryframe *f = pm_try_top;
    if (f == NULL)
        return;
    f->err = err ? err : 16; /* (error() makes it 16 when it was 0) */
    memcpy(f->msg, msg, MAXERRMSG);
    f->msg[MAXERRMSG - 1] = 0;
    longjmp(f->jmp, 1);
}

/* An MMBasic error msg (already untrapped): the end of the C program, or in
   a C CSUB an error at its call - unless a pm_try catches it */
static void __attribute__((noreturn)) pm_program_error_tr_msg(const char *m, int err)
{
    char msg[MAXERRMSG];
    memcpy(msg, m, MAXERRMSG);
    msg[MAXERRMSG - 1] = 0;
    pm_try_catch(msg, err);
    if (!tcc_pm_running)
        error("$", msg);
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error();
}

static void __attribute__((noreturn)) pm_program_error_tr(pm_trap *t)
{
    char msg[MAXERRMSG];
    int err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(t);
    pm_try_catch(msg, err);
    memcpy(MMErrMsg, msg, MAXERRMSG);
    if (!tcc_pm_running)
    { /* a C CSUB: an MMBasic error like any other */
        msg[MAXERRMSG - 1] = 0;
        error("$", msg);
    }
    pm_program_error();
}

static int pm_hres(void) { return HRes; }
static int pm_vres(void) { return VRes; }
static void pm_cls(int c) { PM_CALL(ClearScreen(c)); }
static void pm_pixel(int x, int y, int c) { PM_CALL(DrawPixel(x, y, c)); }
static void pm_line_draw(int x1, int y1, int x2, int y2, int w, int c) { PM_CALL(DrawLine(x1, y1, x2, y2, w, c)); }
static void pm_box(int x1, int y1, int x2, int y2, int c) { PM_CALL(DrawRectangle(x1, y1, x2, y2, c)); }
static void pm_circle(int x, int y, int r, int w, int c, int fill) { PM_CALL(DrawCircle(x, y, r, w, c, fill, 1.0)); }
static void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int c, int fill)
{
    PM_CALL(DrawTriangle(x0, y0, x1, y1, x2, y2, c, fill));
}
static void pm_refresh(void) { PM_CALL(Display_Refresh()); }
static void pm_print(const char *s) { pm_con_write(s, strlen(s)); }
static unsigned long long pm_timer_us(void) { return time_us_64(); }

static void pm_delay_us(unsigned int us)
{
    uint64_t end = time_us_64() + us;
    while (time_us_64() < end)
        pm_poll();
}

static void pm_yield(void);

/* PAUSE: waiting as PAUSE does, through CheckAbort - the firmware's
   background work goes on meanwhile (the USB keyboard and mouse, the GUI
   cursor, ...) */
static void pm_pause(unsigned int ms)
{
    uint64_t end = time_us_64() + (uint64_t)ms * 1000;
    do
        pm_yield();
    while (time_us_64() < end);
}

static int pm_inkey(void)
{
    pm_poll();
    return MMInkey();
}

static void pm_pin_mode(int gp, int mode)
{
    PM_CALL(ExtCfg(codemap(gp), mode, 0));
}

static void pm_pin_write(int gp, int value)
{
    PM_CALL(ExtSet(codemap(gp), value));
}

static int pm_pin_read(int gp)
{
    volatile int v = 0;
    PM_CALL(v = (int)ExtInp(codemap(gp)));
    return v;
}

/* Shared with BASIC PIN(): the voltage filter is in PinVal(), raw ADC
   sampling in ExtInp(). No ADC reconfiguration or memory allocation. */
static int pm_adc_pin(int gp, int raw)
{
    int pin;
    if (!tcc_pm_running || pm == NULL)
        error("ADC requires a running C program");
    pin = codemap(gp);
    if (IsInvalidPin(pin))
        error("Invalid ADC pin");
    if (ADCopen || ADCDualBuffering || dmarunning)
        error("ADC in use");
    if (ExtCurrentConfig[pin] != EXT_ANA_IN &&
        (!raw || ExtCurrentConfig[pin] != EXT_ADCRAW))
        error("Pin not configured for ADC");
    return pin;
}

double pm_adc_read(int gp)
{
    volatile double value = 0;
    PM_CALL(value = (double)PinVal(pm_adc_pin(gp, 0)));
    return value;
}

int pm_adc_read_raw(int gp)
{
    volatile int sample = 0;
    PM_CALL(sample = (int)ExtInp(pm_adc_pin(gp, 1)));
    return sample;
}

/* ------------------------------------------------------------------------ */
/* <picomite/system.h>, <picomite/memory.h>, <picomite/graphics.h>           */
/* The structs must match those headers (core/tcc/include/picomite).         */
/* ------------------------------------------------------------------------ */

#define PM_API_VERSION 9 /* (include/picomite/system.h: what each version added) */

/* fill the caller's struct of 'size' bytes from ours: the first word of both
   is the size, which the caller set */
static int pm_fill(void *dst, const void *src, unsigned have)
{
    unsigned want = *(const unsigned *)dst;
    if (want < sizeof(unsigned))
        return -1;
    memcpy((char *)dst + sizeof(unsigned), (const char *)src + sizeof(unsigned),
           (want < have ? want : have) - sizeof(unsigned));
    return 0;
}

static unsigned pm_api_version(void)
{
    return PM_API_VERSION;
}

typedef struct
{
    unsigned size, api_version;
    char firmware[16], device[32];
    unsigned cpu_khz, sram_heap, psram;
    int hres, vres;
    unsigned caps;
} pm_info_t;

static int pm_get_framebuffer(void *fb);

static int pm_get_info(void *out)
{
    pm_info_t i;
    unsigned char *saved_sret = sret, *d;
    int saved_targ = targ;
    struct
    {
        unsigned size;
        void *pixels;
        int w, h, stride, format;
    } fb;
    memset(&i, 0, sizeof(i));
    i.api_version = PM_API_VERSION;
    snprintf(i.firmware, sizeof(i.firmware), "%s", VERSION);
    fun_device(); /* MM.DEVICE$, into a temporary MMBasic string */
    d = sret;
    sret = saved_sret;
    targ = saved_targ;
    memcpy(i.device, d + 1, d[0] < sizeof(i.device) - 1 ? d[0] : sizeof(i.device) - 1);
    ClearSpecificTempMemory(d);
    i.cpu_khz = (unsigned)Option.CPU_Speed;
    i.sram_heap = heap_memory_size;
    i.psram = PSRAMsize;
    i.hres = HRes;
    i.vres = VRes;
#ifdef PICOMITEVGA
    i.caps |= 0x0001;
#else
    if (Option.DISPLAY_TYPE)
        i.caps |= 0x0001;
#endif
    fb.size = sizeof(fb);
    if (pm_get_framebuffer(&fb) == 0)
        i.caps |= 0x0002;
    if (PSRAMsize)
        i.caps |= 0x0004;
    if (SDCardConfigured())
        i.caps |= 0x0008;
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
    return pm_fill(out, &i, sizeof(i));
}

/* CheckAbort without its jump to the prompt (pm_poll ends the program) */
static void pm_yield(void)
{
    pm_poll();
#ifdef PICOMITEWEB
    if (WIFIconnected || WebScanActive)
        ProcessWeb(1);
#endif
    ProcessBT();
    routinechecks();
    pm_poll();
}

static int pm_abort_requested(void)
{
    return MMAbort != 0;
}

/* UART C API: only ports explicitly opened by this C run may be used.
   An epoch prevents accidentally using a port BASIC closed and reopened. */
/* Port occupies bits 0..1, generation occupies bits 2..30; handles are
   opaque and cannot be reused after a close + reopen in the same run. */
static int pm_serial_port(pm_serial_t handle)
{
    unsigned idx, id = (unsigned)handle;
    int port = (int)(id & 3u);
    if (!tcc_pm_running || pm == NULL || port < 1 || port > 2)
        return 0;
    idx = (unsigned)port - 1;
    if (!(pm->serial_owned & (1u << idx)) ||
        pm->serial_epoch[idx] != SerialPortEpoch(port) ||
        (pm->serial_epoch[idx] & 0x1fffffffu) != (id >> 2) ||
        !SerialPortIsOpen(port))
        return 0;
    return port;
}

static int pm_serial_require(pm_serial_t handle)
{
    int port = pm_serial_port(handle);
    if (!port)
        error("Serial port not owned by C program");
    return port;
}

static void pm_serial_cleanup(void)
{
    int port;
    if (pm == NULL)
        return;
    for (port = 1; port <= 2; ++port)
        if ((pm->serial_owned & (1u << (port - 1))) &&
            pm->serial_epoch[port - 1] == SerialPortEpoch(port) && SerialPortIsOpen(port))
            SerialClose(port);
    pm->serial_owned = 0;
}

static int pm_serial_open_do(const pm_serial_config *cfg)
{
    unsigned flags, idx;
    int parity;
    if (!tcc_pm_running || pm == NULL || cfg == NULL)
        error("Invalid serial configuration");
    if (cfg->port < 1 || cfg->port > 2)
        error("Only COM1/COM2 UART supported");
    flags = cfg->flags;
    if (flags & ~(PM_SERIAL_7BIT | PM_SERIAL_STOP2 | PM_SERIAL_EVEN |
                  PM_SERIAL_ODD | PM_SERIAL_INVERT))
        error("Invalid serial flags");
    if ((flags & PM_SERIAL_EVEN) && (flags & PM_SERIAL_ODD))
        error("Conflicting serial parity");
    idx = (unsigned)cfg->port - 1;
    if (pm->serial_owned & (1u << idx))
        error("Serial port already open");
    parity = (flags & PM_SERIAL_EVEN) ? 1 : (flags & PM_SERIAL_ODD) ? 2 : 0;
    SerialOpenTyped(cfg->port, (int)(cfg->baudrate ? cfg->baudrate : 9600),
                    (int)(cfg->rx_buffer_size ? cfg->rx_buffer_size : 1024),
                    (flags & PM_SERIAL_7BIT) ? 7 : 8, parity,
                    (flags & PM_SERIAL_STOP2) ? 2 : 1,
                    (flags & PM_SERIAL_INVERT) ? 1 : 0);
    pm->serial_epoch[idx] = SerialPortEpoch(cfg->port);
    pm->serial_owned |= 1u << idx;
    return (int)(((pm->serial_epoch[idx] & 0x1fffffffu) << 2) | (unsigned)cfg->port);
}

pm_serial_t pm_serial_open(const pm_serial_config *cfg)
{
    volatile int port = -1;
    PM_CALL(port = pm_serial_open_do(cfg));
    return port;
}

int pm_serial_available(pm_serial_t port)
{
    volatile int count = 0;
    volatile int raw_port = 0;
    PM_CALL(raw_port = pm_serial_require(port));
    count = SerialRxStatus(raw_port);
    return count;
}

/* Wait at most timeout_ms from entry, returning a partial transfer.  Zero
   timeout never waits for another byte / TX slot. */
int pm_serial_read(pm_serial_t port, void *buf, unsigned size, unsigned timeout_ms)
{
    unsigned n = 0;
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    volatile int raw_port = 0;
    PM_CALL(raw_port = pm_serial_require(port));
    if (size > INT_MAX || (size && buf == NULL))
        PM_CALL(error("Invalid serial read buffer"));
    while (n < size)
    {
        if (timeout_ms && n && time_us_64() >= end)
            break;
        int c = SerialGetchar(raw_port);
        if (c >= 0)
        {
            ((unsigned char *)buf)[n++] = (unsigned char)c;
            if ((n & 63u) == 0)
            {
                pm_yield();
                PM_CALL(raw_port = pm_serial_require(port));
            }
            continue;
        }
        if (timeout_ms == 0 || time_us_64() >= end)
            break;
        pm_yield();
        PM_CALL(raw_port = pm_serial_require(port));
    }
    return (int)n;
}

int pm_serial_write(pm_serial_t port, const void *buf, unsigned size, unsigned timeout_ms)
{
    unsigned n = 0;
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    volatile int raw_port = 0;
    PM_CALL(raw_port = pm_serial_require(port));
    if (size > INT_MAX || (size && buf == NULL))
        PM_CALL(error("Invalid serial write buffer"));
    while (n < size)
    {
        if (timeout_ms && n && time_us_64() >= end)
            break;
        /* SerialPutchar blocks on a full ring. Check first so timeout is
           actually respected (the IRQ consumes bytes, never adds them). */
        if (SerialTxStatus(raw_port) < TX_BUFFER_SIZE - 1)
        {
            SerialPutchar(raw_port, ((const unsigned char *)buf)[n++]);
            if ((n & 63u) == 0)
            {
                pm_yield();
                PM_CALL(raw_port = pm_serial_require(port));
            }
            continue;
        }
        if (timeout_ms == 0 || time_us_64() >= end)
            break;
        pm_yield();
        PM_CALL(raw_port = pm_serial_require(port));
    }
    return (int)n;
}

/* 0: software queue and hardware shifter idle; -1: deadline expired. */
int pm_serial_flush(pm_serial_t port, unsigned timeout_ms)
{
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000;
    volatile int raw_port = 0;
    PM_CALL(raw_port = pm_serial_require(port));
    while (!SerialTxIdle(raw_port))
    {
        if (timeout_ms == 0 || time_us_64() >= end)
            return -1;
        pm_yield();
        PM_CALL(raw_port = pm_serial_require(port));
    }
    return 0;
}

void pm_serial_close(pm_serial_t port)
{
    volatile int raw_port = 0;
    PM_CALL(raw_port = pm_serial_require(port));
    pm->serial_owned &= ~(1u << (raw_port - 1));
    SerialClose(raw_port);
}

/* I2C handles: low two bits are bus + 1; the remaining bits are an epoch. */
static int pm_i2c_require(pm_i2c_t handle)
{
    unsigned h = (unsigned)handle;
    int bus = (int)(h & 3u) - 1;
    if (!tcc_pm_running || pm == NULL || bus < 0 || bus > 1 ||
        !(pm->i2c_owned & (1u << bus)) ||
        pm->i2c_epoch[bus] != I2C_MasterEpoch(bus) ||
        (pm->i2c_epoch[bus] & 0x1fffffffu) != (h >> 2) ||
        !I2C_MasterIsOpen(bus))
        error("I2C bus not owned by C program");
    return bus;
}

static void pm_i2c_cleanup(void)
{
    if (pm == NULL)
        return;
    for (int bus = 0; bus < 2; bus++)
        if ((pm->i2c_owned & (1u << bus)) &&
            pm->i2c_epoch[bus] == I2C_MasterEpoch(bus) &&
            I2C_MasterIsOpen(bus))
            I2C_MasterClose(bus);
    pm->i2c_owned = 0;
}

static int pm_i2c_open_do(int bus, unsigned speed_khz, unsigned timeout_ms)
{
    if (!tcc_pm_running || pm == NULL || bus < 0 || bus > 1)
        error("Invalid I2C bus");
    if (pm->i2c_owned & (1u << bus))
        error("I2C bus already owned by C program");
    if (!speed_khz)
        speed_khz = 100;
    if (!timeout_ms)
        timeout_ms = 1000;
    I2C_MasterOpen(bus, (int)speed_khz, timeout_ms);
    pm->i2c_owned |= 1u << bus;
    pm->i2c_epoch[bus] = I2C_MasterEpoch(bus);
    pm->i2c_timeout[bus] = timeout_ms;
    return (int)(((pm->i2c_epoch[bus] & 0x1fffffffu) << 2) | (unsigned)(bus + 1));
}

pm_i2c_t pm_i2c_open(int bus, unsigned speed_khz, unsigned timeout_ms)
{
    volatile int handle = 0;
    PM_CALL(handle = pm_i2c_open_do(bus, speed_khz, timeout_ms));
    return handle;
}

int pm_i2c_transfer(pm_i2c_t handle, unsigned address, const void *tx,
                           unsigned txlen, void *rx, unsigned rxlen)
{
    volatile int bus = -1;
    int status;
    PM_CALL(bus = pm_i2c_require(handle));
    if (address > 0x7fu || (!txlen && !rxlen) ||
        txlen > 65535u || rxlen > 65535u ||
        (txlen && tx == NULL) || (rxlen && rx == NULL))
        PM_CALL(error("Invalid I2C transfer"));
    status = I2C_MasterTransfer(bus, address, tx, txlen, rx, rxlen, 0,
                                pm->i2c_timeout[bus]);
    pm_yield();
    return status;
}

int pm_i2c_write(pm_i2c_t handle, unsigned address, const void *tx, unsigned len)
{
    return pm_i2c_transfer(handle, address, tx, len, NULL, 0);
}

int pm_i2c_read(pm_i2c_t handle, unsigned address, void *rx, unsigned len)
{
    return pm_i2c_transfer(handle, address, NULL, 0, rx, len);
}

/* Like BASIC I2C CHECK, probing reads one byte (it can advance a register pointer). */
int pm_i2c_probe(pm_i2c_t handle, unsigned address)
{
    unsigned char sample = 0;
    int status = pm_i2c_read(handle, address, &sample, 1);
    return status == 0 ? 1 : status == 1 ? 0 : -1;
}

void pm_i2c_close(pm_i2c_t handle)
{
    volatile int bus = -1;
    PM_CALL(bus = pm_i2c_require(handle));
    pm->i2c_owned &= ~(1u << bus);
    I2C_MasterClose(bus);
}

static int pm_spi_require(pm_spi_t handle)
{
    unsigned h = (unsigned)handle;
    int bus = (int)(h & 3u) - 1;
    if (!tcc_pm_running || pm == NULL || bus < 0 || bus > 1 ||
        !(pm->spi_owned & (1u << bus)) ||
        pm->spi_epoch[bus] != SPI_MasterEpoch(bus) ||
        (pm->spi_epoch[bus] & 0x1fffffffu) != (h >> 2) ||
        !SPI_MasterIsOpen(bus))
        error("SPI bus not owned by C program");
    return bus;
}

static void pm_spi_cleanup(void)
{
    if (pm == NULL) return;
    for (int bus = 0; bus < 2; ++bus)
        if ((pm->spi_owned & (1u << bus)) &&
            pm->spi_epoch[bus] == SPI_MasterEpoch(bus) &&
            SPI_MasterIsOpen(bus))
            SPI_MasterClose(bus);
    pm->spi_owned = 0;
}

static int pm_spi_open_do(int bus, unsigned speed_hz, unsigned mode, unsigned bits)
{
    if (!tcc_pm_running || pm == NULL || bus < 0 || bus > 1)
        error("Invalid SPI bus");
    if (pm->spi_owned & (1u << bus))
        error("SPI bus already owned by C program");
    if (mode > 3 || (bits != 0 && (bits < 4 || bits > 16)))
        error("Invalid SPI configuration");
    if (!speed_hz) speed_hz = 1000000;
    if (!bits) bits = 8;
    SPI_MasterOpen(bus, speed_hz, mode, bits);
    pm->spi_owned |= 1u << bus;
    pm->spi_epoch[bus] = SPI_MasterEpoch(bus);
    pm->spi_bits[bus] = bits;
    return (int)(((pm->spi_epoch[bus] & 0x1fffffffu) << 2) | (unsigned)(bus + 1));
}

pm_spi_t pm_spi_open(int bus, unsigned speed_hz, unsigned mode, unsigned bits)
{
    volatile int handle = 0;
    PM_CALL(handle = pm_spi_open_do(bus, speed_hz, mode, bits));
    return handle;
}

int pm_spi_transfer(pm_spi_t handle, const void *tx, void *rx, unsigned count)
{
    volatile int bus = -1;
    unsigned done = 0, bytes;
    PM_CALL(bus = pm_spi_require(handle));
    bytes = pm->spi_bits[bus] <= 8 ? 1 : 2;
    if (count > 65535u || (count && tx == NULL && rx == NULL) ||
        (bytes == 2 && (((uintptr_t)tx & 1u) || ((uintptr_t)rx & 1u))))
        PM_CALL(error("Invalid SPI transfer"));
    while (done < count)
    {
        unsigned n = count - done;
        int sent;
        if (n > 64) n = 64;
        sent = SPI_MasterTransfer(bus,
            tx ? (const unsigned char *)tx + done * bytes : NULL,
            rx ? (unsigned char *)rx + done * bytes : NULL, n);
        if (sent != (int)n)
            PM_CALL(error("SPI transfer failed"));
        done += n;
        pm_yield();
        PM_CALL(bus = pm_spi_require(handle));
    }
    return (int)done;
}

int pm_spi_write(pm_spi_t handle, const void *tx, unsigned count)
{
    return pm_spi_transfer(handle, tx, NULL, count);
}

int pm_spi_read(pm_spi_t handle, void *rx, unsigned count)
{
    return pm_spi_transfer(handle, NULL, rx, count);
}

void pm_spi_close(pm_spi_t handle)
{
    volatile int bus = -1;
    PM_CALL(bus = pm_spi_require(handle));
    pm->spi_owned &= ~(1u << bus);
    SPI_MasterClose(bus);
}

/* PWM is persistent PicoMite state, shared with BASIC; no C-owned handles. */
void pm_pwm_open(int slice, unsigned frequency_hz, double duty_a, double duty_b)
{
    PM_CALL(PWM_Configure(slice, frequency_hz, duty_a, duty_b));
}

void pm_pwm_set_duty(int slice, double duty_a, double duty_b)
{
    PM_CALL(PWM_SetDuty(slice, duty_a, duty_b));
}

void pm_pwm_set_frequency(int slice, unsigned frequency_hz)
{
    PM_CALL(PWM_SetFrequency(slice, frequency_hz));
}

void pm_pwm_close(int slice)
{
    PM_CALL(PWM_Close(slice));
}

static void *pm_alloc(size_t size, int where)
{
    pm_block *b;
    if (pm == NULL || size > (size_t)INT_MAX - sizeof(pm_block))
        return NULL;
    size += sizeof(pm_block);
    if (where == 1)
        b = GetSRAMMemoryNull((int)size);
    else if (where == 2)
        b = PSRAMsize ? GetPSMemoryNull((int)size) : NULL;
    else
        b = GetMemoryNull((int)size);
    if (b == NULL)
        return NULL;
    b->next = pm_blocks;
    pm_blocks = b;
    return b + 1;
}

static void pm_free(void *p)
{
    pm_block **q, *b;
    if (p == NULL)
        return;
    b = (pm_block *)p - 1;
    for (q = &pm_blocks; *q; q = &(*q)->next)
        if (*q == b)
        {
            *q = b->next;
            FreeMemory(b);
            return;
        }
}

static int pm_mem_where(const void *p)
{
    const unsigned char *a = (const unsigned char *)p;
    if (PSRAMsize && a >= (const unsigned char *)PSRAMbase && a < (const unsigned char *)(PSRAMbase + PSRAMsize))
        return 2;
    if (a >= MMHeap && a < MMHeap + heap_memory_size)
        return 1;
    return 0;
}

extern unsigned int SBitsGet(unsigned char *addr); /* core/Memory.c */

static int pm_get_mem_info(void *out)
{
    struct
    {
        unsigned size, sram_total, sram_free, psram_total, psram_free;
    } m;
    unsigned char *a;
    unsigned all = (unsigned)FreeSpaceOnHeap(), ps = 0;
    memset(&m, 0, sizeof(m));
    if (PSRAMsize)
        for (a = (unsigned char *)PSRAMbase; a < (unsigned char *)(PSRAMbase + PSRAMsize); a += PAGESIZE)
            if (!(SBitsGet(a) & PUSED))
                ps += PAGESIZE;
    m.sram_total = heap_memory_size;
    m.sram_free = all - ps;
    m.psram_total = PSRAMsize;
    m.psram_free = ps;
    return pm_fill(out, &m, sizeof(m));
}

/* PM_DEFAULT in <picomite/graphics.h>: the argument as if left out in BASIC */
#define PM_DEFAULT_ARG (-2)

/* getint()'s error for an argument out of its range */
static void pm_range(int v, int lo, int hi)
{
    if (v < lo || v > hi)
        error("~ is invalid (valid is ~ to ~)", (long long)v, (long long)lo, (long long)hi);
}

/* a font and scale as TEXT and FONT check them (PM_DEFAULT_ARG: the current) */
static int pm_font_code(int font, int scale)
{
    if (font == PM_DEFAULT_ARG)
        font = (gui_font >> 4) + 1;
    if (scale == PM_DEFAULT_ARG)
        scale = gui_font & 0b1111;
    pm_range(font, 1, FONT_TABLE_SIZE);
    if (FontTable[font - 1] == NULL)
        error("Invalid font #%", font);
    pm_range(scale, 1, 15);
    return ((font - 1) << 4) | scale;
}

/* a colour argument as BASIC's getint(arg, lo, WHITE) checks it */
static int pm_colour_arg(int c, int lo, int dflt)
{
    if (c == PM_DEFAULT_ARG)
        return dflt;
    pm_range(c, lo, WHITE);
    return c;
}

/* TEXT x, y, s$ [, align$] [, font] [, scale] [, c] [, bc]  (cmd_text) */
static void pm_text_do(int x, int y, const char *s, const char *align, int font, int scale, int fg, int bg)
{
    int jh = 0, jv = 0, jo = 0, code;
    CheckDisplay();
    if (align != NULL && *align)
        if (!GetJustification((char *)align, &jh, &jv, &jo))
            error("Justification");
    code = pm_font_code(font, scale);
    fg = pm_colour_arg(fg, 0, gui_fcolour);
    bg = pm_colour_arg(bg, -1, gui_bcolour);
    GUIPrintString(x, y, code, jh, jv, jo, fg, bg, (char *)s);
    if (Option.Refresh)
        Display_Refresh();
}

static void pm_text(int x, int y, const char *s, const char *align, int font, int scale, int fg, int bg)
{
    PM_CALL(pm_text_do(x, y, s, align, font, scale, fg, bg));
}

/* BOX x, y, w, h [, lw] [, c] [, fill]  (cmd_box, for single values) */
static void pm_draw_box_do(int x, int y, int w, int h, int lw, int c, int fill)
{
    CheckDisplay();
    if (lw == PM_DEFAULT_ARG)
        lw = 1;
    else
        pm_range(lw, 0, 100);
    c = pm_colour_arg(c, 0, gui_fcolour);
    fill = pm_colour_arg(fill, -1, -1);
    if (w != 0 && h != 0)
        DrawBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), lw, c, fill);
    if (Option.Refresh)
        Display_Refresh();
}

static void pm_draw_box(int x, int y, int w, int h, int lw, int c, int fill)
{
    PM_CALL(pm_draw_box_do(x, y, w, h, lw, c, fill));
}

/* RBOX x, y, w, h [, r] [, c] [, fill]  (cmd_rbox: radius 10, the
   foreground colour and no fill by default) */
static void pm_draw_rbox_do(int x, int y, int w, int h, int r, int c, int fill)
{
    CheckDisplay();
    if (r == PM_DEFAULT_ARG)
        r = 10;
    else
        pm_range(r, 0, 100);
    c = pm_colour_arg(c, 0, gui_fcolour);
    fill = pm_colour_arg(fill, -1, -1);
    if (w != 0 && h != 0)
        DrawRBox(x, y, x + w + (w > 0 ? -1 : 1), y + h + (h > 0 ? -1 : 1), r, c, fill);
    if (Option.Refresh)
        Display_Refresh();
}

static void pm_draw_rbox(int x, int y, int w, int h, int r, int c, int fill)
{
    PM_CALL(pm_draw_rbox_do(x, y, w, h, r, c, fill));
}

/* MODE n  (cmd_mode: VGA and HDMI builds) */
static void pm_mode_do(int mode)
{
#ifdef PICOMITEVGA
    pm_range(mode, 1, MAXMODES);
    setmode(mode, true);
#if defined(USBKEYBOARD) && defined(GUICONTROLS) && defined(PICOMITEVGA)
    OSK_Invalidate();
#endif
#else
    (void)mode;
    error("MODE is not available on this firmware");
#endif
}

static void pm_mode(int mode)
{
    PM_CALL(pm_mode_do(mode));
}

static int pm_font_width(int font, int scale)
{
    volatile int w = 0;
    PM_CALL(w = GetFontWidth(pm_font_code(font, scale)));
    return w;
}

static int pm_font_height(int font, int scale)
{
    volatile int h = 0;
    PM_CALL(h = GetFontHeight(pm_font_code(font, scale)));
    return h;
}

/* bits per pixel of WriteBuf, as the firmware's own buffer code sees it
   (scroll_get_bpp in graphics/Sprite.c) */
static int pm_fb_bpp(void)
{
#ifdef PICOMITEVGA
    if (DISPLAY_TYPE == SCREENMODE1)
        return 1;
#ifdef HDMI
    if (DISPLAY_TYPE == SCREENMODE4)
        return 16;
    if (DISPLAY_TYPE == SCREENMODE5)
        return 8;
#endif
#endif
    return 4;
}

static int pm_get_framebuffer(void *out)
{
    struct
    {
        unsigned size;
        void *pixels;
        int width, height, stride, format;
    } f;
    unsigned char *buf = WriteBuf;
#ifndef PICOMITEVGA
    /* a virtual display draws into FRAMEBUFFER (DrawPixel16 sets it on first use) */
#if PICOMITERP2350
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL && Option.DISPLAY_TYPE < VGA222)
        buf = FRAMEBUFFER;
#else
    if (buf == NULL && Option.DISPLAY_TYPE >= VIRTUAL)
        buf = FRAMEBUFFER;
#endif
#endif
    if (buf == NULL || HRes <= 0 || VRes <= 0)
        return -1;
    memset(&f, 0, sizeof(f));
    f.pixels = buf;
    f.width = HRes;
    f.height = VRes;
    f.format = pm_fb_bpp();
    f.stride = f.format == 1 ? (HRes + 7) >> 3 : f.format == 4 ? (HRes + 1) >> 1 : HRes * (f.format >> 3);
    return pm_fill(out, &f, sizeof(f));
}

static unsigned pm_fb_colour(int rgb)
{
    switch (pm_fb_bpp())
    {
    case 1:
        return (rgb & 0xffffff) != 0;
    case 8:
        return RGB332((uint32_t)rgb);
    case 16:
        return RGB555((uint32_t)rgb);
    default:
        return RGB121((uint32_t)rgb);
    }
}

/* ------------------------------------------------------------------------ */
/* <picomite/basic.h>: EXECUTE, EVAL and SETTICK                             */
/* ------------------------------------------------------------------------ */

/* the interpreter's line buffers, which a command line from C goes through:
   kept for whatever was using them (the prompt's RUN line, a statement
   calling a C CSUB) */
typedef struct
{
    unsigned char tkn[STRINGSIZE], inp[STRINGSIZE];
    unsigned char *nextstmt, *cmdline;
    int cmdtoken, multi;
} pm_linebufs;

static void pm_lines_save(pm_linebufs *b)
{
    memcpy(b->tkn, tknbuf, STRINGSIZE);
    memcpy(b->inp, inpbuf, STRINGSIZE);
    b->nextstmt = nextstmt;
    b->cmdline = cmdline;
    b->cmdtoken = cmdtoken;
    b->multi = multi;
}

static void pm_lines_restore(const pm_linebufs *b)
{
    memcpy(tknbuf, b->tkn, STRINGSIZE);
    memcpy(inpbuf, b->inp, STRINGSIZE);
    nextstmt = b->nextstmt;
    cmdline = b->cmdline;
    cmdtoken = b->cmdtoken;
    multi = b->multi;
}

/* the temporary strings a command left: in a C program nothing else holds
   any; in a C CSUB its caller's statement does, and clears them itself */
static void pm_temps_done(void)
{
    if (tcc_pm_running)
        ClearTempMemory();
}

/* A command that is allowed in a BASIC program but replaces it, and with it
   the heap a C program runs in: RUN, CHAIN, NEW, LOAD file$ [, R],
   FLASH RUN / CHAIN, RAM RUN / CHAIN.  (Those MMBasic allows only at the
   prompt are refused by their own "Invalid in a program", ProgramRunning;
   tcc_picomite_reset_guard stops anything else that resets the heap.)
   Refused here, before the command has changed anything. */
static bool pm_replaces_program(void (*fn)(void), unsigned char *args)
{
    static const char *const load_sub[] = {"CONTEXT", "IMAGE", "DATA", "BMP", "JPG", "PNG"};
    unsigned i;
    if (fn == cmd_run || fn == cmd_chain || fn == cmd_new)
        return true;
    if (fn == cmd_flash || fn == cmd_psram)
        return checkstring(args, (unsigned char *)"RUN") || checkstring(args, (unsigned char *)"CHAIN");
    if (fn == cmd_load)
    {
        for (i = 0; i < sizeof(load_sub) / sizeof(load_sub[0]); i++)
            if (checkstring(args, (unsigned char *)load_sub[i]))
                return false;
        return true; /* LOAD file$: a program */
    }
    return false;
}

/* EXECUTE cmd$ (execute() in core/Commands.c): one statement, as typed at
   the prompt, tokenised and run here rather than by ExecuteProgram, whose
   own error trapping would hide its errors from PM_CALL */
static void pm_execute_do(const char *cmd)
{
    int i = 0, toggle = 0;
    unsigned char *p;
    if (strlen(cmd) >= STRINGSIZE)
        error("String too long");
    strcpy((char *)inpbuf, cmd);
    p = inpbuf;
    skipspace(p);
    while (inpbuf[i])
    { /* upper case outside quotes, one statement */
        if (inpbuf[i] == 34)
            toggle = !toggle;
        if (!toggle)
        {
            if (inpbuf[i] == ':')
                error("Only single statements allowed");
            inpbuf[i] = mytoupper(inpbuf[i]);
        }
        i++;
    }
    multi = false;
    tokenise(true);
    memset(inpbuf, 0, STRINGSIZE);
    p = tknbuf;
    skipspace(p);
    if (*p == 0)
        return;
    targ = T_CMD;
    if (!commandtbl_is_cmd(p))
        StandardError(36); /* (no SUBs: a C program has none) */
    {
        CommandToken c = commandtbl_decode(p);
        if (c == cmdWHILE || c == cmdDO || c == cmdFOR)
            error("Invalid inside THEN ... ELSE");
        cmdtoken = c;
        cmdline = p + sizeof(CommandToken);
        skipspace(cmdline);
        if (pm_replaces_program(commandtbl[c].fptr, cmdline))
            error("Not possible from a C program");
        if (!SymAwareCommand(c))
            cmdline = SymExpandStatement(cmdline);
        /* (not EXECUTE's ScrewUpTimer: its 1 s limit would restart the
           board in a PLAY MODFILE or LOAD IMAGE that takes longer, which
           a program's own statement may) */
        commandtbl[c].fptr();
    }
}

static void pm_execute(const char *cmd)
{
    pm_linebufs b;
    pm_trap t;
    char msg[MAXERRMSG];
    int err;
    pm_poll();
    pm_lines_save(&b);
    if (PM_TRY(t))
    {
        pm_execute_do(cmd);
        pm_trap_restore(&t);
        pm_lines_restore(&b);
        pm_temps_done();
        return;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(&t);
    pm_lines_restore(&b);
    pm_temps_done();
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error_tr_msg(msg, err);
}

/* EVAL(s$) (fun_eval in core/Functions.c) */
static int pm_eval_do(const char *expr, MMFLOAT *f, long long *i, unsigned char *str)
{
    unsigned char *s = NULL;
    int t = T_NOTYPE;
    if (strlen(expr) + 2 >= STRINGSIZE)
        error("String too long");
    inpbuf[0] = 'r';
    inpbuf[1] = '='; /* a dummy assignment keeps the tokeniser happy */
    strcpy((char *)inpbuf + 2, expr);
    multi = false;
    tokenise(true);
    evaluate(tknbuf + 2 + sizeof(CommandToken), f, i, &s, &t, false);
    if ((t & T_STR) && str != NULL)
        Mstrcpy(str, s);
    return t;
}

/* the value of expr, as type: 'i', 'f' or 's' */
static void pm_eval(const char *expr, int type, long long *ip, double *fp, char *sp, int size)
{
    int err;
    pm_linebufs b;
    pm_trap t;
    char msg[MAXERRMSG];
    MMFLOAT f = 0;
    long long i = 0;
    unsigned char str[STRINGSIZE];
    volatile int ty = 0;
    pm_poll();
    pm_lines_save(&b);
    if (PM_TRY(t))
    {
        ty = pm_eval_do(expr, &f, &i, str);
        if (type == 's' && !(ty & T_STR))
            error("Expected a string");
        if (type != 's' && (ty & T_STR))
            error("Expected a number");
        pm_trap_restore(&t);
        pm_lines_restore(&b);
        pm_temps_done();
        if (type == 'i')
            *ip = (ty & T_NBR) ? FloatToInt64(f) : i;
        else if (type == 'f')
            *fp = (ty & T_NBR) ? f : (double)i;
        else if (size > 0)
        {
            int n = str[0] < size - 1 ? str[0] : size - 1;
            memcpy(sp, str + 1, (size_t)n);
            sp[n] = 0;
        }
        return;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(&t);
    pm_lines_restore(&b);
    pm_temps_done();
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error_tr_msg(msg, err);
}

static long long pm_eval_int(const char *expr)
{
    long long v = 0;
    pm_eval(expr, 'i', &v, NULL, NULL, 0);
    return v;
}

static double pm_eval_float(const char *expr)
{
    double v = 0;
    pm_eval(expr, 'f', NULL, &v, NULL, 0);
    return v;
}

static char *pm_eval_str(const char *expr, char *buf, int size)
{
    pm_eval(expr, 's', NULL, NULL, buf, size);
    return buf;
}

/* ON ERROR SKIP 1 for fn(arg): an MMBasic error in any pm_ call it makes
   (or in pm_execute, pm_eval_*) returns here instead of ending the program.
   0, or the error's number with MM.ERRNO and MM.ERRMSG$ set as SKIP sets
   them (pm_errno, pm_errmsg); like ON ERROR SKIP it clears them first.
   What fn was doing is abandoned at the error, as a skipped statement is.
   A SETTICK function run meanwhile is outside it, as a BASIC interrupt is
   outside the ON ERROR of the code it interrupts.  CTRL-C and exit() still
   end the program. */
static int pm_try(void (*fn)(void *), void *arg)
{
    pm_tryframe f;
    pm_trap t;
    char msg[MAXERRMSG];
    volatile int err = 0;
    if (fn == NULL)
        return 0;
    pm_poll();
    MMerrno = 0, MMErrMsg[0] = 0; /* (ON ERROR SKIP) */
    f.prev = pm_try_top;
    if (PM_TRY(t))
    {
        if (setjmp(f.jmp) == 0)
        {
            pm_try_top = &f;
            fn(arg);
        }
        else /* an error in a pm_ call (pm_try_catch) */
            err = f.err, memcpy(msg, f.msg, MAXERRMSG);
    }
    else /* an MMBasic error nothing else trapped */
        err = MMerrno ? MMerrno : 16, memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_try_top = f.prev;
    pm_trap_restore(&t);
    if (err)
    {
        pm_temps_done();
        MMerrno = err;
        memcpy(MMErrMsg, msg, MAXERRMSG);
        MMErrMsg[MAXERRMSG - 1] = 0;
    }
    return err;
}

static int pm_errno(void) { return MMerrno; }              /* MM.ERRNO */
static const char *pm_errmsg(void) { return MMErrMsg; }    /* MM.ERRMSG$ */
static void pm_error_clear(void) { MMerrno = 0, MMErrMsg[0] = 0; } /* ON ERROR CLEAR */

/* The try forms of pm_execute and pm_eval_*: pm_try of the call */
typedef struct
{
    const char *s;
    int type, size;
    long long *ip;
    double *fp;
    char *sp;
} pm_try_args;

static void pm_execute_fn(void *p) { pm_execute(((pm_try_args *)p)->s); }
static void pm_eval_fn(void *p)
{
    pm_try_args *a = (pm_try_args *)p;
    pm_eval(a->s, a->type, a->ip, a->fp, a->sp, a->size);
}

static int pm_execute_try(const char *cmd)
{
    pm_try_args a = {.s = cmd};
    return pm_try(pm_execute_fn, &a);
}

static int pm_eval_int_try(const char *expr, long long *v)
{
    pm_try_args a = {.s = expr, .type = 'i', .ip = v};
    *v = 0;
    return pm_try(pm_eval_fn, &a);
}

static int pm_eval_float_try(const char *expr, double *v)
{
    pm_try_args a = {.s = expr, .type = 'f', .fp = v};
    *v = 0;
    return pm_try(pm_eval_fn, &a);
}

static int pm_eval_str_try(const char *expr, char *buf, int size)
{
    pm_try_args a = {.s = expr, .type = 's', .sp = buf, .size = size};
    if (size > 0)
        buf[0] = 0;
    return pm_try(pm_eval_fn, &a);
}

/* SETTICK period, fn [, nbr]: fn is called every period ms, between the
   program's own calls of the firmware (as a BASIC tick SUB runs between
   statements) - never from an interrupt.  period 0 stops it. */
static void pm_settick(int period, void (*fn)(void), int nbr)
{
    int k;
    pm_trap t;
    if (pm == NULL)
        return;
    if (PM_TRY(t))
    {
        pm_range(nbr, 1, TCC_PM_TICKS);
        pm_range(period, 0, INT_MAX);
        if (period && fn == NULL)
            error("Argument count");
        pm_trap_restore(&t);
    }
    else
        pm_program_error_tr(&t);
    pm->ticks[nbr - 1].fn = period ? fn : NULL;
    pm->ticks[nbr - 1].period_us = (uint64_t)period * 1000;
    pm->ticks[nbr - 1].next_us = time_us_64() + (uint64_t)period * 1000;
    pm_tick_any = false;
    for (k = 0; k < TCC_PM_TICKS; k++)
        if (pm->ticks[k].fn)
            pm_tick_any = true;
}

static void pm_tick_service(void)
{
    uint64_t now;
    int k;
    if (pm == NULL || !tcc_pm_running || pm->in_tick)
        return;
    now = time_us_64();
    for (k = 0; k < TCC_PM_TICKS; k++)
    {
        pm_tick *tk = &pm->ticks[k];
        if (tk->fn && now >= tk->next_us)
        {
            tk->next_us += tk->period_us;
            if (tk->next_us <= now) /* (fell behind: no burst of calls) */
                tk->next_us = now + tk->period_us;
            pm_tryframe *outer = pm_try_top; /* (a tick is outside its pm_try) */
            pm_try_top = NULL;
            pm->in_tick = true;
            tk->fn();
            pm->in_tick = false;
            pm_try_top = outer;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* <picomite/sprite.h>, <picomite/audio.h>, the rest of graphics and input   */
/* ------------------------------------------------------------------------ */

/* A command whose arguments the firmware only parses from BASIC text (PLAY,
   LOAD IMAGE, SPRITE LOAD): its typed C arguments are written out as the
   statement and run by pm_execute, so they go through the very same
   parsing and checks. */
static void pm_cmdf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void pm_cmdf(const char *fmt, ...)
{
    char b[STRINGSIZE + 8];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= STRINGSIZE)
        PM_CALL(error("String too long"));
    pm_execute(b);
}

/* a file name or option letters for pm_cmdf: no quote, nothing to split it */
static const char *pm_word(const char *s, bool letters)
{
    const char *p;
    if (s == NULL)
        PM_CALL(error("Argument count"));
    for (p = s; *p; p++)
        if (*p == '"' || (letters && !isalnum((unsigned char)*p)))
            PM_CALL(error("Invalid argument"));
    return s;
}

/* ",n" for an argument, "," for one left out (PM_DEFAULT_ARG) */
static char *pm_arg(char *b, int v)
{
    if (v == PM_DEFAULT_ARG)
        strcpy(b, ",");
    else
        sprintf(b, ",%d", v);
    return b;
}

/* drop the trailing commas of arguments left out at the end */
static void pm_cmd_trim(char *b)
{
    size_t n = strlen(b);
    while (n && b[n - 1] == ',')
        b[--n] = 0;
}

/* SPRITE LOAD file$ [, start] [, mode] */
static void pm_sprite_load(const char *file, int start, int mode)
{
    char a1[16], a2[16], b[STRINGSIZE];
    snprintf(b, sizeof(b), "\"%s\"%s%s", pm_word(file, false), pm_arg(a1, start), pm_arg(a2, mode));
    pm_cmd_trim(b);
    pm_cmdf("SPRITE LOAD %s", b);
}

/* SPRITE SHOW n, x, y, layer [, rotation]; HIDE n; WRITE n, x, y [, orientation]; CLOSE ALL */
static void pm_sprite_show(int n, int x, int y, int layer, int rotation)
{
    PM_CALL(SpriteShowC(n, x, y, layer, rotation == PM_DEFAULT_ARG ? -1 : rotation));
}

static void pm_sprite_hide(int n)
{
    PM_CALL(SpriteHideC(n));
}

static void pm_sprite_write(int n, int x, int y, int orientation)
{
    PM_CALL(SpriteWriteC(n, x, y, orientation == PM_DEFAULT_ARG ? -1 : orientation));
}

static void pm_sprite_close_all(void)
{
    PM_CALL(SpriteCloseAllC());
}

/* PLAY MODFILE file$; MODSAMPLE sample, channel [, volume]; SOUND ...; STOP */
static void pm_play_modfile(const char *file)
{
    pm_cmdf("PLAY MODFILE \"%s\"", pm_word(file, false));
}

static void pm_play_modsample(int sample, int channel, int volume)
{
    char a1[16], b[64];
    snprintf(b, sizeof(b), "PLAY MODSAMPLE %d,%d%s", sample, channel, pm_arg(a1, volume));
    pm_cmd_trim(b);
    pm_execute(b);
}

static void pm_play_sound(int channel, const char *position, const char *type, double frequency, int volume)
{
    char a1[16], b[96];
    snprintf(b, sizeof(b), "PLAY SOUND %d,%s,%s,%.10g%s", channel, pm_word(position, true), pm_word(type, true),
             frequency, pm_arg(a1, volume));
    pm_cmd_trim(b);
    pm_execute(b);
}

static void pm_play_stop(void)
{
    pm_poll();
    if (CurrentlyPlaying != P_NOTHING) /* (cmd_play's PLAY STOP) */
        CloseAudio(1);
}

/* PLAY TONE left, right [, duration] (Hz, ms) */
static void pm_play_tone(double left, double right, int duration)
{
    char a1[16], b[96];
    snprintf(b, sizeof(b), "PLAY TONE %.10g,%.10g%s", left, right, pm_arg(a1, duration));
    pm_cmd_trim(b);
    pm_execute(b);
}

/* PLAY WAV / FLAC / MP3 / MIDIFILE file$ (a file, or a directory to play
   all of; the interrupt SUB of BASIC has no C form) */
static void pm_play_file(const char *what, const char *file)
{
    pm_cmdf("PLAY %s \"%s\"", what, pm_word(file, false));
}
static void pm_play_wav(const char *file) { pm_play_file("WAV", file); }
static void pm_play_flac(const char *file) { pm_play_file("FLAC", file); }
static void pm_play_mp3(const char *file) { pm_play_file("MP3", file); }
static void pm_play_midifile(const char *file) { pm_play_file("MIDIFILE", file); }

static void pm_play_pause(void) { pm_execute("PLAY PAUSE"); }
static void pm_play_resume(void) { pm_execute("PLAY RESUME"); }
static void pm_play_next(void) { pm_execute("PLAY NEXT"); }
static void pm_play_previous(void) { pm_execute("PLAY PREVIOUS"); }
static void pm_play_close(void) { pm_execute("PLAY CLOSE"); }

/* PLAY VOLUME left [, right] (0..100; PM_DEFAULT leaves one as it is) */
static void pm_play_volume(int left, int right)
{
    char a1[16], a2[16], b[48];
    snprintf(b, sizeof(b), "PLAY VOLUME %s%s", pm_arg(a1, left) + 1, pm_arg(a2, right));
    pm_cmd_trim(b);
    pm_execute(b);
}

/* PLAY BBC SOUND channel, amplitude, pitch, duration: the BBC Micro's SOUND
   (it waits, as the BBC did, while the channel's queue is full) */
static void pm_play_bbc_sound(int channel, int amplitude, int pitch, int duration)
{
    pm_cmdf("PLAY BBC SOUND %d,%d,%d,%d", channel, amplitude, pitch, duration);
}

/* PLAY BBC ENVELOPE n, T, PI1, PI2, PI3, PN1, PN2, PN3, AA, AD, AS, AR,
   ALA, ALD: the 14 values in that order */
static void pm_play_bbc_envelope(const int e[14])
{
    char b[192]; /* the command plus fourteen signed 32-bit integers */
    size_t n;
    int k;
    if (e == NULL)
        PM_CALL(error("Argument count"));
    n = (size_t)snprintf(b, sizeof(b), "PLAY BBC ENVELOPE");
    for (k = 0; k < 14; k++)
    {
        int written;
        if (n >= sizeof(b))
            PM_CALL(error("String too long"));
        written = snprintf(b + n, sizeof(b) - n, "%s%d", k ? "," : " ", e[k]);
        if (written < 0 || (size_t)written >= sizeof(b) - n)
            PM_CALL(error("String too long"));
        n += (size_t)written;
    }
    pm_execute(b);
}

/* MM.INFO(SOUND): the player already exposes the state as a C string. */
static char *pm_sound(char *buf, int size)
{
    pm_poll();
    if (size > 0)
        snprintf(buf, (size_t)size, "%s", PlayingStr[CurrentlyPlaying]);
    return buf;
}

/* LOAD IMAGE file$ [, x] [, y] [, mode];  LOAD JPG file$ [, x] [, y] */
static void pm_load_image(const char *file, int x, int y, int mode)
{
    char a1[16], a2[16], a3[16], b[STRINGSIZE];
    snprintf(b, sizeof(b), "\"%s\"%s%s%s", pm_word(file, false), pm_arg(a1, x), pm_arg(a2, y), pm_arg(a3, mode));
    pm_cmd_trim(b);
    pm_cmdf("LOAD IMAGE %s", b);
}

static void pm_load_jpg(const char *file, int x, int y)
{
    char a1[16], a2[16], b[STRINGSIZE];
    snprintf(b, sizeof(b), "\"%s\"%s%s", pm_word(file, false), pm_arg(a1, x), pm_arg(a2, y));
    pm_cmd_trim(b);
    pm_cmdf("LOAD JPG %s", b);
}

/* COLOUR fc [, bc] (cmd_colour), with OPTION LEGACY's colour numbers (getColour) */
extern const int CMM1map[16];
static int pm_colour_value(int c)
{
    if (CMM1)
    {
        pm_range(c, 0, 15);
        return CMM1map[c];
    }
    pm_range(c, 0, 0xFFFFFFF);
    return c;
}

static void pm_colour_do(int fc, int bc)
{
    gui_fcolour = pm_colour_value(fc);
    if (bc != PM_DEFAULT_ARG)
        gui_bcolour = pm_colour_value(bc);
    last_fcolour = gui_fcolour;
    last_bcolour = gui_bcolour;
    if (!CurrentLinePtr)
    {
        PromptFC = gui_fcolour;
        PromptBC = gui_bcolour;
    }
}

static void pm_colour(int fc, int bc)
{
    PM_CALL(pm_colour_do(fc, bc));
}

/* KEYDOWN(n) */
static int pm_keydown(int n)
{
    volatile int v = 0;
    pm_trap t;
    pm_poll();
    if (PM_TRY(t))
    {
        pm_range(n, 0, 8);
        v = KeyDownValue(n);
        pm_trap_restore(&t);
    }
    else
        pm_program_error_tr(&t);
    return v;
}

/* ------------------------------------------------------------------------ */
/* <picomite/files.h>: MMBasic's file commands and functions; and the C      */
/* library's remove, rename, mkdir, stat, opendir... built on them           */
/* ------------------------------------------------------------------------ */

/* KILL, RENAME, COPY, MKDIR, RMDIR, CHDIR, DRIVE: the statement, run by
   pm_execute (their arguments are parsed from text only) */
static void pm_kill(const char *file)
{
    pm_cmdf("KILL \"%s\"", pm_word(file, false));
}

static void pm_rename(const char *from, const char *to)
{
    pm_cmdf("RENAME \"%s\" AS \"%s\"", pm_word(from, false), pm_word(to, false));
}

static void pm_copy(const char *from, const char *to)
{
    pm_cmdf("COPY \"%s\" TO \"%s\"", pm_word(from, false), pm_word(to, false));
}

static void pm_mkdir(const char *dir)
{
    pm_cmdf("MKDIR \"%s\"", pm_word(dir, false));
}

static void pm_rmdir(const char *dir)
{
    pm_cmdf("RMDIR \"%s\"", pm_word(dir, false));
}

static void pm_chdir(const char *dir)
{
    pm_cmdf("CHDIR \"%s\"", pm_word(dir, false));
}

static void pm_drive(const char *drive)
{
    pm_cmdf("DRIVE \"%s\"", pm_word(drive, false));
}

/* An MMBasic function of a file name: "name(\"file\"" + tail, by pm_eval */
static void pm_evalf(int type, long long *ip, char *sp, int size, const char *fmt, const char *a, const char *b)
{
    char e[STRINGSIZE + 48];
    int n = snprintf(e, sizeof(e), fmt, a ? pm_word(a, false) : "", b ? b : "");
    if (n < 0 || n >= STRINGSIZE)
        PM_CALL(error("String too long"));
    pm_eval(e, type, ip, NULL, sp, size);
}

static char *pm_cwd(char *buf, int size) /* CWD$ */
{
    pm_eval("CWD$", 's', NULL, NULL, buf, size);
    return buf;
}

/* DIR$(pattern, FILE | DIR | ALL), and DIR$() for the next name (pattern
   NULL): "" when there are no more */
#define PM_DIR_ALL_ARG 2 /* (PM_DIR_ALL of include/picomite/files.h) */
static char *pm_dir(const char *pattern, int what, char *buf, int size)
{
    static const char *const kind[] = {"FILE", "DIR", "ALL"};
    if (pattern == NULL)
        pm_eval("DIR$()", 's', NULL, NULL, buf, size);
    else
    {
        if (what == PM_DEFAULT_ARG)
            what = 0;
        if (what < 0 || what > 2)
            PM_CALL(error("Invalid argument"));
        pm_evalf('s', NULL, buf, size, "DIR$(\"%s\",%s)", pattern, kind[what]);
    }
    return buf;
}

/* Keep MM.INFO(EXISTS FILE)'s -1 result for a directory.  The
   filesystem helpers return -1 for an unavailable drive as well. */
static int pm_exists_file(const char *file)
{
    volatile int v = 0;
    PM_CALL(
        v = ExistsFile((char *)file);
        if (v == 0)
        {
            char path[FF_MAX_LFN] = {0};
            int fs;
            if (ExistsDir((char *)file, path, &fs) != 0)
                v = -1;
        }
    );
    return v;
}

static int pm_exists_dir(const char *dir)
{
    volatile int v = 0;
    PM_CALL(
        char path[FF_MAX_LFN] = {0};
        int fs;
        v = ExistsDir((char *)dir, path, &fs);
    );
    return v;
}

static long long pm_filesize(const char *file)
{
    volatile long long v = 0;
    PM_CALL(v = FileSize((char *)file));
    return v;
}

static char *pm_modified(const char *file, char *buf, int size)
{
    PM_CALL(MMInfoModified(file, buf, size));
    return buf;
}

static long long pm_free_space(void)
{
    volatile long long v = 0;
    PM_CALL(v = MMInfoDiskSpace(1));
    return v;
}

static long long pm_disk_size_info(void)
{
    volatile long long v = 0;
    PM_CALL(v = MMInfoDiskSpace(0));
    return v;
}

static char *pm_drive_name(char *buf, int size)
{
    pm_poll();
    if (size > 0)
        snprintf(buf, (size_t)size, "%s", MMInfoDriveName());
    return buf;
}

/* The C library's functions: C's meaning, -1 (or NULL) for a failure and
   never an MMBasic error (pm_try): pm_errmsg() then says why. */
typedef struct
{
    const char *a, *b;
    char *buf;
    int size;
    long long v;
} pm_fs_args;

static void pm_kill_fn(void *p) { pm_kill(((pm_fs_args *)p)->a); }
static void pm_rename_fn(void *p) { pm_rename(((pm_fs_args *)p)->a, ((pm_fs_args *)p)->b); }
static void pm_mkdir_fn(void *p) { pm_mkdir(((pm_fs_args *)p)->a); }
static void pm_rmdir_fn(void *p) { pm_rmdir(((pm_fs_args *)p)->a); }
static void pm_chdir_fn(void *p) { pm_chdir(((pm_fs_args *)p)->a); }
static void pm_cwd_fn(void *p) { pm_cwd(((pm_fs_args *)p)->buf, ((pm_fs_args *)p)->size); }
static void pm_exists_file_fn(void *p) { ((pm_fs_args *)p)->v = pm_exists_file(((pm_fs_args *)p)->a); }
static void pm_exists_dir_fn(void *p) { ((pm_fs_args *)p)->v = pm_exists_dir(((pm_fs_args *)p)->a); }
static void pm_filesize_fn(void *p) { ((pm_fs_args *)p)->v = pm_filesize(((pm_fs_args *)p)->a); }
static void pm_modified_fn(void *p) { pm_modified(((pm_fs_args *)p)->a, ((pm_fs_args *)p)->buf, ((pm_fs_args *)p)->size); }

static int pm_fs_try(void (*fn)(void *), const char *a, const char *b)
{
    pm_fs_args x = {.a = a, .b = b};
    return pm_try(fn, &x) ? -1 : 0;
}

static int pm_c_remove(const char *path) { return pm_fs_try(pm_kill_fn, path, NULL); }
static int pm_c_rename(const char *from, const char *to) { return pm_fs_try(pm_rename_fn, from, to); }
static int pm_c_mkdir(const char *path, unsigned mode) { (void)mode; return pm_fs_try(pm_mkdir_fn, path, NULL); }
static int pm_c_rmdir(const char *path) { return pm_fs_try(pm_rmdir_fn, path, NULL); }
static int pm_c_chdir(const char *path) { return pm_fs_try(pm_chdir_fn, path, NULL); }

static char *pm_c_getcwd(char *buf, size_t size)
{
    pm_fs_args x = {.buf = buf, .size = (int)size};
    if (buf == NULL || size == 0 || pm_try(pm_cwd_fn, &x))
        return NULL;
    return buf;
}

/* <sys/stat.h> (include/sys/stat.h has the same layout) */
struct pm_c_stat
{
    unsigned mode;   /* st_mode */
    long size;       /* st_size */
    long long mtime; /* st_mtime (newlib makes st_mtime a macro) */
};
#define PM_S_IFREG 0100000
#define PM_S_IFDIR 0040000

/* "YYYY-MM-DD HH:MM:SS" (MM.INFO(MODIFIED)) as seconds from 1970, the
   clock's own time as time() gives it */
static long long pm_mtime_of(const char *s)
{
    int y, mo, d, h, mi, se;
    long long days;
    if (strlen(s) < 19)
        return 0;
    y = atoi(s), mo = atoi(s + 5), d = atoi(s + 8), h = atoi(s + 11), mi = atoi(s + 14), se = atoi(s + 17);
    if (mo < 1 || mo > 12 || d < 1)
        return 0;
    y -= mo <= 2; /* (days from civil, H. Hinnant) */
    {
        int era = y / 400, yoe = y - era * 400;
        int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        days = (long long)era * 146097 + doe - 719468;
    }
    return days * 86400 + h * 3600 + mi * 60 + se;
}

static int pm_c_stat(const char *path, struct pm_c_stat *st)
{
    pm_fs_args x = {.a = path};
    char when[32];
    if (st == NULL || pm_try(pm_exists_file_fn, &x))
        return -1;
    memset(st, 0, sizeof(*st));
    if (x.v == 1)
    {
        if (pm_try(pm_filesize_fn, &x))
            return -1;
        st->mode = PM_S_IFREG;
        st->size = (long)x.v;
    }
    else
    {
        if (pm_try(pm_exists_dir_fn, &x) || x.v != 1)
            return -1;
        st->mode = PM_S_IFDIR;
    }
    x.buf = when, x.size = sizeof(when);
    if (pm_try(pm_modified_fn, &x) == 0)
        st->mtime = pm_mtime_of(when);
    return 0;
}

/* <dirent.h>: opendir reads the whole listing at once (DIR$, then DIR$()
   to its end), so each DIR is independent of the others and of DIR$ /
   pm_dir afterwards.  (include/dirent.h has the same layout.) */
#define PM_DT_REG 8
#define PM_DT_DIR 4
struct pm_c_dirent
{
    unsigned char d_type;
    char d_name[256];
};
_Static_assert(FF_MAX_LFN < 256, "struct dirent d_name");
typedef struct
{
    struct pm_c_dirent ent;
    char *list; /* the entries: a type byte, the name, a 0 - one after another */
    int len, pos;
} pm_c_dir;

typedef struct
{
    const char *pattern; /* DIR$(pattern, ALL), or NULL for DIR$() */
    const char *path;
    pm_c_dir *d;
} pm_dir_args;

static void pm_dir_read_fn(void *p)
{
    pm_dir_args *a = (pm_dir_args *)p;
    char name[FF_MAX_LFN + 1], full[FF_MAX_LFN * 2 + 2];
    size_t n = strlen(a->path);
    int cap = 0;
    pm_dir(a->pattern, PM_DIR_ALL_ARG, name, sizeof(name));
    while (name[0])
    {
        int k = (int)strlen(name) + 2;
        if (a->d->len + k > cap)
        {
            char *l = tcc_pm_realloc(a->d->list, (size_t)(cap = cap * 2 + k + 256));
            if (l == NULL)
                error("Not enough memory");
            a->d->list = l;
        }
        snprintf(full, sizeof(full), "%s%s%s", a->path,
                 n == 0 || a->path[n - 1] == '/' || a->path[n - 1] == ':' ? "" : "/", name);
        a->d->list[a->d->len] = pm_exists_dir(full) == 1 ? PM_DT_DIR : PM_DT_REG;
        strcpy(a->d->list + a->d->len + 1, name);
        a->d->len += k;
        pm_dir(NULL, PM_DIR_ALL_ARG, name, sizeof(name)); /* DIR$() */
    }
}

static void *pm_c_opendir(const char *path)
{
    pm_c_dir *d;
    pm_dir_args a;
    char pat[FF_MAX_LFN + 4];
    size_t n;
    if (path == NULL || (n = strlen(path)) + 3 > FF_MAX_LFN)
        return NULL;
    if ((d = pm_calloc(1, sizeof(pm_c_dir))) == NULL)
        return NULL;
    if (n == 0 || strcmp(path, ".") == 0)
        strcpy(pat, "*"), path = "";
    else
        snprintf(pat, sizeof(pat), "%s%s*", path, path[n - 1] == '/' || path[n - 1] == ':' ? "" : "/");
    a.pattern = pat, a.path = path, a.d = d;
    if (pm_try(pm_dir_read_fn, &a))
    {
        tcc_pm_free(d->list);
        tcc_pm_free(d);
        return NULL;
    }
    return d;
}

static struct pm_c_dirent *pm_c_readdir(void *dir)
{
    pm_c_dir *d = (pm_c_dir *)dir;
    if (d == NULL || d->pos >= d->len)
        return NULL;
    d->ent.d_type = (unsigned char)d->list[d->pos];
    strcpy(d->ent.d_name, d->list + d->pos + 1);
    d->pos += (int)strlen(d->list + d->pos + 1) + 2;
    return &d->ent;
}

static int pm_c_closedir(void *dir)
{
    pm_c_dir *d = (pm_c_dir *)dir;
    if (d == NULL)
        return -1;
    tcc_pm_free(d->list);
    tcc_pm_free(d);
    return 0;
}

/* ------------------------------------------------------------------------ */
/* <picomite/framebuffer.h>: FRAMEBUFFER and BLIT                           */
/* ------------------------------------------------------------------------ */

/* a buffer letter of FRAMEBUFFER / BLIT FRAMEBUFFER: N F L T 2 */
static char pm_fb_letter(int b)
{
    b = toupper(b);
    if (b != 'N' && b != 'F' && b != 'L' && b != 'T' && b != '2')
        PM_CALL(error("Invalid argument"));
    return (char)b;
}

/* FRAMEBUFFER CREATE (buffer 'F') / CREATE 2 ('2') */
static void pm_framebuffer_create(int buffer)
{
    char b = pm_fb_letter(buffer);
    if (b != 'F' && b != '2')
        PM_CALL(error("Invalid argument"));
    pm_execute(b == '2' ? "FRAMEBUFFER CREATE 2" : "FRAMEBUFFER CREATE");
}

/* FRAMEBUFFER LAYER [transparent] / LAYER TOP [transparent] */
static void pm_framebuffer_layer(int transparent)
{
    char a1[16];
    pm_cmdf("FRAMEBUFFER LAYER %s", pm_arg(a1, transparent) + 1);
}

static void pm_framebuffer_layer_top(int transparent)
{
    char a1[16];
    pm_cmdf("FRAMEBUFFER LAYER TOP %s", pm_arg(a1, transparent) + 1);
}

static void pm_framebuffer_write(int buffer) /* FRAMEBUFFER WRITE N | F | L | T | 2 */
{
    pm_cmdf("FRAMEBUFFER WRITE %c", pm_fb_letter(buffer));
}

/* FRAMEBUFFER COPY from, to [, B]: wait for blanking on VGA/HDMI,
   background screen copy on supported LCD builds. */
static void pm_framebuffer_copy(int from, int to, int at_blank)
{
    char src = pm_fb_letter(from), dst = pm_fb_letter(to);
    PM_CALL(FrameBufferCopyC(src, dst, at_blank != 0));
}

/* FRAMEBUFFER MERGE [colour] [, mode [, rate]]: mode 'B', 'R', 'A' or
   PM_DEFAULT; rate PM_DEFAULT if left out (the LCD builds take it) */
static void pm_framebuffer_merge(int colour, int mode, int rate)
{
    char a1[16], a2[16], b[64];
    if (mode != PM_DEFAULT_ARG)
    {
        mode = toupper(mode);
        if (mode != 'B' && mode != 'R' && mode != 'A')
            PM_CALL(error("Invalid argument"));
        snprintf(a2, sizeof(a2), ",%c", mode);
    }
    else
        strcpy(a2, ",");
    snprintf(b, sizeof(b), "FRAMEBUFFER MERGE %s%s", pm_arg(a1, colour) + 1, a2);
    if (rate != PM_DEFAULT_ARG)
        snprintf(b + strlen(b), sizeof(b) - strlen(b), ",%d", rate);
    pm_cmd_trim(b);
    pm_execute(b);
}

static void pm_framebuffer_sync(void) { pm_execute("FRAMEBUFFER SYNC"); }
static void pm_framebuffer_wait(void) { pm_execute("FRAMEBUFFER WAIT"); }

/* FRAMEBUFFER CLOSE [F | L | T | 2]: PM_DEFAULT closes them all */
static void pm_framebuffer_close(int buffer)
{
    if (buffer == PM_DEFAULT_ARG)
        pm_execute("FRAMEBUFFER CLOSE");
    else
        pm_cmdf("FRAMEBUFFER CLOSE %c", pm_fb_letter(buffer));
}

/* BLIT FRAMEBUFFER from, to, x1, y1, x2, y2, w, h [, colour]: straight to
   the firmware (graphics/Blit.c), as it is often used for each frame */
static void pm_blit_framebuffer(int from, int to, int x1, int y1, int x2, int y2, int w, int h, int colour)
{
    char f = pm_fb_letter(from), t = pm_fb_letter(to);
    PM_CALL(BlitFramebufferC(f, t, x1, y1, x2, y2, w, h, colour == PM_DEFAULT_ARG ? -1 : colour));
}

/* BLIT x1, y1, x2, y2, w, h; BLIT READ / WRITE / LOAD / CLOSE of a BLIT
   buffer (1..), run by pm_execute */
static void pm_blit(int x1, int y1, int x2, int y2, int w, int h)
{
    pm_cmdf("BLIT %d,%d,%d,%d,%d,%d", x1, y1, x2, y2, w, h);
}

static void pm_blit_read(int n, int x, int y, int w, int h)
{
    pm_cmdf("BLIT READ %d,%d,%d,%d,%d", n, x, y, w, h);
}

static void pm_blit_write(int n, int x, int y, int mode)
{
    char a1[16], b[64];
    snprintf(b, sizeof(b), "BLIT WRITE %d,%d,%d%s", n, x, y, pm_arg(a1, mode));
    pm_cmd_trim(b);
    pm_execute(b);
}

static void pm_blit_load(int n, const char *file, int x, int y, int w, int h)
{
    char a1[16], a2[16], a3[16], a4[16], b[STRINGSIZE];
    snprintf(b, sizeof(b), "%d,\"%s\"%s%s%s%s", n, pm_word(file, false), pm_arg(a1, x), pm_arg(a2, y), pm_arg(a3, w),
             pm_arg(a4, h));
    pm_cmd_trim(b);
    pm_cmdf("BLIT LOAD %s", b);
}

static void pm_blit_close(int n)
{
    pm_cmdf("BLIT CLOSE %d", n);
}

/* ------------------------------------------------------------------------ */
/* <picomite/input.h>: the mouse and the GUI cursor                         */
/* ------------------------------------------------------------------------ */

/* DEVICE(MOUSE channel, what): what is the letter X Y L R M W B D T, as in
   BASIC (fun_dev in misc/External.c) */
static int pm_mouse_do(int channel, int what)
{
    volatile struct s_nunstruct *m;
    int v;
    pm_range(channel, 1, 4);
    m = &nunstruct[channel];
    switch (toupper(what))
    {
    case 'X': return m->ax;
    case 'Y': return m->ay;
    case 'L': return m->L;
    case 'R': return m->R;
    case 'M': return m->C;
    case 'W': return m->az;
    case 'B': return m->x0;
    case 'D': /* a double click, reported once */
        v = m->Z;
        m->Z = 0;
        return v;
    case 'T': return m->classic[0];
    default:
        SyntaxError();
    }
    return 0;
}

static int pm_mouse(int channel, int what)
{
    volatile int v = 0;
    PM_CALL(v = pm_mouse_do(channel, what));
    return v;
}

static int pm_usb_device(int port) /* MM.INFO(USB port) */
{
#ifdef USBKEYBOARD
    volatile int v = 0;
    PM_CALL(
        pm_range(port, 0, 4);
        v = port == 0 ? Current_USB_devices : HID[port - 1].Device_type;
    );
    return v;
#else
    (void)port;
    PM_CALL(SyntaxError()); /* same availability as MM.INFO(USB) */
    return 0;
#endif
}

/* GUI CURSOR ON [no [, x, y [, colour]]], OFF, HIDE, SHOW, COLOUR c,
   LINK MOUSE / UNLINK MOUSE, LOAD file$: the statement, run by pm_execute */
static void pm_cursor_on(int no, int x, int y, int colour)
{
    char a1[16], a2[16], a3[16], b[96];
    if (no == PM_DEFAULT_ARG)
        strcpy(b, "GUI CURSOR ON");
    else
    {
        snprintf(b, sizeof(b), "GUI CURSOR ON %d%s%s%s", no, pm_arg(a1, x), pm_arg(a2, y), pm_arg(a3, colour));
        pm_cmd_trim(b);
    }
    pm_execute(b);
}

static void pm_cursor_off(void) { pm_execute("GUI CURSOR OFF"); }
static void pm_cursor_hide(void) { pm_execute("GUI CURSOR HIDE"); }
static void pm_cursor_show(void) { pm_execute("GUI CURSOR SHOW"); }

static void pm_cursor_colour(int colour)
{
    pm_cmdf("GUI CURSOR COLOUR %d", colour);
}

static void pm_cursor_link(int link)
{
    pm_execute(link ? "GUI CURSOR LINK MOUSE" : "GUI CURSOR UNLINK MOUSE");
}

static void pm_cursor_load(const char *file)
{
    pm_cmdf("GUI CURSOR LOAD \"%s\"", pm_word(file, false));
}

/* GUI CURSOR x, y: often, so straight to the firmware where it has it */
static void pm_cursor_move(int x, int y)
{
#if defined(PICOMITEVGA) || defined(GUICONTROLS)
    PM_CALL(CursorMoveC(x, y));
#else
    pm_cmdf("GUI CURSOR %d,%d", x, y);
#endif
}

/* ------------------------------------------------------------------------ */
/* <picomite/vars.h>: BASIC variables and arrays                            */
/* ------------------------------------------------------------------------ */

/* (include/picomite/vars.h has the same layout) */
#define PM_MAXDIM 6
typedef struct
{
    int type;              /* PM_VAR_FLOAT 1, PM_VAR_STRING 2, PM_VAR_INT 4 (MMBasic's T_NBR, T_STR, T_INT) */
    int size;              /* a string's capacity, LENGTH (255 by default) */
    int constant;          /* CONST: not to be written */
    int ndims;             /* 0: not an array */
    int dims[PM_MAXDIM];   /* elements of each dimension (OPTION BASE taken into account) */
    int elements;          /* all of them (1 for a scalar) */
    int element_size;      /* bytes of one element: 8, or size + 1 for a string */
} pm_varinfo;
_Static_assert(MAXDIM <= PM_MAXDIM, "pm_varinfo.dims");

/* findvar() of the reference ref - "n%", "a$", "x!(3, i)", "buf%()" - as
   VARADDR finds it: its address and, if info, what it is */
static void *pm_var_do(const char *ref, pm_varinfo *info)
{
    void *addr;
    struct s_vartbl *v;
    int k;
    if (strlen(ref) + 2 >= STRINGSIZE)
        error("String too long");
    inpbuf[0] = 'r';
    inpbuf[1] = '='; /* (as pm_eval_do: a line the tokeniser takes) */
    strcpy((char *)inpbuf + 2, ref);
    multi = false;
    tokenise(true);
    addr = findvar(tknbuf + 2 + sizeof(CommandToken), V_FIND | V_EMPTY_OK | V_NOFIND_ERR);
    v = VREC(g_VarIndex);
    if (v->type & T_STRUCT)
        error("A structure is not supported here");
    if (info)
    {
        memset(info, 0, sizeof(*info));
        info->type = v->type & (T_NBR | T_STR | T_INT);
        info->size = (v->type & T_STR) ? v->size : 0;
        info->constant = (v->type & T_CONST) != 0;
        info->element_size = (v->type & T_STR) ? v->size + 1 : 8;
        info->elements = 1;
        if (DimIsRealArray(RAW_DIM(*v, 0)))
            for (k = 0; k < MAXDIM && DimIsRealArray(RAW_DIM(*v, k)); k++)
            {
                info->dims[k] = DimElements(RAW_DIM(*v, k));
                info->elements *= info->dims[k];
                info->ndims = k + 1;
            }
    }
    return addr;
}

static void *pm_var(const char *ref, pm_varinfo *info)
{
    pm_linebufs b;
    pm_trap t;
    void *volatile addr = NULL;
    char msg[MAXERRMSG];
    int err;
    if (ref == NULL)
        PM_CALL(error("Argument count"));
    pm_poll();
    pm_lines_save(&b);
    if (PM_TRY(t))
    {
        addr = pm_var_do(ref, info);
        pm_trap_restore(&t);
        pm_lines_restore(&b);
        pm_temps_done();
        return addr;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(&t);
    pm_lines_restore(&b);
    pm_temps_done();
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error_tr_msg(msg, err);
}

/* DIM decl: the variable it declares first, as pm_var finds it ("x%(9)":
   the array) */
static void *pm_dim(const char *decl, pm_varinfo *info)
{
    static const char *const types[] = {"INTEGER ", "FLOAT ", "STRING "};
    char ref[MAXVARLEN + 8];
    const char *p;
    unsigned i;
    int n = 0;
    if (decl == NULL)
        PM_CALL(error("Argument count"));
    pm_cmdf("DIM %s", decl);
    for (p = decl; *p == ' '; p++)
        ;
    for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if (strncasecmp(p, types[i], strlen(types[i])) == 0)
            for (p += strlen(types[i]); *p == ' '; p++)
                ;
    while ((isalnum((unsigned char)*p) || *p == '_' || *p == '.') && n < MAXVARLEN)
        ref[n++] = *p++;
    if (*p == '%' || *p == '!' || *p == '$')
        ref[n++] = *p++;
    while (*p == ' ')
        p++;
    if (*p == '(')
        ref[n++] = '(', ref[n++] = ')';
    ref[n] = 0;
    return pm_var(ref, info);
}

static void pm_erase(const char *names) /* ERASE name [, name ...] */
{
    pm_cmdf("ERASE %s", names ? names : "");
}

/* MMBasic strings: a length byte, then the characters (no terminating 0) */
static char *pm_str_get(const void *mmstr, char *buf, int size)
{
    const unsigned char *m = (const unsigned char *)mmstr;
    int n;
    if (buf == NULL || size <= 0)
        return buf;
    n = m ? m[0] : 0;
    if (n > size - 1)
        n = size - 1;
    if (n)
        memcpy(buf, m + 1, (size_t)n);
    buf[n] = 0;
    return buf;
}

static void pm_str_set(void *mmstr, int capacity, const char *s)
{
    unsigned char *m = (unsigned char *)mmstr;
    size_t n = s ? strlen(s) : 0;
    if (m == NULL || (int)n > capacity || n > MAXSTRLEN)
        PM_CALL(error("String too long"));
    m[0] = (unsigned char)n;
    if (n)
        memcpy(m + 1, s, n);
}

/* ------------------------------------------------------------------------ */
/* Run-time symbol table for -run                                            */
/* ------------------------------------------------------------------------ */

/* The ARM run-time ABI helpers TinyCC calls for what Thumb-2 cannot do
   inline (64-bit division and shifts, double arithmetic, block copies).
   Their prototypes do not matter: only the addresses are taken.  The pico
   double/float libraries wrap several of them (--wrap), so the fast ones are
   what TinyCC's code calls, as the firmware's does. */
#define PM_AEABI(X)                                                               \
    X(__aeabi_memcpy) X(__aeabi_memcpy4) X(__aeabi_memcpy8) X(__aeabi_memmove)   \
    X(__aeabi_memset) X(__aeabi_idiv) X(__aeabi_uidiv) X(__aeabi_idivmod)        \
    X(__aeabi_uidivmod) X(__aeabi_ldivmod) X(__aeabi_uldivmod) X(__aeabi_lasr)   \
    X(__aeabi_llsr) X(__aeabi_llsl) X(__aeabi_dadd) X(__aeabi_dsub)              \
    X(__aeabi_dmul) X(__aeabi_ddiv) X(__aeabi_dcmpeq) X(__aeabi_dcmplt)          \
    X(__aeabi_dcmple) X(__aeabi_dcmpgt) X(__aeabi_dcmpge) X(__aeabi_i2d)         \
    X(__aeabi_ui2d) X(__aeabi_d2iz) X(__aeabi_d2uiz) X(__aeabi_f2d)              \
    X(__aeabi_d2f) X(__aeabi_l2f) X(__aeabi_l2d) X(__aeabi_f2lz) X(__aeabi_d2lz) \
    X(__aeabi_ul2f) X(__aeabi_ul2d) X(__aeabi_f2ulz) X(__aeabi_d2ulz)

#define PM_DECLARE(n) extern void n(void);
PM_AEABI(PM_DECLARE)
#undef PM_DECLARE

typedef struct
{
    const char *name;
    const void *addr;
} pm_sym;

#define PM_SYM(n) {#n, (const void *)&n},
#define PM_SYM_AS(n, f) {#n, (const void *)&f},

static const pm_sym pm_syms[] = {
    /* stdio */
    PM_SYM_AS(printf, tcc_pm_printf)
    PM_SYM_AS(fprintf, tcc_pm_fprintf)
    PM_SYM_AS(vprintf, pm_vprintf)
    PM_SYM_AS(vfprintf, tcc_pm_vfprintf)
    PM_SYM_AS(sprintf, pm_sprintf)
    PM_SYM(snprintf)
    PM_SYM_AS(vsprintf, pm_vsprintf)
    PM_SYM(vsnprintf)
    PM_SYM_AS(puts, tcc_pm_puts)
    PM_SYM_AS(putchar, tcc_pm_putchar)
    PM_SYM_AS(fputs, tcc_pm_fputs)
    PM_SYM_AS(fputc, tcc_pm_fputc)
    PM_SYM_AS(putc, tcc_pm_fputc)
    PM_SYM_AS(getchar, pm_getchar)
    PM_SYM_AS(fgetc, pm_fgetc)
    PM_SYM_AS(getc, pm_fgetc)
    PM_SYM_AS(fgets, pm_fgets)
    PM_SYM_AS(fopen, tcc_pm_fopen)
    PM_SYM_AS(fclose, tcc_pm_fclose)
    PM_SYM_AS(fread, tcc_pm_fread)
    PM_SYM_AS(fwrite, tcc_pm_fwrite)
    PM_SYM_AS(fseek, tcc_pm_fseek)
    PM_SYM_AS(ftell, tcc_pm_ftell)
    PM_SYM_AS(rewind, pm_rewind)
    PM_SYM_AS(feof, pm_feof)
    PM_SYM_AS(ferror, pm_ferror)
    PM_SYM_AS(fflush, tcc_pm_fflush)
    PM_SYM(__tcc_pm_stdin)
    PM_SYM(__tcc_pm_stdout)
    PM_SYM(__tcc_pm_stderr)
    /* stdlib */
    PM_SYM_AS(malloc, tcc_pm_malloc)
    PM_SYM_AS(calloc, pm_calloc)
    PM_SYM_AS(realloc, tcc_pm_realloc)
    PM_SYM_AS(free, tcc_pm_free)
    PM_SYM_AS(exit, tcc_pm_exit)
    PM_SYM_AS(abort, pm_abort)
    PM_SYM(atoi)
    PM_SYM(atol)
    PM_SYM(atof)
    PM_SYM(strtol)
    PM_SYM(strtoul)
    PM_SYM(strtoll)
    PM_SYM(strtoull)
    PM_SYM(strtod)
    PM_SYM(strtof)
    PM_SYM(abs)
    PM_SYM(labs)
    PM_SYM(llabs)
    PM_SYM(rand)
    PM_SYM(srand)
    PM_SYM(qsort)
    PM_SYM(bsearch)
    /* string */
    PM_SYM(memcpy)
    PM_SYM(memmove)
    PM_SYM(memset)
    PM_SYM(memcmp)
    PM_SYM(memchr)
    PM_SYM(strlen)
    PM_SYM(strcpy)
    PM_SYM(strncpy)
    PM_SYM(strcat)
    PM_SYM(strncat)
    PM_SYM(strcmp)
    PM_SYM(strncmp)
    PM_SYM(strchr)
    PM_SYM(strrchr)
    PM_SYM(strstr)
    PM_SYM(strspn)
    PM_SYM(strcspn)
    PM_SYM(strpbrk)
    PM_SYM(strtok)
    PM_SYM_AS(strdup, pm_strdup)
    /* ctype */
    PM_SYM(isalnum)
    PM_SYM(isalpha)
    PM_SYM(iscntrl)
    PM_SYM(isdigit)
    PM_SYM(isgraph)
    PM_SYM(islower)
    PM_SYM(isprint)
    PM_SYM(ispunct)
    PM_SYM(isspace)
    PM_SYM(isupper)
    PM_SYM(isxdigit)
    PM_SYM(tolower)
    PM_SYM(toupper)
    /* math */
    PM_SYM(sin)
    PM_SYM(cos)
    PM_SYM(tan)
    PM_SYM(asin)
    PM_SYM(acos)
    PM_SYM(atan)
    PM_SYM(atan2)
    PM_SYM(sinh)
    PM_SYM(cosh)
    PM_SYM(tanh)
    PM_SYM(exp)
    PM_SYM(log)
    PM_SYM(log10)
    PM_SYM(log2)
    PM_SYM(exp2)
    PM_SYM(pow)
    PM_SYM(sqrt)
    PM_SYM(cbrt)
    PM_SYM(hypot)
    PM_SYM(fabs)
    PM_SYM(floor)
    PM_SYM(ceil)
    PM_SYM(round)
    PM_SYM(trunc)
    PM_SYM(fmod)
    PM_SYM(ldexp)
    PM_SYM(frexp)
    PM_SYM(modf)
    PM_SYM(sinf)
    PM_SYM(cosf)
    PM_SYM(tanf)
    PM_SYM(atan2f)
    PM_SYM(sqrtf)
    PM_SYM(powf)
    PM_SYM(expf)
    PM_SYM(logf)
    PM_SYM(fabsf)
    PM_SYM(floorf)
    PM_SYM(ceilf)
    /* time */
    PM_SYM_AS(time, pm_c_time)
    PM_SYM_AS(clock, pm_clock)
    /* PicoMite */
    PM_SYM(pm_hres)
    PM_SYM(pm_vres)
    PM_SYM(pm_cls)
    PM_SYM(pm_pixel)
    PM_SYM_AS(pm_line, pm_line_draw)
    PM_SYM(pm_box)
    PM_SYM(pm_circle)
    PM_SYM(pm_triangle)
    PM_SYM(pm_refresh)
    PM_SYM(pm_print)
    PM_SYM(pm_timer_us)
    PM_SYM(pm_delay_us)
    PM_SYM(pm_pause)
    PM_SYM(pm_inkey)
    PM_SYM(pm_pin_mode)
    PM_SYM(pm_pin_write)
    PM_SYM(pm_pin_read)
    /* <picomite/adc.h>: existing PIN() ADC implementation */
    PM_SYM(pm_adc_read)
    PM_SYM(pm_adc_read_raw)
    /* <picomite/serial.h>: native UART COM1/COM2 */
    PM_SYM(pm_serial_open)
    PM_SYM(pm_serial_read)
    PM_SYM(pm_serial_write)
    PM_SYM(pm_serial_available)
    PM_SYM(pm_serial_flush)
    PM_SYM(pm_serial_close)
    /* <picomite/i2c.h> */
    PM_SYM(pm_i2c_open)
    PM_SYM(pm_i2c_close)
    PM_SYM(pm_i2c_transfer)
    PM_SYM(pm_i2c_write)
    PM_SYM(pm_i2c_read)
    PM_SYM(pm_i2c_probe)
    /* <picomite/spi.h> */
    PM_SYM(pm_spi_open)
    PM_SYM(pm_spi_close)
    PM_SYM(pm_spi_transfer)
    PM_SYM(pm_spi_write)
    PM_SYM(pm_spi_read)
    /* <picomite/pwm.h> */
    PM_SYM(pm_pwm_open)
    PM_SYM(pm_pwm_set_duty)
    PM_SYM(pm_pwm_set_frequency)
    PM_SYM(pm_pwm_close)
    /* <picomite/system.h>, <picomite/memory.h>, <picomite/graphics.h> */
    PM_SYM(pm_api_version)
    PM_SYM(pm_get_info)
    PM_SYM(pm_yield)
    PM_SYM(pm_abort_requested)
    PM_SYM(pm_alloc)
    PM_SYM(pm_free)
    PM_SYM(pm_mem_where)
    PM_SYM(pm_get_mem_info)
    PM_SYM(pm_text)
    PM_SYM(pm_draw_box)
    PM_SYM(pm_draw_rbox)
    PM_SYM(pm_execute)
    PM_SYM(pm_eval_int)
    PM_SYM(pm_eval_float)
    PM_SYM(pm_eval_str)
    PM_SYM(pm_try)
    PM_SYM(pm_execute_try)
    PM_SYM(pm_eval_int_try)
    PM_SYM(pm_eval_float_try)
    PM_SYM(pm_eval_str_try)
    PM_SYM(pm_errno)
    PM_SYM(pm_errmsg)
    PM_SYM(pm_error_clear)
    PM_SYM(pm_settick)
    PM_SYM(pm_sprite_load)
    PM_SYM(pm_sprite_show)
    PM_SYM(pm_sprite_hide)
    PM_SYM(pm_sprite_write)
    PM_SYM(pm_sprite_close_all)
    PM_SYM(pm_play_modfile)
    PM_SYM(pm_play_modsample)
    PM_SYM(pm_play_sound)
    PM_SYM(pm_play_stop)
    PM_SYM(pm_play_tone)
    PM_SYM(pm_play_wav)
    PM_SYM(pm_play_flac)
    PM_SYM(pm_play_mp3)
    PM_SYM(pm_play_midifile)
    PM_SYM(pm_play_pause)
    PM_SYM(pm_play_resume)
    PM_SYM(pm_play_next)
    PM_SYM(pm_play_previous)
    PM_SYM(pm_play_close)
    PM_SYM(pm_play_volume)
    PM_SYM(pm_play_bbc_sound)
    PM_SYM(pm_play_bbc_envelope)
    PM_SYM(pm_sound)
    PM_SYM(pm_load_image)
    PM_SYM(pm_load_jpg)
    PM_SYM(pm_colour)
    PM_SYM(pm_keydown)
    /* <picomite/files.h> */
    PM_SYM(pm_kill)
    PM_SYM(pm_rename)
    PM_SYM(pm_copy)
    PM_SYM(pm_mkdir)
    PM_SYM(pm_rmdir)
    PM_SYM(pm_chdir)
    PM_SYM(pm_drive)
    PM_SYM(pm_cwd)
    PM_SYM(pm_dir)
    PM_SYM(pm_exists_file)
    PM_SYM(pm_exists_dir)
    PM_SYM(pm_filesize)
    PM_SYM(pm_modified)
    PM_SYM(pm_free_space)
    PM_SYM_AS(pm_disk_size, pm_disk_size_info)
    PM_SYM(pm_drive_name)
    /* remove, rename (stdio.h), unistd.h, sys/stat.h, dirent.h */
    PM_SYM_AS(remove, pm_c_remove)
    PM_SYM_AS(unlink, pm_c_remove)
    PM_SYM_AS(rename, pm_c_rename)
    PM_SYM_AS(mkdir, pm_c_mkdir)
    PM_SYM_AS(rmdir, pm_c_rmdir)
    PM_SYM_AS(chdir, pm_c_chdir)
    PM_SYM_AS(getcwd, pm_c_getcwd)
    PM_SYM_AS(stat, pm_c_stat)
    PM_SYM_AS(opendir, pm_c_opendir)
    PM_SYM_AS(readdir, pm_c_readdir)
    PM_SYM_AS(closedir, pm_c_closedir)
    /* <picomite/framebuffer.h> */
    PM_SYM(pm_framebuffer_create)
    PM_SYM(pm_framebuffer_layer)
    PM_SYM(pm_framebuffer_layer_top)
    PM_SYM(pm_framebuffer_write)
    PM_SYM(pm_framebuffer_copy)
    PM_SYM(pm_framebuffer_merge)
    PM_SYM(pm_framebuffer_sync)
    PM_SYM(pm_framebuffer_wait)
    PM_SYM(pm_framebuffer_close)
    PM_SYM(pm_blit_framebuffer)
    PM_SYM(pm_blit)
    PM_SYM(pm_blit_read)
    PM_SYM(pm_blit_write)
    PM_SYM(pm_blit_load)
    PM_SYM(pm_blit_close)
    /* <picomite/input.h>: the mouse and the cursor */
    PM_SYM(pm_mouse)
    PM_SYM(pm_usb_device)
    PM_SYM(pm_cursor_on)
    PM_SYM(pm_cursor_off)
    PM_SYM(pm_cursor_hide)
    PM_SYM(pm_cursor_show)
    PM_SYM(pm_cursor_colour)
    PM_SYM(pm_cursor_link)
    PM_SYM(pm_cursor_load)
    PM_SYM(pm_cursor_move)
    /* <picomite/vars.h> */
    PM_SYM(pm_var)
    PM_SYM(pm_dim)
    PM_SYM(pm_erase)
    PM_SYM(pm_str_get)
    PM_SYM(pm_str_set)
    PM_SYM(pm_mode)
    PM_SYM(pm_font_width)
    PM_SYM(pm_font_height)
    PM_SYM(pm_get_framebuffer)
    PM_SYM(pm_fb_colour)
    /* ARM EABI run-time helpers */
    PM_AEABI(PM_SYM)
};

/* What a C CSUB cannot use: it runs outside any C run, with no arena (for
   malloc), no stdin, no files and nothing for exit() to end. */
static const char *const pm_csub_refused[] = {
    "malloc", "calloc", "realloc", "free", "strdup", "exit", "abort",
    "getchar", "fgetc", "getc", "fgets", "fopen", "fclose", "fread", "fwrite",
    "fseek", "ftell", "rewind", "feof", "ferror", "__tcc_pm_stdin",
    "pm_alloc", "pm_free", "pm_settick", "opendir", "readdir", "closedir",
    "pm_serial_open", "pm_serial_read", "pm_serial_write", "pm_serial_available",
    "pm_serial_flush", "pm_serial_close",
    "pm_i2c_open", "pm_i2c_close", "pm_i2c_transfer", "pm_i2c_write",
    "pm_i2c_read", "pm_i2c_probe",
    "pm_spi_open", "pm_spi_close", "pm_spi_transfer", "pm_spi_write",
    "pm_spi_read", "pm_adc_read", "pm_adc_read_raw",
    "pm_pwm_open", "pm_pwm_set_duty", "pm_pwm_set_frequency", "pm_pwm_close", NULL};

void *tcc_pm_resolve_symbol(const char *name)
{
    unsigned int i;
    if (pm_csub_linking)
        for (i = 0; pm_csub_refused[i]; i++)
            if (strcmp(pm_csub_refused[i], name) == 0)
                return NULL; /* "undefined symbol" */
    for (i = 0; i < sizeof(pm_syms) / sizeof(pm_syms[0]); i++)
        if (strcmp(pm_syms[i].name, name) == 0)
            return (void *)pm_syms[i].addr;
    return NULL;
}

/* The generated code was written through the data side: make sure it is
   complete before it is fetched as instructions. */
void tcc_pm_code_ready(void *ptr, unsigned long length)
{
    (void)ptr;
    (void)length;
    __asm volatile("dsb\n\tisb" ::: "memory");
}

int tcc_pm_call_main(int (*prog_main)(int, char **), int argc_, char **argv_)
{
    volatile int ret = 0;
    if (MMAbort)
    { /* CTRL-C during the compile: do not start */
        pm_prog_end = PM_END_ABORT;
        return 1;
    }
    pm_prog_started = true;
    pm_prog_end = PM_END_NONE;
    pm_line_len = pm_line_pos = 0;
    if (setjmp(pm->prog_jmp) == 0)
    {
        tcc_pm_running = true;
        ret = prog_main(argc_, argv_);
    }
    else
        ret = pm_exit_code;
    tcc_pm_running = false;
    return ret;
}

/* ------------------------------------------------------------------------ */
/* Entry                                                                     */
/* ------------------------------------------------------------------------ */

/* The bottom of the private stack holds a pattern: overwritten, the compiler
   or the program went past the end of its stack.  (No MSPLIM guard: the
   fault it raises could not stack its own exception frame there.) */
#define TCC_PM_STACK_MARK 64 /* words */
static void pm_stack_mark(void *stack)
{
    uint32_t *w = (uint32_t *)stack;
    int i;
    for (i = 0; i < TCC_PM_STACK_MARK; i++)
        w[i] = 0x5AC0FFEEu ^ (uint32_t)i;
}

static bool pm_stack_intact(const void *stack)
{
    const uint32_t *w = (const uint32_t *)stack;
    int i;
    for (i = 0; i < TCC_PM_STACK_MARK; i++)
        if (w[i] != (0x5AC0FFEEu ^ (uint32_t)i))
            return false;
    return true;
}

/* MMBasic checks its stack against the top of the C heap (TestStackOverflow
   in core/MMBasic.c: heapend, stackwarn).  Our stack is in the MMBasic heap,
   far below it, so while we are on it the check is moved to our stack's
   bottom (above the pattern): a BASIC command run from C (pm_execute) that
   goes too deep is then the usual "Stack overflow" error. */
extern uint32_t heapend, stackwarn, StackLow;
extern int StackLowDepth;

static void pm_stack_check_enter(void)
{
    pm->heapend = heapend, pm->stackwarn = stackwarn;
    pm->stacklow = StackLow, pm->stacklowdepth = StackLowDepth;
    heapend = (uint32_t)pm->stack + TCC_PM_STACK_MARK * 4 + 256;
    stackwarn = heapend + 1024;
}

static void pm_stack_check_leave(void)
{
    heapend = pm->heapend, stackwarn = pm->stackwarn;
    StackLow = pm->stacklow, StackLowDepth = pm->stacklowdepth; /* (no warning about our stack) */
}

/* Call fn(arg) with the stack pointer at stack_top. */
static int __attribute__((naked, noinline)) pm_call_on_stack(int (*fn)(void *), void *arg, void *stack_top)
{
    __asm volatile(
        "push {r4, lr}\n\t"
        "mov  r4, sp\n\t"
        "mov  sp, r2\n\t"
        "mov  r3, r0\n\t"
        "mov  r0, r1\n\t"
        "blx  r3\n\t"
        "mov  sp, r4\n\t"
        "pop  {r4, pc}\n\t");
}

static int pm_csub_entry(pm_run_args *a);

static int pm_run_entry(void *p)
{
    pm_run_args *a = (pm_run_args *)p;
    if (a->csub_entry)
        a->ret = pm_csub_entry(a);
    else
        a->ret = pico_tcc_main(a->argc, a->argv);
    return 0;
}

static void pm_state_reset(void)
{
    pm = NULL;
    pm_try_top = NULL;
    pm_tick_any = false;
    pm_blocks = NULL;
    pm_chunks = NULL;
    pm_freep = NULL;
    tcc_pm_running = pm_prog_started = false;
    pm_prog_end = PM_END_NONE;
    pm_exit_code = 0;
    pm_line = NULL;
    pm_line_len = pm_line_pos = 0;
    __tcc_pm_stdin = NULL;
    __tcc_pm_stdout = __tcc_pm_stderr = (FILE *)&pm_con_rom;
    pm_csub_linking = false;
}

/* Split a command line into words in place: blanks separate them, "..."
   keeps blanks in one.  Returns the number of words stored (at most max). */
static int pm_split(char *p, char **argv_, int max)
{
    int n = 0;
    char *d;
    while (n < max)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == 0)
            break;
        argv_[n++] = d = p;
        while (*p && *p != ' ' && *p != '\t')
        {
            if (*p == '"')
            {
                p++;
                while (*p && *p != '"')
                    *d++ = *p++;
                if (*p)
                    p++;
            }
            else
                *d++ = *p++;
        }
        if (*p)
            p++;
        *d = 0;
    }
    return n;
}

/* Run the TinyCC command line prefix[] + the words of args.  fail is the
   MMBasic error for a non-zero result when no program was started.  With
   csub, link a C CSUB instead (pm_csub_entry): its result and error are
   returned in *csub and no MMBasic error is raised (-1 for a failure). */
static int pm_exec(const char *const *prefix, int nprefix, const char *args, const char *fail,
                   pm_run_args *csub)
{
    volatile int how = PM_END_NONE;
    char errmsg[MAXERRMSG];
    char *argbuf;
    pm_run_args *a;
    pm_trap t;
    int i, max;

    if (pm_active)
    {
        /* Not cleaned up at the prompt (tcc_picomite_abandoned): the heap
           may have been reset since, so only drop the pointers into it. */
        tcc_pm_forget_globals();
        pm_active = false;
    }
    pm_state_reset();

    if (args == NULL)
        args = "";
    max = nprefix + (int)strlen(args) / 2 + 2;
    pm = pm_calloc(1, sizeof(pm_state));
    argbuf = pm_strdup(args);
    if (pm == NULL || argbuf == NULL ||
        (pm->args.argv = pm_calloc((size_t)max, sizeof(char *))) == NULL ||
        (pm->stack = GetMemoryNull(TCC_PM_STACK_SIZE)) == NULL)
    {
        pm_heap_release();
        pm_state_reset();
        error("Not enough memory");
    }
    pm->con_in.kind = PM_FILE_CONIN;
    pm->con_out.kind = pm->con_err.kind = PM_FILE_CONOUT;
    pm_stack_mark(pm->stack);
    __tcc_pm_stdin = (FILE *)&pm->con_in;
    __tcc_pm_stdout = (FILE *)&pm->con_out;
    __tcc_pm_stderr = (FILE *)&pm->con_err;

    a = &pm->args;
    for (i = 0; i < nprefix; i++)
        a->argv[a->argc++] = (char *)prefix[i];
    a->argc += pm_split(argbuf, a->argv + a->argc, max - a->argc - 1);
    a->argv[a->argc] = NULL;
    if (csub != NULL)
    {
        a->csub_entry = csub->csub_entry;
        a->csub_base = csub->csub_base;
        a->csub_dir = csub->csub_dir;
    }

    __asm volatile("mrs %0, msplim" : "=r"(pm->msplim));
    __asm volatile("msr msplim, %0" ::"r"(0));
    pm_stack_check_enter();
    pm_active = true;
    if (setjmp(pm->run_jmp) == 0)
    {
        if (PM_TRY(t))
            pm_call_on_stack(pm_run_entry, &pm->args, (char *)pm->stack + TCC_PM_STACK_SIZE);
        else
        { /* an MMBasic error that no adapter call trapped */
            memcpy(errmsg, MMErrMsg, MAXERRMSG);
            how = PM_END_ERROR;
            tcc_pm_forget_globals();
        }
    }
    else
    { /* exit() of the compiler itself: TinyCC's state is abandoned */
        pm->args.ret = pm_exit_code;
        tcc_pm_forget_globals();
    }
    pm_trap_restore(&t);
    __asm volatile("msr msplim, %0" ::"r"(pm->msplim));
    pm_stack_check_leave();
    pm_active = false;
    tcc_pm_running = false;

    if (how == PM_END_NONE)
        how = pm_prog_end;
    if (how == PM_END_ERROR && pm_prog_end == PM_END_ERROR)
        memcpy(errmsg, pm->prog_errmsg, MAXERRMSG);
    if (how == PM_END_NONE && !pm_prog_started && pm->args.ret != 0)
        how = -1; /* the compile failed */
    if (csub != NULL)
    { /* (the image is outside the arena) */
        csub->csub_image = how == PM_END_NONE ? pm->args.csub_image : NULL;
        csub->csub_size = pm->args.csub_size;
        if (how != PM_END_NONE && pm->args.csub_image != NULL)
            FreeMemory(pm->args.csub_image);
    }

    if (!pm_stack_intact(pm->stack) && how != PM_END_ABORT)
    { /* (found afterwards: whatever lay below the stack may be damaged) */
        how = PM_END_ERROR;
        snprintf(errmsg, sizeof(errmsg), "C stack overflow (%d KB)", TCC_PM_STACK_SIZE / 1024);
    }
    pm_close_all();
    pm_serial_cleanup();
    pm_i2c_cleanup();
    pm_spi_cleanup();
    FreeMemory(pm->stack);
    pm_heap_release(); /* (pm itself is in the arena) */
    pm_state_reset();

    if (csub != NULL)
    {
        if (how == PM_END_NONE)
            return 0;
        snprintf(csub->csub_err, MAXERRMSG, "%s", how == PM_END_ERROR ? errmsg : fail);
        return -1;
    }
    switch (how)
    {
    case PM_END_ABORT:
        MMAbort = true;
        CheckAbort(); /* back to the prompt as for a BASIC program */
        break;
    case PM_END_ERROR:
        errmsg[MAXERRMSG - 1] = 0;
        error("$", errmsg);
        break;
    case -1:
        error("$", fail);
        break;
    default:
        break;
    }
    return 0;
}

/* RUN "file.c" [, "args"]: tcc -xc -D__PICOMITE__ -run file.c args...
   RUN "file" [, "args"] for an ELF object made by the TCC command:
   tcc -xb -run file args... - linked to the firmware as it is loaded. */
void tcc_picomite_run(const char *filename, const char *args, bool is_c)
{
    static const char *const c_prefix[] = {"tcc", "-xc", "-D__PICOMITE__", "-run"};
    static const char *const elf_prefix[] = {"tcc", "-xb", "-run"};
    char line[STRINGSIZE * 2 + 4]; /* "file" args, the name quoted for pm_split */
    snprintf(line, sizeof(line), "\"%s\" %s", filename, args ? args : "");
    if (is_c)
        pm_exec(c_prefix, 4, line, "C compilation failed", NULL);
    else
        pm_exec(elf_prefix, 3, line, "Cannot run the ELF file", NULL);
}

/* Does the file start with the ELF signature?  False for a file that does
   not exist or cannot be read - never an MMBasic error. */
bool tcc_picomite_is_elf(const char *filename)
{
    unsigned char sig[4] = {0};
    unsigned int got = 0;
    volatile bool elf = false;
    int fnbr = pm_free_fnbr();
    pm_trap t;
    if (fnbr < 0 || !pm_disk_open(filename, fnbr, FA_READ, true))
        return false;
    if (PM_TRY(t))
        elf = FileGetData(fnbr, sig, 4, &got) == 0 && got == 4 &&
              sig[0] == 0x7f && sig[1] == 'E' && sig[2] == 'L' && sig[3] == 'F';
    pm_trap_restore(&t);
    ForceFileClose(fnbr);
    return elf;
}

/* TCC "command line": TinyCC's own command line (see TCC "-h"), e.g.
     TCC "-c util.c"                     object file util.o
     TCC "-o game.elf main.c util.o"     executable for RUN "game.elf"
     TCC "@game.rsp"                     the arguments read from a file
     TCC "-run test.c arg"               compile and run, as RUN "test.c"
   Without -c, -r, -E or -run the files are linked into an executable (an ELF
   object holding the whole program, which RUN binds to the firmware). */
void cmd_tcc(void)
{
    static const char *const prefix[] = {"tcc", "-D__PICOMITE__"};
    char line[STRINGSIZE];
    snprintf(line, sizeof(line), "%s", (char *)getCstring(cmdline));
    pm_exec(prefix, 2, line, "TCC failed", NULL);
}

/* Called before the heap or the program is reset (InitHeap, m_alloc,
   ClearRuntime): under a C program, or the compiler, that would free its
   arena and the stack it runs on.  Whatever a command run from C that does
   it (pm_execute) got past is stopped here, before any memory is touched. */
void tcc_picomite_reset_guard(void)
{
    if (pm_active)
        error("Not possible from a C program");
}

/* Called where the MMBasic prompt lands after a longjmp(mark) (PicoMite.c).
   If a run was in progress, it was left past its own cleanup - CTRL-C inside
   a firmware call that checks for it itself (CheckAbort from getConsole, for
   instance), or an error nothing trapped.  Nothing has used the heap since,
   so the stack and the arena are still ours to give back. */
void tcc_picomite_abandoned(void)
{
    int i;
    pm_try_top = NULL; /* (a C CSUB's pm_try left by CTRL-C: its frame is gone) */
    if (!pm_active)
        return;
    pm_active = false;
    tcc_pm_running = false;
    if (pm != NULL)
    {
        __asm volatile("msr msplim, %0" ::"r"(pm->msplim));
        pm_stack_check_leave();
        pm_serial_cleanup();
        pm_i2c_cleanup();
        pm_spi_cleanup();
        for (i = 0; i < TCC_PM_MAX_FILES; i++)
            if (pm_files[i].kind == PM_FILE_DISK)
            {
                /* (do_end's CloseAllFiles may have closed it already) */
                if (filesource[pm_files[i].fnbr] != NONEFILE)
                    ForceFileClose(pm_files[i].fnbr);
                pm_files[i].kind = PM_FILE_FREE;
            }
        if (pm->stack != NULL)
            FreeMemory(pm->stack);
    }
    pm_heap_release(); /* (pm itself is in the arena) */
    tcc_pm_forget_globals();
    pm_state_reset();
}

/* ------------------------------------------------------------------------ */
/* C CSUBs                                                                   */
/* ------------------------------------------------------------------------ */

/* An image linked against the firmware is valid only while every address it
   was bound to is unchanged: the id hashes the addresses of the run-time
   symbol table, which is all a C CSUB binds to (FNV-1a). */
uint32_t tcc_pm_firmware_id(void)
{
    static uint32_t id;
    unsigned int i;
    if (id == 0)
    {
        uint32_t h = 2166136261u;
        for (i = 0; i < sizeof(pm_syms) / sizeof(pm_syms[0]); i++)
        {
            uint32_t a = (uint32_t)pm_syms[i].addr;
            int b;
            for (b = 0; b < 4; b++, a >>= 8)
                h = (h ^ (a & 0xff)) * 16777619u;
        }
        id = (h == 0 || h == 0xffffffffu) ? 1 : h;
    }
    return id;
}

static void pm_push_word(char ***argv_, int *argc_, int *cap, char *w)
{
    if (*argc_ + 2 > *cap)
    {
        char **n = tcc_pm_realloc(*argv_, (size_t)(*cap *= 2) * sizeof(char *));
        if (n == NULL)
            return; /* (the compile then reports a missing file or option) */
        *argv_ = n;
    }
    (*argv_)[(*argc_)++] = w;
}

static bool pm_is_abs(const char *p)
{
    return p[0] == '/' || (p[0] && p[1] == ':');
}

/* dir + name for a relative name (dir ends with '/'), else the name */
static char *pm_join(const char *dir, const char *name)
{
    char *r;
    if (dir == NULL || *name == 0 || pm_is_abs(name))
        return (char *)name;
    r = tcc_pm_malloc(strlen(dir) + strlen(name) + 1);
    if (r == NULL)
        return (char *)name;
    strcpy(r, dir);
    strcat(r, name);
    return r;
}

/* the directory part of a path, with its '/', or NULL */
static char *pm_dirname(const char *path)
{
    const char *e = strrchr(path, '/');
    char *r;
    size_t n;
    if (e == NULL)
        e = (path[0] && path[1] == ':') ? path + 1 : NULL;
    if (e == NULL)
        return NULL;
    n = (size_t)(e - path) + 1;
    r = tcc_pm_malloc(n + 1);
    if (r != NULL)
    {
        memcpy(r, path, n);
        r[n] = 0;
    }
    return r;
}

/* the whole of a file, zero-terminated, or NULL */
static char *pm_read_all(const char *path)
{
    pm_fmode m;
    pm_file *f;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    int r;
    pm_fmode_parse("r", &m);
    f = pm_open(path, &m);
    if (f == NULL)
        return NULL;
    do
    {
        if (len + 257 > cap)
        {
            char *n = tcc_pm_realloc(buf, cap = cap ? cap * 2 : 512);
            if (n == NULL)
            {
                tcc_pm_free(buf);
                pm_close(f);
                return NULL;
            }
            buf = n;
        }
        r = pm_read(f, buf + len, 256);
        if (r > 0)
            len += (size_t)r;
    } while (r > 0);
    pm_close(f);
    if (r < 0)
    {
        tcc_pm_free(buf);
        return NULL;
    }
    buf[len] = 0;
    return buf;
}

/* The words of a C CSUB with relative names made relative to dir: input
   files, -I/-L directories and response files - whose own words are then
   relative to the response file's directory. */
static int pm_csub_words(char **w, int n, const char *dir, char ***argv_, int *argc_, int *cap, int depth)
{
    int i;
    for (i = 0; i < n; i++)
    {
        char *x = w[i];
        if (x[0] == '@')
        {
            char *path = pm_join(dir, x + 1), *text, *p, **ws;
            int k;
            if (depth >= 4)
            {
                fprintf(stderr, "tcc: error: '%s': response files nested too deep\n", path);
                return -1;
            }
            if ((text = pm_read_all(path)) == NULL)
            {
                fprintf(stderr, "tcc: error: cannot read '%s'\n", path);
                return -1;
            }
            for (p = text; *p; p++)
                if (*p == '\r' || *p == '\n')
                    *p = ' ';
            k = (int)strlen(text) / 2 + 2;
            if ((ws = tcc_pm_malloc((size_t)k * sizeof(char *))) == NULL)
                return -1;
            k = pm_split(text, ws, k);
            if (pm_csub_words(ws, k, pm_dirname(path), argv_, argc_, cap, depth + 1) < 0)
                return -1;
        }
        else if (x[0] == '-')
        {
            if ((x[1] == 'I' || x[1] == 'L') && x[2])
            {
                char *d = pm_join(dir, x + 2), *o = tcc_pm_malloc(strlen(d) + 3);
                if (o == NULL)
                    return -1;
                o[0] = '-', o[1] = x[1];
                strcpy(o + 2, d);
                pm_push_word(argv_, argc_, cap, o);
            }
            else
            {
                pm_push_word(argv_, argc_, cap, x);
                if (x[2] == 0 && i + 1 < n)
                {
                    if (x[1] == 'I' || x[1] == 'L')
                        pm_push_word(argv_, argc_, cap, pm_join(dir, w[++i]));
                    else if (x[1] && strchr("DUxo", x[1]))
                        pm_push_word(argv_, argc_, cap, w[++i]);
                }
            }
        }
        else
            pm_push_word(argv_, argc_, cap, pm_join(dir, x));
    }
    return 0;
}

int pico_tcc_csub(int argc, char **argv, const char *entry, unsigned base,
                  unsigned char **pimage, unsigned *psize);

/* (on the private stack) */
static int pm_csub_entry(pm_run_args *a)
{
    int argc_ = 0, cap = 16, i;
    char **argv_ = tcc_pm_malloc((size_t)cap * sizeof(char *));
    unsigned char *img;
    unsigned size;
    if (argv_ == NULL)
        return 1;
    argv_[argc_++] = "tcc";
    if (pm_csub_words(a->argv + 1, a->argc - 1, a->csub_dir, &argv_, &argc_, &cap, 0) < 0)
        return 1;
    argv_[argc_] = NULL;
    pm_csub_linking = true;
    i = pico_tcc_csub(argc_, argv_, a->csub_entry, a->csub_base, &img, &size);
    pm_csub_linking = false;
    if (i)
        return 1;
    if ((a->csub_image = GetMemoryNull((int)size)) == NULL)
    {
        fprintf(stderr, "tcc: error: not enough memory for the CSUB\n");
        return 1;
    }
    memcpy(a->csub_image, img, size);
    a->csub_size = size;
    return 0;
}

/* The C body of a CSUB, from its first body line p (as the hex loops find
   it) to END CSUB: the file names and options in quotes become the TinyCC
   command line in line.  Returns where END CSUB is (as the hex loops leave
   p), or NULL if the body is hex. */
unsigned char *tcc_picomite_csub_source(unsigned char *p, unsigned short endtoken, char *line, int size)
{
    int n = 0;
    skipspace(p);
    if (*p != '"')
        return NULL;
    *line = 0;
    for (;;)
    {
        while (*p && *p != '\'')
        {
            unsigned char *e = NULL;
            skipspace(p);
            if (*p == 0 || *p == '\'')
                break;
            if (*p == '"')
                e = (unsigned char *)strchr((char *)p + 1, '"');
            if (e == NULL)
                error("CSUB: a C CSUB holds file names and options in quotes");
            if (n + (e - p) + 2 >= size)
                error("CSUB: too many files and options");
            memcpy(line + n, p, (size_t)(e - p) + 1); /* with its quotes (pm_split) */
            n += (int)(e - p) + 1;
            line[n++] = ' ';
            line[n] = 0;
            p = e + 1;
            skipspace(p);
            if (*p == ',')
                p++;
        }
        while (*p)
            p++; // to the end of the line
        p++;     // the start of the next line
        if (*p == 0)
            error("Missing END declaration");
        if (*p == T_NEWLINE)
        {
            CurrentLinePtr = p;
            p += T_NEWLINE_HDR;
        }
        if (*p == T_LINENBR)
            p += 3;
        skipspace(p);
        if (commandtbl_at(p) == endtoken)
            return p;
    }
}

/* The directory of the program, from its "'#B:/dir/prog.bas" first line */
static bool pm_prog_dir(const unsigned char *prog, char *dir, int size)
{
    const char *path, *e;
    if (prog == NULL || prog[0] != T_NEWLINE || prog[T_NEWLINE_HDR] != '\'' || prog[T_NEWLINE_HDR + 1] != '#')
        return false;
    path = (const char *)prog + T_NEWLINE_HDR + 2;
    e = strrchr(path, '/');
    if (e == NULL || e - path + 2 > size)
        return false;
    memcpy(dir, path, (size_t)(e - path) + 1);
    dir[e - path + 1] = 0;
    return true;
}

/* Compile and link a C CSUB (tcc_picomite_csub_source's line) for the
   address base of its record's entry word.  decl is the CSUB statement (for
   its name: the function to enter), prog the program (for its directory).
   Returns the image (MMBasic heap, *words words; FreeMemory it) or NULL, the
   error in err (MAXERRMSG) - TinyCC's own messages are on the console. */
uint32_t *tcc_picomite_csub_compile(const unsigned char *decl, const char *line, uint32_t base,
                                    const unsigned char *prog, int *words, char *err)
{
    static const char *const prefix[] = {"tcc"};
    char name[MAXVARLEN + 1], dir[STRINGSIZE];
    pm_run_args c;
    bool irqs = irqs_off_pico;
    int n = 0, r;

    decl += 2; // the CSUB token
    skipspace(decl);
    while (isnamechar(*decl) && n < MAXVARLEN)
        name[n++] = (char)*decl++;
    name[n] = 0;
    memset(&c, 0, sizeof(c));
    c.csub_entry = name;
    c.csub_base = base;
    c.csub_dir = pm_prog_dir(prog, dir, sizeof(dir)) ? dir : NULL;
    c.csub_err = err;
    if (irqs) /* (SaveProgramToFlash: no flash write is under way between records) */
        enable_interrupts_pico();
    r = pm_exec(prefix, 1, line, "CSUB: C compilation failed", &c);
    if (irqs)
        disable_interrupts_pico();
    if (r < 0)
        return NULL;
    *words = (int)((c.csub_size + 3) / 4);
    return (uint32_t *)c.csub_image;
}

/* Before a C CSUB is called (CallCFunction): code is its record's code
   after the entry word (3), so code[3] is the entry. */
void tcc_picomite_csub_check(const uint32_t *code)
{
    if (code[0] == TCC_PM_CSUB_MAGIC &&
        (code[1] != tcc_pm_firmware_id() || code[2] != (uint32_t)(code - 1)))
        error("C CSUB built for another firmware or place: LOAD the program again");
}

/* Is a C CSUB of the program built for another firmware? */
bool tcc_picomite_csub_stale(void)
{
    const uint32_t *p = (const uint32_t *)CFunctionFlash;
    if (p == NULL)
        return false;
    while (*p != 0xffffffff)
    {
        uint32_t decl = *p++;
        if (!(decl & 0x80000000u) && *p >= 24 && p[1] == 3 && p[2] == TCC_PM_CSUB_MAGIC &&
            (p[3] != tcc_pm_firmware_id() || p[4] != (uint32_t)(p + 1)))
            return true;
        p += (*p + 4) / sizeof(uint32_t);
    }
    return false;
}

/* Save the program again from flash, which compiles its C CSUBs again (for
   a new firmware: RUN, when tcc_picomite_csub_stale). */
void tcc_picomite_csub_rebuild(void)
{
    unsigned char *buf, *q, *p, line[STRINGSIZE];
    if (ProgMemory != flash_progmemory)
        error("C CSUB built for another firmware: LOAD the program again");
    MMPrintString("Recompiling the C CSUBs for this firmware\r\n");
    q = buf = GetTempMemory(EDIT_BUFFER_SIZE - 2048);
    p = ProgMemory;
    while (!(*p == 0 || *p == 0xff))
    {
        size_t n;
        p = llist(line, p);
        n = strlen((char *)line);
        if ((size_t)(q - buf) + n + 2 >= EDIT_BUFFER_SIZE - 2048)
            error("Not enough memory");
        memcpy(q, line, n);
        q += n;
        *q++ = '\n';
        if (p[0] == 0 && p[1] == 0)
            break;
    }
    *q = 0;
    SaveProgramToFlash(buf, false, PROGRAM_FLASH);
}

/* ------------------------------------------------------------------------ */
/* Predefined macros: the firmware's build, for PicoCFunctions.h             */
/* ------------------------------------------------------------------------ */

const char *const tcc_pm_predefined[] = {
    "__PM_FIRMWARE__",
#ifdef PICOMITE
    "__PM_PICOMITE",
#endif
#ifdef PICOMITEVGA
    "__PM_PICOMITEVGA",
#endif
#ifdef PICOMITEWEB
    "__PM_PICOMITEWEB",
#endif
#ifdef PICOMITEHDMIWEB
    "__PM_PICOMITEHDMIWEB",
#endif
#ifdef PICOMITEBT
    "__PM_PICOMITEBT",
#endif
#ifdef PICOMITEBTH
    "__PM_PICOMITEBTH",
#endif
#ifdef PICOMITEHDMIBTH
    "__PM_PICOMITEHDMIBTH",
#endif
#ifdef PICOMITEPC3
    "__PM_PICOMITEPC3",
#endif
#ifdef GUICONTROLS
    "__PM_GUICONTROLS",
#endif
#ifdef rp2350
    "__PM_PICORP2350",
#endif
#if PICOMITERP2350
    "__PM_PICOMITERP2350",
#endif
#ifdef HDMI
    "__PM_HDMI",
#endif
#ifdef USBKEYBOARD
    "__PM_USBKEYBOARD",
#endif
    NULL};
