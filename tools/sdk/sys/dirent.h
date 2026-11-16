/* sys/dirent.h: directories for a loadable program (tools/sdk), which newlib's <dirent.h> includes and newlib does
   not have for this target (its own says #error). MIT License, as pmimg.h.

   opendir, readdir and closedir are the services' dir_open, dir_read and dir_close (syscalls.c): up to four open at
   once, the names as the card has them, with "." first and then, below a drive's root, "..", as Linux has them.
   d_type is DT_DIR or DT_REG. */
#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H
#ifdef __cplusplus
extern "C" {
#endif

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

struct dirent {
    unsigned long d_ino;                        /* (always 0) */
    unsigned char d_type;
    char d_name[256];
};

typedef struct {
    int handle;                                 /* the service's, 0-3 */
    int dot;                                    /* 1 while "." is still to be given */
    int dotdot;                                 /* 1 while ".." is still to be given */
    struct dirent entry;
} DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

#ifdef __cplusplus
}
#endif
#endif
