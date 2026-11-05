/* PicoMite stdio for externally compiled ELF and for TinyCC -run programs.
 * Compiler's own tcc_pm_* descriptor/ROM-file adapter remains in tcc_picomite.
 * No newlib file descriptors are used; the PicoMite filesystem is shared.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_runtime.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <setjmp.h>
#include <fcntl.h>

#define PM_RT_FILES 8
#define PM_RT_FREE 0
#define PM_RT_DISK 1
#define PM_RT_CONIN 2
#define PM_RT_CONOUT 3

typedef struct {
    int kind, fnbr;
    unsigned char read, write, append, eof, error;
} pm_rt_file;
static pm_rt_file pm_rt_console_in = {.kind = PM_RT_CONIN};
static pm_rt_file pm_rt_console_out = {.kind = PM_RT_CONOUT};
static pm_rt_file *pm_rt_files;
static char *pm_rt_input_line;
static int pm_rt_input_len, pm_rt_input_pos;

/* TinyCC's builtin stdio.h names globals, not functions. Expose the
 * addresses of these variables from the common resolver on both MCUs.
 * The TCC adapter retains its private compiler-side console variables. */
static FILE *pm_rt_stdin = (FILE *)&pm_rt_console_in;
static FILE *pm_rt_stdout = (FILE *)&pm_rt_console_out;
static FILE *pm_rt_stderr = (FILE *)&pm_rt_console_out;
static bool pm_rt_running;

/* The adapter's BASIC error catcher does not need the compiler. */
typedef struct {
    jmp_buf saved;
    int skip, err;
    unsigned char *line;
    char msg[MAXERRMSG];
} pm_rt_trap;
static unsigned char pm_rt_cmdline[] = " ";
static void pm_rt_trap_save(pm_rt_trap *t)
{
    memcpy(t->saved, ErrNext, sizeof(jmp_buf));
    t->skip = OptionErrorSkip;
    t->err = MMerrno;
    t->line = cmdline;
    memcpy(t->msg, MMErrMsg, MAXERRMSG);
    OptionErrorSkip = 1;
    cmdline = pm_rt_cmdline;
}
static void pm_rt_trap_restore(pm_rt_trap *t)
{
    memcpy(ErrNext, t->saved, sizeof(jmp_buf));
    OptionErrorSkip = t->skip;
    MMerrno = t->err;
    cmdline = t->line;
    memcpy(MMErrMsg, t->msg, MAXERRMSG);
}
#define PM_RT_TRY(t) (pm_rt_trap_save(&(t)), setjmp(ErrNext) == 0)

void *pm_runtime_malloc(size_t size);
void pm_runtime_free(void *p);

void pm_runtime_files_begin(void)
{
    pm_rt_running = true;
    pm_rt_files = NULL;
    pm_rt_input_line = NULL;
    pm_rt_input_len = pm_rt_input_pos = 0;
}

static pm_rt_file *pm_rt_file_of(FILE *file)
{
    unsigned i;
    if (file == (FILE *)&pm_rt_console_in) return &pm_rt_console_in;
    if (file == (FILE *)&pm_rt_console_out) return &pm_rt_console_out;
    if (!pm_rt_files) return NULL;
    for (i = 0; i < PM_RT_FILES; ++i)
        if (file == (FILE *)&pm_rt_files[i] && pm_rt_files[i].kind == PM_RT_DISK)
            return &pm_rt_files[i];
    return NULL;
}

static long pm_rt_size(pm_rt_file *f)
{
    if (filesource[f->fnbr] == FLASHFILE)
        return (long)lfs_file_size(&lfs, FileTable[f->fnbr].lfsptr);
    return (long)f_size(FileTable[f->fnbr].fptr);
}

static int pm_rt_close_file(pm_rt_file *f)
{
    if (!f || f->kind != PM_RT_DISK) return -1;
    ForceFileClose(f->fnbr);
    f->kind = PM_RT_FREE;
    return FSerror ? -1 : 0;
}
void pm_runtime_files_end(void)
{
    unsigned i;
    if (!pm_rt_running) return;
    pm_rt_running = false;
    if (pm_rt_files) {
        for (i = 0; i < PM_RT_FILES; ++i)
            if (pm_rt_files[i].kind == PM_RT_DISK) pm_rt_close_file(&pm_rt_files[i]);
        pm_runtime_free(pm_rt_files);
        pm_rt_files = NULL;
    }
    pm_runtime_free(pm_rt_input_line);
    pm_rt_input_line = NULL;
    pm_rt_input_len = pm_rt_input_pos = 0;
}

