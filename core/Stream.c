/*
 * @cond
 * The following section will be excluded from the documentation.
 */
/* *********************************************************************************************************************
PicoMite MMBasic

Stream.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
/*
 * Route B: the compiled statement stream.  See Stream.h and
 * docs/Interpreter_RouteB_Design.html.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "Stream.h"
#include "hardware/flash.h"            // FLASH_SECTOR_SIZE
#include "hardware/regs/addressmap.h" // XIP_NOCACHE_NOALLOC_BASE

#ifdef rp2350 // Route B is RP2350-only (see Stream.h)
int RBMode = RB_OFF;

// A board with PSRAM keeps the stream in a region of its own, what the PSRAM
// reserve leaves above the RAM slots (PSRAMstream in configuration.h), written
// at memory speed.  One without keeps it in flash slot 2 (slot 3 holds the
// library and slot 1 is left for the user) until it has a flash area of its own.
#define RB_FLASH_SLOT 2

static int RBInPsram(void)
{
    return PSRAMsize != 0;
}

// While OPTION COMPILE is on, a stream in flash slot 2 makes that slot not the
// user's: saving, loading, erasing or running it would destroy the stream or
// run it as a program.
void RBGuardFlashSlot(int slot)
{
    if (RBMode == RB_OFF || RBInPsram() || slot != RB_FLASH_SLOT)
        return;
    error("Flash slot % holds the compiled program: OPTION COMPILE OFF first", slot);
}

/* ---------------------------------------------------------------------------
   P1b: the stamp and the slot.

   A slot holds one stream.  Its first 256-byte page is the header, written
   last, so a stream whose writing was interrupted has no valid header and
   reads as stale.
   --------------------------------------------------------------------------- */
#define RB_MAGIC 0x31304252 // "RB01"
#define RB_VERSION 6        // the stream format
#define RB_PAGE 256

typedef struct
{
    uint32_t magic;    // RB_MAGIC
    uint16_t version;  // RB_VERSION
    uint16_t size;     // sizeof(rbheader_t)
    uint32_t build;    // RBBuildId(): what the stream's numbers mean in this firmware
    uint32_t progcrc;  // the stamp: CRC32 of the program's slot-sized image
    uint32_t libcrc;   // and of the library's (0 without one)
    uint32_t stmts;    // records (from P1c)
    uint32_t codeoff;  // where the records start, from the slot's start
    uint32_t codelen;
    uint32_t mapoff;   // the statement map
    uint32_t maplen;
    uint32_t tabof;    // its bucket index (uint16_t each)
    uint32_t nbprog;   // buckets for the program; the library's follow
    uint32_t ntab;     // entries in the index: both images' buckets and a sentinel
    uint32_t hdrcrc;   // CRC32 of everything above
} rbheader_t;

int RBLive = 0;
static int RBCompiles = 0;
static int RBReused = 0;
static uint32_t RBRan = 0;  // statements run from the stream since RUN
static uint32_t RBMiss = 0; // map lookups the cache did not answer (a bucket search each)
static uint32_t RBCode = 0; // statements run as compiled code since RUN
static const char *RBWhy = NULL; // why the last RUN ran as text

// A stream is tied to the firmware that wrote it: a record holds command
// token numbers, which another build may number differently.
static uint32_t RBBuildId(void)
{
    return ((uint32_t)RB_VERSION << 24) ^ ((uint32_t)CommandTableSize << 12) ^ (uint32_t)TokenTableSize ^ (uint32_t)MAX_PROG_SIZE;
}

static uint8_t *RBSlotBase(void)
{
    if (RBInPsram())
        return (uint8_t *)PSRAMstream;
    return (uint8_t *)(flash_target_contents + (RB_FLASH_SLOT - 1) * MAX_PROG_SIZE);
}

// the most the stream may take
static uint32_t RBSlotSize(void)
{
    return RBInPsram() ? PSRAMstreamsize : MAX_PROG_SIZE;
}

// the flash offset of the slot, for safe_flash_range_erase/program
static uint32_t RBSlotFlashOffset(void)
{
    return FLASH_TARGET_OFFSET + FLASH_ERASE_SIZE + SAVEDVARS_FLASH_SIZE + (RB_FLASH_SLOT - 1) * MAX_PROG_SIZE;
}

/* The writers.  RBWriteBegin erases the flash the stream will need (in
   PSRAM it only invalidates the header); then each region - map, code - is
   written sequentially by its own writer, a page at a time in flash; the
   header page is written last. */
typedef struct
{
    uint32_t pos;  // next byte's offset in the slot
    uint8_t *page; // flash: the page being filled
} rbwriter_t;

static struct
{
    int full; // the stream would not fit the slot: abandoned, the program runs as text
} W;

static void RBFlashPage(uint32_t off, const uint8_t *data)
{
    disable_interrupts_pico();
    safe_flash_range_program(RBSlotFlashOffset() + off, data, RB_PAGE);
    enable_interrupts_pico();
}

