/* PicoMite TinyCC: <picomite/files.h> - MMBasic's file commands and
   functions.  (API version 1.)  Each is the command or function of its
   name, with the same checks and the same MMBasic errors (pm_try in
   <picomite/basic.h> is ON ERROR SKIP).  The C library's remove, rename,
   mkdir, stat, opendir... (stdio.h, unistd.h, sys/stat.h, dirent.h) are
   built on them, with C's meaning: -1 for a failure. */
#ifndef _PICOMITE_FILES_H
#define _PICOMITE_FILES_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_kill(const char *file);                   /* KILL file$ */
void pm_rename(const char *from, const char *to); /* RENAME from$ AS to$ */
void pm_copy(const char *from, const char *to);   /* COPY from$ TO to$ */
void pm_mkdir(const char *dir);                   /* MKDIR dir$ */
void pm_rmdir(const char *dir);                   /* RMDIR dir$ */
void pm_chdir(const char *dir);                   /* CHDIR dir$ */
void pm_drive(const char *drive);                 /* DRIVE "A:" / "B:" ... */

char *pm_cwd(char *buf, int size);                /* CWD$ (returns buf) */
char *pm_drive_name(char *buf, int size);         /* MM.INFO$(DRIVE) */

/* DIR$(pattern, what): the first name, "" if none; pattern NULL is DIR$()
   for the next one.  what: PM_DIR_FILE (or PM_DEFAULT), PM_DIR_DIR,
   PM_DIR_ALL. */
#define PM_DIR_FILE 0
#define PM_DIR_DIR 1
#define PM_DIR_ALL 2
char *pm_dir(const char *pattern, int what, char *buf, int size);

int pm_exists_file(const char *file);           /* MM.INFO(EXISTS FILE f$): 1, 0, -1 a directory */
int pm_exists_dir(const char *dir);             /* MM.INFO(EXISTS DIR d$) */
long long pm_filesize(const char *file);        /* MM.INFO(FILESIZE f$) */
char *pm_modified(const char *file, char *buf, int size); /* MM.INFO(MODIFIED f$) */
long long pm_free_space(void);                  /* MM.INFO(FREE SPACE) */
long long pm_disk_size(void);                   /* MM.INFO(DISK SIZE) */

#endif /* _PICOMITE_FILES_H */