static bool pm_rt_file_open(const char *path, int fnbr, int mode, bool must_exist)
{
    char name[FF_MAX_LFN + 4];
    volatile bool ok = false;
    pm_rt_trap t;
    if (!path || strlen(path) >= sizeof(name)) return false;
    strcpy(name, path);
    if (PM_RT_TRY(t)) {
        FatFSFileSystemSave = FatFSFileSystem;
        if (!must_exist || ExistsFile(name) == 1)
            ok = BasicFileOpen(name, fnbr, mode);
    } else {
        FatFSFileSystem = FatFSFileSystemSave;
        if (filesource[fnbr] != NONEFILE && FileTable[fnbr].com != 0)
            ForceFileClose(fnbr);
    }
    pm_rt_trap_restore(&t);
    return ok;
}

static int pm_rt_free_fnbr(void)
{
    int i;
    for (i = MAXOPENFILES; i >= 1; --i)
        if (FileTable[i].com == 0) return i;
    return -1;
}

static FILE *pm_rt_fopen(const char *path, const char *mode)
{
    pm_rt_file *f = NULL;
    const char *q;
    bool plus = false, read = false, write = false, append = false;
    bool must_exist = false, truncate = false, rewind_open = false;
    int fa, fnbr, i;
    if (!pm_rt_running || !path || !mode ||
        (*mode != 'r' && *mode != 'w' && *mode != 'a')) return NULL;
    for (q = mode + 1; *q; ++q) {
        if (*q == '+') plus = true;
        else if (*q != 'b' && *q != 't') return NULL;
    }
    if (*mode == 'r') {
        fa = plus ? (FA_WRITE | FA_OPEN_APPEND | FA_READ) : FA_READ;
        must_exist = read = true; write = plus; rewind_open = plus;
    } else if (*mode == 'w') {
        fa = plus ? (FA_WRITE | FA_OPEN_APPEND | FA_READ) : (FA_WRITE | FA_CREATE_ALWAYS);
        write = true; read = plus; truncate = rewind_open = plus;
    } else {
        fa = plus ? (FA_WRITE | FA_OPEN_APPEND | FA_READ) : (FA_WRITE | FA_OPEN_APPEND);
        write = append = true; read = plus; rewind_open = plus;
    }
    if (!pm_rt_files) {
        pm_rt_files = pm_runtime_malloc(sizeof(*pm_rt_files) * PM_RT_FILES);
        if (!pm_rt_files) return NULL;
        memset(pm_rt_files, 0, sizeof(*pm_rt_files) * PM_RT_FILES);
    }
    for (i = 0; i < PM_RT_FILES; ++i)
        if (pm_rt_files[i].kind == PM_RT_FREE) { f = &pm_rt_files[i]; break; }
    if (!f || !*path || (fnbr = pm_rt_free_fnbr()) < 0) return NULL;
    memset(f, 0, sizeof(*f));
    if (truncate) {
        if (!pm_rt_file_open(path, fnbr, FA_WRITE | FA_CREATE_ALWAYS, false)) return NULL;
        ForceFileClose(fnbr);
    }
    if (!pm_rt_file_open(path, fnbr, fa, must_exist)) return NULL;
    f->kind = PM_RT_DISK;
    f->fnbr = fnbr;
    f->read = read;
    f->write = write;
    f->append = append;
    if (rewind_open && !append) {
        pm_rt_trap t;
        volatile bool ok = true;
        if (PM_RT_TRY(t)) positionfile(fnbr, 0, true);
        else ok = false;
        pm_rt_trap_restore(&t);
        if (!ok) { pm_rt_close_file(f); return NULL; }
    }
    return (FILE *)f;
}
static int pm_rt_fclose(FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    if (!f) return EOF;
    if (f->kind != PM_RT_DISK) return 0;
    return pm_rt_close_file(f) ? EOF : 0;
}
static int pm_rt_console_getc(void);
static size_t pm_rt_fread(void *ptr, size_t size, size_t count, FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    size_t remaining, got_total = 0;
    pm_rt_trap t;
    if (!f || !size || !count || count > SIZE_MAX / size) return 0;
    remaining = size * count;
    if (f->kind == PM_RT_CONIN) {
        unsigned char *dst = ptr;
        while (got_total < remaining) {
            int c = pm_rt_console_getc();
            if (c == EOF) break;
            dst[got_total++] = (unsigned char)c;
            if (c == '\n') break;
        }
        return got_total / size;
    }
    if (!f->read) return 0;
    while (remaining) {
        unsigned got = 0;
        int chunk = remaining > INT_MAX ? INT_MAX : (int)remaining;
        volatile bool ok = true;
        if (PM_RT_TRY(t)) {
            if (FileGetData(f->fnbr, (unsigned char *)ptr + got_total,
                            chunk, &got) != 0) ok = false;
        } else ok = false;
        pm_rt_trap_restore(&t);
        if (!ok) { f->error = true; break; }
        got_total += got;
        remaining -= got;
        if (!got) { f->eof = true; break; }
    }
    return got_total / size;
}
static size_t pm_rt_fwrite(const void *ptr, size_t size, size_t count, FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    size_t want, total = 0;
    pm_rt_trap t;
    if (!f || !size || !count || count > SIZE_MAX / size) return 0;
    want = size * count;
    if (f->kind == PM_RT_CONOUT) {
        const char *s = ptr;
        for (size_t i = 0; i < want; ++i) {
            char b[3] = {0,0,0};
            if (s[i] == '\n') { b[0] = '\r'; b[1] = '\n'; }
            else b[0] = s[i];
            MMPrintString(b);
        }
        return count;
    }
    if (f->kind != PM_RT_DISK || !f->write) return 0;
    while (total < want) {
        size_t n = want - total;
        volatile bool ok = true;
        if (n > 4096) n = 4096;
        if (PM_RT_TRY(t)) {
            if (f->append) positionfile(f->fnbr, (int)pm_rt_size(f), true);
            FilePutData((char *)ptr + total, f->fnbr, (int)n);
        } else ok = false;
        pm_rt_trap_restore(&t);
        if (!ok) { f->error = true; break; }
        total += n;
    }
    return total / size;
}
static int pm_rt_fseek(FILE *file, long off, int whence)
{
    pm_rt_file *f = pm_rt_file_of(file);
    pm_rt_trap t;
    volatile long pos = -1;
    if (!f || f->kind != PM_RT_DISK) return -1;
    if (PM_RT_TRY(t)) {
        if (whence == SEEK_SET) pos = off;
        else if (whence == SEEK_CUR) pos = filegetpos(f->fnbr) + off;
        else if (whence == SEEK_END) pos = pm_rt_size(f) + off;
        if (pos >= 0) positionfile(f->fnbr, (int)pos, true);
    } else pos = -1;
    pm_rt_trap_restore(&t);
    if (pos < 0) { f->error = true; return -1; }
    f->eof = false;
    return 0;
}
static long pm_rt_ftell(FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    pm_rt_trap t;
    volatile long pos = -1;
    if (!f || f->kind != PM_RT_DISK) return -1;
    if (PM_RT_TRY(t)) pos = filegetpos(f->fnbr);
    pm_rt_trap_restore(&t);
    return pos;
}
static int pm_rt_feof(FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    return f ? f->eof : 0;
}
static int pm_rt_ferror(FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    return f ? f->error : 1;
}
static int pm_rt_fflush(FILE *file)
{
    /* NULL flushes all streams as in ISO C; writes are synchronous. */
    return file == NULL || pm_rt_file_of(file) ? 0 : EOF;
}
static void pm_rt_rewind(FILE *file)
{
    (void)pm_rt_fseek(file, 0, SEEK_SET);
}

