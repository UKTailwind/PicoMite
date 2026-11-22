/* cmm2user.h: what a USER module sees of MMBasic (tools/sdk): the interpreter's routines, MMBasic's types and the
   module's table, all in loader/pmuser.h. cmm2_start->basic is the table (cmm2.h), set before module_main() is called.
   Build a module with build.py --user; examples/user/proforma.c is one to start from. The name is the Colour Maximite
   2's, so that a module's source builds for either. MIT License, as pmimg.h. */
#ifndef CMM2USER_SDK_H
#define CMM2USER_SDK_H
#include "cmm2.h"
#include "../../loader/pmuser.h"        /* (by its path, as cmm2.h includes pmimg.h) */
#endif
