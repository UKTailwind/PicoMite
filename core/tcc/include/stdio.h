/* PicoMite TinyCC: <stdio.h>.  The console is the MMBasic console; files are
   opened with MMBasic's file names ("B:/dir/file.txt", relative to the current
   directory).  fopen() modes: "r", "w" (+ "b"); no append or update. */
#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct __tcc_pm_file FILE;
typedef long fpos_t;

extern FILE *__tcc_pm_stdin, *__tcc_pm_stdout, *__tcc_pm_stderr;
#define stdin __tcc_pm_stdin
#define stdout __tcc_pm_stdout
#define stderr __tcc_pm_stderr

#define EOF (-1)
#define BUFSIZ 512
#define FILENAME_MAX 128
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *s, const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsprintf(char *s, const char *fmt, va_list ap);
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);

int puts(const char *s);
int putchar(int c);
int fputs(const char *s, FILE *f);
int fputc(int c, FILE *f);
int putc(int c, FILE *f);
int getchar(void);
int fgetc(FILE *f);
int getc(FILE *f);
char *fgets(char *s, int n, FILE *f);

FILE *fopen(const char *name, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t size, size_t n, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
void rewind(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
int fflush(FILE *f);

#endif /* _STDIO_H */