/* Read an editable line from the PicoMite keyboard and retain its characters
 * across getchar/fgets calls. Allocated from the RUN heap on first use. */
static int pm_rt_console_getc(void)
{
    if (!pm_rt_input_line) {
        pm_rt_input_line = pm_runtime_malloc(STRINGSIZE);
        if (!pm_rt_input_line) return EOF;
    }
    if (pm_rt_input_pos >= pm_rt_input_len) {
        pm_rt_input_pos = pm_rt_input_len = 0;
        for (;;) {
            int c;
            if (MMAbort) CheckAbort();
            c = MMInkey();
            if (c == -1) { routinechecks(); continue; }
            if (c == '\r' || c == '\n') {
                MMPrintString("\r\n");
                pm_rt_input_line[pm_rt_input_len++] = '\n';
                break;
            }
            if ((c == '\b' || c == 127) && pm_rt_input_len) {
                --pm_rt_input_len;
                MMPrintString("\b \b");
            } else if (c >= ' ' && c < 127 && pm_rt_input_len < STRINGSIZE - 1) {
                char b[2] = {(char)c, 0};
                pm_rt_input_line[pm_rt_input_len++] = (char)c;
                MMPrintString(b);
            }
        }
    }
    return (unsigned char)pm_rt_input_line[pm_rt_input_pos++];
}
static int pm_rt_fgetc(FILE *file)
{
    pm_rt_file *f = pm_rt_file_of(file);
    unsigned char ch = 0;
    if (!f) return EOF;
    if (f->kind == PM_RT_CONIN) return pm_rt_console_getc();
    return pm_rt_fread(&ch, 1, 1, file) == 1 ? ch : EOF;
}
static int pm_rt_getchar(void) { return pm_rt_console_getc(); }
static char *pm_rt_fgets(char *dst, int n, FILE *file)
{
    int i = 0, c;
    if (!dst || n <= 0) return NULL;
    while (i < n - 1 && (c = pm_rt_fgetc(file)) != EOF) {
        dst[i++] = (char)c;
        if (c == '\n') break;
    }
    if (!i) return NULL;
    dst[i] = 0;
    return dst;
}
/* Stream-aware formatting is separate from the console-only printf:
 * TinyCC programs use a FILE * exported by this resolver, never its
 * compiler-side private FILE descriptors. */
