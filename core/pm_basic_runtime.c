/* Compiler-independent PicoMite BASIC interpreter bridge for native C.
 * Save the tokenizer's scratch buffers and the MMBasic error trampoline;
 * a C CSUB may be called in the middle of an existing BASIC statement.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_runtime.h"
#include "core/pm_api.h"
#include "core/tcc/include/picomite/basic.h"
#include "core/tcc/include/picomite/vars.h"
#include <ctype.h>
#include <strings.h>
#include "pico/time.h"
#include <string.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>

typedef struct {
    jmp_buf saved;
    int skip, mmerrno;
    unsigned char *line;
    char msg[MAXERRMSG];
} pm_basic_trap;
static void pm_basic_trap_save(pm_basic_trap *t)
{
    memcpy(t->saved, ErrNext, sizeof(jmp_buf));
    t->skip = OptionErrorSkip;
    t->mmerrno = MMerrno;
    t->line = cmdline;
    memcpy(t->msg, MMErrMsg, MAXERRMSG);
    OptionErrorSkip = 1;
}
static void pm_basic_trap_restore(pm_basic_trap *t)
{
    memcpy(ErrNext, t->saved, sizeof(jmp_buf));
    OptionErrorSkip = t->skip;
    MMerrno = t->mmerrno;
    cmdline = t->line;
    memcpy(MMErrMsg, t->msg, MAXERRMSG);
}
#define PM_BASIC_TRY(t) (pm_basic_trap_save(&(t)), setjmp(ErrNext) == 0)

typedef struct {
    unsigned char tkn[STRINGSIZE], inp[STRINGSIZE];
    unsigned char *nextstmt, *cmdline;
    int cmdtoken, multi;
} pm_basic_lines;
static void pm_basic_lines_save(pm_basic_lines *b)
{
    memcpy(b->tkn, tknbuf, STRINGSIZE);
    memcpy(b->inp, inpbuf, STRINGSIZE);
    b->nextstmt = nextstmt;
    b->cmdline = cmdline;
    b->cmdtoken = cmdtoken;
    b->multi = multi;
}
static void pm_basic_lines_restore(const pm_basic_lines *b)
{
    memcpy(tknbuf, b->tkn, STRINGSIZE);
    memcpy(inpbuf, b->inp, STRINGSIZE);
    nextstmt = b->nextstmt;
    cmdline = b->cmdline;
    cmdtoken = b->cmdtoken;
    multi = b->multi;
}
static void pm_basic_temps_done(void)
{
    if (pm_runtime_is_running()) ClearTempMemory();
}

static void __attribute__((noreturn)) pm_basic_rethrow(int err, const char *text)
{
    MMerrno = err ? err : 16;
    memcpy(MMErrMsg, text, MAXERRMSG);
    MMErrMsg[MAXERRMSG - 1] = 0;
    char saved[MAXERRMSG];
    memcpy(saved, MMErrMsg, MAXERRMSG);
    error("$", saved);
    __builtin_unreachable();
}

/* The RAM command is linked only for RP2350. On RP2040 there is no
 * handler to compare with, so the predicate must always return zero. */
static int pm_basic_is_psram_command(void (*fn)(void))
{
#ifdef rp2350
    return fn == cmd_psram;
#else
    (void)fn;
    return 0;
#endif
}

