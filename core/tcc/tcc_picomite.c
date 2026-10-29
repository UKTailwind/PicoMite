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
#define TCC_PM_MAX_ARGS 16

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
static inline void pm_poll(void)
{
    if (MMAbort && pm_in_program)
        pm_end_program(PM_END_ABORT, 1);
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
static void pm_heap_release(void)
{
    pm_chunk *c = pm_chunks;
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
} pm_run_args;

struct pm_state
{
    pm_file files[TCC_PM_MAX_FILES];
    pm_file con_in, con_out, con_err;
    jmp_buf prog_jmp; /* tcc_pm_call_main: exit() and errors of the program */
    jmp_buf run_jmp;  /* tcc_picomite_run: exit() of the compiler */
    char prog_errmsg[MAXERRMSG];
    pm_run_args args;
    void *stack;
    uint32_t msplim;
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
FILE *__tcc_pm_stdin, *__tcc_pm_stdout, *__tcc_pm_stderr;

static pm_file *pm_file_of(FILE *f)
{
    pm_file *p = (pm_file *)f;
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

static void __attribute__((noreturn)) pm_program_error_tr(pm_trap *t)
{
    char msg[MAXERRMSG];
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_trap_restore(t);
    memcpy(MMErrMsg, msg, MAXERRMSG);
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
    /* ARM EABI run-time helpers */
    PM_AEABI(PM_SYM)
};

void *tcc_pm_resolve_symbol(const char *name)
{
    unsigned int i;
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

static int pm_run_entry(void *p)
{
    pm_run_args *a = (pm_run_args *)p;
    a->ret = pico_tcc_main(a->argc, a->argv);
    return 0;
}

static void pm_state_reset(void)
{
    pm = NULL;
    pm_chunks = NULL;
    pm_freep = NULL;
    pm_in_program = pm_prog_started = false;
    pm_prog_end = PM_END_NONE;
    pm_exit_code = 0;
    pm_line = NULL;
    pm_line_len = pm_line_pos = 0;
    __tcc_pm_stdin = __tcc_pm_stdout = __tcc_pm_stderr = NULL;
}

void tcc_picomite_run(const char *filename, const char *args)
{
    volatile int how = PM_END_NONE;
    char errmsg[MAXERRMSG];
    char *argbuf, *p;
    pm_run_args *a;
    pm_trap t;

    if (pm_active)
    {
        /* The last run was left by a jump straight to the prompt (CTRL-C
           inside a console read): its memory and files went with
           ClearRuntime, so only TinyCC's pointers to them are left. */
        tcc_pm_forget_globals();
        pm_active = false;
    }
    pm_state_reset();

    pm = pm_calloc(1, sizeof(pm_state));
    argbuf = pm_strdup(args ? args : "");
    if (pm == NULL || argbuf == NULL ||
        (pm->args.argv = pm_calloc(TCC_PM_MAX_ARGS + 6, sizeof(char *))) == NULL ||
        (pm->stack = GetMemoryNull(TCC_PM_STACK_SIZE)) == NULL)
    {
        pm_heap_release();
        error("Not enough memory");
    }
    pm->con_in.kind = PM_FILE_CONIN;
    pm->con_out.kind = pm->con_err.kind = PM_FILE_CONOUT;
    __tcc_pm_stdin = (FILE *)&pm->con_in;
    __tcc_pm_stdout = (FILE *)&pm->con_out;
    __tcc_pm_stderr = (FILE *)&pm->con_err;

    /* tcc -xc -D__PICOMITE__ -run file [args...] */
    a = &pm->args;
    a->argv[a->argc++] = "tcc";
    a->argv[a->argc++] = "-xc";
    a->argv[a->argc++] = "-D__PICOMITE__";
    a->argv[a->argc++] = "-run";
    a->argv[a->argc++] = pm_strdup(filename);
    for (p = argbuf; *p && a->argc < TCC_PM_MAX_ARGS + 5;)
    {
        while (*p == ' ')
            *p++ = 0;
        if (*p == 0)
            break;
        a->argv[a->argc++] = p;
        while (*p && *p != ' ')
            p++;
    }
    a->argv[a->argc] = NULL;
    if (a->argv[4] == NULL)
    {
        FreeMemory(pm->stack);
        pm_heap_release();
        error("Not enough memory");
    }

    __asm volatile("mrs %0, msplim" : "=r"(pm->msplim));
    __asm volatile("msr msplim, %0" ::"r"(0));
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
    pm_active = false;
    pm_in_program = false;

    if (how == PM_END_NONE)
        how = pm_prog_end;
    if (how == PM_END_ERROR && pm_prog_end == PM_END_ERROR)
        memcpy(errmsg, pm->prog_errmsg, MAXERRMSG);
    if (how == PM_END_NONE && !pm_prog_started && pm->args.ret != 0)
        how = -1; /* the compile failed */

    pm_close_all();
    FreeMemory(pm->stack);
    pm_heap_release(); /* (pm itself is in the arena) */
    pm_state_reset();

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
        error("C compilation failed");
        break;
    default:
        break;
    }
}
