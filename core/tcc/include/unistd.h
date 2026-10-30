/* PicoMite TinyCC: <unistd.h> - the directory functions, as MMBasic's
   commands: -1 (NULL) if they fail, never an MMBasic error (pm_errmsg() in
   <picomite/basic.h> says why). */
#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

int chdir(const char *dir);             /* CHDIR */
int rmdir(const char *dir);             /* RMDIR */
int unlink(const char *name);           /* KILL */
char *getcwd(char *buf, size_t size);   /* CWD$, e.g. "A:/games" */

#endif /* _UNISTD_H */