/* Like EXECUTE: reject commands that replace the C program's heap. */
static bool pm_basic_replaces_program(void (*fn)(void), unsigned char *args)
{
    static const char *const load_sub[] = {"CONTEXT", "IMAGE", "DATA", "BMP", "JPG", "PNG"};
    unsigned i;
    if (fn == cmd_run || fn == cmd_chain || fn == cmd_new) return true;
    if (fn == cmd_flash || pm_basic_is_psram_command(fn))
        return checkstring(args, (unsigned char *)"RUN") ||
               checkstring(args, (unsigned char *)"CHAIN");
    if (fn == cmd_load) {
        for (i = 0; i < sizeof(load_sub)/sizeof(load_sub[0]); ++i)
            if (checkstring(args, (unsigned char *)load_sub[i])) return false;
        return true;
    }
    return false;
}
static void pm_basic_execute_do(const char *cmd)
{
    int i = 0, toggle = 0;
    unsigned char *p;
    if (!cmd || strlen(cmd) >= STRINGSIZE) error("String too long");
    strcpy((char *)inpbuf, cmd);
    p = inpbuf;
    skipspace(p);
    while (inpbuf[i]) {
        if (inpbuf[i] == 34) toggle = !toggle;
        if (!toggle) {
            if (inpbuf[i] == ':') error("Only single statements allowed");
            inpbuf[i] = mytoupper(inpbuf[i]);
        }
        i++;
    }
    multi = false;
    tokenise(true);
    memset(inpbuf, 0, STRINGSIZE);
    p = tknbuf;
    skipspace(p);
    if (!*p) return;
    targ = T_CMD;
    if (!commandtbl_is_cmd(p)) StandardError(36);
    {
        CommandToken c = commandtbl_decode(p);
        if (c == cmdWHILE || c == cmdDO || c == cmdFOR)
            error("Invalid inside THEN ... ELSE");
        cmdtoken = c;
        cmdline = p + sizeof(CommandToken);
        skipspace(cmdline);
        if (pm_basic_replaces_program(commandtbl[c].fptr, cmdline))
            error("Not possible from a C program");
        if (!SymAwareCommand(c)) cmdline = SymExpandStatement(cmdline);
        commandtbl[c].fptr();
    }
}

