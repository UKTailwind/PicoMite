/* PicoMite TinyCC: <dirent.h> - a directory's entries, over MMBasic's
   DIR$(path + "/*", ALL).  DIR$ keeps one listing at a time, so one DIR can
   be open at a time (opendir returns NULL while another is).  Not in a C
   CSUB. */
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