static void RBWriteBegin(uint32_t size)
{
    if (RBInPsram())
    {
        memset(RBSlotBase(), 0, RB_PAGE); // no valid header until the stream is complete
        return;
    }
    size = (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    disable_interrupts_pico();
    safe_flash_range_erase(RBSlotFlashOffset(), size);
    enable_interrupts_pico();
}

static void RBWriterStart(rbwriter_t *w, uint32_t pos)
{
    w->pos = pos;
    w->page = NULL;
    if (!RBInPsram())
    {
        w->page = GetTempMemory(RB_PAGE);
        memset(w->page, 0xFF, RB_PAGE);
    }
}

static void RBPut(rbwriter_t *w, const void *data, uint32_t n)
{
    const uint8_t *d = data;
    if (RBInPsram())
    {
        memcpy(RBSlotBase() + w->pos, d, n);
        w->pos += n;
        return;
    }
    while (n--)
    {
        w->page[w->pos % RB_PAGE] = *d++;
        if (++w->pos % RB_PAGE == 0)
        {
            RBFlashPage(w->pos - RB_PAGE, w->page);
            memset(w->page, 0xFF, RB_PAGE);
        }
    }
}

static void RBFlush(rbwriter_t *w)
{
    if (!RBInPsram() && w->pos % RB_PAGE)
        RBFlashPage(w->pos - w->pos % RB_PAGE, w->page); // the rest of the page is 0xFF
}

static void RBWriteEnd(rbheader_t *h)
{
    uint8_t *hp;
    h->hdrcrc = lfs_crc(0xffffffff, h, offsetof(rbheader_t, hdrcrc));
    if (RBInPsram())
    {
        memcpy(RBSlotBase(), h, sizeof(*h));
        return;
    }
    hp = GetTempMemory(RB_PAGE);
    memset(hp, 0xFF, RB_PAGE);
    memcpy(hp, h, sizeof(*h));
    RBFlashPage(0, hp);
}

/* ---------------------------------------------------------------------------
   P1c: the records and the statement map.

   One walk over each image meets its statements exactly as ExecuteProgram
   does, and writes a record for each: STMT (where the statement is in the
   text) followed by CMD or SUBCALL, the fallbacks that call the statement's
   own handler.  A comment, and a line with nothing to run (a label alone),
   gets a NOP: the text loop runs its line's head and nothing else, not even
   a tail, and a GOTO or LOOP that lands on it must find it in the map.  END
   closes each image.  All offsets in a record are from the
   statement's entry, which is inside one tokenised line, so a byte holds them.

   The map takes a text position back to its record.  Its key is where the
   text loop stands after stepping over the zero that separates elements,
   which is also where a GOTO, RETURN or NEXT arrives, with RB_LIBBIT for the
   library.  Written in walk order, program first, it is sorted by key.

   The bucket index finds a key in the map without a binary search, which in
   PSRAM cost 11 dependent reads, about 2.5 us (Exile missed the cache on a
   quarter of its statements).  Entry b is the first map entry whose key lies
   in bucket b or later, a bucket being 32 bytes of text, program first, then
   the library's; so a key's entries are those from its bucket's to the next
   bucket's, one or two.  It is written in the same sequential walk.

   Slot layout: header page, map (8 bytes a statement), bucket index, code,
   each from a page boundary so no two writers share a flash page.  A first
   pass counts, so every part's place is known before the second writes.
   --------------------------------------------------------------------------- */
enum
{
    RB_OP_STMT = 1, // key (2 words); flags: RB_LINESTART
    RB_OP_CMD,      // high byte: token offset; cmdtoken; cmdline | nextstmt offsets
    RB_OP_SUBCALL,  // high byte: name offset; cmdline | nextstmt offsets
    RB_OP_END,      // key (2 words) of the image's end
    RB_OP_NOP       // a comment, or a line with nothing to run: the head only
};
#define RB_LINESTART 0x100    // STMT: this statement starts a line (a T_NEWLINE at the entry)
#define RB_COMPILED 0x200     // STMT: its CMD record is followed by compiled code (P2a)
#define RB_LIBBIT 0x80000000u // a key in the library's image
#define RB_BSHIFT 5           // a bucket of the map's index is 32 bytes of text
#define RB_PAGEUP(n) (((n) + RB_PAGE - 1) & ~(RB_PAGE - 1))

static struct
{
    int pass;         // 1: count, 2: write
    uint32_t stmts;   // records
    uint32_t codelen; // bytes of code
    rbwriter_t map, tab, code;
    uint32_t progend, libend; // each image's end, from pass 1
    uint32_t nbprog, ntab;    // the bucket index's size
    uint32_t nextb;           // pass 2: the next bucket to write
    int toolong;              // an offset did not fit its byte: the program runs as text
    uint8_t *types;           // the survey: each name's declared type (see RBSurvey)
    int ntypes;
    int deftype; // OPTION DEFAULT in force at this point of the walk (text order)
} C;

// pass 2: every bucket up to and including b starts at map entry C.stmts
static void RBTabTo(uint32_t b)
{
    uint16_t i = C.stmts;
    while (C.nextb <= b)
    {
        RBPut(&C.tab, &i, sizeof(i));
        C.nextb++;
    }
}

/* ---------------------------------------------------------------------------
   P2a: compiled statements.

   A statement the compiler handles is still a CMD record, whose fields are
   now its fallback, with RB_COMPILED in its STMT word and its code after it:

     [n] [nbind] nbind x [symoff | suffix << 8 | RC_TARGET] [type]  wordcode

   n counts the words after itself.  Each bind names a variable by the offset
   of its symbol from the statement's entry, with the suffix it is written
   with (0, T_INT or T_NBR) and the type the code was compiled for.  The
   executor binds each one exactly as findvar's fast path would find it (the
   local at this level, else the global when no text local can hide it),
   checks that it is a scalar of that type which the reference may name, and
   runs the fallback instead if any check fails, before anything has
   happened; that is also how a variable is first made and bound, by the
   text path.  The wordcode runs on a stack of 64-bit cells.

   LET compiles when its target and every variable on the right are global
   integer or float scalars and the right-hand side is a numeric expression
   of literals, those variables, brackets, unary - + NOT INV and the binary
   operators (P2b).  The compiler mirrors getvalue, doexpr and evaluate step
   by step: the same operator table and precedence, the same order, the same
   promotions and conversions.  A literal is read at compile time by getvalue
   itself.  Float and integer + - * are done inline, with op_add's and
   op_mul's overflow check; every other operator calls its own op_ function
   with the arguments doexpr would give it, so the answer cannot differ.
   What the compiler cannot decide statically stays text: strings,
   functions, arrays, and integer ^ (a negative exponent makes it a float).
   --------------------------------------------------------------------------- */
enum
{
    RC_END,    // the statement is done
    RC_LDG,    // push bound variable a
    RC_STG,    // pop into bound variable a
    RC_LK,     // push the 64-bit constant in the next four words
    RC_CVIF,   // top: integer to float
    RC_CVFI,   // top: float to integer through FloatToInt64
    RC_CVIF2,  // the cell below the top, the same
    RC_CVFI2,
    RC_SHADOW, // OPTION COMPILE SHADOW: compare the top with the text evaluator's result on
               // the text at a, converted to the type in the next word
    RC_ADDF,   // + - * inline, as op_add, op_subtract and op_mul
    RC_ADDI,
    RC_SUBF,
    RC_SUBI,
    RC_MULF,
    RC_MULI,
    RC_OPF,    // any other operator a: its op_ function on two floats
    RC_OPI,    // and on two integers
    RC_NEGF,   // unary -, NOT and INV, as getvalue does them
    RC_NEGI,
    RC_NOTF,
    RC_NOTI,
    RC_INV,
};
#define RC_TARGET 0x8000 // bind: the statement assigns to it
#define RB_MAXBIND 12
#define RB_MAXCODE 96  // words of code one statement may compile to
#define RB_MAXDEPTH 16 // cells of stack it may use

static CommandToken RBTokLet; // the LET command, looked up once a compile
static CommandToken RBTokDim, RBTokLocal, RBTokStatic, RBTokConst, RBTokOption;
static CommandToken RBTokEndSub, RBTokEndFun;
static unsigned char *RBLiteral(unsigned char *p);
static int RBSuffix(unsigned char **p);

/* The survey.  Before the first pass one walk reads every declaration, so an
   unsuffixed name compiles with the type the program gives it rather than a
   guess: DIM, LOCAL and STATIC with a type (DIM INTEGER a, b / DIM a AS
   FLOAT) or a suffix, and CONST with a literal value.  A name declared with
   two different types has none (RB_TMIXED).  A name that is a local
   anywhere - LOCAL, STATIC, a parameter, a FUNCTION's own name, a CONST
   inside a SUB - is marked RB_TLOCAL and not compiled in P2, whose
   statements bind globals only: its bind would fail at every run and cost a
   check on top of the fallback.  Types are kept per canonical entry
   (SymCanonAt), so every spelling of a name shares one.  OPTION DEFAULT is
   followed in text order by the walks themselves (deftype).  A wrong answer
   costs only speed: the bind check sends the statement to its fallback. */
#define RB_TTYPE (T_INT | T_NBR | T_STR) // the declared type's bits
#define RB_TMIXED 0x08                   // declared with two types
#define RB_TLOCAL 0x40                   // a local somewhere

static void RBTypeName(unsigned char *p, int type, int local)
{
    int k = SymCanonAt(p), cur;
    if (k < 0 || k >= C.ntypes)
        return;
    if (local)
    {
        C.types[k] |= RB_TLOCAL;
        return; // a local's type is the unit's business (P3)
    }
    type &= RB_TTYPE;
    cur = C.types[k] & RB_TTYPE;
    if (!type || (C.types[k] & RB_TMIXED))
        return;
    if (cur == 0)
        C.types[k] |= type;
    else if (cur != type)
        C.types[k] = (C.types[k] & ~RB_TTYPE) | RB_TMIXED;
}

// SUB and FUNCTION: every program symbol after the command is a local (the
// parameters, their type words, and a FUNCTION's own name)
static void RBSurveyUnit(unsigned char *p)
{
    for (; *p && *p != '\''; p++)
        if (issymbol(*p) && !(*p & 2))
        {
            RBTypeName(p, 0, 1);
            p += symbolsize(*p) - 1;
        }
}

// the type keyword at p (INTEGER, FLOAT, STRING) or 0; steps *pp over it
static int RBTypeWord(unsigned char **pp)
{
    unsigned char *tp;
    if ((tp = checkstring(*pp, (unsigned char *)"INTEGER")) != NULL)
        return *pp = tp, T_INT;
    if ((tp = checkstring(*pp, (unsigned char *)"FLOAT")) != NULL)
        return *pp = tp, T_NBR;
    if ((tp = checkstring(*pp, (unsigned char *)"STRING")) != NULL)
        return *pp = tp, T_STR;
    return 0;
}

// the next ',' at bracket depth 0 in the element at p, or its end
static unsigned char *RBNextItem(unsigned char *p)
{
    int depth = 0, quote = 0;
    for (; *p; p++)
    {
        if (*p == '"')
            quote = !quote;
        else if (quote)
            continue;
        else if (*p == '(' || (*p >= C_BASETOKEN && (tokentype(*p) & T_FUN)))
            depth++;
        else if (*p == ')')
            depth--;
        else if ((*p == ',' && depth == 0) || *p == '\'')
            break;
    }
    return p;
}

// DIM, LOCAL and STATIC: [AS] [type] name[suffix][(...)] [AS type] [= v], ...
static void RBSurveyDim(unsigned char *p, int local)
{
    int implied, type, suf;
    unsigned char *q;
    skipspace(p);
    if (*p == tokenAS)
        p++;
    skipspace(p);
    implied = RBTypeWord(&p);
    while (1)
    {
        skipspace(p);
        if (!issymbol(*p) || (*p & 2))
            return; // a name the survey cannot read ends it
        q = p + symbolsize(*p);
        suf = RBSuffix(&q);
        type = suf ? suf : implied;
        if (*q == '(')
        { // the dimensions (getclosebracket would raise an error on bad text: the survey must not)
            int d = 0;
            for (; *q; q++)
                if (*q == '(' || (*q >= C_BASETOKEN && (tokentype(*q) & T_FUN)))
                    d++;
                else if (*q == ')' && --d == 0)
                {
                    q++;
                    break;
                }
        }
        skipspace(q);
        if (*q == tokenAS)
        {
            q++;
            skipspace(q);
            if (!type)
                type = RBTypeWord(&q);
        }
        RBTypeName(p, type, local);
        p = RBNextItem(q);
        if (*p != ',')
            return;
        p++;
    }
}

// CONST name = literal, ...: the literal's type, as getvalue reads it
static void RBSurveyConst(unsigned char *p, int local)
{
    unsigned char *q, *v;
    int suf, t;
    while (1)
    {
        skipspace(p);
        if (!issymbol(*p) || (*p & 2))
            return;
        q = p + symbolsize(*p);
        suf = RBSuffix(&q);
        skipspace(q);
        if (*q != tokenEQUAL)
            return;
        v = q + 1;
        skipspace(v);
        t = suf;
        if (!t && *v == '"')
            t = T_STR;
        else if (!t && (q = RBLiteral(v)) != NULL)
        {
            unsigned char *e = q;
            skipspace(e);
            if (*e == 0 || *e == ',' || *e == '\'')
            { // a literal and nothing more: digits alone are an integer, as getvalue has it
                t = T_INT;
                for (; v < q; v++)
                    if (*v == '.' || *v == 'E' || *v == 'e')
                        t = T_NBR;
            }
        }
        RBTypeName(p, t, local);
        p = RBNextItem(v);
        if (*p != ',')
            return;
        p++;
    }
}

// OPTION DEFAULT type at cmdl: the new default, or -1 if this is another OPTION
static int RBDefaultOption(unsigned char *p)
{
    unsigned char *tp = checkstring(p, (unsigned char *)"DEFAULT");
    if (tp == NULL)
        return -1;
    if (checkstring(tp, (unsigned char *)"INTEGER"))
        return T_INT;
    if (checkstring(tp, (unsigned char *)"FLOAT"))
        return T_NBR;
    if (checkstring(tp, (unsigned char *)"STRING"))
        return T_STR;
    return T_NOTYPE; // NONE, or anything the text path will reject
}
unsigned char *getvalue(unsigned char *p, MMFLOAT *fa, long long int *ia, unsigned char **sa, int *oo, int *ta); // MMBasic.c

static void RBEmitWords(const uint16_t *w, int n)
{
    if (C.pass == 2)
        RBPut(&C.code, w, n * 2);
    C.codelen += n * 2;
}

// A numeric literal the compiler can read: digits with an optional point and
// exponent, at most 15 digits, then the end of the statement.  Returns the
// end of the literal, or NULL.
static unsigned char *RBLiteral(unsigned char *p)
{
    int digits = 0;
    while (*p >= '0' && *p <= '9')
        p++, digits++;
    if (*p == '.')
        for (p++; *p >= '0' && *p <= '9'; p++)
            digits++;
    if (digits == 0 || digits > 15)
        return NULL;
    if (*p == 'E' || *p == 'e')
    {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!(*p >= '0' && *p <= '9'))
            return NULL;
        while (*p >= '0' && *p <= '9')
            p++;
    }
    return p;
}

