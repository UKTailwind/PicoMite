/*
 * pmuser.h: USER modules for the PicoMite (MMBasic V8). A USER module is the user's own C, built with the SDK
 * (tools/sdk: build.py --user) and loaded by USER LOAD; MMBasic then hands it each USER command and each USER( function.
 * The module reads its part of the statement itself, with the interpreter's own routines in the table below
 * (cmm2_start->basic), and follows MMBasic's rules as a built-in command does. The firmware (misc/User.c) and the SDK
 * are both built from this file; tools/sdk/examples/user/proforma.c is a module to start from. The names are the
 * Colour Maximite 2's (its Inc/cmm2user.h), so that a module's source builds for either.
 *
 * MIT License. Copyright (c) 2026 Peter Mather.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this file, to deal in it without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of it, and to permit persons to whom it is furnished to do so, subject to the following condition:
 * the above copyright notice and this permission notice shall be included in all copies or substantial portions.
 * THE FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE FILE OR THE USE OR OTHER DEALINGS IN IT.
 */
#ifndef PMUSER_H
#define PMUSER_H
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "pmimg.h"

#define CMM2USER_MAGIC       0x52455355u       /* "USER": after the module's cmm2mod_head */
#define CMM2USER_VERSION     1                 /* this file's: the table below, and the module's table */

/* MMBasic's types: a value's, a variable's */
#define CMM2_T_NBR           0x01              /* a float: a double */
#define CMM2_T_STR           0x02              /* a string, MMBasic's: a length byte (0-255), then the characters,
                                                  with no zero at the end */
#define CMM2_T_INT           0x04              /* an integer: an int64_t */
#define CMM2_T_STRUCT        0x80              /* a structure (TYPE ... END TYPE): its bytes as the TYPE lays them out */
#define CMM2_STRINGSIZE      256               /* a string's room: its length byte and 255 characters */

/* a value: the one evaluate() gives, and the one a USER( function gives back */
typedef struct {
    int      type;                             /* CMM2_T_NBR, CMM2_T_INT or CMM2_T_STR */
    double   f;                                /* CMM2_T_NBR's */
    int64_t  i;                                /* CMM2_T_INT's */
    char    *s;                                /* CMM2_T_STR's, an MMBasic string. For a USER( function's result the
                                                  firmware's room of CMM2_STRINGSIZE bytes, to be written there */
} cmm2_value;

/* a variable, as var() found it */
typedef struct {
    int      type;                             /* CMM2_T_NBR, CMM2_T_INT, CMM2_T_STR or CMM2_T_STRUCT */
    int      size;                             /* a string's longest length (each takes size + 1 bytes: its length
                                                  byte first); a structure's bytes; 8 for a number */
    int      constant;                         /* 1: a CONST (var() with CMM2_VAR_WRITE refuses one) */
    int      dims;                             /* 0: one variable, element or member; 1-5: a whole array, named with
                                                  empty brackets (a()), whose elements follow each other, the first
                                                  dimension's changing fastest */
    int      bound[5];                         /* each dimension's elements, OPTION BASE applied (DIM a(10) has 11
                                                  under OPTION BASE 0) */
    int      elements;                         /* all its elements: 1 unless dims */
} cmm2_var;

#define CMM2_VAR_READ        0                 /* var(): the variable must be there already (else MMBasic's error) */
#define CMM2_VAR_WRITE       1                 /* var(): made if it is not there, as an assignment makes it (OPTION
                                                  EXPLICIT's error); a CONST is an error */

/* The interpreter's routines. Each works as it does for MMBasic's own commands: an error is MMBasic's error, which ends
   the statement (or goes to the program's ON ERROR) and does not return; a string one gives is in temporary memory,
   freed when the statement ends.

   The text a module is given is the statement as MMBasic stores it: the functions and the operators (=, +, AND, NOT,
   TO, STEP, THEN, ELSE...) are one byte each, 0x80 or more, which token() names; anything else is the text as typed,
   less the spaces MMBasic takes out, with the names of variables spelt out. A keyword of the module's own (LED, ON,
   OFF) is text, so checkstring() finds it. */
