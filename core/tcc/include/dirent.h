/* PicoMite TinyCC: <dirent.h> - a directory's entries, from MMBasic's
   DIR$(path + "/*", ALL).  opendir reads the whole listing at once, so any
   number of DIRs can be open, and DIR$ or pm_dir used meanwhile does not
   disturb them; a file made or removed after opendir is not seen.  Not in
   a C CSUB. */
#ifndef _DIRENT_H
#define _DIRENT_H

#define DT_DIR 4
#define DT_REG 8

struct dirent
{
    unsigned char d_type; /* DT_DIR or DT_REG */
    char d_name[256];
};

typedef struct __tcc_pm_dir DIR;

DIR *opendir(const char *dir);
struct dirent *readdir(DIR *d); /* NULL at the end */
int closedir(DIR *d);

#endif /* _DIRENT_H */