// The type suffix written after a name, as findvar reads it: 0, T_INT, T_NBR,
// or T_STR (which the compiler leaves alone).  Steps *p over it.
static int RBSuffix(unsigned char **p)
{
    int t = **p == '%' ? T_INT : **p == '!' ? T_NBR : **p == '$' ? T_STR : 0;
    if (t)
        (*p)++;
    return t;
}

// A variable the compiler can bind: a program symbol whose spelling has no
// '.' (a structure member path), with its suffix, not followed by a bracket.
// Returns the byte after it, or NULL.
static unsigned char *RBVarRef(unsigned char *p, int *suffix)
{
    const unsigned char *sp;
    int len;
    if (!issymbol(*p) || (*p & 2))
        return NULL;
    sp = SymSpelling(p, &len);
    if (memchr(sp, '.', len))
        return NULL;
    p += symbolsize(*p);
    *suffix = RBSuffix(&p);
    if (*suffix == T_STR || *p == '(' || *p == '.')
        return NULL;
    return p;
}

// One statement's compilation.
typedef struct
{
    unsigned char *entry;
    int nbind;
    uint16_t bind[RB_MAXBIND][2];
    uint16_t w[RB_MAXCODE];
    int n, depth, maxdepth, fail;
} rbcx_t;

static void RBOp(rbcx_t *x, int w, int cells)
{
    if (x->n >= RB_MAXCODE)
    {
        x->fail = 1;
        return;
    }
    x->w[x->n++] = w;
    x->depth += cells;
    if (x->depth > x->maxdepth)
        x->maxdepth = x->depth;
}