struct cmm2_basic {
    uint32_t version;                          /* CMM2USER_VERSION */
    uint32_t size;                             /* sizeof(struct cmm2_basic) in the firmware */
    /* reading the statement */
    char    *(*checkstring)(char *p, const char *word);   /* p, after its spaces, starts with word (in any case, and not
                                                  as the start of a longer name): the text after it, else NULL */
    void     (*getargs)(char **p, int max, const char *delims, char **argv, int *argc);   /* the text at *p split at
                                                  any of delims (outside brackets and strings): argv[0], argv[2] ...
                                                  the items (an empty one ""), argv[1], argv[3] ... the delimiters
                                                  between them; argc counts both, so 2n - 1 for n items. More than
                                                  max is MMBasic's "Syntax" error */
    int      (*token)(const char *name);       /* the byte of a function or an operator ("=", "TO", "SIN(": any
                                                  case), or 0 */
    char    *(*close_bracket)(char *p);        /* p just after a '(': its ')' */
    /* values */
    char    *(*evaluate)(char *p, cmm2_value *v);   /* the expression at p: its value and type, and where it ends */
    int64_t  (*get_integer)(char *p);          /* the expression at p as an integer (a float rounded) */
    int64_t  (*get_int)(char *p, int64_t min, int64_t max);   /* the same, MMBasic's error outside min to max */
    double   (*get_number)(char *p);           /* as a float */
    char    *(*get_string)(char *p);           /* as an MMBasic string */
    char    *(*get_cstring)(char *p);          /* as a C string */
    /* variables */
    void    *(*var)(char *p, int mode, cmm2_var *v);   /* the variable p names (x, n%, s$, a(3), a(), rec.field): its
                                                  data, and v filled in; mode CMM2_VAR_READ or CMM2_VAR_WRITE */
    void     (*assign)(char *p, const cmm2_value *v);  /* the variable p names given v as an assignment gives it (a
                                                  float to an integer rounded; a string to a number, or one too
                                                  long, an error) */
    /* memory, errors and Ctrl-C */
    void    *(*temp)(int bytes);               /* zeroed, and freed when the statement ends */
    void     (*error)(const char *msg, ...);   /* MMBasic's error: in msg, $ is the next argument (a char *), % the
                                                  next (an int) */
    void     (*check_abort)(void);             /* Ctrl-C pressed: MMBasic's break (call it in a long loop) */
    /* output, and conversions */
    void     (*print)(const char *s);          /* on the console and the screen, as PRINT */
    void     (*int_to_str)(char *out, int64_t n, int base);
    void     (*float_to_str)(char *out, double f, int m, int n, char pad);   /* m digits before the point (0: as
                                                  many as it takes), n after it (999: as PRINT has it), pad before */
    char    *(*c_to_m)(char *s);               /* a C string made an MMBasic string, in place */
    char    *(*m_to_c)(char *s);               /* and back */
    /* the interpreter's state */
    int      (*in_program)(void);              /* 1 in a program, 0 at the command prompt */
    int      (*option_base)(void);             /* OPTION BASE: 0 or 1 */
    /* formatting */
    int      (*vsnprintf)(char *out, size_t n, const char *fmt, va_list ap);   /* the firmware's own, so a module need
                                                  not carry one: build.py --user gives a module's snprintf, sprintf and
                                                  printf through it. %d, %s, %x and %f are right; %e and %g are not
                                                  (1e6 comes out as 10.000000e+05, and %g keeps trailing zeros: 0.5 as
                                                  0.500000): format those with float_to_str */
};

/* A USER module's table, the one module_main() returns: a cmm2mod_head first, as for any module. Its functions are
   called on MMBasic's own stack (shared with the interpreter: keep big buffers off it, in temp() or malloc's memory), so
   they can call the routines above, and a USER( in a USER command's arguments calls the module again. A module calls
   the services (cmm2->...) as a program does, but not leave() (an error: it has no program to leave), and it should
   not change the screen's mode */
typedef struct {
    cmm2mod_head head;                         /* magic CMM2MOD_MAGIC; version CMM2USER_VERSION; forget, called before
                                                  the module goes (USER CLOSE, another USER LOAD, RUN, NEW, CHAIN): it
                                                  gives back what it holds */
    uint32_t     magic;                        /* CMM2USER_MAGIC */
    void       (*command)(char *line);         /* USER ...: line is the statement after USER (NULL: no command) */
    void       (*function)(char *args, cmm2_value *result);   /* USER(...): args the text inside the brackets; set
                                                  result->type and f, i or the string at result->s (NULL: none) */
} cmm2user_head;

#endif
