/* PicoMite TinyCC: <picomite/basic.h> - MMBasic itself: its commands and
   functions as text, and SETTICK.  (API version 1, as <picomite/system.h>.)

   With pm_execute and pm_eval_* a C program can use every MMBasic command
   and function that has no C function of its own yet - BLIT, OPTION,
   MM.INFO()... - with exactly their BASIC meaning, at the cost of
   tokenising the text each time.  An MMBasic error in one, as in any pm_
   call, ends the program with that error (in a C CSUB: the error at its
   call) - unless it is made inside pm_try, which is ON ERROR SKIP.  CTRL-C
   ends the program as usual. */
#ifndef _PICOMITE_BASIC_H
#define _PICOMITE_BASIC_H

/* EXECUTE cmd$: one MMBasic statement, as a statement of a running program
   (what is only for the prompt is "Invalid in a program"; RUN, CHAIN, NEW
   and LOAD of a program are not possible).  As with EXECUTE, a single
   statement (no ':').  There are no BASIC
   variables or SUBs in a C program: use the values themselves, e.g.
     char cmd[64];
     snprintf(cmd, sizeof cmd, "SPRITE SHOW %d,%d,%d,1", n, x, y);
     pm_execute(cmd);                                                     */
void pm_execute(const char *cmd);

/* EVAL(s$): the value of an MMBasic expression - as an integer (a float is
   rounded, as when BASIC assigns it to an integer), as a float, or as a
   string into buf (size bytes with its terminating 0).  A number where a
   string is asked for, or the other way round, is an error.
     int vres = (int)pm_eval_int("MM.VRES");                              */
long long pm_eval_int(const char *expr);
double pm_eval_float(const char *expr);
char *pm_eval_str(const char *expr, char *buf, int size); /* returns buf */

/* ON ERROR SKIP 1 for fn(arg): an MMBasic error in any pm_ call made by
   fn (pm_execute, pm_eval_*, pm_sprite_show, ...) comes back here instead
   of ending the program.  Returns 0, or the error's number, with MM.ERRNO
   and MM.ERRMSG$ set as SKIP sets them; like ON ERROR SKIP it clears them
   first.  fn is left at the error, as a skipped statement is.  A SETTICK
   function is outside it (as a BASIC interrupt is outside the ON ERROR of
   what it interrupts).  CTRL-C and exit() still end the program.
     static void show(void *p) { pm_sprite_show(*(int *)p, 10, 10, 1, PM_DEFAULT); }
     if (pm_try(show, &n)) printf("%s\n", pm_errmsg());                   */
int pm_try(void (*fn)(void *arg), void *arg);

/* pm_try of a single pm_execute / pm_eval_*: ON ERROR SKIP : statement.
   0 or MM.ERRNO; on an error the value is 0 (the string "").
     if (pm_execute_try("GUI CURSOR ON 0, 320, 240")) ... no mouse ...      */
int pm_execute_try(const char *cmd);
int pm_eval_int_try(const char *expr, long long *value);
int pm_eval_float_try(const char *expr, double *value);
int pm_eval_str_try(const char *expr, char *buf, int size);

int pm_errno(void);          /* MM.ERRNO: of the last error a pm_try caught */
const char *pm_errmsg(void); /* MM.ERRMSG$ */
void pm_error_clear(void);   /* ON ERROR CLEAR: both cleared */

/* SETTICK period, fn, nbr: call fn every period milliseconds (nbr 1..4),
   period 0 to stop it.  As a BASIC tick SUB runs between statements, fn
   runs between the program's own calls of the firmware - in pm_pause,
   pm_yield, output, input and the pm_ drawing calls - never from an
   interrupt, and not again while it is running.  Not in a C CSUB. */
void pm_settick(int period, void (*fn)(void), int nbr);

#endif /* _PICOMITE_BASIC_H */