// The bind of the variable whose symbol is at p, made if it is new; -1 if
// there is no room.  A symbol with its suffix is one variable.
static int RBBind(rbcx_t *x, unsigned char *p, int suffix, int target)
{
    int off = p - x->entry, j;
    if (off > 255)
        return -1;
    for (j = 0; j < x->nbind; j++)
        if (((x->bind[j][0] >> 8) & 0x7F) == suffix &&
            !memcmp(x->entry + (x->bind[j][0] & 0xFF), p, symbolsize(*p)))
        {
            if (target)
                x->bind[j][0] |= RC_TARGET;
            return j;
        }
    if (x->nbind >= RB_MAXBIND)
        return -1;
    {
        int k = SymCanonAt(p), ty = (k >= 0 && k < C.ntypes) ? C.types[k] : 0, t = ty & RB_TTYPE;
        if (ty & RB_TLOCAL)
            return -1; // a local somewhere: P2 compiles globals only
        if (!suffix)
        { // an unsuffixed name: its declared type, else the OPTION DEFAULT in force,
          // else (DEFAULT NONE with the declaration out of sight, in a library say,
          // or two declarations) a guess of float, which the bind check verifies
            if (t == T_STR)
                return -1;
            if ((ty & RB_TMIXED) || (t != T_INT && t != T_NBR))
                t = (C.deftype == T_INT || C.deftype == T_NBR) ? C.deftype : T_NBR;
            x->bind[j][1] = t;
        }
        else
            x->bind[j][1] = suffix;
    }
    x->bind[j][0] = off | (suffix << 8) | (target ? RC_TARGET : 0);
    x->nbind++;
    return j;
}

// the operator after a value, as getvalue reads it
static unsigned char *RBNextOp(unsigned char *p, int *op)
{
    skipspace(p);
    if (tokentype(*p) & T_OPER)
        *op = *p++ - C_BASETOKEN;
    else
        *op = E_END;
    return p;
}

static int RBEvaluate(rbcx_t *x, unsigned char **pp);

// getvalue: one value, and the operator after it in *op.  Returns its type,
// T_INT or T_NBR, or 0 if the compiler cannot take it.
static int RBValue(rbcx_t *x, unsigned char **pp, int *op)
{
    unsigned char *p = *pp, c;
    int t, j, suf;
    skipspace(p);
    c = *p;
    if (c >= C_BASETOKEN)
    {
        void (*fn)(void);
        if (c > 131)
            return 0;
        fn = tokenfunction(c);
        if (fn != op_not && fn != op_inv && fn != op_subtract && fn != op_add)
            return 0; // a function, or anything else: P5 and later
        p++;
        if ((t = RBValue(x, &p, op)) == 0)
            return 0;
        if (fn == op_not)
            RBOp(x, t == T_NBR ? RC_NOTF : RC_NOTI, 0);
        else if (fn == op_inv)
        {
            if (t == T_NBR)
                RBOp(x, RC_CVFI, 0);
            RBOp(x, RC_INV, 0);
            t = T_INT;
        }
        else if (fn == op_subtract)
            RBOp(x, t == T_NBR ? RC_NEGF : RC_NEGI, 0);
        *pp = p;
        return t;
    }
    if (isnamestartsym(c))
    {
        unsigned char *q = RBVarRef(p, &suf);
        if (q == NULL || (j = RBBind(x, p, suf, 0)) < 0)
            return 0;
        RBOp(x, RC_LDG | (j << 8), 1);
        *pp = RBNextOp(q, op);
        return x->bind[j][1];
    }
    if ((c >= '0' && c <= '9') || c == '.')
    { // a decimal literal, read by getvalue itself (whose number reader raises
      // no error, so compiling cannot fail a RUN; &H and the like stay text)
        MMFLOAT f;
        long long i64;
        unsigned char *str;
        union
        {
            long long i;
            MMFLOAT f;
            uint16_t w[4];
        } k;
        if (RBLiteral(p) == NULL)
            return 0;
        p = getvalue(p, &f, &i64, &str, op, &t);
        if (t & T_NBR)
            k.f = f, t = T_NBR;
        else if (t & T_INT)
            k.i = i64, t = T_INT;
        else
            return 0;
        RBOp(x, RC_LK, 1);
        for (j = 0; j < 4; j++)
            RBOp(x, k.w[j], 0);
        *pp = p;
        return t;
    }
    if (c == '(')
    {
        p++;
        if ((t = RBEvaluate(x, &p)) == 0 || *p != ')')
            return 0;
        *pp = RBNextOp(p + 1, op);
        return t;
    }
    return 0;
}

// doexpr's step: the operator in *o1 with the left value of type *t1 already
// on the stack.  Leaves the result's type in *t1 and the next operator in *o1.
static int RBDoExpr(rbcx_t *x, unsigned char **pp, int *t1, int *o1)
{
    const struct s_tokentbl *op;
    void (*fn)(void);
    int o2, t2, ty, targ, a = *t1;
    if ((t2 = RBValue(x, pp, &o2)) == 0)
        return 0;
    while (o2 != E_END && tokentbl[*o1].precedence > tokentbl[o2].precedence)
        if (!RBDoExpr(x, pp, &t2, &o2)) // the next operator binds tighter
            return 0;
    op = &tokentbl[*o1];
    fn = op->fptr;
    ty = op->type;
    targ = ty & (T_NBR | T_INT);
    if (targ == T_NBR)
    {
        if (a == T_INT)
            RBOp(x, RC_CVIF2, 0), a = T_NBR;
        if (t2 == T_INT)
            RBOp(x, RC_CVIF, 0), t2 = T_NBR;
    }
    else if (targ == T_INT)
    {
        if (a == T_NBR)
            RBOp(x, RC_CVFI2, 0), a = T_INT;
        if (t2 == T_NBR)
            RBOp(x, RC_CVFI, 0), t2 = T_INT;
    }
    else if (a == T_NBR && t2 == T_INT)
        RBOp(x, RC_CVIF, 0), t2 = T_NBR;
    else if (a == T_INT && t2 == T_NBR)
        RBOp(x, RC_CVIF2, 0), a = T_NBR;
    if (!(ty & T_OPER) || !(ty & a))
        return 0; // "Invalid operator": the text path raises it
    if (fn == op_add)
        RBOp(x, a == T_NBR ? RC_ADDF : RC_ADDI, -1);
    else if (fn == op_subtract)
        RBOp(x, a == T_NBR ? RC_SUBF : RC_SUBI, -1);
    else if (fn == op_mul)
        RBOp(x, a == T_NBR ? RC_MULF : RC_MULI, -1);
    else if (fn == op_div || (fn == op_exp && a == T_NBR))
        RBOp(x, RC_OPF | (*o1 << 8), -1), a = T_NBR;
    else if (fn == op_ne || fn == op_equal || fn == op_gte || fn == op_lte || fn == op_lt || fn == op_gt)
        RBOp(x, (a == T_NBR ? RC_OPF : RC_OPI) | (*o1 << 8), -1), a = T_INT;
    else if (fn == op_divint || fn == op_mod || fn == op_shiftleft || fn == op_shiftright ||
             fn == op_and || fn == op_or || fn == op_xor)
        RBOp(x, RC_OPI | (*o1 << 8), -1), a = T_INT;
    else
        return 0; // integer ^, and anything else whose type is not certain
    *t1 = a;
    *o1 = o2;
    return 1;
}

// evaluate, without its end check: the expression's type, or 0
static int RBEvaluate(rbcx_t *x, unsigned char **pp)
{
    int o, t = RBValue(x, pp, &o);
    while (t && o != E_END)
        if (!RBDoExpr(x, pp, &t, &o))
            return 0;
    return x->fail ? 0 : t;
}

