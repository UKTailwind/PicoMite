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
static volatile bool pm_in_program;   /* the compiled program is running */
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
    if (MMAbort && pm_in_program)
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

/* End the compiled program now (exit(), CTRL-C, an MMBasic error). */
static void __attribute__((noreturn)) pm_end_program(int how, int code)
{
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

/* BasicFileOpen without the MMBasic error: false if the file cannot be opened
   (a missing file is not an error for TinyCC, which tries each include
   directory in turn). */
static bool pm_disk_open(const char *path, int fnbr, bool write)
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
        if (write || ExistsFile(name) == 1)
            ok = BasicFileOpen(name, fnbr, write ? (FA_WRITE | FA_CREATE_ALWAYS) : FA_READ);
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

static pm_file *pm_open(const char *path, bool write)
{
    pm_file *f = NULL;
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
    if (f->fnbr < 0 || !pm_disk_open(path, f->fnbr, write))
        return NULL;
    if (write && (f->wbuf = tcc_pm_malloc(TCC_PM_WBUF)) == NULL)
    {
        ForceFileClose(f->fnbr);
        return NULL;
    }
    f->kind = PM_FILE_DISK;
    f->write = write;
    return f;
}

static bool pm_disk_flush(pm_file *f)
{
    volatile bool ok = true;
    pm_trap t;
    if (f->kind != PM_FILE_DISK || f->wlen == 0)
        return true;
    if (PM_TRY(t))
        FilePutData(f->wbuf, f->fnbr, f->wlen);
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
        r = pm_disk_read(f, buf, count);
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
    f = pm_open(path, false);
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

FILE *tcc_pm_fopen(const char *path, const char *mode)
{
    bool write;
    if (mode == NULL || (mode[0] != 'r' && mode[0] != 'w'))
        return NULL; /* append and update are not supported */
    write = mode[0] == 'w';
    if (strchr(mode, '+'))
        return NULL;
    return (FILE *)pm_open(path, write);
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
    if (pm_in_program)
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

/* An MMBasic error msg (already untrapped): the end of the C program, or in
   a C CSUB an error at its call */
static void __attribute__((noreturn)) pm_program_error_tr_msg(const char *m)
{
    char msg[MAXERRMSG];
    memcpy(msg, m, MAXERRMSG);
    msg[MAXERRMSG - 1] = 0;
    if (!pm_in_program)
        error("$", msg);
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error();
}

static void __attribute__((noreturn)) pm_program_error_tr(pm_trap *t)
{
    char msg[MAXERRMSG];
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(t);
    memcpy(MMErrMsg, msg, MAXERRMSG);
    if (!pm_in_program)
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

static void pm_pause(unsigned int ms)
{
    uint64_t end = time_us_64() + (uint64_t)ms * 1000;
    while (time_us_64() < end)
        pm_poll();
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

/* ------------------------------------------------------------------------ */
/* <picomite/system.h>, <picomite/memory.h>, <picomite/graphics.h>           */
/* The structs must match those headers (core/tcc/include/picomite).         */
/* ------------------------------------------------------------------------ */

#define PM_API_VERSION 1

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
    if (pm_in_program)
        ClearTempMemory();
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
    if (checkstring(p, (unsigned char *)"RUN"))
        error("RUN is not possible from C");
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
        if (!SymAwareCommand(c))
            cmdline = SymExpandStatement(cmdline);
        ScrewUpTimer = 1000;
        commandtbl[c].fptr();
        ScrewUpTimer = 0;
    }
}

static void pm_execute(const char *cmd)
{
    pm_linebufs b;
    pm_trap t;
    char msg[MAXERRMSG];
    pm_poll();
    pm_lines_save(&b);
    if (PM_TRY(t))
    {
        pm_execute_do(cmd);
        pm_trap_restore(&t);
        ScrewUpTimer = 0;
        pm_lines_restore(&b);
        pm_temps_done();
        return;
    }
    ScrewUpTimer = 0;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(&t);
    pm_lines_restore(&b);
    pm_temps_done();
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error_tr_msg(msg);
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
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(&t);
    pm_lines_restore(&b);
    pm_temps_done();
    memcpy(MMErrMsg, msg, MAXERRMSG);
    pm_program_error_tr_msg(msg);
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
    if (pm == NULL || !pm_in_program || pm->in_tick)
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
            pm->in_tick = true;
            tk->fn();
            pm->in_tick = false;
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
    PM_SYM(pm_execute)
    PM_SYM(pm_eval_int)
    PM_SYM(pm_eval_float)
    PM_SYM(pm_eval_str)
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
    PM_SYM(pm_load_image)
    PM_SYM(pm_load_jpg)
    PM_SYM(pm_colour)
    PM_SYM(pm_keydown)
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
    "pm_alloc", "pm_free", "pm_settick", NULL};

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
        pm_in_program = true;
        ret = prog_main(argc_, argv_);
    }
    else
        ret = pm_exit_code;
    pm_in_program = false;
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
    pm_tick_any = false;
    pm_blocks = NULL;
    pm_chunks = NULL;
    pm_freep = NULL;
    pm_in_program = pm_prog_started = false;
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
    pm_in_program = false;

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
    if (fnbr < 0 || !pm_disk_open(filename, fnbr, false))
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

/* Called where the MMBasic prompt lands after a longjmp(mark) (PicoMite.c).
   If a run was in progress, it was left past its own cleanup - CTRL-C inside
   a firmware call that checks for it itself (CheckAbort from getConsole, for
   instance), or an error nothing trapped.  Nothing has used the heap since,
   so the stack and the arena are still ours to give back. */
void tcc_picomite_abandoned(void)
{
    int i;
    if (!pm_active)
        return;
    pm_active = false;
    pm_in_program = false;
    if (pm != NULL)
    {
        __asm volatile("msr msplim, %0" ::"r"(pm->msplim));
        pm_stack_check_leave();
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
    pm_file *f = pm_open(path, false);
    char *buf = NULL;
    size_t len = 0, cap = 0;
    int r;
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
