/* PicoMite TinyCC: <picomite/basic.h> - MMBasic itself: its commands and
   functions as text, and SETTICK.  (API version 1, as <picomite/system.h>.)

   With pm_execute and pm_eval_* a C program can use every MMBasic command
   and function that has no C function of its own yet - SPRITE, PLAY, LOAD
   IMAGE, BLIT, COLOUR, KEYDOWN(), MM.INFO()... - with exactly their BASIC
   meaning, at the cost of tokenising the text each time.  An MMBasic error
   in one ends the program with that error (in a C CSUB: the error at its
   call), and CTRL-C ends the program as usual. */
#ifndef _PICOMITE_BASIC_H
#define _PICOMITE_BASIC_H

/* EXECUTE cmd$: one MMBasic statement, as if typed at the prompt.  As with
   EXECUTE, a single statement (no ':'), and not RUN.  There are no BASIC
   variables or SUBs in a C program: use the values themselves, e.g.
     char cmd[64];
     snprintf(cmd, sizeof cmd, "SPRITE SHOW %d,%d,%d,1", n, x, y);
     pm_execute(cmd);                                                     */
void pm_execute(const char *cmd);

/* EVAL(s$): the value of an MMBasic expression - as an integer (a float is
   rounded, as when BASIC assigns it to an integer), as a float, or as a
   string into buf (size bytes with its terminating 0).  A number where a
   string is asked for, or the other way round, is an error.
     if (pm_eval_int("KEYDOWN(0)")) ...                                    */
long long pm_eval_int(const char *expr);
double pm_eval_float(const char *expr);
char *pm_eval_str(const char *expr, char *buf, int size); /* returns buf */

/* SETTICK period, fn, nbr: call fn every period milliseconds (nbr 1..4),
   period 0 to stop it.  As a BASIC tick SUB runs between statements, fn
   runs between the program's own calls of the firmware - in pm_pause,
   pm_yield, output, input and the pm_ drawing calls - never from an
   interrupt, and not again while it is running.  Not in a C CSUB. */
void pm_settick(int period, void (*fn)(void), int nbr);

#endif /* _PICOMITE_BASIC_H */