// Compile LET g = expression into code[] as [nbind] binds wordcode; returns
// the words, or 0 to leave the statement to its fallback.
static int RBCompileLet(unsigned char *entry, unsigned char *p, uint16_t *code)
{
    rbcx_t x;
    unsigned char *q, *rhs;
    int tsuf, tgt, ttype, t, n = 0, j;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    if ((q = RBVarRef(p, &tsuf)) == NULL || (tgt = RBBind(&x, p, tsuf, 1)) < 0)
        return 0;
    ttype = x.bind[tgt][1];
    p = q;
    skipspace(p);
    if (*p != tokenEQUAL)
        return 0;
    p++;
    skipspace(p);
    rhs = p;
    if (rhs - entry > 255 || (t = RBEvaluate(&x, &p)) == 0)
        return 0;
    skipspace(p);
    if (*p && *p != '\'')
        return 0; // evaluate's and checkend's errors are the text path's
    if (t != ttype) // as evaluate converts for cmd_let's type, then cmd_let stores it
        RBOp(&x, ttype == T_NBR ? RC_CVIF : RC_CVFI, 0);
    RBOp(&x, RC_SHADOW | ((rhs - entry) << 8), 0);
    RBOp(&x, ttype, 0);
    RBOp(&x, RC_STG | (tgt << 8), -1);
    RBOp(&x, RC_END, 0);
    if (x.fail || x.maxdepth > RB_MAXDEPTH || 1 + 2 * x.nbind + x.n > RB_MAXCODE)
        return 0;
    code[n++] = x.nbind;
    for (j = 0; j < x.nbind; j++)
    {
        code[n++] = x.bind[j][0];
        code[n++] = x.bind[j][1];
    }
    memcpy(code + n, x.w, x.n * 2);
    return n + x.n;
}

static void RBEmitStmt(unsigned char *base, uint32_t libbit, unsigned char *entry, int linestart,
                       unsigned char *tok, int cmd, unsigned char *cmdl, unsigned char *next)
{
    uint16_t code[RB_MAXCODE + 2];
    int ncode = 0;
    uint32_t key = (uint32_t)(entry - base) | libbit;
    uint16_t w[6];
    int n = 0;
    if (tok - entry > 255 || cmdl - entry > 255 || next - entry > 255)
    {
        C.toolong = 1;
        return;
    }
    if (C.pass == 2)
    {
        uint32_t m[2];
        RBTabTo((libbit ? C.nbprog : 0) + ((entry - base) >> RB_BSHIFT));
        m[0] = key;
        m[1] = C.code.pos; // the record's offset in the slot
        RBPut(&C.map, m, sizeof(m));
    }
    if (cmd > 0)
    {
        CommandToken ct = commandtbl_decode(tok);
        int d;
        if (ct == RBTokLet)
            ncode = RBCompileLet(entry, cmdl, code + 1);
        else if (ct == RBTokOption && (d = RBDefaultOption(cmdl)) >= 0)
            C.deftype = d;
    }
    w[n++] = RB_OP_STMT | (linestart ? RB_LINESTART : 0) | (ncode ? RB_COMPILED : 0);
    w[n++] = key & 0xFFFF;
    w[n++] = key >> 16;
    if (cmd > 0)
    {
        w[n++] = RB_OP_CMD | ((tok - entry) << 8);
        w[n++] = commandtbl_decode(tok);
    }
    else if (cmd == 0)
        w[n++] = RB_OP_SUBCALL | ((tok - entry) << 8);
    else
        w[n++] = RB_OP_NOP;
    if (cmd >= 0)
        w[n++] = (cmdl - entry) | ((next - entry) << 8);
    RBEmitWords(w, n);
    if (ncode)
    {
        code[0] = ncode;
        RBEmitWords(code, ncode + 1);
    }
    C.stmts++;
}

// Walk one image as ExecuteProgram would, recording each statement.
static void RBWalk(unsigned char *base, uint32_t libbit)
{
    unsigned char *p = base, *entry, *tok, *cmdl, *next;
    int linestart, cmd;
    uint32_t endkey;
    uint16_t w[3];
    skipspace(p);
    while (1)
    {
        if (*p == 0)
            p++; // the zero that begins an element
        entry = p;
        linestart = (*p == T_NEWLINE);
        if (linestart)
            p += T_NEWLINE_HDR;
        if (*p == T_LINENBR)
            p += 3;
        skipspace(p);
        if (p[0] == T_LABEL)
        {
            p += p[1] + 2;
            skipspace(p);
        }
        if (*p)
        {
            tok = p;
            if (*p == '\'')
            { // a comment: the text loop runs its line's head, nothing more
                cmdl = next = p + 1;
                cmd = -1;
            }
            else if (p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN)
            {
                cmdl = next = p + sizeof(CommandToken);
                cmd = 1;
            }
            else
            { // a user SUB's name, perhaps one letter
                cmdl = next = p;
                cmd = 0;
            }
            skipspace(cmdl);
            skipelement(next);
            RBEmitStmt(base, libbit, entry, linestart, tok, cmd, cmdl, next);
            p = next;
        }
        else if (linestart) // a label or a line number alone
            RBEmitStmt(base, libbit, entry, linestart, p, -1, p, p);
        if ((p[0] == 0 && p[1] == 0) || (p[0] == 0xff && p[1] == 0xff))
            break; // the end of the image
    }
    endkey = (uint32_t)(p - base) | libbit;
    if (libbit)
        C.libend = p - base;
    else
        C.progend = p - base;
    w[0] = RB_OP_END;
    w[1] = endkey & 0xFFFF;
    w[2] = endkey >> 16;
    RBEmitWords(w, 3);
}

static void RBWalkAll(void)
{
    C.stmts = 0;
    C.codelen = 0;
    C.nextb = 0;
    C.deftype = T_NBR; // as ClearRuntime leaves DefaultType at RUN
    RBWalk(ProgMemory, 0);
    if (C.pass == 2)
        RBTabTo(C.nbprog - 1); // the program's last buckets end where the library's entries start
    if (LibPresent())
        RBWalk(LibMemory, RB_LIBBIT);
    if (C.pass == 2)
        RBTabTo(C.ntab - 1); // and the library's, and the sentinel, at the end of the map
}

// The survey's walk over the program: every DIM, LOCAL, STATIC and CONST.
static void RBSurvey(void)
{
    unsigned char *p = ProgMemory, *cmdl;
    CommandToken ct;
    int inunit = 0;
    C.ntypes = SymCanonOf ? SymCanonCount : 0;
    C.types = C.ntypes ? GetTempMemory(C.ntypes) : NULL; // zeroed: no type known
    if (!C.types)
    {
        C.ntypes = 0;
        return;
    }
    skipspace(p);
    while (1)
    {
        if (*p == 0)
            p++;
        if (*p == T_NEWLINE)
            p += T_NEWLINE_HDR;
        if (*p == T_LINENBR)
            p += 3;
        skipspace(p);
        if (p[0] == T_LABEL)
        {
            p += p[1] + 2;
            skipspace(p);
        }
        if (*p && *p != '\'')
        {
            if (p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN)
            {
                ct = commandtbl_decode(p);
                cmdl = p + sizeof(CommandToken);
                if (ct == RBTokDim)
                    RBSurveyDim(cmdl, 0); // DIM makes a global, even inside a SUB
                else if (ct == RBTokLocal || ct == RBTokStatic)
                    RBSurveyDim(cmdl, 1);
                else if (ct == RBTokConst)
                    RBSurveyConst(cmdl, inunit);
                else if (ct == cmdSUB || ct == cmdFUN)
                {
                    inunit = 1;
                    RBSurveyUnit(cmdl);
                }
                else if (ct == RBTokEndSub || ct == RBTokEndFun)
                    inunit = 0;
            }
            skipelement(p);
        }
        else if (*p)
            skipelement(p);
        if ((p[0] == 0 && p[1] == 0) || (p[0] == 0xff && p[1] == 0xff))
            break;
    }
}