static int pm_rt_vfprintf(FILE *file, const char *fmt, va_list ap)
{
    va_list copy;
    int n;
    char *buf;
    size_t written;
    if (!pm_rt_file_of(file)) return -1;
    va_copy(copy, ap);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0 || n == INT_MAX) return -1;
    buf = GetMemoryNull(n + 1);
    if (!buf) return -1;
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    written = pm_rt_fwrite(buf, 1, (size_t)n, file);
    FreeMemory(buf);
    return written == (size_t)n ? n : -1;
}
static int pm_rt_fprintf(FILE *file, const char *fmt, ...)
{
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = pm_rt_vfprintf(file, fmt, ap);
    va_end(ap);
    return result;
}
static int pm_rt_fputc(int c, FILE *file)
{
    unsigned char b = (unsigned char)c;
    return pm_rt_fwrite(&b, 1, 1, file) ? b : EOF;
}
static int pm_rt_fputs(const char *s, FILE *file)
{
    return pm_rt_fwrite(s, 1, strlen(s), file) == strlen(s) ? 0 : EOF;
}

/* Separate table: the compiler can keep using its private ROM/source-file
 * handles, while compiled programs link these common addresses. */
typedef struct { const char *name; const void *addr; } pm_stdio_symbol;
#define PM_STDIO_AS(n,f) {#n, (const void *)&f},
static const pm_stdio_symbol pm_stdio_symbols[] = {
    PM_STDIO_AS(fprintf, pm_rt_fprintf)
    PM_STDIO_AS(vfprintf, pm_rt_vfprintf)
    PM_STDIO_AS(fopen, pm_rt_fopen)
    PM_STDIO_AS(fclose, pm_rt_fclose)
    PM_STDIO_AS(fread, pm_rt_fread)
    PM_STDIO_AS(fwrite, pm_rt_fwrite)
    PM_STDIO_AS(fseek, pm_rt_fseek)
    PM_STDIO_AS(ftell, pm_rt_ftell)
    PM_STDIO_AS(rewind, pm_rt_rewind)
    PM_STDIO_AS(feof, pm_rt_feof)
    PM_STDIO_AS(ferror, pm_rt_ferror)
    PM_STDIO_AS(fflush, pm_rt_fflush)
    PM_STDIO_AS(fgetc, pm_rt_fgetc)
    PM_STDIO_AS(getc, pm_rt_fgetc)
    PM_STDIO_AS(getchar, pm_rt_getchar)
    PM_STDIO_AS(fgets, pm_rt_fgets)
    PM_STDIO_AS(fputc, pm_rt_fputc)
    PM_STDIO_AS(putc, pm_rt_fputc)
    PM_STDIO_AS(fputs, pm_rt_fputs)
    PM_STDIO_AS(__tcc_pm_stdin, pm_rt_stdin)
    PM_STDIO_AS(__tcc_pm_stdout, pm_rt_stdout)
    PM_STDIO_AS(__tcc_pm_stderr, pm_rt_stderr)
};
const void *pm_runtime_stdio_resolve(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_stdio_symbols) / sizeof(pm_stdio_symbols[0]); ++i)
        if (!strcmp(name, pm_stdio_symbols[i].name)) return pm_stdio_symbols[i].addr;
    return NULL;
}
unsigned pm_runtime_stdio_firmware_id(void)
{
    uint32_t h = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_stdio_symbols) / sizeof(pm_stdio_symbols[0]); ++i) {
        uint32_t v = (uint32_t)(uintptr_t)pm_stdio_symbols[i].addr;
        for (j = 0; j < 4; ++j, v >>= 8) h = (h ^ (v & 255u)) * 16777619u;
    }
    return h ? h : 1u;
}
