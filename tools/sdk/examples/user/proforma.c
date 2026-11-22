/* proforma.c: a USER module to start from (tools/sdk). MIT License, as pmimg.h.

   A USER module adds commands and functions of your own to MMBasic. Load it with USER LOAD, then call its commands
   with USER and its functions with USER(:

     USER LOAD "A:/proforma.mod"
     USER HELLO                       prints a greeting; USER HELLO "Peter" greets Peter
     USER SWAP a, b                   two variables of the same type swapped (numbers, strings or structures)
     USER FILL a(), value             every element of an array set to value
     USER TOTAL a(), t                t given the sum of a numeric array's elements
     PRINT USER(ADD 2, 3.5)           5.5: an integer if both are integers, else a float
     PRINT USER(REVERSE$ "abc")       cba
     PRINT USER(FIXED$ PI, 3)         3.142: snprintf's %f, the firmware's own
     PRINT USER(CALLS)                how many times the module has been called since it was loaded
     USER CLOSE                       the module unloaded

   Build it from tools/sdk:

     python build.py out/proforma.mod examples/user/proforma.c --user

   and copy out/proforma.mod (3 KB) to A: or B: (by TFTP, or a transfer that keeps the exact size). --user makes it a
   USER module whose snprintf, sprintf and printf are the firmware's own (tools/sdk/userlib.c), so it carries no printf of
   its own; their %f is right, but %e and %g are not (1e6 comes out as 10.000000e+05), so format those with
   B->float_to_str. Rename the file and
   change the tables below to make a module of your own.

   How it works. MMBasic hands the module the rest of the statement after USER (to command()), or the text inside the
   brackets of USER( (to function()), as it stores it: the names of MMBasic's functions and operators (=, +, AND, TO,
   LEN( ...) are one byte each, anything else is text. The module reads it with the interpreter's own routines, B->...
   (loader/pmuser.h), just as a built-in command does: checkstring() for its keywords, getargs() to split arguments,
   get_integer(), get_number() and get_string() for values, var() and assign() for variables. An error is MMBasic's
   error: B->error() reports it and leaves the module at once, as any MMBasic error leaves a command, so nothing after
   it runs. The module's functions run on MMBasic's own stack: keep large buffers in B->temp() (freed when the
   statement ends) or malloc()'s memory, not in local variables.

   Keep the keywords plain words that are not MMBasic functions or operators (ADD, not LEN or AND), and not LOAD or
   CLOSE, which USER keeps for itself. */
#include <stdio.h>
#include <string.h>
#include "cmm2user.h"

static const struct cmm2_basic *B;              /* the interpreter's routines (cmm2_start->basic) */
static int calls;                               /* the module's state: its variables start again at each load (USER
                                                   LOAD, and the next USER after RUN, NEW or CHAIN) */

/* the text at p after its spaces */
static char *skip(char *p) { while(*p == ' ') p++; return p; }

/* p must have nothing more in it */
static void end_of(char *p) { if(*skip(p)) B->error("Unexpected text: $", skip(p)); }

/* ---- the commands: each is given the text after its keyword ---- */

/* USER HELLO [name$] */
static void c_hello(char *p) {
    char out[CMM2_STRINGSIZE + 40];
    const char *name = "world";
    if(*skip(p)) name = B->get_cstring(p);      /* (an expression: "Peter", n$, "x" + STR$(3)) */
    snprintf(out, sizeof(out), "Hello, %s, from the USER module\r\n", name);
    B->print(out);
}

/* USER SWAP a, b */
static void c_swap(char *p) {
    char *argv[3], *a, *b, *t;
    int argc, n;
    cmm2_var va, vb;
    B->getargs(&p, 3, ",", argv, &argc);        /* argv[0] a, argv[1] the comma, argv[2] b */
    if(argc != 3) B->error("Argument count");
    a = B->var(argv[0], CMM2_VAR_WRITE, &va);
    b = B->var(argv[2], CMM2_VAR_WRITE, &vb);
    if(va.dims || vb.dims) B->error("Expected two variables, not arrays");
    if(va.type != vb.type) B->error("The variables are not the same type");
    if(va.type == CMM2_T_STR) {                 /* each must fit in the other's room */
        if((unsigned char)*a > vb.size || (unsigned char)*b > va.size) B->error("String too long");
        n = 1 + ((unsigned char)*a > (unsigned char)*b ? (unsigned char)*a : (unsigned char)*b);
    } else {
        if(va.size != vb.size) B->error("The variables are not the same type");
        n = va.size;
    }
    t = B->temp(n);
    memcpy(t, a, n);
    memcpy(a, b, n);
    memcpy(b, t, n);
}

/* USER FILL a(), value */
static void c_fill(char *p) {
    char *argv[3], *d, *s;
    int argc, i;
    cmm2_var v;
    B->getargs(&p, 3, ",", argv, &argc);
    if(argc != 3) B->error("Argument count");
    d = B->var(argv[0], CMM2_VAR_WRITE, &v);
    if(!v.dims) B->error("Expected an array: a()");
    if(v.type == CMM2_T_INT) {
        int64_t n = B->get_integer(argv[2]);
        for(i = 0; i < v.elements; i++) ((int64_t *)d)[i] = n;
    } else if(v.type == CMM2_T_NBR) {
        double f = B->get_number(argv[2]);
        for(i = 0; i < v.elements; i++) ((double *)d)[i] = f;
    } else if(v.type == CMM2_T_STR) {           /* each element is size + 1 bytes: its length, then its text */
        s = B->get_string(argv[2]);
        if((unsigned char)*s > v.size) B->error("String too long");
        for(i = 0; i < v.elements; i++) memcpy(d + i * (v.size + 1), s, (unsigned char)*s + 1);
    } else B->error("Expected an array of numbers or strings");
}