// Compile the program (and the library) into the slot.
static void RBCompile(rbheader_t *h)
{
    uint32_t codeoff;
    memset(&C, 0, sizeof(C));
    W.full = 0;
    RBTokLet = GetCommandValue((unsigned char *)"Let");
    RBTokDim = GetCommandValue((unsigned char *)"Dim");
    RBTokLocal = GetCommandValue((unsigned char *)"Local");
    RBTokStatic = GetCommandValue((unsigned char *)"Static");
    RBTokConst = GetCommandValue((unsigned char *)"Const");
    RBTokOption = GetCommandValue((unsigned char *)"Option");
    RBTokEndSub = GetCommandValue((unsigned char *)"End Sub");
    RBTokEndFun = GetCommandValue((unsigned char *)"End Function");
    RBSurvey();
    C.pass = 1;
    RBWalkAll();
    C.nbprog = (C.progend >> RB_BSHIFT) + 1;
    C.ntab = C.nbprog + (LibPresent() ? (C.libend >> RB_BSHIFT) + 1 : 0) + 1;
    h->stmts = C.stmts;
    h->mapoff = RB_PAGE;
    h->maplen = C.stmts * 8;
    h->tabof = RB_PAGEUP(h->mapoff + h->maplen);
    h->nbprog = C.nbprog;
    h->ntab = C.ntab;
    codeoff = RB_PAGEUP(h->tabof + C.ntab * 2);
    h->codeoff = codeoff;
    h->codelen = C.codelen;
    if (C.toolong || C.stmts > 0xFFFF || codeoff + C.codelen > RBSlotSize())
    {
        W.full = 1;
        return;
    }
    RBWriteBegin(codeoff + C.codelen);
    RBWriterStart(&C.map, h->mapoff);
    RBWriterStart(&C.tab, h->tabof);
    RBWriterStart(&C.code, codeoff);
    C.pass = 2;
    RBWalkAll();
    RBFlush(&C.map);
    RBFlush(&C.tab);
    RBFlush(&C.code);
    RBWriteEnd(h);
}

/* ---------------------------------------------------------------------------
   P1d: the executor.

   ExecuteProgram hands over at the top of its loop whenever a live stream
   exists and p lies in the program or library image: on entry (RUN, or a
   FUNCTION's body) and after each statement it ran itself.  RunStream starts
   at that statement's head: the text loop has already run the tail of the
   statement before.

   Each STMT runs the tail of the statement before it, exactly as the text
   loop does after a statement (the ON ERROR SKIP count, temporary memory, the
   core 1 stack check, CheckAbort and check_interrupt), with nextstmt set to
   this statement's text first, so an interrupt or CTRL-C records what it
   would from the text loop.  If the tail moved nextstmt (an interrupt), the
   executor goes there instead, entering at the head.

   CMD and SUBCALL set cmdline, nextstmt, cmdtoken and CmdTokenPtr as the text
   loop does and call the statement's own handler.  If the handler left
   nextstmt where the statement ends, the next record follows; otherwise the
   new nextstmt is looked up in the map.  A position the map does not hold
   (the statement after THEN, say), one outside the images, and the END of an
   image go back to the text loop, after the pending tail.

   The executor's position is in locals.  ON ERROR SKIP's setjmp is armed in
   RBExecSkip's own frame, so its longjmp returns there and RunStream's
   registers come back through an ordinary return.  A FUNCTION called in a
   statement runs its body in a nested ExecuteProgram and so a nested
   RunStream, with a frame of its own.
   --------------------------------------------------------------------------- */
// The executor runs for every statement, so it lives in RAM, as ExecuteProgram
// does: from flash it and the handlers it calls evict each other from the XIP
// cache (on the RP2040, pixart ran 29% slower than text, most of it in
// RBTail/RBExec fetches and in findvar lines they pushed out)
#define RBRAM(f) __not_in_flash_func(f)
// Map lookups remembered, so a loop driven by a fallback NEXT finds its
// target here.  Exile misses on a quarter of its statements with 64 entries
// and on 15% with 512: its jump targets outnumber any cache RAM allows, so
// the miss path itself has to be cheap (the bucket index).  A lookup that
// found nothing is remembered too, as RB_ABSENT: a single-line IF that is
// true sends nextstmt to the part after THEN, which no record starts, and
// without this each one cost a bucket search.
#define RB_CACHE_BITS 6
#define RB_CACHE (1 << RB_CACHE_BITS)
static const uint16_t RBAbsent[1];
#define RB_ABSENT RBAbsent

static struct
{
    uint32_t key;
    const uint16_t *rec;
} RBCache[RB_CACHE];

static const uint8_t *RBBase;          // the live stream's slot
static const uint32_t *RBMap;          // its statement map: key, record offset
static const uint16_t *RBTab;          // the map's bucket index
static uint32_t RBNbProg, RBNTab;      // its program buckets, and all its entries
extern uint32_t core1stack[];
extern int TraceOn;
extern unsigned char *TraceBuff[TRACE_BUFF_SIZE];
extern int TraceBuffIndex;
extern uint32_t g_perf_usercmd_count;
#define PERF_CMDTOKEN_MAX 1024 // as in MMBasic.c

static void RBMapStream(void)
{
    const rbheader_t *h = (const rbheader_t *)RBSlotBase();
    RBBase = RBSlotBase();
    RBMap = (const uint32_t *)(RBBase + h->mapoff);
    RBTab = (const uint16_t *)(RBBase + h->tabof);
    RBNbProg = h->nbprog;
    RBNTab = h->ntab;
    memset(RBCache, 0, sizeof(RBCache));
}

static inline unsigned char *RBImage(uint32_t key)
{
    return (key & RB_LIBBIT ? LibMemory : ProgMemory) + (key & ~RB_LIBBIT);
}

static inline uint32_t RBKeyAt(const uint16_t *r)
{
    return r[1] | ((uint32_t)r[2] << 16);
}

int RBRAM(RBInImage)(unsigned char *p)
{
    return (p >= ProgMemory && p < ProgMemory + MAX_PROG_SIZE) ||
           (LibPresent() && p >= LibMemory && p < LibMemory + MAX_PROG_SIZE);
}

// The record for the text position p, or NULL.  p is normalised as the text
// loop's first step does: over the zero that begins an element.
static const uint16_t *RBRAM(RBFind)(unsigned char *p)
{
    uint32_t key, b, i, n;
    int c;
    if (!RBLive)
        return NULL;
    if (*p == 0)
        p++;
    if (p >= ProgMemory && p < ProgMemory + MAX_PROG_SIZE)
        key = p - ProgMemory;
    else if (LibPresent() && p >= LibMemory && p < LibMemory + MAX_PROG_SIZE)
        key = (p - LibMemory) | RB_LIBBIT;
    else
        return NULL;
    c = (key * 2654435761u) >> (32 - RB_CACHE_BITS);
    if (RBCache[c].rec && RBCache[c].key == key)
        return RBCache[c].rec == RB_ABSENT ? NULL : RBCache[c].rec;
    RBMiss++;
    b = (key & RB_LIBBIT ? RBNbProg : 0) + ((key & ~RB_LIBBIT) >> RB_BSHIFT);
    RBCache[c].key = key;
    RBCache[c].rec = RB_ABSENT;
    if (b + 1 >= RBNTab)
        return NULL;
    for (i = RBTab[b], n = RBTab[b + 1]; i < n; i++)
        if (RBMap[i * 2] == key)
            break;
    if (i >= n)
        return NULL;
    RBCache[c].rec = (const uint16_t *)(RBBase + RBMap[i * 2 + 1]);
    return RBCache[c].rec;
}

