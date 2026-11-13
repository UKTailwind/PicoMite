/* PicoMite TinyCC: <sys/stat.h> - stat() and mkdir() on MMBasic's file
   system: -1 if they fail, never an MMBasic error.  st_mode tells a file
   from a directory, st_size is MM.INFO(FILESIZE), st_mtime MM.INFO(MODIFIED)
   as seconds from 1970 in the clock's own time (as time() gives it). */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <time.h>

struct stat
{
    unsigned st_mode;
    long st_size;
    time_t st_mtime;
};

#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

int stat(const char *name, struct stat *st);
int mkdir(const char *dir, unsigned mode); /* MKDIR (mode is ignored) */

#endif /* _SYS_STAT_H */
