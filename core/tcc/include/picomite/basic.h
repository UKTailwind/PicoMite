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
/* Native BASIC callback arguments/results: values, not BASIC expressions.
   No BYREF, arrays, or structures in API 10. The string pointer in an input
   value is a NUL-terminated C string. */
#ifndef PM_BASIC_VALUE_DEFINED
#define PM_BASIC_VALUE_DEFINED
#define PM_BASIC_INTEGER 1
#define PM_BASIC_FLOAT   2
#define PM_BASIC_STRING  3

typedef struct {
    int type;
    long long integer;
    double number;
    const char *string;
} pm_basic_value;
#endif

/* RUN .c/.elf: LIBRARY only.  CSUB: caller's SUB/FUNCTIONs and LIBRARY.
   Errors use the normal PicoMite mechanism and may be caught by pm_try().
   For a STRING result supply string_buf/string_capacity; result->string
   points into string_buf, not into interpreter temporary memory. */
void pm_basic_sub(const char *name, const pm_basic_value *args, unsigned count);
void pm_basic_function(const char *name, const pm_basic_value *args,
                       unsigned count, pm_basic_value *result,
                       char *string_buf, unsigned string_capacity);

#endif