// The text loop's tail after a statement, with nextstmt at here.  True if it
// moved nextstmt (an interrupt).
static int RBRAM(RBTail)(unsigned char *here)
{
    nextstmt = here;
    if (OptionErrorSkip > 0 && OptionErrorSkip < 100000)
        OptionErrorSkip--;
    if (g_TempMemoryIsChanged)
        ClearTempMemory();
#ifndef PICOMITEWEB
    if (core1stack[0] != 0x12345678)
        error("CPU2 Stack overflow");
#endif
    if (!OptionNoCheck)
    {
        CheckAbort();
        check_interrupt();
    }
    return nextstmt != here;
}

// Run the statement whose STMT record is r and whose text starts at e, as
// the text loop would.  Returns where the statement ends, the nextstmt it was
// given: if the handler leaves nextstmt there, the next record follows.
static unsigned char *RBRAM(RBExec)(const uint16_t *r, unsigned char *e)
{
    unsigned char *p, *end;
    int i;
    RBRan++;
    if ((r[3] & 0xFF) == RB_OP_CMD)
    {
        p = e + (r[3] >> 8);
        cmdline = e + (r[5] & 0xFF);
        nextstmt = end = e + (r[5] >> 8);
        cmdtoken = r[4];
        if (g_perf_cmdcount && cmdtoken < PERF_CMDTOKEN_MAX)
            g_perf_cmdcount[cmdtoken]++;
        targ = T_CMD;
        CmdTokenPtr = p;
        if (!SymAwareCommand(cmdtoken))
            cmdline = SymExpandStatement(cmdline);
        commandtbl[cmdtoken].fptr();
    }
    else
    { // a call to a user SUB
        p = e + (r[3] >> 8);
        cmdline = e + (r[4] & 0xFF);
        nextstmt = end = e + (r[4] >> 8);
        if (!isnamestartsym(*p) && *p == '~')
            StandardError(36);
        else if (!isnamestartsym(*p))
            error("Invalid character: @", (int)(*p));
        i = FindSubFun(p, false);
        if (i >= 0)
        {
            if (g_option_profiling)
            {
                g_perf_usercmd_count++;
                if (g_perf_subcall_count && i < MAXSUBFUN)
                    g_perf_subcall_count[i]++;
            }
            DefinedSubFun(false, p, i, NULL, NULL, NULL, NULL);
        }
        else
            StandardError(36);
    }
    return end;
}

// OPTION COMPILE SHADOW: evaluate the right-hand side at p through the text
// evaluator, convert it as cmd_let would for type, and stop if the compiled
// value v differs from it in any bit.
static void RBShadow(unsigned char *p, int type, const void *v)
{
    MMFLOAT f;
    long long i64;
    unsigned char *str;
    int t = type;
    union
    {
        long long i;
        MMFLOAT f;
    } x, c;
    char a[40], b[40];
    evaluate(p, &f, &i64, &str, &t, false);
    if (type == T_NBR)
        x.f = (t & T_NBR) ? f : (MMFLOAT)i64;
    else
        x.i = (t & T_INT) ? i64 : FloatToInt64(f);
    memcpy(&c, v, sizeof(c));
    if (x.i == c.i)
        return;
    if (type == T_NBR)
    {
        FloatToStr(a, c.f, 0, STR_AUTO_PRECISION, ' ');
        FloatToStr(b, x.f, 0, STR_AUTO_PRECISION, ' ');
    }
    else
    {
        IntToStr(a, c.i, 10);
        IntToStr(b, x.i, 10);
    }
    error("SHADOW: compiled $, text $", a, b);
}

// Bind a compiled statement's variables and run its code.  Returns the end
// it gave nextstmt, or NULL if a bind failed and the fallback must run.
static unsigned char *RBRAM(RBRun)(const uint16_t *r, unsigned char *e)
{
    const uint16_t *c = r + 7, *pc;
    union cell
    {
        long long i;
        MMFLOAT f;
    } * slot[RB_MAXBIND], st[RB_MAXDEPTH], *sp = st;
    unsigned int nb = *c++, j, w;
    for (j = 0; j < nb; j++, c += 2)
    {
        int k = SymCanonAt(e + (c[0] & 0xFF)), i, suf = (c[0] >> 8) & 0x7F;
        struct s_vartbl *v;
        if (k < 0)
            return NULL;
        i = SymL[k];
        if (i >= 0 && g_vartbl[i].level == g_LocalIndex)
            return NULL; // a local: P2a compiles globals only
        i = SymG[k];
        if (i < 0 || (g_LocalIndex && SymTextLocals))
            return NULL; // not bound yet, or a text local may hide it: findvar decides
        v = &g_vartbl[i];
        if (!DimIsScalar(RAW_DIM(*v, 0)) || (v->type & (T_PTR | T_STRUCT | T_STR)) ||
            (v->type & (T_INT | T_NBR)) != c[1] ||
            (suf ? !(v->type & suf) : !(v->type & (DefaultType | T_IMPLIED))) ||
            ((c[0] & RC_TARGET) && (v->type & T_CONST)))
            return NULL;
        slot[j] = (union cell *)&v->val;
    }
    for (pc = c;;)
    {
        w = *pc++;
        switch (w & 0xFF)
        {
        case RC_LDG:
            *sp++ = *slot[w >> 8];
            break;
        case RC_STG:
            *slot[w >> 8] = *--sp;
            break;
        case RC_LK:
            memcpy(sp++, pc, 8);
            pc += 4;
            break;
        case RC_CVIF:
            sp[-1].f = (MMFLOAT)sp[-1].i;
            break;
        case RC_CVFI:
            sp[-1].i = FloatToInt64(sp[-1].f);
            break;
        case RC_CVIF2:
            sp[-2].f = (MMFLOAT)sp[-2].i;
            break;
        case RC_CVFI2:
            sp[-2].i = FloatToInt64(sp[-2].f);
            break;
        case RC_SHADOW:
            if (RBMode == RB_SHADOW)
                RBShadow(e + (w >> 8), *pc, &sp[-1]);
            pc++;
            break;
        case RC_ADDF:
        {
            MMFLOAT r = sp[-2].f + sp[-1].f;
            if (r == INFINITY)
                StandardError(15);
            (--sp)[-1].f = r;
            break;
        }
        case RC_ADDI:
            sp[-2].i = sp[-2].i + sp[-1].i;
            sp--;
            break;
        case RC_SUBF:
            sp[-2].f = sp[-2].f - sp[-1].f;
            sp--;
            break;
        case RC_SUBI:
            sp[-2].i = sp[-2].i - sp[-1].i;
            sp--;
            break;
        case RC_MULF:
        {
            MMFLOAT r = sp[-2].f * sp[-1].f;
            if (r == INFINITY)
                StandardError(15);
            (--sp)[-1].f = r;
            break;
        }
        case RC_MULI:
            sp[-2].i = sp[-2].i * sp[-1].i;
            sp--;
            break;
        case RC_OPF: // doexpr's call, on floats
            farg1 = sp[-2].f;
            farg2 = sp[-1].f;
            targ = T_NBR;
            tokentbl[w >> 8].fptr();
            sp--;
            if (targ & T_NBR)
                sp[-1].f = fret;
            else
                sp[-1].i = iret;
            break;
        case RC_OPI: // and on integers
            iarg1 = sp[-2].i;
            iarg2 = sp[-1].i;
            targ = T_INT;
            tokentbl[w >> 8].fptr();
            sp--;
            if (targ & T_NBR)
                sp[-1].f = fret;
            else
                sp[-1].i = iret;
            break;
        case RC_NEGF:
            sp[-1].f = -sp[-1].f;
            break;
        case RC_NEGI:
            sp[-1].i = -sp[-1].i;
            break;
        case RC_NOTF:
            sp[-1].f = (sp[-1].f != 0) ? 0 : 1;
            break;
        case RC_NOTI:
            sp[-1].i = (sp[-1].i != 0) ? 0 : 1;
            break;
        case RC_INV:
            sp[-1].i = ~sp[-1].i;
            break;
        default: // RC_END
            RBRan++;
            RBCode++;
            return nextstmt = e + (r[5] >> 8);
        }
    }
}

