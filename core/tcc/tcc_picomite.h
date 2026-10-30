/*
 * TinyCC <-> PicoMite adapter interface.
 *
 * TinyCC is compiled as its own library (PicoMiteTinyCC) without any
 * PicoMite header, so everything it needs from the host - memory, files,
 * console, time, the run-time symbol table for "-run" - is declared here and
 * implemented in tcc_picomite.c, which is compiled with the firmware and sees
 * the whole of PicoMite.
 *
 * When included from the TinyCC sources (TCC_PM_REDIRECT_LIBC defined by
 * tcc.h) the libc calls TinyCC makes are redirected to the adapter with
 * function-like macros, so a struct member or a function's address with the
 * same name is left alone.
 */
#ifndef TCC_PICOMITE_H
#define TCC_PICOMITE_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The built-in headers (core/tcc/include, embedded in the firmware at build
   time) are found under this directory, which is TinyCC's system include
   path.  "rom:" is no PicoMite drive, so it cannot hide a real file. */
#define TCC_PM_ROM_INCLUDE "rom:/include"

typedef struct tcc_pm_rom_file
{
    const char *name;          /* file name inside TCC_PM_ROM_INCLUDE */
    const unsigned char *data; /* contents */
    unsigned int size;         /* bytes */
} tcc_pm_rom_file;

extern const tcc_pm_rom_file tcc_pm_rom_files[]; /* generated: tcc_rom_headers.c */
extern const unsigned int tcc_pm_rom_count;

/* Memory: a private arena carved from the PicoMite heap (SRAM first, then
   PSRAM).  NULL on exhaustion, never an MMBasic error. Released as a whole
   when the run ends. */
void *tcc_pm_malloc(size_t size);
void *tcc_pm_realloc(void *ptr, size_t size);
void tcc_pm_free(void *ptr);

/* Descriptor-style reads (sources, headers, objects) */
int tcc_pm_open(const char *path, int flags, ...);
int tcc_pm_read(int fd, void *buf, unsigned int count);
long tcc_pm_lseek(int fd, long offset, int whence);
int tcc_pm_close(int fd);
int tcc_pm_unlink(const char *path);

/* stdio: stdout/stderr (newlib's, or the run-time ones exported to compiled
   programs) go to the MMBasic console; any other FILE * is an adapter file. */
FILE *tcc_pm_fopen(const char *path, const char *mode);
int tcc_pm_fclose(FILE *f);
size_t tcc_pm_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
size_t tcc_pm_fread(void *ptr, size_t size, size_t nmemb, FILE *f);
int tcc_pm_fseek(FILE *f, long offset, int whence);
long tcc_pm_ftell(FILE *f);
int tcc_pm_fputc(int c, FILE *f);
int tcc_pm_fputs(const char *s, FILE *f);
int tcc_pm_fflush(FILE *f);
int tcc_pm_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int tcc_pm_fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int tcc_pm_vfprintf(FILE *f, const char *fmt, va_list ap);
int tcc_pm_puts(const char *s);
int tcc_pm_putchar(int c);
char *tcc_pm_getenv(const char *name);

/* exit() of the compiler or of a compiled program */
void tcc_pm_exit(int code) __attribute__((noreturn));

/* Time */
unsigned int tcc_pm_clock_ms(void);
time_t tcc_pm_time(void); /* (time_t)-1 when unavailable */

/* -run support */
void *tcc_pm_resolve_symbol(const char *name);
int tcc_pm_call_main(int (*prog_main)(int, char **), int argc, char **argv);
void tcc_pm_code_ready(void *ptr, unsigned long length);

/* The TinyCC command line (core/tcc/tcc.c) */
int pico_tcc_main(int argc, char **argv);

/* (libtcc.c) Drop TinyCC's pointers into an abandoned arena: a run that was
   left by a longjmp past TinyCC's own cleanup. */
void tcc_pm_forget_globals(void);

/* Entry points for the firmware:
   RUN "file.c" [, "args"] (is_c) and RUN "file" of an ELF made by TCC */
void tcc_picomite_run(const char *filename, const char *args, bool is_c);
bool tcc_picomite_is_elf(const char *filename);
/* at the MMBasic prompt after a longjmp(mark): clean up a run left there */
void tcc_picomite_abandoned(void);
/* the TCC command (AllCommands.h) */
void cmd_tcc(void);

#ifdef TCC_PM_REDIRECT_LIBC
#undef open
#undef read
#undef lseek
#undef close
#undef unlink
#undef remove
#undef fopen
#undef fclose
#undef fwrite
#undef fread
#undef fseek
#undef ftell
#undef fputc
#undef fputs
#undef fflush
#undef printf
#undef fprintf
#undef vfprintf
#undef puts
#undef putchar
#undef getenv
#undef exit
#define open(...) tcc_pm_open(__VA_ARGS__)
#define read(fd, buf, n) tcc_pm_read(fd, buf, n)
#define lseek(fd, off, wh) tcc_pm_lseek(fd, off, wh)
#define close(fd) tcc_pm_close(fd)
#define unlink(p) tcc_pm_unlink(p)
#define remove(p) tcc_pm_unlink(p)
#define fopen(p, m) tcc_pm_fopen(p, m)
#define fclose(f) tcc_pm_fclose(f)
#define fwrite(p, s, n, f) tcc_pm_fwrite(p, s, n, f)
#define fread(p, s, n, f) tcc_pm_fread(p, s, n, f)
#define fseek(f, o, w) tcc_pm_fseek(f, o, w)
#define ftell(f) tcc_pm_ftell(f)
#define fputc(c, f) tcc_pm_fputc(c, f)
#define fputs(s, f) tcc_pm_fputs(s, f)
#define fflush(f) tcc_pm_fflush(f)
#define printf(...) tcc_pm_printf(__VA_ARGS__)
#define fprintf(...) tcc_pm_fprintf(__VA_ARGS__)
#define vfprintf(f, fmt, ap) tcc_pm_vfprintf(f, fmt, ap)
#define puts(s) tcc_pm_puts(s)
#define putchar(c) tcc_pm_putchar(c)
#define getenv(n) tcc_pm_getenv(n)
#define exit(c) tcc_pm_exit(c)
#endif /* TCC_PM_REDIRECT_LIBC */

#ifdef __cplusplus
}
#endif

#endif /* TCC_PICOMITE_H */