/* USER TOTAL a(), t */
static void c_total(char *p) {
    char *argv[3], *d;
    int argc, i;
    cmm2_var v;
    cmm2_value r;
    B->getargs(&p, 3, ",", argv, &argc);
    if(argc != 3) B->error("Argument count");
    d = B->var(argv[0], CMM2_VAR_READ, &v);
    if(!v.dims || !(v.type & (CMM2_T_INT | CMM2_T_NBR))) B->error("Expected a numeric array: a()");
    memset(&r, 0, sizeof(r));
    r.type = v.type;
    for(i = 0; i < v.elements; i++) {
        if(v.type == CMM2_T_INT) r.i += ((int64_t *)d)[i];
        else r.f += ((double *)d)[i];
        if((i & 1023) == 1023) B->check_abort();   /* (Ctrl-C in a long loop) */
    }
    B->assign(argv[2], &r);                     /* (an integer total to a float variable converts, as LET does) */
}

/* ---- the functions: each is given the text after its keyword, and sets r's type and value ---- */

/* USER(ADD a, b) */
static void f_add(char *p, cmm2_value *r) {
    char *argv[3];
    int argc;
    cmm2_value a, b;
    B->getargs(&p, 3, ",", argv, &argc);
    if(argc != 3) B->error("Argument count");
    B->evaluate(argv[0], &a);
    B->evaluate(argv[2], &b);
    if((a.type | b.type) & CMM2_T_STR) B->error("Expected a number");
    if(a.type == CMM2_T_INT && b.type == CMM2_T_INT) {
        r->type = CMM2_T_INT;
        r->i = a.i + b.i;
    } else {
        r->type = CMM2_T_NBR;
        r->f = (a.type == CMM2_T_INT ? (double)a.i : a.f) + (b.type == CMM2_T_INT ? (double)b.i : b.f);
    }
}

/* USER(REVERSE$ s$) */
static void f_reverse(char *p, cmm2_value *r) {
    char *s = B->get_string(p);                 /* (an MMBasic string: s[0] its length) */
    int n = (unsigned char)s[0], i;
    r->type = CMM2_T_STR;
    r->s[0] = (char)n;                          /* (r->s: the firmware's room, CMM2_STRINGSIZE bytes) */
    for(i = 0; i < n; i++) r->s[1 + i] = s[n - i];
}

/* USER(FIXED$ x, n) */
static void f_fixed(char *p, cmm2_value *r) {
    char *argv[3];
    int argc, n;
    B->getargs(&p, 3, ",", argv, &argc);
    if(argc != 3) B->error("Argument count");
    n = snprintf(r->s + 1, CMM2_STRINGSIZE - 1, "%.*f", (int)B->get_int(argv[2], 0, 15), B->get_number(argv[0]));
    if(n > CMM2_STRINGSIZE - 2) B->error("String too long");
    r->s[0] = (char)n;                          /* (the length byte, before the text snprintf wrote) */
    r->type = CMM2_T_STR;
}

/* USER(CALLS) */
static void f_calls(char *p, cmm2_value *r) {
    end_of(p);
    r->type = CMM2_T_INT;
    r->i = calls;
}

/* ---- the tables: a keyword and its routine ---- */
typedef struct { const char *name; void (*fn)(char *p); } command_t;
typedef struct { const char *name; void (*fn)(char *p, cmm2_value *r); } function_t;

static const command_t commands[] = {
    {"HELLO", c_hello},
    {"SWAP", c_swap},
    {"FILL", c_fill},
    {"TOTAL", c_total},
};
static const function_t functions[] = {
    {"ADD", f_add},
    {"REVERSE$", f_reverse},
    {"FIXED$", f_fixed},
    {"CALLS", f_calls},
};

/* USER ...: the statement after USER */
static void command(char *line) {
    char *p;
    calls++;
    for(unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        if((p = B->checkstring(line, commands[i].name)) != NULL) { commands[i].fn(p); return; }
    B->error("Unknown USER command");
}

/* USER(...): the text inside the brackets */
static void function(char *args, cmm2_value *r) {
    char *p;
    calls++;
    for(unsigned i = 0; i < sizeof(functions) / sizeof(functions[0]); i++)
        if((p = B->checkstring(args, functions[i].name)) != NULL) { functions[i].fn(p, r); return; }
    B->error("Unknown USER function");
}

/* called before the module goes (USER CLOSE, another USER LOAD, RUN, NEW, LOAD, CHAIN): give back here anything that would
   outlive it, such as files left open, pins set up or a timer started. This module holds nothing */
static void forget(void) {}

static const cmm2user_head table = {
    {CMM2MOD_MAGIC, CMM2USER_VERSION, forget},
    CMM2USER_MAGIC,
    command,                                    /* (NULL if the module has no commands) */
    function,                                   /* (NULL if it has no functions) */
};

/* called once when the module is loaded: its table */
const void *module_main(void) {
    B = cmm2_start->basic;
    if(B == NULL || B->version < CMM2USER_VERSION || B->size < sizeof(struct cmm2_basic))
        return NULL;                            /* (not loaded by USER LOAD, or a firmware older than this header:
                                                   the error "not a module") */
    return &table;
}