// RBExec while ON ERROR SKIP/IGNORE is in force: an error in the statement
// comes back here, as it does to the text loop's setjmp, and the statement
// counts as run.  Its own frame holds the jmp_buf's registers.
static __attribute__((noinline)) unsigned char *RBRAM(RBExecSkip)(const uint16_t *r, unsigned char *e)
{
    int save = g_LocalIndex;
    if (setjmp(ErrNext) == 0)
        return RBExec(r, e);
    g_LocalIndex = save; // clean up after the error, as the text loop does
    ClearTempMemory();
    return e + ((r[3] & 0xFF) == RB_OP_CMD ? r[5] >> 8 : r[4] >> 8);
}

unsigned char *RBRAM(RunStream)(unsigned char *p)
{
    const uint16_t *r, *t;
    unsigned char *entry, *end, *ret;
    int first = 1; // entering: the text loop has run the tail
    r = RBFind(p);
    if (r == NULL)
        return p;
    for (;;)
    {
        if ((r[0] & 0xFF) == RB_OP_END)
        { // the end of an image: the text loop sees it and stops
            ret = RBImage(RBKeyAt(r));
            if (!first && RBTail(ret))
            {
                if ((t = RBFind(nextstmt)) != NULL)
                {
                    r = t;
                    first = 1;
                    continue;
                }
                ret = nextstmt;
            }
            return ret;
        }
        // STMT: the tail of the statement before, then this one's head
        entry = RBImage(RBKeyAt(r));
        if (!first && RBTail(entry))
        { // an interrupt: go to its handler
            if ((t = RBFind(nextstmt)) != NULL)
            {
                r = t;
                first = 1;
                continue;
            }
            return nextstmt;
        }
        first = 0;
        if (r[0] & RB_LINESTART)
        {
            CurrentLinePtr = entry; // the line's T_NEWLINE, for errors
            TraceBuff[TraceBuffIndex] = entry;
            if (++TraceBuffIndex >= TRACE_BUFF_SIZE)
                TraceBuffIndex = 0;
            if (TraceOn && entry > ProgMemory && entry < ProgMemory + MAX_PROG_SIZE)
            {
                inpbuf[0] = '[';
                IntToStr((char *)inpbuf + 1, CountLines(entry), 10);
                strcat((char *)inpbuf, "]");
                MMPrintString((char *)inpbuf);
                uSec(1000);
            }
        }
        if ((r[3] & 0xFF) == RB_OP_NOP)
        { // no statement, so no tail after it either
            first = 1;
            r += 4;
            continue;
        }
        if (!(r[0] & RB_COMPILED) || OptionErrorSkip != 0 || (end = RBRun(r, entry)) == NULL)
            end = OptionErrorSkip == 0 ? RBExec(r, entry) : RBExecSkip(r, entry);
        // where next: the next record, the record of a jump's target, or the text loop
        if (nextstmt == end)
        {
            r += (r[3] & 0xFF) != RB_OP_CMD ? 5 : (r[0] & RB_COMPILED) ? 7 + r[6] : 6;
            continue;
        }
        if ((t = RBFind(nextstmt)) != NULL) // NULL too if the statement compiled the program again (RUN)
        {
            r = t;
            continue;
        }
        ret = nextstmt; // leaving the stream, after this statement's tail
        if (RBTail(ret) && (t = RBFind(nextstmt)) != NULL)
        {
            r = t;
            first = 1;
            continue;
        }
        return nextstmt;
    }
}

/* An image's CRC for the stamp.  An image in flash is read past the XIP
   cache: read through it, 128 KB of text would evict everything the program
   ran last time, and its first milliseconds would run from flash again.
   Words are read, as a byte read would cost a flash transaction each. */
static uint32_t RBImageCrc(const unsigned char *p)
{
    uint32_t a = (uint32_t)p, crc = 0xffffffff, buf[64];
    const volatile uint32_t *u;
    int i, j, n;
    if (a < XIP_BASE || a >= XIP_BASE + 0x01000000 || (a & 3))
        return lfs_crc(crc, p, MAX_PROG_SIZE); // RAM, or PSRAM: no cache to spare
    u = (const volatile uint32_t *)(a - XIP_BASE + XIP_NOCACHE_NOALLOC_BASE);
    for (i = 0; i < MAX_PROG_SIZE / 4; i += n)
    {
        n = MAX_PROG_SIZE / 4 - i < 64 ? MAX_PROG_SIZE / 4 - i : 64;
        for (j = 0; j < n; j++)
            buf[j] = u[i + j];
        crc = lfs_crc(crc, buf, n * 4);
    }
    return crc;
}

void RBPrepare(void)
{
    rbheader_t h;
    const rbheader_t *old = (const rbheader_t *)RBSlotBase();
    RBLive = 0;
    RBWhy = NULL;
    RBRan = RBMiss = RBCode = 0;
    if (RBMode == RB_OFF)
        return;
    if (SymTabProg == NULL)
    { // records name variables by symbol (from P2), so an image saved as text runs as text
        RBWhy = "saved without symbols";
        return;
    }
    memset(&h, 0, sizeof(h));
    h.magic = RB_MAGIC;
    h.version = RB_VERSION;
    h.size = sizeof(h);
    h.build = RBBuildId();
    h.progcrc = RBImageCrc(ProgMemory);
    h.libcrc = LibPresent() ? RBImageCrc(LibMemory) : 0;
    if (old->magic == h.magic && old->version == h.version && old->size == h.size && old->build == h.build &&
        old->progcrc == h.progcrc && old->libcrc == h.libcrc &&
        old->hdrcrc == lfs_crc(0xffffffff, old, offsetof(rbheader_t, hdrcrc)))
    {
        RBReused++;
        RBLive = 1;
        RBMapStream();
        return;
    }
    RBCompile(&h);
    if (W.full)
    {
        RBWhy = "too big for the slot";
        return;
    }
    RBCompiles++;
    RBLive = 1;
    RBMapStream();
}

void RBStatus(char *out)
{
    if (RBMode == RB_OFF)
        strcpy(out, "OFF");
    else if (RBWhy)
    {
        strcpy(out, "TEXT: ");
        strcat(out, RBWhy);
    }
    else
    {
        strcpy(out, RBLive ? "COMPILED " : "NONE ");
        IntToStr(out + strlen(out), RBCompiles, 10);
        strcat(out, " REUSED ");
        IntToStr(out + strlen(out), RBReused, 10);
        strcat(out, " STMTS ");
        IntToStr(out + strlen(out), ((const rbheader_t *)RBSlotBase())->stmts, 10);
        strcat(out, " RAN ");
        IntToStr(out + strlen(out), RBRan, 10);
        strcat(out, " MISS ");
        IntToStr(out + strlen(out), RBMiss, 10);
        strcat(out, " CODE ");
        IntToStr(out + strlen(out), RBCode, 10);
    }
}
#endif // rp2350
/*  @endcond */
