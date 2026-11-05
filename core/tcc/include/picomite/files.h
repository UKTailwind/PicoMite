/* TinyCC: <picomite/files.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_FILES_H
#define _PICOMITE_FILES_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

void pm_kill(const char *file);
void pm_rename(const char *from, const char *to);
void pm_copy(const char *from, const char *to);
void pm_mkdir(const char *dir);
void pm_rmdir(const char *dir);
void pm_chdir(const char *dir);
void pm_drive(const char *drive);

char *pm_cwd(char *buf, int size);
char *pm_drive_name(char *buf, int size);

#define PM_DIR_FILE 0
#define PM_DIR_DIR 1
#define PM_DIR_ALL 2
char *pm_dir(const char *pattern, int what, char *buf, int size);

int pm_exists_file(const char *file);
int pm_exists_dir(const char *dir);
long long pm_filesize(const char *file);
char *pm_modified(const char *file, char *buf, int size);
long long pm_free_space(void);
long long pm_disk_size(void);

#endif
