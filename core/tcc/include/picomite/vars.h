/* TinyCC: <picomite/vars.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_VARS_H
#define _PICOMITE_VARS_H

#define PM_VAR_FLOAT 1
#define PM_VAR_STRING 2
#define PM_VAR_INT 4
#define PM_MAXDIM 6

typedef struct
{
    int type;
    int size;
    int constant;
    int ndims;
    int dims[PM_MAXDIM];
    int elements;
    int element_size;
} pm_varinfo;

void *pm_var(const char *ref, pm_varinfo *info);

void *pm_dim(const char *decl, pm_varinfo *info);

void pm_erase(const char *names);

char *pm_str_get(const void *mmstr, char *buf, int size);
void pm_str_set(void *mmstr, int capacity, const char *s);

#endif
