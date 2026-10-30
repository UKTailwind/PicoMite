/* PicoMite TinyCC: <picomite/vars.h> - BASIC variables and arrays.
   (API version 1.)

   A C program can make BASIC variables with DIM and reach any variable by
   its BASIC name - those it made, and in a C CSUB those of the BASIC
   program that called it.  The address is that of the variable's data in
   the MMBasic heap, the same as VARADDR gives: it does not move while the
   variable exists, so it can be kept and used in a loop, and a firmware
   command given the variable by name (MATH, WEB TCP CLIENT STREAM, ...)
   works on the same memory.  The data are MMBasic's:
     PM_VAR_INT     long long, per element
     PM_VAR_FLOAT   double, per element
     PM_VAR_STRING  a length byte and the characters (pm_str_get, pm_str_set),
                    size + 1 bytes per element
   Arrays are stored with the first index varying fastest.  The variables
   a C program makes stay after it ends, as a BASIC program's do, until the
   next RUN.  Errors are MMBasic's ("... is not declared", ...) - pm_try in
   <picomite/basic.h> is ON ERROR SKIP. */
#ifndef _PICOMITE_VARS_H
#define _PICOMITE_VARS_H

#define PM_VAR_FLOAT 1
#define PM_VAR_STRING 2
#define PM_VAR_INT 4
#define PM_MAXDIM 6

typedef struct
{
    int type;          /* PM_VAR_INT, PM_VAR_FLOAT or PM_VAR_STRING */
    int size;          /* a string's capacity (LENGTH, 255 by default); 0 for a number */
    int constant;      /* a CONST: not to be written */
    int ndims;         /* 0 for a variable that is not an array */
    int dims[PM_MAXDIM]; /* the elements of each dimension (OPTION BASE included) */
    int elements;      /* the elements in all (1 for a variable that is not an array) */
    int element_size;  /* bytes per element: 8, or size + 1 for a string */
} pm_varinfo;

/* The address of a variable or array element, by its BASIC reference:
   "n%", "x!", "name$", "grid%(3, 4)", or "buf%()" for a whole array (its
   first element).  info (or NULL) is filled with what it is.  As VARADDR: an
   error if the variable does not exist. */
void *pm_var(const char *ref, pm_varinfo *info);

/* DIM decl - e.g. "buf%(511)", "INTEGER n(10)", "s$ LENGTH 16",
   "m!(2, 3)" - and the address of the variable it declares first (an
   array: its first element), as pm_var gives it. */
void *pm_dim(const char *decl, pm_varinfo *info);

void pm_erase(const char *names); /* ERASE name [, name ...] */

/* A BASIC string's text as a C string (buf, at most size - 1 characters),
   and a C string into a BASIC string of the given capacity (pm_varinfo
   size): "String too long" if it does not fit, as an assignment. */
char *pm_str_get(const void *mmstr, char *buf, int size);
void pm_str_set(void *mmstr, int capacity, const char *s);

#endif /* _PICOMITE_VARS_H */
