/* TinyCC: <picomite/basic.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_BASIC_H
#define _PICOMITE_BASIC_H

void pm_execute(const char *cmd);

long long pm_eval_int(const char *expr);
double pm_eval_float(const char *expr);
char *pm_eval_str(const char *expr, char *buf, int size);

int pm_try(void (*fn)(void *arg), void *arg);

int pm_execute_try(const char *cmd);
int pm_eval_int_try(const char *expr, long long *value);
int pm_eval_float_try(const char *expr, double *value);
int pm_eval_str_try(const char *expr, char *buf, int size);

int pm_errno(void);
const char *pm_errmsg(void);
void pm_error_clear(void);

void pm_settick(int period, void (*fn)(void), int nbr);

#endif