void pm_execute(const char *cmd)
{
    pm_basic_lines b;
    pm_basic_trap t;
    int err;
    char msg[MAXERRMSG];
    pm_basic_lines_save(&b);
    if (PM_BASIC_TRY(t)) {
        pm_basic_execute_do(cmd);
        pm_basic_trap_restore(&t);
        pm_basic_lines_restore(&b);
        pm_basic_temps_done();
        return;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_basic_trap_restore(&t);
    pm_basic_lines_restore(&b);
    pm_basic_temps_done();
    pm_basic_rethrow(err, msg);
}

static int pm_basic_eval_do(const char *expr, MMFLOAT *f,
                            long long *i, unsigned char *str)
{
    unsigned char *s = NULL;
    int t = T_NOTYPE;
    if (!expr || strlen(expr) + 2 >= STRINGSIZE) error("String too long");
    inpbuf[0] = 'r';
    inpbuf[1] = '=';
    strcpy((char *)inpbuf + 2, expr);
    multi = false;
    tokenise(true);
    evaluate(tknbuf + 2 + sizeof(CommandToken), f, i, &s, &t, false);
    if ((t & T_STR) && str) Mstrcpy(str, s);
    return t;
}
static void pm_basic_eval(const char *expr, int type,
                          long long *ip, double *fp, char *sp, int size)
{
    pm_basic_lines b;
    pm_basic_trap t;
    char msg[MAXERRMSG];
    MMFLOAT f = 0;
    long long i = 0;
    unsigned char str[STRINGSIZE];
    volatile int ty = 0;
    int err;
    pm_basic_lines_save(&b);
    if (PM_BASIC_TRY(t)) {
        ty = pm_basic_eval_do(expr, &f, &i, str);
        if (type == 's' && !(ty & T_STR)) error("Expected a string");
        if (type != 's' && (ty & T_STR)) error("Expected a number");
        pm_basic_trap_restore(&t);
        pm_basic_lines_restore(&b);
        pm_basic_temps_done();
        if (type == 'i') *ip = (ty & T_NBR) ? FloatToInt64(f) : i;
        else if (type == 'f') *fp = (ty & T_NBR) ? f : (double)i;
        else if (size > 0) {
            int n = str[0] < size - 1 ? str[0] : size - 1;
            memcpy(sp, str + 1, (size_t)n);
            sp[n] = 0;
        }
        return;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_basic_trap_restore(&t);
    pm_basic_lines_restore(&b);
    pm_basic_temps_done();
    pm_basic_rethrow(err, msg);
}
long long pm_eval_int(const char *s)
{
    long long v = 0;
    pm_basic_eval(s, 'i', &v, NULL, NULL, 0);
    return v;
}
double pm_eval_float(const char *s)
{
    double v = 0;
    pm_basic_eval(s, 'f', NULL, &v, NULL, 0);
    return v;
}
char *pm_eval_str(const char *s, char *buf, int size)
{
    pm_basic_eval(s, 's', NULL, NULL, buf, size);
    return buf;
}

/* The top try-frame is on the C caller's stack. It must be detached on
 * abnormal exit; the common RUN cleanup always clears it. */
typedef struct pm_basic_try_frame {
    jmp_buf jump;
    struct pm_basic_try_frame *previous;
    int error;
    char message[MAXERRMSG];
} pm_basic_try_frame;
static pm_basic_try_frame *pm_basic_try_top;

void pm_runtime_try_catch(const char *msg, int err)
{
    pm_basic_try_frame *f = pm_basic_try_top;
    if (!f) return;
    f->error = err ? err : 16;
    memcpy(f->message, msg, MAXERRMSG);
    f->message[MAXERRMSG - 1] = 0;
    longjmp(f->jump, 1);
}
void pm_runtime_try_clear(void) { pm_basic_try_top = NULL; }

int pm_try(void (*fn)(void *), void *arg)
{
    pm_basic_try_frame f;
    pm_basic_trap t;
    char msg[MAXERRMSG];
    volatile int err = 0;
    if (!fn) return 0;
    MMerrno = 0;
    MMErrMsg[0] = 0;
    f.previous = pm_basic_try_top;
    if (PM_BASIC_TRY(t)) {
        if (setjmp(f.jump) == 0) {
            pm_basic_try_top = &f;
            fn(arg);
        } else {
            err = f.error;
            memcpy(msg, f.message, MAXERRMSG);
        }
    } else {
        err = MMerrno ? MMerrno : 16;
        memcpy(msg, MMErrMsg, MAXERRMSG);
    }
    pm_basic_try_top = f.previous;
    pm_basic_trap_restore(&t);
    if (err) {
        pm_basic_temps_done();
        MMerrno = err;
        memcpy(MMErrMsg, msg, MAXERRMSG);
        MMErrMsg[MAXERRMSG - 1] = 0;
    }
    return err;
}

typedef struct {
    const char *expr;
    int type, size;
    long long *i;
    double *f;
    char *s;
} pm_basic_try_args;
static void pm_execute_for_try(void *p)
{ pm_execute(((pm_basic_try_args *)p)->expr); }
static void pm_eval_for_try(void *p)
{
    pm_basic_try_args *a = p;
    pm_basic_eval(a->expr, a->type, a->i, a->f, a->s, a->size);
}
int pm_execute_try(const char *cmd)
{
    pm_basic_try_args a = {.expr = cmd};
    return pm_try(pm_execute_for_try, &a);
}
int pm_eval_int_try(const char *expr, long long *v)
{
    pm_basic_try_args a = {.expr = expr, .type = 'i', .i = v};
    if (v) *v = 0;
    return pm_try(pm_eval_for_try, &a);
}
int pm_eval_float_try(const char *expr, double *v)
{
    pm_basic_try_args a = {.expr = expr, .type = 'f', .f = v};
    if (v) *v = 0;
    return pm_try(pm_eval_for_try, &a);
}
int pm_eval_str_try(const char *expr, char *buf, int size)
{
    pm_basic_try_args a = {.expr = expr, .type = 's', .s = buf, .size = size};
    if (size > 0 && buf) buf[0] = 0;
    return pm_try(pm_eval_for_try, &a);
}

static void pm_basic_invoke(int isfun, const char *name,
                            const pm_basic_value *args, unsigned count,
                            pm_basic_value *result, char *strbuf, unsigned cap)
{
    pm_basic_trap t;
    int err;
    char msg[MAXERRMSG];
    if (PM_BASIC_TRY(t)) {
        BasicCallTyped(isfun, name, args, count, result, strbuf, cap);
        pm_basic_trap_restore(&t);
        pm_basic_temps_done();
        return;
    }
    err = MMerrno;
    memcpy(msg, MMErrMsg, MAXERRMSG);
    pm_basic_trap_restore(&t);
    pm_basic_temps_done();
    pm_basic_rethrow(err, msg);
}
void pm_core_basic_sub(const char *name, const pm_basic_value *args, unsigned count)
{ pm_basic_invoke(0, name, args, count, NULL, NULL, 0); }
void pm_core_basic_function(const char *name, const pm_basic_value *args,
                       unsigned count, pm_basic_value *result,
                       char *string_buf, unsigned string_capacity)
{ pm_basic_invoke(1, name, args, count, result, string_buf, string_capacity); }

/* SETTICK is polled synchronously from the C API boundary, never from IRQ. */
#define PM_BASIC_TICKS 4
typedef struct {
    void (*callback)(void);
    uint64_t period, next;
} pm_basic_tick;
static pm_basic_tick pm_basic_ticks[PM_BASIC_TICKS];
static bool pm_basic_in_tick;
static bool pm_basic_ticks_active;
void pm_runtime_tick_reset(void)
{
    memset(pm_basic_ticks, 0, sizeof(pm_basic_ticks));
    pm_basic_in_tick = pm_basic_ticks_active = false;
    pm_runtime_try_clear();
}
void pm_runtime_tick_service(void)
{
    unsigned i;
    uint64_t now;
    if (!pm_runtime_is_running() || !pm_basic_ticks_active || pm_basic_in_tick) return;
    now = time_us_64();
    for (i = 0; i < PM_BASIC_TICKS; ++i) {
        pm_basic_tick *t = &pm_basic_ticks[i];
        if (!t->callback || now < t->next) continue;
        t->next += t->period;
        if (t->next <= now) t->next = now + t->period;
        pm_basic_try_frame *saved = pm_basic_try_top;
        pm_basic_try_top = NULL;
        pm_basic_in_tick = true;
        t->callback();
        pm_basic_in_tick = false;
        pm_basic_try_top = saved;
    }
}
void pm_settick(int period, void (*fn)(void), int nbr)
{
    unsigned k;
    if (!pm_runtime_is_running()) return;
    if (nbr < 1 || nbr > PM_BASIC_TICKS || period < 0)
        error("Invalid argument");
    if (period && !fn) error("Argument count");
    pm_basic_ticks[nbr - 1].callback = period ? fn : NULL;
    pm_basic_ticks[nbr - 1].period = (uint64_t)period * 1000;
    pm_basic_ticks[nbr - 1].next = time_us_64() + (uint64_t)period * 1000;
    pm_basic_ticks_active = false;
    for (k = 0; k < PM_BASIC_TICKS; ++k)
        if (pm_basic_ticks[k].callback) pm_basic_ticks_active = true;
}

/* BASIC variables belong to the MMBasic VM; preserve its tokeniser state
 * around VARADDR/DIM just as EXECUTE and EVAL do. */
_Static_assert(MAXDIM <= PM_MAXDIM, "pm_varinfo dimension count");
static void *pm_core_var_do(const char *ref, pm_varinfo *info)
{
    void *addr;
    struct s_vartbl *v;
    int k;
    if (!ref || strlen(ref)+2 >= STRINGSIZE) error("String too long");
    inpbuf[0]='r';inpbuf[1]='=';
    strcpy((char *)inpbuf+2,ref);
    multi=false;
    tokenise(true);
    addr=findvar(tknbuf+2+sizeof(CommandToken),V_FIND|V_EMPTY_OK|V_NOFIND_ERR);
    v=VREC(g_VarIndex);
    if(v->type & T_STRUCT) error("A structure is not supported here");
    if(info) {
        memset(info,0,sizeof(*info));
        info->type=v->type&(T_NBR|T_STR|T_INT);
        info->size=(v->type&T_STR)?v->size:0;
        info->constant=(v->type&T_CONST)!=0;
        info->element_size=(v->type&T_STR)?v->size+1:8;
        info->elements=1;
        if(DimIsRealArray(RAW_DIM(*v,0)))
            for(k=0;k<MAXDIM&&DimIsRealArray(RAW_DIM(*v,k));++k){
                info->dims[k]=DimElements(RAW_DIM(*v,k));
                info->elements*=info->dims[k];
                info->ndims=k+1;
            }
    }
    return addr;
}
static void *pm_core_var(const char *ref,pm_varinfo *info)
{
    pm_basic_lines b;
    pm_basic_trap t;
    void *volatile addr=NULL;
    char msg[MAXERRMSG];
    int err;
    pm_basic_lines_save(&b);
    if(PM_BASIC_TRY(t)) {
        addr=pm_core_var_do(ref,info);
        pm_basic_trap_restore(&t);
        pm_basic_lines_restore(&b);
        pm_basic_temps_done();
        return addr;
    }
    err=MMerrno;
    memcpy(msg,MMErrMsg,MAXERRMSG);
    pm_basic_trap_restore(&t);
    pm_basic_lines_restore(&b);
    pm_basic_temps_done();
    pm_basic_rethrow(err,msg);
}
static void *pm_core_dim(const char *decl,pm_varinfo *info)
{
    static const char *const types[]={"INTEGER ","FLOAT ","STRING "};
    char cmd[STRINGSIZE+8],ref[MAXVARLEN+8];
    const char *p;
    unsigned i;
    int n=0;
    if(!decl)error("Argument count");
    if(snprintf(cmd,sizeof(cmd),"DIM %s",decl)>=STRINGSIZE)error("String too long");
    pm_execute(cmd);
    for(p=decl;*p==' ';++p){}
    for(i=0;i<sizeof(types)/sizeof(types[0]);++i)
        if(strncasecmp(p,types[i],strlen(types[i]))==0)
            for(p+=strlen(types[i]);*p==' ';++p){}
    while((isalnum((unsigned char)*p)||*p=='_'||*p=='.')&&n<MAXVARLEN)
        ref[n++]=*p++;
    if(*p=='%'||*p=='!'||*p=='$')ref[n++]=*p++;
    while(*p==' ')++p;
    if(*p=='('){ref[n++]='(';ref[n++]=')';}
    ref[n]=0;
    return pm_core_var(ref,info);
}
static void pm_core_erase(const char *names)
{
    char cmd[STRINGSIZE+8];
    if(snprintf(cmd,sizeof(cmd),"ERASE %s",names?names:"")>=STRINGSIZE)
        error("String too long");
    pm_execute(cmd);
}
static char *pm_core_str_get(const void *mmstr,char *buf,int size)
{
    const unsigned char *m=mmstr;
    int n;
    if(!buf||size<=0)return buf;
    n=m?m[0]:0;
    if(n>size-1)n=size-1;
    if(n)memcpy(buf,m+1,(size_t)n);
    buf[n]=0;
    return buf;
}
static void pm_core_str_set(void *mmstr,int capacity,const char *s)
{
    unsigned char *m=mmstr;
    size_t n=s?strlen(s):0;
    if(!m||(int)n>capacity||n>MAXSTRLEN)error("String too long");
    m[0]=(unsigned char)n;
    if(n)memcpy(m+1,s,n);
}

typedef struct { const char *name; const void *address; } pm_basic_symbol;
#define PM_BASIC_SYMBOL(n) {#n, (const void *)&n},
#define PM_BASIC_AS(n, fn) {#n, (const void *)&fn},
static const pm_basic_symbol pm_basic_symbols[] = {
    PM_BASIC_SYMBOL(pm_execute)
    PM_BASIC_SYMBOL(pm_eval_int)
    PM_BASIC_SYMBOL(pm_eval_float)
    PM_BASIC_SYMBOL(pm_eval_str)
    PM_BASIC_SYMBOL(pm_try)
    PM_BASIC_SYMBOL(pm_execute_try)
    PM_BASIC_SYMBOL(pm_eval_int_try)
    PM_BASIC_SYMBOL(pm_eval_float_try)
    PM_BASIC_SYMBOL(pm_eval_str_try)
    PM_BASIC_AS(pm_basic_sub, pm_core_basic_sub)
    PM_BASIC_AS(pm_basic_function, pm_core_basic_function)
    PM_BASIC_AS(pm_var, pm_core_var)
    PM_BASIC_AS(pm_dim, pm_core_dim)
    PM_BASIC_AS(pm_erase, pm_core_erase)
    PM_BASIC_AS(pm_str_get, pm_core_str_get)
    PM_BASIC_AS(pm_str_set, pm_core_str_set)
    PM_BASIC_SYMBOL(pm_settick)
};
const void *pm_runtime_basic_resolve(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_basic_symbols)/sizeof(pm_basic_symbols[0]); ++i)
        if (!strcmp(pm_basic_symbols[i].name, name))
            return pm_basic_symbols[i].address;
    return NULL;
}
unsigned pm_runtime_basic_firmware_id(void)
{
    uint32_t h = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_basic_symbols)/sizeof(pm_basic_symbols[0]); ++i) {
        uint32_t v = (uint32_t)(uintptr_t)pm_basic_symbols[i].address;
        for (j = 0; j < 4; ++j, v >>= 8) h = (h ^ (v & 255u)) * 16777619u;
    }
    return h ? h : 1u;
}
