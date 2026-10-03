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
// FloatToInt32/64 out of line: MMBasic.c's copies are in RAM already, and
// every copy MMBasic.h would inline into RBRun would be RAM too, which is the
// stack's (see RBRun)
#define MMBASIC_C_INTERNAL
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "Stream.h"
#include "hardware/flash.h"            // FLASH_SECTOR_SIZE
#include "hardware/regs/addressmap.h" // XIP_NOCACHE_NOALLOC_BASE

#ifdef rp2350 // Route B is RP2350-only (see Stream.h)
int RBMode = RB_OFF;

// A board with PSRAM keeps the stream in a region of its own, what the PSRAM
// reserve leaves above the RAM slots (PSRAMstream in configuration.h), written
// at memory speed.  One without keeps it in a hidden flash area of
// RB_STREAM_SLOTS program sizes after the program's (RB_STREAM_FLASH), so every
// flash slot is the user's.
static int RBInPsram(void)
{
    return PSRAMsize != 0;
}

/* ---------------------------------------------------------------------------
   P1b: the stamp and the slot.

   A slot holds one stream.  Its first 256-byte page is the header, written
   last, so a stream whose writing was interrupted has no valid header and
   reads as stale.
   --------------------------------------------------------------------------- */
#define RB_MAGIC 0x31304252 // "RB01"
#define RB_VERSION 34       // the stream format
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
static uint32_t RBNeed = 0;      // what the last compile needed of the slot, in bytes

// A stream is tied to the firmware that wrote it: a record holds command
// token numbers, which another build may number differently, and the code
// the build compiles.  So the id has the build itself in it, when this file
// was compiled and where the image ends (PSRAM and the flash area keep a
// stream across a firmware update, and RB_VERSION alone missed that).  And
// the mode: SHADOW compiles its checks in, ON leaves them out.
extern char __flash_binary_end;
static uint32_t RBBuildId(void)
{
    const char *s = __DATE__ __TIME__;
    uint32_t h = 2166136261u; // FNV-1a
    while (*s)
        h = (h ^ (unsigned char)*s++) * 16777619u;
    return ((uint32_t)RB_VERSION << 24) ^ ((uint32_t)CommandTableSize << 12) ^ (uint32_t)TokenTableSize ^
           (uint32_t)MAX_PROG_SIZE ^ (RBMode == RB_SHADOW ? 0x80000000u : 0) ^ h ^ (uint32_t)&__flash_binary_end;
}

static uint8_t *RBSlotBase(void)
{
    if (RBInPsram())
        return (uint8_t *)PSRAMstream;
    return (uint8_t *)(XIP_BASE + RB_STREAM_FLASH);
}

// the most the stream may take
static uint32_t RBSlotSize(void)
{
    return RBInPsram() ? PSRAMstreamsize : RB_STREAM_FLASH_SIZE;
}

// the flash offset of the area, for safe_flash_range_erase/program
static uint32_t RBSlotFlashOffset(void)
{
    return RB_STREAM_FLASH;
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
#define RB_PART 0x400         // STMT: the THEN or ELSE part of a single-line IF (see RBEmitIfParts)
#define RB_LIBBIT 0x80000000u // a key in the library's image
// the header words of a CMD record (STMT, CMD, token, cmdl | next) or a
// SUBCALL record (STMT, SUBCALL, cmdl | next); compiled code follows either
#define RB_HDR(r) (((r)[3] & 0xFF) == RB_OP_CMD ? 6 : 5)
#define RB_BSHIFT 5           // a bucket of the map's index is 32 bytes of text
#define RB_PAGEUP(n) (((n) + RB_PAGE - 1) & ~(RB_PAGE - 1))

#define RB_MAXDO 32 // DOs whose LOOP may compile their condition, per walk
#define RB_MAXULOCAL 64 // locals one SUB or FUNCTION may list (see RBUnitBegin)
// The compiler's state, in temporary memory while it runs: RAM here would
// be taken from the stack (see the design's notes on the stack)
static struct rbcomp
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
    int part;    // the record being emitted is an IF's THEN or ELSE part
    int ndo;     // DOs met so far in this walk, for their LOOPs:
    struct
    {
        unsigned char *loop, *cond; // the LOOP's token, the DO's condition (NULL: none)
    } dotab[RB_MAXDO];
    int unit;    // the walk is inside a SUB or FUNCTION, whose locals are ulocal[]
    int nulocal; // how many; RB_MAXULOCAL + 1: the unit could not be listed
    struct
    {
        uint16_t k;   // canonical entry
        uint8_t type; // RB_TTYPE bits as declared (0: none, OPTION DEFAULT's), RB_TMIXED
    } ulocal[RB_MAXULOCAL];
} *RBComp;
#define C (*RBComp)

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

     [n] [nbind] [stamp lo] [stamp hi] nbind x [id | suffix << 12 | RC_LIB | RC_TARGET] [type]
     [pad] nbind x [address lo] [address hi]  wordcode

   (the pad word is there when the addresses would not otherwise start on a
   4-byte boundary, so that the executor can use them in place)

   n counts the words after itself.  Each bind names a variable by its
   symbol's id (SymCanonOf[id] finds its canonical entry in one load, and the
   symbol may be anywhere: a LOOP compiles its DO's condition), RC_LIB for a
   library symbol, with the suffix it is written with (RC_SUFNBR, RC_SUFINT
   or none) and the type the code was compiled for.  The
   executor binds each one exactly as findvar's fast path would find it (the
   local at this level, else the global when no text local can hide it),
   checks that it is a scalar of that type which the reference may name, and
   runs the fallback instead if any check fails, before anything has
   happened; that is also how a variable is first made and bound, by the
   text path.  The wordcode runs on a stack of 64-bit cells.

   The bind cache.  Binding was a quarter of a compiled loop's time.  In a
   stream in PSRAM a record keeps the addresses its binds found, stamped with
   what can change them at the level it runs at (Symbols.h: the globals'
   generation and that level's locals', so a call a loop makes, whose locals
   are gone when it returns, does not change them), and uses them again
   while the stamp matches; a text local that may hide a global sends the
   record to its binds.  A stream in flash cannot be written, so there every
   record binds each time.  A stream used again after a RUN, perhaps after a
   reboot that restarted the count, has every stamp cleared first
   (RBClearCaches).

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
    RC_JFF,     // pop a float; if it is 0, skip the number of words in the next word
    RC_JFI,     // the same for an integer: IF's truth, value <> 0 as getnumber gives it
    RC_JMP,     // skip the number of words in the next word
    RC_GOTO,    // the statement ends by going to the text position in the next two words (a key)
    RC_SHADOWC, // OPTION COMPILE SHADOW: IF's condition at a, of the type in the next word
    RC_FORP,    // FOR, before its values: cmd_for's stack work for the loop variable, bind a
    RC_FORT,    // FOR, after: pop STEP and TO into its entry, test; keys of NEXT and after it follow
    RC_DOP,     // DO: cmd_do's stack work; a = the condition's offset; flags, LOOP's key, after it
    RC_DOT,     // DO's entry test on the condition (a = its type); the key after LOOP follows
    RC_LOOPF,   // LOOP: find its stack entry as cmd_loop does
    RC_LOOPT,   // LOOP's test (a = RL_ flags): back to after the DO, or the entry goes
    RC_SHADOWCK, // OPTION COMPILE SHADOW: a condition anywhere: its type, then its key
    RC_CALL,     // a call to a user SUB with a parameters (see RBCompileCall)
    RC_NEXT,     // NEXT, as cmd_next does it
    RC_FCALL,    // a call to a user FUNCTION with a parameters, its value pushed (see RBFcall)
    RC_CMPF,     // a comparison a (RK_) of two floats, as op_lt and the rest make it: 1 or 0
    RC_CMPI,     // the same of two integers
    RC_BITI,     // AND, OR or XOR (a: RK_AND...) of two integers, as op_and and the rest
    RC_IDX,      // an array index on the top: to an int as findvar makes it (a: its type), base checked
    RC_LDEL,     // an element of array bind a, its k indices (the next word) on the stack: its value
    RC_ADEL,     // the same: its address, for RC_STP
    RC_STP,      // pop a value, then an address, and store the value there
    RC_EXITFOR,  // cmd_exitfor
    RC_EXITDO,   // cmd_exit
    RC_RETURN,   // cmd_return: END SUB, EXIT SUB, RETURN
    RC_ENDFUN,   // cmd_endfun: END FUNCTION, EXIT FUNCTION
    RC_SEL,      // push SELECT CASE's selector (the statement's first cell)
    RC_EQSEL,    // pop a value of the selector's type (a): 1 if the selector == it, else 0
    RC_RANGE,    // pop the TO value and then the one before it: 1 if the selector is in the range
    RC_CLSET,    // CurrentLinePtr to a CASE's line (the key in the next two words), as cmd_select
    RC_CGOTO,    // CurrentLinePtr back, then the statement ends by going to the key in the next two words
    RC_LDS,      // push string bind a's data, as findvar returns it
    RC_LKS,      // push the literal in the next a words (an MMBasic string: its length, then its bytes)
    RC_OPS,      // operator a on two strings, as doexpr calls it: + a string, a comparison an integer
    RC_STS,      // pop a string into string bind a, as cmd_let stores it
    RC_CONTFOR,  // CONTINUE FOR: the loop's NEXT, run as cmd_next runs it
    RC_FN,       // built-in function a (RF_) on the top, as its fun_ handler computes it (RBFn)
    RC_LOCAL,    // LOCAL of the a names whose offsets follow, as cmd_dim makes them (RBLocal)
    RC_GUARD,    // to the fallback before anything happens if OPTION LEGACY (CMM1) is on, or (a) OPTION DEFAULT
                 // is not what the FUNCTION calls need (bit 0: a number, 1: float, 2: integer)
    RC_SPLICE,   // the command, its n values (the next word) on the stack, its spliced text (a words) after (RBSpliceCmd)
    RC_FSPLICE,  // a built-in function: n | token << 8 (the next word), its n values on the stack, its spliced text (a words) after (RBFnSpliceRun)
    RC_DUPLD,    // push the value at the address on the top, keeping the address (INC of an element)
    RC_PARTCHK,  // to the fallback if a bind in the mask (the next word) is unbound (RBPartCheck)
    RC_INCF,     // cmd_inc's float add: the two floats on the top, added with no overflow check
    RC_INCS,     // cmd_inc's string: pop a string and add it to string bind a (RBStoreStr)
};
enum
{ // RC_FN's functions: each its Functions.c helper (FnSin...)
    RF_SIN,
    RF_COS,
    RF_TAN,
    RF_ATN,
    RF_SQR,
    RF_EXP,
    RF_LOG,
    RF_DEG,
    RF_RAD,
    RF_INT,  // float in, integer out
    RF_FIX,
    RF_ABSF, // ABS and SGN keep the argument's type, as fun_abs and fun_sgn do
    RF_ABSI,
    RF_SGNF,
    RF_SGNI,
    RF_RND, // no argument: RndVal() replaces the cell pushed for it
};
#define RK_LT 0 // RC_CMPF, RC_CMPI: op_lt
#define RK_LTE 1
#define RK_GT 2
#define RK_GTE 3
#define RK_EQ 4
#define RK_NE 5
#define RK_AND 0 // RC_BITI
#define RK_OR 1
#define RK_XOR 2
#define RP_VAR 1   // RC_CALL: the argument is a variable, bind (bits 8-15); else the next value
#define RP_BYVAL 2 // RC_CALL: the parameter is BYVAL
#define RP_INT 4   // RC_CALL: the argument is an integer, else a float
#define RP_ELEM 8  // RC_CALL: the argument is an array element, its address the next value (RC_ADEL)
#define RD_UNTIL 1  // RC_DOP: DO UNTIL
#define RD_COND 2   // RC_DOP: DO WHILE or UNTIL: the condition is the DO's
#define RL_ALWAYS 1 // RC_LOOPT: a plain LOOP
#define RL_ENTRY 2  // RC_LOOPT: the DO's condition; WHILE or UNTIL as the entry says
#define RL_UNTIL 4  // RC_LOOPT: LOOP UNTIL
#define RL_NBR 8    // RC_LOOPT: the condition is a float
#define RC_TARGET 0x8000 // bind: the statement assigns to it
#define RC_SUFNBR 0x1000 // bind: written with ! (T_NBR)
#define RC_SUFINT 0x2000 // bind: written with % (T_INT)
#define RC_SUFSTR 0x3000 // bind: written with $ (T_STR)
#define RC_LIB 0x4000    // bind: a library symbol (its id follows the program's in SymCanonOf)
#define RC_IDMASK 0x0FFF // bind: the symbol id
#define RB_BARRAY 0x100  // bind's type word: an array, bound to its variable (RC_LDEL, RC_ADEL)
#define RB_BOPT 0x200    // bind's type word: may be unbound at the start (an IF part's: RBPartCheck)
#define RB_BCONST 0x400  // bind's type word: must be a CONST (an argument passed by value: RBArgs)
#define RB_MAXLIT 64     // the longest string literal the code keeps
#define RB_MAXBIND 12
#define RB_MAXCODE 160 // words of code one statement may compile to
#define RB_MAXDEPTH 16 // cells of stack it may use

static CommandToken RBTokLet; // the LET command, looked up once a compile
static CommandToken RBTokIf, RBTokElse, RBTokEndIf, RBTokEnd_If;
static CommandToken RBTokFor;
static CommandToken RBTokDim, RBTokLocal, RBTokStatic, RBTokConst, RBTokOption, RBTokInc;
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
#define RB_TCONST 0x80                   // a CONST (a unit's own in ulocal[], else a global)

static int RBCollect; // RBUnitBegin is listing a unit's locals: RBTypeName records them there

// a local of the unit being listed, with its declared type
static void RBUnitAdd(int k, int type)
{
    int i;
    if (C.nulocal > RB_MAXULOCAL)
        return;
    type &= RB_TTYPE;
    for (i = 0; i < C.nulocal; i++)
        if (C.ulocal[i].k == k)
        {
            if ((C.ulocal[i].type & RB_TTYPE) != type)
                C.ulocal[i].type = RB_TMIXED; // declared twice, two ways (or once untyped)
            return;
        }
    if (C.nulocal == RB_MAXULOCAL)
    {
        C.nulocal = RB_MAXULOCAL + 1; // too many to list
        return;
    }
    C.ulocal[C.nulocal].k = k;
    C.ulocal[C.nulocal].type = type;
    C.nulocal++;
}

static void RBTypeName(unsigned char *p, int type, int local)
{
    int k = SymCanonAt(p), cur;
    if (k < 0 || k >= C.ntypes)
        return;
    if (RBCollect)
    { // a unit's declarations: only its locals matter (DIM makes a global)
        if (local)
            RBUnitAdd(k, type);
        return;
    }
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
        if (issymbol(*p))
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
        if (!issymbol(*p))
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

// A CONST's name marked as one, so that an argument naming it can go by value
// as DefinedSubFun passes it (RBArgs): a unit's own CONST in its local list,
// any other in the survey
static void RBConstName(unsigned char *p, int local)
{
    int k = SymCanonAt(p), i;
    if (k < 0 || k >= C.ntypes)
        return;
    if (RBCollect)
    {
        if (local && C.nulocal <= RB_MAXULOCAL)
            for (i = 0; i < C.nulocal; i++)
                if (C.ulocal[i].k == k)
                    C.ulocal[i].type |= RB_TCONST;
        return;
    }
    if (!local)
        C.types[k] |= RB_TCONST;
}

// The name at p is a CONST where the statement being compiled is: its unit's
// own, or a global one that no local of the unit hides.  Only a guess: the
// bind checks it (RB_BCONST), and a name that is not one sends the statement
// to its fallback.
static int RBIsConst(unsigned char *p)
{
    int k = SymCanonAt(p), i;
    if (k < 0 || k >= C.ntypes)
        return 0;
    if (C.unit)
    {
        if (C.nulocal > RB_MAXULOCAL)
            return 0;
        for (i = 0; i < C.nulocal; i++)
            if (C.ulocal[i].k == k)
                return (C.ulocal[i].type & RB_TCONST) != 0;
    }
    return (C.types[k] & RB_TCONST) != 0;
}

// CONST name = literal, ...: the literal's type, as getvalue reads it
static void RBSurveyConst(unsigned char *p, int local)
{
    unsigned char *q, *v;
    int suf, t;
    while (1)
    {
        skipspace(p);
        if (!issymbol(*p))
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
        RBConstName(p, local);
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

// The type suffix written after a name, as findvar reads it: 0, T_INT, T_NBR
// or T_STR.  Steps *p over it.
static int RBSuffix(unsigned char **p)
{
    int t = **p == '%' ? T_INT : **p == '!' ? T_NBR : **p == '$' ? T_STR : 0;
    if (t)
        (*p)++;
    return t;
}

// A dotted name (srv.pCurr, R.running) is a plain name, which findvar binds
// like any other, unless the program or its library defines a TYPE: then
// findvar may take it for a structure member path and does not bind it
// (FINDVAR_DOTBLOCKS), so it stays text.  PrepareProgram registers the TYPEs
// before the compile.
static int RBDotBlocked(const unsigned char *sp, int len)
{
#ifdef STRUCTENABLED
    return g_structcnt > 0 && memchr(sp, '.', len) != NULL;
#else
    (void)sp;
    (void)len;
    return 0;
#endif
}

// A variable the compiler can bind: a symbol (dotted only where RBDotBlocked
// allows it), with its suffix, not followed by a bracket or a member's dot.
// Returns the byte after it, or NULL.  RBVarRef takes only a number's.
static unsigned char *RBVarRefS(unsigned char *p, int *suffix)
{
    const unsigned char *sp;
    int len;
    *suffix = 0;
    if (!issymbol(*p))
        return NULL;
    sp = SymSpelling(p, &len);
    if (RBDotBlocked(sp, len))
        return NULL;
    p += symbolsize(*p);
    *suffix = RBSuffix(&p);
    if (*p == '(' || *p == '.')
        return NULL;
    return p;
}

static unsigned char *RBVarRef(unsigned char *p, int *suffix)
{
    unsigned char *q = RBVarRefS(p, suffix);
    return *suffix == T_STR ? NULL : q;
}

// One statement's compilation.
typedef struct
{
    unsigned char *entry;
    int nbind;
    uint16_t bind[RB_MAXBIND][2];
    uint16_t w[RB_MAXCODE];
    int n, depth, maxdepth, fail;
    int lk; // 1 + where the last RC_LK's constant starts, 0: none (see RBCvif)
    int calls, ncall; // a FUNCTION call may compile here (LET, IF, INC); how many did (see RBFcall), RND's
                      // too: no SHADOW check of a statement with one, which the text would make again
    int gbits;        // what the calls need of OPTION DEFAULT: RBFinish's guard (see RBFcall)
    unsigned used;    // the binds met since it was cleared (bit j: bind j), and
    unsigned opt;     // those that may be unbound at the start (RBPartCheck)
    unsigned cmask;   // those that must be CONSTs (RB_BCONST, added by RBFinish)
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
// there is no room.  A symbol with its suffix is one variable.  A string
// scalar binds only where the caller takes one (str): its type word is T_STR.
static int RBBindK(rbcx_t *x, unsigned char *p, int suffix, int target, int arr, int str)
{
    unsigned int id = SymIdAt(p), w0, j;
    if (id > RC_IDMASK)
        return -1;
    w0 = id | ((*p & 2) ? RC_LIB : 0) | (suffix == T_NBR ? RC_SUFNBR : suffix == T_INT ? RC_SUFINT : suffix == T_STR ? RC_SUFSTR : 0);
    for (j = 0; j < (unsigned)x->nbind; j++)
        if ((x->bind[j][0] & ~RC_TARGET) == w0)
        {
            if ((x->bind[j][1] & RB_BARRAY) != arr || (!str && (x->bind[j][1] & T_STR)))
                return -1; // the name as an array and as a scalar: the text path's error
            if (target)
                x->bind[j][0] |= RC_TARGET;
            x->used |= 1u << j;
            return j;
        }
    if (x->nbind >= RB_MAXBIND)
        return -1;
    {
        int k = SymCanonAt(p), ty = (k >= 0 && k < C.ntypes) ? C.types[k] : 0, t, i;
        if (C.unit && C.nulocal > RB_MAXULOCAL)
        { // a unit that could not be listed: its locals stay text (as P2 had it)
            if (ty & RB_TLOCAL)
                return -1;
        }
        else if (C.unit)
            for (i = 0; i < C.nulocal; i++)
                if (C.ulocal[i].k == k)
                {
                    ty = C.ulocal[i].type; // a local here: its type is the unit's
                    break;
                }
        t = ty & RB_TTYPE;
        if (!suffix)
        { // an unsuffixed name: its declared type, else the OPTION DEFAULT in force,
          // else (DEFAULT NONE with the declaration out of sight, in a library say,
          // or two declarations) a guess of float, which the bind check verifies
            if ((ty & RB_TMIXED) || (t != T_INT && t != T_NBR && t != T_STR))
                t = (C.deftype == T_INT || C.deftype == T_NBR || (C.deftype == T_STR && str && !arr)) ? C.deftype : T_NBR;
            if (t == T_STR && (!str || arr))
                return -1;
            x->bind[j][1] = t;
        }
        else if (suffix == T_STR && (!str || arr))
            return -1;
        else
            x->bind[j][1] = suffix;
        x->bind[j][1] |= arr;
    }
    x->bind[j][0] = w0 | (target ? RC_TARGET : 0);
    x->nbind++;
    x->used |= 1u << j;
    return j;
}

static int RBBind(rbcx_t *x, unsigned char *p, int suffix, int target)
{
    return RBBindK(x, p, suffix, target, 0, 0);
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
static int RBEvaluateS(rbcx_t *x, unsigned char **pp);
static int RBFcall(rbcx_t *x, unsigned char **pp, int *op);
static unsigned char *RBArgEnd(unsigned char *p, int close);

/* P4a: an element of a numeric array, name(i [, j ...]) as findvar reads it
   (a bracket straight after the name and its suffix, not a FUNCTION's): its
   array bound, each index compiled and then RC_IDX, which converts and checks
   it as findvar does straight after evaluating it; then op (RC_LDEL for its
   value, RC_ADEL for its address) checks the count and the bounds.  Returns
   the element's type, 0 if the compiler cannot take it; *pp is left after the
   closing bracket. */
static int RBElement(rbcx_t *x, unsigned char **pp, int op, int target)
{
    unsigned char *p = *pp, *q, *ae;
    const unsigned char *sp;
    int suf, j, t, k = 0, len;
    if (!issymbol(*p))
        return 0;
    sp = SymSpelling(p, &len);
    if (RBDotBlocked(sp, len))
        return 0; // perhaps a structure's
    q = p + symbolsize(*p);
    suf = RBSuffix(&q);
    if (suf == T_STR || *q != '(' || FindSubFun(p, 1) >= 0)
        return 0;
    if ((j = RBBindK(x, p, suf, target, RB_BARRAY, 0)) < 0)
        return 0;
    q++;
    while (1)
    {
        ae = RBArgEnd(q, ')');
        skipspace(q);
        if (q == ae || k == MAXDIM)
            return 0; // an empty array, or too many indices: the text path's
        if ((t = RBEvaluate(x, &q)) == 0)
            return 0;
        skipspace(q);
        if (q != ae)
            return 0;
        RBOp(x, RC_IDX | (t << 8), 0);
        k++;
        if (*q != ',')
            break;
        q++;
    }
    if (*q != ')')
        return 0;
    RBOp(x, op | (j << 8), 1 - k);
    RBOp(x, k, 0);
    *pp = q + 1;
    return x->bind[j][1] & (T_INT | T_NBR);
}

/* P5a: a pure numeric built-in function at *pp, as getvalue calls it: the
   argument (between the function's token, whose bracket is part of it, and
   the closing bracket getclosebracket finds) evaluated as the handler reads
   it - getnumber, or for ABS and SGN evaluate with no type - then the
   handler's own work on the value (RC_FN).  PI is its constant.  Returns the
   type, or 0: another function, or an argument that does not end at the
   closing bracket (a second argument), stays text. */
static int RBValue(rbcx_t *x, unsigned char **pp, int *op);
static void RBCvif(rbcx_t *x);
static int RBEvaluateS(rbcx_t *x, unsigned char **pp);

/* P5c: a string function at *pp through the value splice (see RBFnSpliceRun),
   and MAP( and RGB( (P5d):
   its arguments compiled onto the VM's stack, as getvalue would have them
   evaluated, and in the code a copy of its argument text with each value
   T_VALUE and a letter ('A'+i an integer, 'a'+i a float, '0'+i a string),
   which its handler reads as it reads getvalue's copy.  Only the functions
   below, whose handlers read every argument as a value and whose result has
   one type; LEFT$, RIGHT$, UCASE$ and LCASE$ are tokenise's SChange$ with its
   selector letter first, HEX$, OCT$ and BIN$ its base$ with the base.  No
   FUNCTION call in the arguments (the handler reads them in its own order),
   no empty argument.  Returns the type, or 0. */
static int RBFnSplice(rbcx_t *x, unsigned char **pp, int *op)
{
    unsigned char *p = *pp, c = *p, *ae, txt[2 * RB_MAXLIT];
    void (*fn)(void) = tokenfunction(c);
    int n = 0, len = 0, t, rt, i, calls = x->calls, types[16];
    if (fn != fun_len && fn != fun_asc && fn != fun_chr && fn != fun_mid && fn != fun_instr && fn != fun_str &&
        fn != fun_space && fn != fun_trim && fn != fun_schange && fn != fun_base && fn != fun_rgb
#if defined(PICOMITEVGA) || PICOMITERP2350
        && fn != fun_map // (where the build has MAP(: a colour from getint, and no argument text in its errors)
#endif
    )
        return 0;
    rt = tokentype(c) & (T_NBR | T_INT | T_STR);
    if (rt != T_NBR && rt != T_INT && rt != T_STR)
        return 0; // (every one of them has one)
    p++; // (the token's bracket is part of it)
    if (fn == fun_rgb)
    { // RGB(name): fun_rgb reads its one argument as a colour's name, which getvalue
      // would spell out for it: the spelling goes in the text, with no values
        unsigned char *q = p;
        const unsigned char *sp = NULL;
        int slen = 0;
        skipspace(q);
        if (issymbol(*q))
        {
            sp = SymSpelling(q, &slen);
            q += symbolsize(*q);
            skipspace(q);
        }
        if (sp != NULL && *q == ')' && slen <= (int)sizeof(txt) - 2)
        { // a name alone (a name with a suffix, or in an expression, goes on below)
            memcpy(txt, sp, slen);
            txt[slen] = 0;
            RBOp(x, RC_FSPLICE | (((slen + 2) / 2) << 8), 1);
            RBOp(x, 0 | (c << 8), 0);
            for (i = 0; i < slen + 1; i += 2)
                RBOp(x, txt[i] | ((i + 1 < slen + 1 ? txt[i + 1] : 0) << 8), 0);
            *pp = RBNextOp(q + 1, op);
            return rt;
        }
    }
    if (fn == fun_schange)
    { // tokenise's selector: E LEFT$, R RIGHT$, U UCASE$, L LCASE$
        if (!(p[0] == 'E' || p[0] == 'R' || p[0] == 'U' || p[0] == 'L') || p[1] != ',')
            return 0;
        txt[len++] = p[0];
        txt[len++] = ',';
        p += 2;
    }
    x->calls = 0;
    while (1)
    {
        ae = RBArgEnd(p, ')'); // as getcsargs splits them
        skipspace(p);
        if (p == ae || n == 16 || len > (int)sizeof(txt) - 6 || (t = RBEvaluateS(x, &p)) == 0)
            return 0;
        skipspace(p);
        if (p != ae)
            return 0;
        types[n] = t;
        txt[len++] = T_VALUE;
        txt[len++] = (t == T_NBR ? 'a' : t == T_INT ? 'A' : '0') + n++;
        if (*ae != ',')
            break;
        txt[len++] = ',';
        p = ae + 1;
    }
    x->calls = calls;
    if (*ae != ')')
        return 0;
    if (fn == fun_instr && !(n == 2 || (n == 3 && types[0] != T_STR)))
        return 0; // the pattern forms: a variable for the match's length
    if (fn == fun_trim && n > 2)
        return 0; // its third argument can be a keyword
    if (fn == fun_rgb && n != 3)
        return 0; // one argument is a name (above), any other count fun_rgb's error
    txt[len++] = 0;
    RBOp(x, RC_FSPLICE | (((len + 1) / 2) << 8), 1 - n);
    RBOp(x, n | (c << 8), 0);
    for (i = 0; i < len; i += 2)
        RBOp(x, txt[i] | ((i + 1 < len ? txt[i + 1] : 0) << 8), 0);
    *pp = RBNextOp(ae + 1, op);
    return rt;
}

static int RBFunction(rbcx_t *x, unsigned char **pp, int *op)
{
    unsigned char *p = *pp, c = *p;
    void (*fn)(void) = tokenfunction(c);
    int id, t, rt;
    if (fn == fun_pi && (tokentype(c) & T_FNA))
    { // fun_pi's M_PI, as a constant
        union
        {
            MMFLOAT f;
            uint16_t w[4];
        } k;
        k.f = M_PI;
        RBOp(x, RC_LK, 1);
        x->lk = x->n + 1;
        for (id = 0; id < 4; id++)
            RBOp(x, k.w[id], 0);
        *pp = RBNextOp(p + 1, op);
        return T_NBR;
    }
    if (fn == fun_rnd)
    { // fun_rnd's RndVal() into a cell pushed for it.  fun_rnd reads no argument, so
      // RND( ... )'s is not evaluated.  SHADOW leaves the statement unchecked (ncall):
      // its text evaluator would draw another number and move the sequence on
        if (tokentype(c) & T_FUN)
        {
            unsigned char *q = RBArgEnd(p + 1, ')');
            if (*q != ')')
                return 0;
            p = q;
        }
        RBOp(x, RC_LK, 1);
        for (id = 0; id < 4; id++)
            RBOp(x, 0, 0);
        RBOp(x, RC_FN | (RF_RND << 8), 0);
        x->ncall++;
        *pp = RBNextOp(p + 1, op);
        return T_NBR;
    }
    if (!(tokentype(c) & T_FUN))
        return 0;
    id = fn == fun_sin ? RF_SIN : fn == fun_cos ? RF_COS : fn == fun_tan ? RF_TAN : fn == fun_atn ? RF_ATN : fn == fun_sqr ? RF_SQR : fn == fun_exp ? RF_EXP : fn == fun_log ? RF_LOG : fn == fun_deg ? RF_DEG : fn == fun_rad ? RF_RAD : fn == fun_int ? RF_INT : fn == fun_fix ? RF_FIX : fn == fun_abs ? RF_ABSF : fn == fun_sgn ? RF_SGNF : -1;
    if (id < 0)
        return RBFnSplice(x, pp, op); // P5c: through the value splice, if it is one of those
    p++;
    if ((t = RBEvaluate(x, &p)) == 0)
        return 0;
    skipspace(p);
    if (*p != ')')
        return 0;
    p++;
    if (id == RF_ABSF || id == RF_SGNF)
    { // evaluate with no type: the argument keeps its own
        if (t == T_INT)
            id++; // RF_ABSI, RF_SGNI
        rt = id == RF_ABSF ? T_NBR : T_INT;
    }
    else
    { // getnumber: an integer becomes a float
        if (t == T_INT)
            RBCvif(x);
        rt = (id == RF_INT || id == RF_FIX) ? T_INT : T_NBR;
    }
    RBOp(x, RC_FN | (id << 8), 0);
    *pp = RBNextOp(p, op);
    return rt;
}

// getvalue: one value, and the operator after it in *op.  Returns its type,
// T_INT, T_NBR or T_STR, or 0 if the compiler cannot take it.
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
        { // P5a: a built-in function
            *pp = p;
            return RBFunction(x, pp, op);
        }
        fn = tokenfunction(c);
        if (fn != op_not && fn != op_inv && fn != op_subtract && fn != op_add)
            return 0; // a function, or anything else: P5 and later
        p++;
        if ((t = RBValue(x, &p, op)) == 0 || t == T_STR)
            return 0; // (on a string: getvalue's error)
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
        unsigned char *q = RBVarRefS(p, &suf);
        if (q == NULL && issymbol(c) && (t = RBFcall(x, &p, op)) != 0)
        {
            *pp = p;
            return t;
        }
        if (q == NULL && issymbol(c) && (t = RBElement(x, &p, RC_LDEL, 0)) != 0)
        {
            *pp = RBNextOp(p, op);
            return t;
        }
        if (q == NULL || (j = RBBindK(x, p, suf, 0, 0, 1)) < 0)
            return 0; // (after a call that moved the bind stamp, L_FCALL binds it again)
        RBOp(x, (x->bind[j][1] == T_STR ? RC_LDS : RC_LDG) | (j << 8), 1);
        *pp = RBNextOp(q, op);
        return x->bind[j][1];
    }
    if (c == '"')
    { // a literal with no backslash, which OPTION ESCAPE cannot change: kept in the
      // code as the MMBasic string getvalue copies into temporary memory
        unsigned char *tp = (unsigned char *)strchr((char *)p + 1, '"');
        int n, k;
        if (tp == NULL || (n = tp - p - 1) > RB_MAXLIT || memchr(p + 1, '\\', n))
            return 0;
        RBOp(x, RC_LKS | (((n + 2) / 2) << 8), 1);
        for (k = 0; k <= n; k += 2) // byte 0 its length, then p[1] to p[n]
            RBOp(x, (k ? p[k] : n) | ((k + 1 <= n ? p[k + 1] : 0) << 8), 0);
        *pp = RBNextOp(tp + 1, op);
        return T_STR;
    }
    if ((c >= '0' && c <= '9') || c == '.' ||
        (c == '&' && (toupper(p[1]) == 'H' || toupper(p[1]) == 'O' || toupper(p[1]) == 'B')))
    { // a decimal literal, or &H, &O or &B (P5d), read by getvalue itself: its
      // number reader raises no error, and its based reader none once the
      // letter is one of those (another is its "Type prefix", left to the
      // text path), so compiling cannot fail a RUN
        MMFLOAT f;
        long long i64;
        unsigned char *str;
        union
        {
            long long i;
            MMFLOAT f;
            uint16_t w[4];
        } k;
        if (c != '&' && RBLiteral(p) == NULL)
            return 0;
        p = getvalue(p, &f, &i64, &str, op, &t);
        if (t & T_NBR)
            k.f = f, t = T_NBR;
        else if (t & T_INT)
            k.i = i64, t = T_INT;
        else
            return 0;
        RBOp(x, RC_LK, 1);
        x->lk = x->n + 1;
        for (j = 0; j < 4; j++)
            RBOp(x, k.w[j], 0);
        *pp = p;
        return t;
    }
    if (c == '(')
    {
        p++;
        if ((t = RBEvaluateS(x, &p)) == 0 || *p != ')')
            return 0;
        *pp = RBNextOp(p + 1, op);
        return t;
    }
    return 0;
}

// RC_CVIF, unless the top of the stack is the constant just pushed: that is
// converted now, as RC_CVIF would convert it
static void RBCvif(rbcx_t *x)
{
    if (x->lk && x->n == x->lk + 3)
    {
        union
        {
            long long i;
            MMFLOAT f;
            uint16_t w[4];
        } k;
        memcpy(k.w, x->w + x->lk - 1, 8);
        k.f = (MMFLOAT)k.i;
        memcpy(x->w + x->lk - 1, k.w, 8);
        return;
    }
    RBOp(x, RC_CVIF, 0);
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
    if (a == T_STR || t2 == T_STR)
    { // doexpr's type check, then the operator's own function on sarg1 and sarg2
        if (a != t2 || !(ty & T_OPER) || !(ty & T_STR))
            return 0; // "Incompatible types in expression", "Invalid operator": the text path's
        if (fn == op_add)
            a = T_STR;
        else if (fn == op_ne || fn == op_equal || fn == op_gte || fn == op_lte || fn == op_lt || fn == op_gt)
            a = T_INT;
        else
            return 0;
        RBOp(x, RC_OPS | (*o1 << 8), -1);
        *t1 = a;
        *o1 = o2;
        return 1;
    }
    targ = ty & (T_NBR | T_INT);
    if (targ == T_NBR)
    {
        if (a == T_INT)
            RBOp(x, RC_CVIF2, 0), a = T_NBR;
        if (t2 == T_INT)
            RBCvif(x), t2 = T_NBR;
    }
    else if (targ == T_INT)
    {
        if (a == T_NBR)
            RBOp(x, RC_CVFI2, 0), a = T_INT;
        if (t2 == T_NBR)
            RBOp(x, RC_CVFI, 0), t2 = T_INT;
    }
    else if (a == T_NBR && t2 == T_INT)
        RBCvif(x), t2 = T_NBR;
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
    { // inline: what compare() and the operator make of the two values
        int k = fn == op_lt ? RK_LT : fn == op_lte ? RK_LTE : fn == op_gt ? RK_GT : fn == op_gte ? RK_GTE : fn == op_equal ? RK_EQ : RK_NE;
        RBOp(x, (a == T_NBR ? RC_CMPF : RC_CMPI) | (k << 8), -1), a = T_INT;
    }
    else if (fn == op_and || fn == op_or || fn == op_xor)
        RBOp(x, RC_BITI | ((fn == op_and ? RK_AND : fn == op_or ? RK_OR : RK_XOR) << 8), -1), a = T_INT;
    else if (fn == op_divint || fn == op_mod || fn == op_shiftleft || fn == op_shiftright)
        RBOp(x, RC_OPI | (*o1 << 8), -1), a = T_INT;
    else
        return 0; // integer ^, and anything else whose type is not certain
    *t1 = a;
    *o1 = o2;
    return 1;
}

// evaluate, without its end check: the expression's type, or 0
static int RBEvaluateS(rbcx_t *x, unsigned char **pp)
{
    int o, t = RBValue(x, pp, &o);
    while (t && o != E_END)
        if (!RBDoExpr(x, pp, &t, &o))
            return 0;
    return x->fail ? 0 : t;
}

// the same for a number: 0 for a string
static int RBEvaluate(rbcx_t *x, unsigned char **pp)
{
    int t = RBEvaluateS(x, pp);
    return t == T_STR ? 0 : t;
}

// LET's assignment, from its target at p, into x.  Returns the byte after
// it, or NULL.  stop is a token that may end it besides the element (the
// ELSE of a single-line IF), or 0.
static unsigned char *RBLetInto(rbcx_t *x, unsigned char *p, int stop)
{
    unsigned char *q, *rhs;
    int tsuf, tgt, ttype, t;
    if ((q = RBVarRefS(p, &tsuf)) == NULL)
    { // an element: its address first, as cmd_let's findvar makes it before the right-hand side
        q = p;
        if ((ttype = RBElement(x, &q, RC_ADEL, 1)) == 0)
            return NULL;
        tgt = -1;
    }
    else
    {
        if ((tgt = RBBindK(x, p, tsuf, 1, 0, 1)) < 0)
            return NULL;
        ttype = x->bind[tgt][1];
    }
    p = q;
    skipspace(p);
    if (*p != tokenEQUAL)
        return NULL;
    p++;
    skipspace(p);
    rhs = p;
    if (rhs - x->entry > 255 || (t = RBEvaluateS(x, &p)) == 0 || (t == T_STR) != (ttype == T_STR))
        return NULL; // (a string for a number or the other way: evaluate's error)
    skipspace(p);
    if (*p && *p != '\'' && !(stop && *p == stop))
        return NULL; // evaluate's and checkend's errors are the text path's
    if (t != ttype) // as evaluate converts for cmd_let's type, then cmd_let stores it
    {
        if (ttype == T_NBR)
            RBCvif(x);
        else
            RBOp(x, RC_CVFI, 0);
    }
    if (RBMode == RB_SHADOW && !x->ncall)
    { // SHADOW's check, compiled in only in that mode (it is in the stamp); not of a
      // call, which the text evaluator would make again
        RBOp(x, RC_SHADOW | ((rhs - x->entry) << 8), 0);
        RBOp(x, ttype, 0);
    }
    if (tgt < 0)
        RBOp(x, RC_STP, -2);
    else
        RBOp(x, (ttype == T_STR ? RC_STS : RC_STG) | (tgt << 8), -1);
    return p;
}

// x's binds and code into code[] as [nbind] binds wordcode: the words, or 0
static int RBFinish(rbcx_t *x, uint16_t *code)
{
    int n = 0, j;
    if (x->gbits && !x->fail)
    { // the statement's FUNCTION calls need OPTION DEFAULT's type: checked first,
      // before any runs (see RBFcall; the code's jumps are relative)
        if (x->n >= RB_MAXCODE)
            return 0;
        memmove(x->w + 1, x->w, x->n * 2);
        x->w[0] = RC_GUARD | (x->gbits << 8);
        x->n++;
    }
    if (x->fail || x->maxdepth > RB_MAXDEPTH || 1 + 2 * x->nbind + x->n > RB_MAXCODE)
        return 0;
    if (1 + 2 + 4 * x->nbind + x->n > RB_MAXCODE)
        return 0;
    code[n++] = x->nbind;
    code[n++] = 0; // the bind cache's stamp: none yet
    code[n++] = 0;
    for (j = 0; j < x->nbind; j++)
    {
        code[n++] = x->bind[j][0];
        code[n++] = x->bind[j][1] | (((x->opt >> j) & 1) ? RB_BOPT : 0) | (((x->cmask >> j) & 1) ? RB_BCONST : 0);
    }
    for (j = 0; j < 2 * x->nbind; j++)
        code[n++] = 0; // the cached addresses
    memcpy(code + n, x->w, x->n * 2);
    return n + x->n;
}

/* P3b: a call to a user SUB.  The compiler reads the SUB's parameters once
   (DefinedSubFun splits both lists with makeargs on every call) and the
   call's arguments: an expression compiles to its value; a variable binds,
   as a target so that a CONST, which DefinedSubFun passes by value, goes to
   the fallback.  RC_CALL then does what DefinedSubFun does for a SUB (see
   L_CALL).  An array element alone as an argument goes by its address, as
   DefinedSubFun passes it (RP_ELEM).  Left to DefinedSubFun: a CSUB, a
   FUNCTION called as a SUB, arguments in brackets, a missing argument, a call
   as an argument, an element for a BYVAL parameter, strings, arrays and
   structures, BYREF with an expression or an untyped parameter, and a header
   longer than 255 bytes. */
#define RB_MAXPARAM 16
unsigned char *CheckByKeyword(unsigned char *p, int kind); // MMBasic.c

// a SUB or FUNCTION's parameters, and a call's arguments for them
typedef struct
{
    int np, untyped;
    int by[RB_MAXPARAM], ptype[RB_MAXPARAM], flags[RB_MAXPARAM]; // BYVAL 1, BYREF -1; declared type (0: OPTION DEFAULT's)
    uint16_t name[RB_MAXPARAM], astype[RB_MAXPARAM];
} rbparams_t;

// The parameters of the definition at def, *pp at the list (after the name
// and its suffix): 0 if the compiler cannot take them.  *pp is left after them.
static int RBParams(unsigned char *def, unsigned char **pp, rbparams_t *pr)
{
    unsigned char *p = *pp, *b, *v;
    int paren, suf, t, np = 0;
    pr->untyped = 0;
    skipspace(p);
    paren = (*p == '(');
    if (paren)
        p++;
    skipspace(p);
    if (paren ? *p != ')' : (*p && *p != '\''))
        while (1)
        {
            if (np == RB_MAXPARAM)
                return 0;
            pr->by[np] = 0;
            pr->astype[np] = 0;
            if ((b = CheckByKeyword(p, 'V')) != NULL)
                p = b, pr->by[np] = 1;
            else if ((b = CheckByKeyword(p, 'R')) != NULL)
                p = b, pr->by[np] = -1;
            skipspace(p);
            if (!issymbol(*p))
                return 0;
            v = p;
            p += symbolsize(*p);
            suf = RBSuffix(&p);
            if (suf == T_STR || p - def > 255)
                return 0;
            pr->name[np] = (p - v) | ((v - def) << 8); // the name and its suffix, for findvar
            pr->ptype[np] = suf;
            skipspace(p);
            if (*p == '(')
                return 0; // an array
            if (*p == tokenAS)
            {
                p++;
                skipspace(p);
                t = RBTypeWord(&p);
                if ((t != T_INT && t != T_NBR) || suf)
                    return 0; // a string, a structure, or a type twice
                pr->astype[np] = t | T_IMPLIED;
                pr->ptype[np] = t;
                skipspace(p);
            }
            else if (!suf)
            {
                if (pr->by[np] < 0)
                    return 0; // BYREF: DefinedSubFun checks the types match
                pr->untyped = 1; // OPTION DEFAULT's type, at the call
            }
            np++;
            if (*p == ',')
            {
                p++;
                skipspace(p);
                continue;
            }
            if (paren ? *p != ')' : (*p && *p != '\''))
                return 0;
            break;
        }
    if (paren)
        p++;
    pr->np = np;
    *pp = p;
    return 1;
}

// the end of the argument at p: the next ',' at bracket depth 0, a comment,
// the end, or (close) the ')' that closes the list
static unsigned char *RBArgEnd(unsigned char *p, int close)
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
        {
            if (depth == 0 && close)
                break;
            depth--;
        }
        else if ((*p == ',' && depth == 0) || *p == '\'')
            break;
    }
    return p;
}

// An argument that is an array element alone, name(i [, j]) and not a
// FUNCTION's call, which DefinedSubFun takes for a variable: the byte after it
// and its spaces, or NULL.  m(1, 1) is one element (it was compiled as a
// value, a copy, from P4a until V7.0.00b1's goldens caught it).
static unsigned char *RBLoneElement(unsigned char *q)
{
    unsigned char *e;
    if (!issymbol(*q))
        return NULL;
    e = q + symbolsize(*q);
    RBSuffix(&e);
    if (*e != '(' || FindSubFun(q, 1) >= 0)
        return NULL;
    e++;
    while (*(e = RBArgEnd(e, ')')) == ',')
        e++; // past every index
    if (*e != ')')
        return NULL;
    e++;
    skipspace(e);
    return e;
}

// The call's arguments at *qq, up to the statement's end or (close) the
// bracket that closes them: an expression compiles to its value, a variable
// binds (as a target, so a CONST, which DefinedSubFun passes by value, goes
// to the fallback), an element alone goes by its address.  0 if the compiler
// cannot take them.
static int RBArgs(rbcx_t *x, unsigned char **qq, int close, rbparams_t *pr, int *nval)
{
    unsigned char *q = *qq, *ae, *vq;
    int na = 0, j, t, suf;
    *nval = 0;
    skipspace(q);
    if (close ? *q != ')' : (*q && *q != '\''))
        while (1)
        {
            ae = RBArgEnd(q, close);
            skipspace(q);
            if (q == ae || na == pr->np)
                return 0; // a missing argument, or too many
            vq = RBVarRef(q, &suf);
            if (vq != NULL)
                skipspace(vq);
            if (vq == ae && RBIsConst(q))
            { // a CONST: DefinedSubFun passes its value (the bind checks it is one, RB_BCONST)
                if (pr->by[na] < 0)
                    return 0; // BYREF: "Variable required", which the fallback reports
                if ((j = RBBind(x, q, suf, 0)) < 0)
                    return 0;
                x->cmask |= 1u << j; // (not in bind[j][1], which is the type RBValue reads)
                RBOp(x, RC_LDG | (j << 8), 1);
                pr->flags[na] = x->bind[j][1] == T_INT ? RP_INT : 0;
                (*nval)++;
            }
            else if (vq == ae)
            { // a variable: by reference, as DefinedSubFun passes it
                if ((j = RBBind(x, q, suf, 1)) < 0)
                    return 0;
                if (pr->by[na] < 0 && x->bind[j][1] != pr->ptype[na])
                    return 0; // BYREF of another type: DefinedSubFun's error
                pr->flags[na] = (pr->by[na] > 0 ? RP_BYVAL : 0) | RP_VAR | (j << 8) | (x->bind[j][1] == T_INT ? RP_INT : 0);
            }
            else if (RBLoneElement(q) == ae)
            { // name(i [, j]) alone and not a FUNCTION: DefinedSubFun passes the element by
              // reference (findvar's pointer to it), so its address goes on the stack (RC_ADEL),
              // indices evaluated and bounds checked where findvar would do them
                if (pr->by[na] > 0)
                    return 0; // BYVAL: DefinedSubFun evaluates the element again for its value
                if ((t = RBElement(x, &q, RC_ADEL, 1)) == 0)
                    return 0; // a string or structure element, or an index the compiler cannot take
                if (pr->by[na] < 0 && t != pr->ptype[na])
                    return 0; // BYREF of another type: DefinedSubFun's error
                pr->flags[na] = RP_ELEM | (t == T_INT ? RP_INT : 0);
                (*nval)++;
            }
            else
            { // an expression: its value
                if (pr->by[na] < 0)
                    return 0; // BYREF: "Variable required", which the fallback reports
                if ((t = RBEvaluate(x, &q)) == 0)
                    return 0;
                skipspace(q);
                if (q != ae)
                    return 0;
                pr->flags[na] = t == T_INT ? RP_INT : 0;
                (*nval)++;
            }
            na++;
            q = ae;
            if (*q != ',')
                break;
            q++;
        }
    if (na != pr->np || (close && *q != ')'))
        return 0; // a missing argument
    *qq = q;
    return 1;
}

// each parameter's words for RBMakeParams
static void RBParamOps(rbcx_t *x, rbparams_t *pr)
{
    for (int i = 0; i < pr->np; i++)
    {
        RBOp(x, pr->flags[i], 0);
        RBOp(x, pr->name[i], 0);
        RBOp(x, pr->astype[i], 0);
    }
}

static int RBCompileCall(unsigned char *entry, unsigned char *tok, uint16_t *code)
{
    rbcx_t x;
    rbparams_t pr;
    unsigned char *def, *p, *q;
    int idx, nval;
    if (!issymbol(*tok))
        return 0;
    q = tok + symbolsize(*tok);
    if (*q == '$' || *q == '%' || *q == '!' || (idx = FindSubFun(tok, 0)) < 0)
        return 0;
    def = subfun[idx];
    if (commandtbl_decode(def) != cmdSUB)
        return 0;
    // the definition's parameters
    p = def + sizeof(CommandToken);
    skipspace(p);
    if (!issymbol(*p))
        return 0;
    p += symbolsize(*p);
    if (!RBParams(def, &p, &pr))
        return 0;
    // the body: DefinedSubFun sets nextstmt to the zero that ends the header
    p = def;
    skipelement(p);
    if (p - def > 255)
        return 0;
    // the call's arguments
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    skipspace(q);
    if (*q == '(' || !RBArgs(&x, &q, 0, &pr, &nval))
        return 0;
    RBOp(&x, RC_CALL | (pr.np << 8), -nval);
    RBOp(&x, idx, 0);
    RBOp(&x, nval, 0);
    RBOp(&x, (p - def) | (pr.untyped << 8), 0);
    RBParamOps(&x, &pr);
    return RBFinish(&x, code);
}

/* P3d: a call to a user FUNCTION in an expression, as getvalue makes it (a
   name with a bracket straight after it).  RC_FCALL does what DefinedSubFun
   does for a FUNCTION (see RBCallFun).  Once one has run the statement
   cannot go to its fallback, which would make the call again, so calls
   compile only where the statement allows it (LET, IF and INC: FOR and DO
   have done their stack work before their expressions), and what could send
   a call to DefinedSubFun is checked before the first: gosubindex, which is
   the same at every call of a statement, and OPTION DEFAULT, by the guard
   RBFinish puts at the start (gbits).  A call that erases a variable or
   changes OPTION DEFAULT moves the bind stamp, and L_FCALL then binds the
   statement's variables again (RBRebind), as the text evaluator finds them
   after the call.  A FUNCTION without a type of its own has OPTION DEFAULT's
   at the call: taken to be the one in force here in text order (float
   without one).  Left to DefinedSubFun: a string or array result, DEFAULT
   NONE or STRING for an untyped one, and what RBCompileCall leaves. */
static int RBFcall(rbcx_t *x, unsigned char **pp, int *op)
{
    rbparams_t pr;
    unsigned char *p = *pp, *q, *def, *d, *fn;
    int idx, suf, dsuf, t, ftype, astype = 0, nval, fdef = 0;
    if (!x->calls)
        return 0;
    q = p + symbolsize(*p);
    suf = RBSuffix(&q);
    if (*q != '(' || (idx = FindSubFun(p, 1)) < 0)
        return 0;
    def = subfun[idx];
    if (commandtbl_decode(def) != cmdFUN)
        return 0;
    d = def + sizeof(CommandToken);
    skipspace(d);
    if (!issymbol(*d))
        return 0;
    fn = d;
    d += symbolsize(*d);
    dsuf = RBSuffix(&d);
    if (dsuf != suf || *d != '(' || d - def > 255)
        return 0; // DefinedSubFun's errors
    ftype = dsuf;
    if (!RBParams(def, &d, &pr))
        return 0;
    skipspace(d);
    if (*d == tokenAS)
    {
        d++;
        skipspace(d);
        t = RBTypeWord(&d);
        if ((t != T_INT && t != T_NBR) || dsuf)
            return 0;
        skipspace(d);
        if (*d == '(')
            return 0; // an array result
        ftype = t;
        astype = t | T_IMPLIED;
    }
    if (!ftype && (C.deftype == T_INT || C.deftype == T_NBR))
    { // untyped: the result OPTION DEFAULT gives it (bit 1 float, bit 2 integer
      // beside the parameters' bit 0), which the guard checks
        ftype = C.deftype;
        fdef = ftype == T_NBR ? 2 : 4;
    }
    if (ftype != T_INT && ftype != T_NBR)
        return 0; // a string, or DEFAULT NONE or STRING
    d = def;
    skipelement(d);
    if (d - def > 255)
        return 0;
    q++;
    if (!RBArgs(x, &q, ')', &pr, &nval))
        return 0;
    q++;
    RBOp(x, RC_FCALL | (pr.np << 8), 1 - nval);
    RBOp(x, idx, 0);
    RBOp(x, nval, 0);
    RBOp(x, (d - def) | ((pr.untyped | fdef) << 8), 0);
    x->gbits |= pr.untyped | fdef;
    RBOp(x, (fn + symbolsize(*fn) + (suf ? 1 : 0) - fn) | ((fn - def) << 8), 0); // the result's name, for findvar
    RBOp(x, astype, 0);
    RBParamOps(x, &pr);
    x->ncall++;
    *pp = RBNextOp(q, op);
    return ftype;
}

// P4b: EXIT FOR, EXIT DO, cmd_return and cmd_endfun, which take no operands:
// only checkend's error (something after the command) is left to the text
static int RBJump(rbcx_t *x, int op, int cells);
static void RBLand(rbcx_t *x, int at);
static int RBPartCheck(rbcx_t *x, int at, unsigned m);
static void RBGoto(rbcx_t *x, unsigned char *base, uint32_t libbit, unsigned char *target);
static void RBKey(rbcx_t *x, unsigned char *base, uint32_t libbit, unsigned char *p);

/* P4e: a statement whose handler does nothing (cmd_null: END IF, END SELECT,
   DATA, RANDOMIZE and the rest) compiles to nothing.  ELSE, ELSEIF and ELSE IF
   are cmd_else when the block before them runs into them: past the END IF
   the IF table gives for the statement's token (not cmdl - 2: spaces after
   the token are kept); ELSE checks its end first.  CONTINUE FOR runs its
   loop's NEXT (RC_CONTFOR); CONTINUE DO and plain CONTINUE stay text. */
/* P5b: BOX, LINE and PIXEL through the value splice (see RBSpliceCmd), and
   COLOUR (P5d): 0 if the compiler cannot take every argument, or for another
   form. */
static int RBCompileSplice(unsigned char *entry, unsigned char *cmdl, CommandToken ct, uint16_t *code)
{
    rbcx_t x;
    void (*fn)(void) = commandtbl[ct].fptr;
    unsigned char *p = cmdl, *ae, txt[2 * RB_MAXLIT];
    int n = 0, len = 0, t, i;
    if (fn != cmd_box && fn != cmd_line && fn != cmd_pixel && fn != cmd_colour)
        return 0;
    if (fn == cmd_line && (checkstring(p, (unsigned char *)"PLOT") || checkstring(p, (unsigned char *)"GRAPH") || checkstring(p, (unsigned char *)"AA")))
        return 0; // the forms whose arguments are arrays or keywords
    memset(&x, 0, sizeof(x));
    x.entry = entry; // (x.calls 0: no FUNCTION call, which getargaddress would make twice)
    if (fn == cmd_line || fn == cmd_pixel)
        RBOp(&x, RC_GUARD, 0); // LINE and PIXEL read OPTION LEGACY's syntax when it is on (COLOUR's getColour maps its colours itself)
    while (1)
    {
        ae = RBArgEnd(p, 0); // as getcsargs splits them
        skipspace(p);
        if (p != ae)
        { // a value
            if (n == 26 || len > (int)sizeof(txt) - 6)
                return 0;
            if ((t = RBEvaluate(&x, &p)) == 0)
                return 0;
            skipspace(p);
            if (p != ae)
                return 0;
            txt[len++] = T_VALUE;
            txt[len++] = (t == T_NBR ? 'a' : 'A') + n++;
        }
        if (*ae != ',')
            break;
        txt[len++] = ',';
        p = ae + 1;
    }
    txt[len++] = 0;
    RBOp(&x, RC_SPLICE | (((len + 1) / 2) << 8), -n);
    RBOp(&x, n, 0);
    for (i = 0; i < len; i += 2)
        RBOp(&x, txt[i] | ((i + 1 < len ? txt[i + 1] : 0) << 8), 0);
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

// P5c: INC var [, value], as cmd_inc does it (see the RC_ ops): 0 if another form.
// cmd_inc finds the variable and reads it before it evaluates the value (GCC's
// order for its *p = *p + getnumber(...)), so the code does too, and a FUNCTION
// in the value that changes the variable does not change what the value is
// added to.
static int RBCompileInc(unsigned char *entry, unsigned char *cmdl, uint16_t *code)
{
    rbcx_t x;
    unsigned char *p = cmdl, *q, *rhs;
    int suf, j, vt, t, i;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    x.calls = 1;
    skipspace(p);
    if ((q = RBVarRefS(p, &suf)) != NULL)
    { // a scalar: bound as a target (a CONST sends the record to cmd_inc's error)
        if ((j = RBBindK(&x, p, suf, 1, 0, 1)) < 0)
            return 0;
        vt = x.bind[j][1];
        if (vt != T_STR)
            RBOp(&x, RC_LDG | (j << 8), 1);
    }
    else
    { // an element: findvar's address, then the value there
        q = p;
        if ((vt = RBElement(&x, &q, RC_ADEL, 1)) == 0)
            return 0;
        RBOp(&x, RC_DUPLD, 1);
        j = -1;
    }
    p = q;
    skipspace(p);
    if (*p == 0 || *p == '\'')
    { // INC var: 1 (a string's is cmd_inc's error)
        union
        {
            long long i;
            MMFLOAT f;
            uint16_t w[4];
        } k;
        if (vt == T_STR)
            return 0;
        if (vt == T_NBR)
            k.f = 1.0;
        else
            k.i = 1;
        RBOp(&x, RC_LK, 1);
        for (i = 0; i < 4; i++)
            RBOp(&x, k.w[i], 0);
    }
    else
    {
        if (*p != ',')
            return 0;
        p++;
        skipspace(p);
        rhs = p;
        if (rhs - entry > 255 || (t = RBEvaluateS(&x, &p)) == 0 || (t == T_STR) != (vt == T_STR))
            return 0; // (getstring's and getnumber's errors are the text path's)
        skipspace(p);
        if (*p && *p != '\'')
            return 0; // getcsargs' and checkend's errors
        if (t != vt) // as getnumber and getinteger convert it
        {
            if (vt == T_NBR)
                RBCvif(&x);
            else
                RBOp(&x, RC_CVFI, 0);
        }
        if (RBMode == RB_SHADOW && vt != T_STR && !x.ncall)
        { // (not with a call, which the text evaluator would make again)
            RBOp(&x, RC_SHADOW | ((rhs - entry) << 8), 0);
            RBOp(&x, vt, 0);
        }
    }
    if (vt == T_STR)
        RBOp(&x, RC_INCS | (j << 8), -1); // (a string scalar: RBElement takes no string array)
    else
    {
        RBOp(&x, vt == T_NBR ? RC_INCF : RC_ADDI, -1); // (RC_ADDI is a plain add, as cmd_inc's)
        if (j >= 0)
            RBOp(&x, RC_STG | (j << 8), -1);
        else
            RBOp(&x, RC_STP, -2);
    }
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

// P4f: LOCAL [AS] [type] name {, name} (see RBLocal): 0 if it is another form
static int RBCompileLocal(unsigned char *entry, unsigned char *cmdl, uint16_t *code)
{
    rbcx_t x;
    unsigned char *p = cmdl, *tp;
    uint16_t off[RB_MAXCODE / 2];
    int n = 0, i;
    const unsigned char *sp;
    if (*p == tokenAS)
        p++;
    if ((tp = checkstring(p, (unsigned char *)"INTEGER")) != NULL || (tp = checkstring(p, (unsigned char *)"STRING")) != NULL ||
        (tp = checkstring(p, (unsigned char *)"FLOAT")) != NULL)
        p = tp; // (CheckIfTypeSpecified's reading; any other word is a name, or a structure type which RBLocal leaves to cmd_dim)
    while (1)
    {
        skipspace(p);
        if (!issymbol(*p) || n == RB_MAXCODE / 2 - 2 || p - entry > 0xFFFF)
            return 0;
        sp = SymSpelling(p, &i);
        if (RBDotBlocked(sp, i))
            return 0; // perhaps a structure member
        off[n++] = p - entry;
        p += symbolsize(*p);
        RBSuffix(&p);
        skipspace(p);
        if (*p == 0)
            break;
        if (*p != ',')
            return 0; // AS, brackets, LENGTH, a value, a comment: cmd_dim's
        p++;
    }
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    RBOp(&x, RC_LOCAL | (n << 8), 0);
    for (i = 0; i < n; i++)
        RBOp(&x, off[i], 0);
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

static int RBCompileExit(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *tok, unsigned char *cmdl, CommandToken ct, uint16_t *code)
{
    rbcx_t x;
    void (*fn)(void) = commandtbl[ct].fptr;
    int op = fn == cmd_exitfor ? RC_EXITFOR : fn == cmd_exit ? RC_EXITDO : fn == cmd_return ? RC_RETURN : fn == cmd_endfun ? RC_ENDFUN : 0;
    unsigned char *p = cmdl;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    if (fn == cmd_null)
        RBOp(&x, RC_END, 0);
    else if (fn == cmd_else)
    {
        struct iftab_entry *e = IfTableLookup(tok);
        skipspace(p);
        if (ct == cmdELSE && *p && *p != '\'')
            return 0; // checkend's error
        if (e == NULL || e->endif_tok == NULL)
            return 0; // cmd_else's scan: text
        p = e->endif_tok;
        skipelement(p);
        RBGoto(&x, base, libbit, p);
    }
    else if (fn == cmd_continue && *cmdl == tokenFOR)
        RBOp(&x, RC_CONTFOR, 0);
    else
    {
        skipspace(p);
        if (!op || (*p && *p != '\''))
            return 0;
        RBOp(&x, op, 0);
    }
    return RBFinish(&x, code);
}

/* P4c: SELECT CASE, CASE and END SELECT.  RBNextCmd is GetNextCommand
   without its error: the next command token after the element at p (a SUB
   call, a comment or an empty element is passed over), *line the start of
   the line it is on when the walk moves to a new one; NULL at the end. */
static unsigned char *RBNextCmd(unsigned char *p, unsigned char **line)
{
    do
    {
        if (*p != T_NEWLINE)
        {
            while (*p)
                p++;
            p++;
        }
        if (*p == 0)
            return NULL;
        if (*p == T_NEWLINE)
        {
            *line = p;
            p += T_NEWLINE_HDR;
        }
        if (*p == T_LINENBR)
            p += 3;
        skipspace(p);
        if (*p == T_LABEL)
        {
            p += p[1] + 2;
            skipspace(p);
        }
    } while (*p < C_BASETOKEN);
    return p;
}

// after the END SELECT that closes the SELECT or CASE whose element ends at
// p (cmd_case's scan); NULL if there is none
static unsigned char *RBEndSelect(unsigned char *p)
{
    unsigned char *line = NULL;
    int level = 1;
    while ((p = RBNextCmd(p, &line)) != NULL)
    {
        CommandToken tkn = commandtbl_at(p);
        if (tkn == cmdSELECT_CASE)
            level++;
        if (tkn == cmdEND_SELECT && --level == 0)
        {
            skipelement(p);
            return p;
        }
        p += sizeof(CommandToken);
    }
    return NULL;
}

// a CASE or CASE ELSE reached from the body before it, or END SELECT
static int RBCompileCase(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *cmdl, unsigned char *next, CommandToken ct, uint16_t *code)
{
    rbcx_t x;
    unsigned char *after;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    if (ct == cmdEND_SELECT)
        RBOp(&x, RC_END, 0); // cmd_null
    else
    {
        if ((after = RBEndSelect(next)) == NULL)
            return 0; // "No matching END SELECT": cmd_case raises it
        RBGoto(&x, base, libbit, after);
    }
    return RBFinish(&x, code);
}

// SELECT CASE on a number, as cmd_select: 0 if the compiler cannot take it
static int RBCompileSelect(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *cmdl, unsigned char *next, uint16_t *code)
{
    rbcx_t x;
    unsigned char *p = cmdl, *q = next, *line = NULL, *body, *isp;
    int st, t, o, jf, level = 1;
    CommandToken tkn;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    if ((st = RBEvaluate(&x, &p)) == 0)
        return 0; // a string selector, or one the compiler cannot take
    skipspace(p);
    if (*p && *p != '\'')
        return 0;
    while ((q = RBNextCmd(q, &line)) != NULL)
    {
        tkn = commandtbl_at(q);
        if (tkn == cmdSELECT_CASE)
            level++;
        if (tkn == cmdCASE && level == 1)
        {
            if (line == NULL)
                return 0; // (cmd_select would report errors against no line)
            p = q + sizeof(CommandToken);
            body = p;
            skipelement(body); // where a match goes: after this CASE's element
            RBOp(&x, RC_CLSET, 0); // errors against the CASE's line while its items are tested
            RBKey(&x, base, libbit, line);
            while (1)
            {
                skipspace(p);
                if ((isp = checkstring(p, (unsigned char *)"IS")) != NULL ||
                    ((tokentype(*p) & T_OPER) && *p != GetTokenValue((unsigned char *)"+") && *p != GetTokenValue((unsigned char *)"-")))
                { // CASE IS op value: doexpr with the selector on the left
                    if (isp)
                        p = isp;
                    skipspace(p);
                    if (!(tokentype(*p) & T_OPER))
                        return 0; // cmd_select's syntax error
                    o = *p++ - C_BASETOKEN;
                    RBOp(&x, RC_SEL, 1);
                    t = st;
                    while (o != E_END)
                        if (!RBDoExpr(&x, &p, &t, &o))
                            return 0;
                    if (t != T_INT)
                        return 0; // cmd_select's syntax error
                }
                else
                { // a value, or a range: each converted to the selector's type, as evaluate does
                    if ((t = RBEvaluate(&x, &p)) == 0)
                        return 0;
                    if (t != st)
                    {
                        if (st == T_NBR)
                            RBCvif(&x);
                        else
                            RBOp(&x, RC_CVFI, 0);
                    }
                    skipspace(p);
                    if (*p == tokenTO)
                    {
                        p++;
                        if ((t = RBEvaluate(&x, &p)) == 0)
                            return 0;
                        if (t != st)
                        {
                            if (st == T_NBR)
                                RBCvif(&x);
                            else
                                RBOp(&x, RC_CVFI, 0);
                        }
                        skipspace(p);
                        if (*p && *p != ',' && *p != '\'')
                            return 0; // evaluate's end check
                        RBOp(&x, RC_RANGE | (st << 8), -1);
                    }
                    else
                        RBOp(&x, RC_EQSEL | (st << 8), 0);
                }
                jf = RBJump(&x, RC_JFI, -1);
                RBOp(&x, RC_CGOTO, 0); // a match: after this CASE's element
                RBKey(&x, base, libbit, body);
                RBLand(&x, jf);
                skipspace(p);
                if (*p != ',')
                    break;
                p++;
            }
            if (*p && *p != '\'')
                return 0; // checkend's error
            q += sizeof(CommandToken);
            continue;
        }
        if (tkn == cmdCASE_ELSE && level == 1)
        {
            p = q + sizeof(CommandToken);
            skipspace(p);
            if (*p && *p != '\'')
                return 0; // checkend's error
            skipelement(p);
            RBOp(&x, RC_CGOTO, 0);
            RBKey(&x, base, libbit, p);
            return RBFinish(&x, code);
        }
        if (tkn == cmdEND_SELECT && --level == 0)
        { // no match: after END SELECT
            skipelement(q);
            RBOp(&x, RC_CGOTO, 0);
            RBKey(&x, base, libbit, q);
            return RBFinish(&x, code);
        }
        q += sizeof(CommandToken);
    }
    return 0; // no matching END SELECT: cmd_select raises it
}

// NEXT: cmd_next finds its loop by the NEXT's position, so the op needs no
// operands and no binds
static int RBCompileNext(unsigned char *entry, uint16_t *code)
{
    rbcx_t x;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    RBOp(&x, RC_NEXT, 0);
    return RBFinish(&x, code);
}

// Compile LET g = expression into code[]: the words, or 0 to leave the
// statement to its fallback.
static int RBCompileLet(unsigned char *entry, unsigned char *p, uint16_t *code)
{
    rbcx_t x;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    x.calls = 1;
    if (RBLetInto(&x, p, 0) == NULL)
        return 0;
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

// a forward jump: its op and a placeholder; returns the placeholder's index
static int RBJump(rbcx_t *x, int op, int cells)
{
    RBOp(x, op, cells);
    RBOp(x, 0, 0);
    return x->n - 1;
}

// point the jump whose placeholder is at w[at] here
static void RBLand(rbcx_t *x, int at)
{
    x->w[at] = x->n - (at + 1);
}

// end the statement by going to the text at target
static void RBGoto(rbcx_t *x, unsigned char *base, uint32_t libbit, unsigned char *target)
{
    uint32_t key = (uint32_t)(target - base) | libbit;
    RBOp(x, RC_GOTO, 0);
    RBOp(x, key & 0xFFFF, 0);
    RBOp(x, key >> 16, 0);
}

// the LET command token at p
static int RBIsLet(unsigned char *p)
{
    return p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN && commandtbl_decode(p) == RBTokLet;
}

/* IF, as cmd_if does it.  The condition is true when its value is not 0, as
   getnumber gives it.  A multi-line IF (nothing after THEN) compiles when
   the IF table knows its arms: false goes to the next arm, as cmd_if's table
   path does, and past the ELSE or ENDIF that ends the chain; an ELSEIF arm's
   condition is tested with CurrentLinePtr at its line, which cmd_if does not
   put back, and true goes after the arm's element (P4e).  A
   single-line IF compiles when the part after THEN, and after ELSE if there
   is one, is an assignment the compiler takes; false with no ELSE skips the
   rest of the line, as cmd_if's skipline does.  IF ... GOTO, THEN GOTO and
   THEN linenumber stay text. */
static int RBCompileIf(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *tok,
                       unsigned char *p, uint16_t *code)
{
    rbcx_t x;
    unsigned char *cond, *target, *thenp;
    int t, jf, jmp;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    x.calls = 1;
    skipspace(p);
    cond = p;
    if (cond - entry > 255 || (t = RBEvaluate(&x, &p)) == 0)
        return 0;
    skipspace(p);
    if (*p != tokenTHEN)
        return 0;
    p++;
    thenp = p; // the THEN part's record, when it has one (RBEmitIfParts)
    if (RBMode == RB_SHADOW && !x.ncall)
    { // (not of a call, which the text evaluator would make again)
        RBOp(&x, RC_SHADOWC | ((cond - entry) << 8), 0);
        RBOp(&x, t, 0);
    }
    jf = RBJump(&x, t == T_NBR ? RC_JFF : RC_JFI, -1);
    skipspace(p);
    if (*p == 0 || *p == '\'')
    { // a multi-line IF
        struct iftab_entry *e = IfTableLookup(tok);
        CommandToken ct;
        if (e == NULL || e->next_arm == NULL)
            return 0;
        RBOp(&x, RC_END, 0); // true: on into the IF's body
        RBLand(&x, jf);
        while ((ct = commandtbl_decode(e->next_arm)) == cmdELSEIF || ct == cmdELSE_IF)
        { // an ELSEIF: cmd_if's retest of its condition
            unsigned char *q = e->next_arm + sizeof(CommandToken), *c2, *after;
            int t2;
            skipspace(q);
            if (*q == 0)
                return 0; // cmd_if's syntax error
            after = q;
            skipelement(after);
            RBOp(&x, RC_CLSET, 0); // CurrentLinePtr at the ELSEIF's line, and left there
            RBKey(&x, base, libbit, e->line_ptr);
            c2 = q;
            if ((t2 = RBEvaluate(&x, &q)) == 0)
                return 0;
            skipspace(q);
            if (*q != tokenTHEN)
                return 0; // "IF without THEN": the text path's
            q++;
            skipspace(q);
            if (*q && *q != '\'')
                return 0; // "Unexpected text"
            if (RBMode == RB_SHADOW && !x.ncall)
            {
                RBOp(&x, RC_SHADOWCK, 0);
                RBOp(&x, t2, 0);
                RBKey(&x, base, libbit, c2);
            }
            jf = RBJump(&x, t2 == T_NBR ? RC_JFF : RC_JFI, -1);
            RBGoto(&x, base, libbit, after); // true: after the ELSEIF
            RBLand(&x, jf);
            if ((e = IfTableLookup(e->next_arm)) == NULL || e->next_arm == NULL)
                return 0;
        }
        if (ct != RBTokElse && ct != RBTokEndIf && ct != RBTokEnd_If)
            return 0;
        target = e->next_arm;
        skipelement(target); // false: past the ELSE or ENDIF
        RBGoto(&x, base, libbit, target);
    }
    else
    { // a single-line IF
        if (!RBIsLet(p))
        { // P4b: a THEN part that is not a LET: with no ELSE, true goes to its record, as
          // cmd_if sends nextstmt there (with an ELSE cmd_if runs it itself: text)
            unsigned char *e = p;
            if (*p >= '0' && *p <= '9')
                return 0; // THEN linenumber: cmd_if's findline
            if (!(p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN && commandtbl_decode(p) == RBTokIf))
            { // (THEN IF: the rest of the element is the THEN part, ELSE and all)
                e = p + sizeof(CommandToken);
                while (*e && *e != tokenELSE)
                    e++;
                if (*e == tokenELSE)
                    return 0;
            }
            RBGoto(&x, base, libbit, thenp);
            RBLand(&x, jf);
            target = p;
            skipline(target); // false: past the line
            RBGoto(&x, base, libbit, target);
            return RBFinish(&x, code);
        }
        int ncond = x.nbind, pure = !x.ncall, at; // (the binds the condition made; no call or RND in it)
        unsigned m;
        p += sizeof(CommandToken);
        skipspace(p);
        at = x.n;
        x.used = 0;
        if ((p = RBLetInto(&x, p, tokenELSE)) == NULL)
            return 0;
        if (pure && (m = x.used & ~((1u << ncond) - 1)) != 0 && !RBPartCheck(&x, at, m))
            return 0;
        if (*p == tokenELSE)
        {
            p++;
            skipspace(p);
            if (!RBIsLet(p))
                return 0;
            jmp = RBJump(&x, RC_JMP, 0);
            RBLand(&x, jf);
            p += sizeof(CommandToken);
            skipspace(p);
            at = x.n;
            x.used = 0;
            if (RBLetInto(&x, p, 0) == NULL)
                return 0;
            if (pure && (m = x.used & ~((1u << ncond) - 1)) != 0 && !RBPartCheck(&x, at, m))
                return 0;
            RBLand(&x, jmp);
            RBOp(&x, RC_END, 0);
        }
        else
        {
            RBOp(&x, RC_END, 0);
            RBLand(&x, jf);
            target = p;
            skipline(target); // false: the rest of the line is the THEN part
            RBGoto(&x, base, libbit, target);
        }
    }
    return RBFinish(&x, code);
}

/* P5d: a single-line IF's THEN or ELSE assignment (its code from w[at]) may
   use variables its condition does not (mask m: binds the condition did not
   make).  The IF binds every variable when it starts, and one that is not
   bound yet - made by nothing so far, or not yet met - sent the whole IF to
   text on every run, taken branch or not.  Those binds are optional: the
   record runs with them unbound, and this check before the part sends the
   IF to its fallback only when the part that needs one is taken, which is
   safe because its condition made no call and drew no RND (the text path
   evaluates it again).  The check goes in front of the part's code; the
   jumps are relative and the only one reaching the part lands at at. */
static int RBPartCheck(rbcx_t *x, int at, unsigned m)
{
    if (x->n + 2 > RB_MAXCODE)
        return 0;
    memmove(x->w + at + 2, x->w + at, (x->n - at) * 2);
    x->w[at] = RC_PARTCHK;
    x->w[at + 1] = m;
    x->n += 2;
    x->opt |= m;
    x->lk = 0; // (a constant's position, for RBCvif: stale now)
    return 1;
}

/* FOR var = start TO limit [STEP step], as cmd_for does it: its stack work
   for the variable first (a stale entry for it at this level removed, the
   depth checked, the entry pushed), then start stored in the variable before
   limit and step are read, each converted to the variable's type as getnumber
   or getinteger would; then the entry completed and the first test made.  The
   matching NEXT is found at compile time by cmd_for's own scan (ForFindNext).
   A FOR whose parts the compiler cannot take stays text. */
// one of FOR's values: its code, converted to the loop variable's type as
// getnumber or getinteger would, and its shadow check; 0 if it cannot compile
static int RBForPart(rbcx_t *x, unsigned char **pp, int vt)
{
    unsigned char *p = *pp, *ex;
    int t;
    skipspace(p);
    ex = p;
    if (ex - x->entry > 255 || (t = RBEvaluate(x, &p)) == 0)
        return 0;
    if (t != vt)
    {
        if (vt == T_NBR)
            RBCvif(x);
        else
            RBOp(x, RC_CVFI, 0);
    }
    if (RBMode == RB_SHADOW && !x->ncall)
    {
        RBOp(x, RC_SHADOW | ((ex - x->entry) << 8), 0);
        RBOp(x, vt, 0);
    }
    skipspace(p);
    *pp = p;
    return 1;
}

static int RBCompileFor(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *p,
                        unsigned char *next, uint16_t *code)
{
    rbcx_t x;
    unsigned char *q, *var, *nx, *after, vname[MAXVARLEN + 8];
    int suf, b, vt, j;
    uint32_t key;
    union
    {
        long long i;
        MMFLOAT f;
        uint16_t w[4];
    } one;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    skipspace(p);
    var = p;
    if ((q = RBVarRef(p, &suf)) == NULL || (b = RBBind(&x, p, suf, 1)) < 0 || q - var > MAXVARLEN)
        return 0;
    vt = x.bind[b][1];
    memcpy(vname, var, q - var); // cmd_for's vname: the variable as written
    vname[q - var] = 0;
    if ((nx = ForFindNext(next, vname, q - var, NULL)) == NULL)
        return 0; // no matching NEXT: the text path's error
    after = nx;
    skipelement(after);
    p = q;
    skipspace(p);
    if (*p != tokenEQUAL)
        return 0;
    p++;
    RBOp(&x, RC_FORP | (b << 8), 0);
    RBOp(&x, vt, 0);
    if (!RBForPart(&x, &p, vt) || *p != tokenTO)
        return 0;
    RBOp(&x, RC_STG | (b << 8), -1); // start goes into the variable before the limit is read
    p++;
    if (!RBForPart(&x, &p, vt))
        return 0;
    if (*p == tokenSTEP)
    {
        p++;
        if (!RBForPart(&x, &p, vt))
            return 0;
    }
    else
    { // no STEP: +1
        if (vt == T_NBR)
            one.f = 1.0;
        else
            one.i = 1;
        RBOp(&x, RC_LK, 1);
        for (j = 0; j < 4; j++)
            RBOp(&x, one.w[j], 0);
    }
    if (*p && *p != '\'')
        return 0;
    RBOp(&x, RC_FORT | (b << 8), -2);
    key = (uint32_t)(nx - base) | libbit;
    RBOp(&x, key & 0xFFFF, 0);
    RBOp(&x, key >> 16, 0);
    key = (uint32_t)(after - base) | libbit;
    RBOp(&x, key & 0xFFFF, 0);
    RBOp(&x, key >> 16, 0);
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

/* DO [WHILE|UNTIL cond] and LOOP [WHILE|UNTIL cond], as cmd_do and cmd_loop
   do them.  DO: its stack work (a stale entry for this DO removed, the depth
   checked, the entry pushed with the DO fast path off, so a fallback LOOP
   evaluates the text as it would anyway), then the entry test.  Its LOOP is
   found at compile time by cmd_do's own scan (DoFindLoop).  LOOP: its entry
   found as cmd_loop finds it, then the condition - the DO's, compiled again
   here (binds are by symbol id, so its names may be anywhere), or the
   LOOP's own, or none - and back to after the DO or the entry dropped.  A
   form the text path would reject (a condition on both, text after LOOP)
   stays text. */
static void RBKey(rbcx_t *x, unsigned char *base, uint32_t libbit, unsigned char *p)
{
    uint32_t key = (uint32_t)(p - base) | libbit;
    RBOp(x, key & 0xFFFF, 0);
    RBOp(x, key >> 16, 0);
}

static int RBCompileDo(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *cmdl,
                       unsigned char *next, uint16_t *code)
{
    rbcx_t x;
    unsigned char *p = cmdl, *cond = NULL, *loop, *after, *arg;
    int until = 0, t;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    while (*p && *p != tokenWHILE && *p != tokenUNTIL)
        p++; // cmd_do's own search for the condition
    if (*p)
    {
        until = (*p == tokenUNTIL);
        cond = p + 1;
    }
    if ((loop = DoFindLoop(next, cmdDO, NULL)) == NULL)
        return 0; // no matching LOOP: the text path's error
    after = loop;
    skipelement(after);
    arg = loop + sizeof(CommandToken);
    while (*arg && *arg < 0x80)
        arg++;
    if (cond && (*arg == tokenWHILE || *arg == tokenUNTIL))
        return 0; // "LOOP has a WHILE test": the text path's error
    if (C.ndo < RB_MAXDO)
    { // for the LOOP, which the walk meets later
        C.dotab[C.ndo].loop = loop;
        C.dotab[C.ndo].cond = cond;
        C.ndo++;
    }
    if (cond && cond - entry > 255)
        return 0;
    RBOp(&x, RC_DOP | ((cond ? cond - entry : 0) << 8), 0);
    RBOp(&x, (until ? RD_UNTIL : 0) | (cond ? RD_COND : 0), 0);
    RBKey(&x, base, libbit, loop);
    if (cond)
    {
        p = cond;
        if ((t = RBEvaluate(&x, &p)) == 0)
            return 0;
        skipspace(p);
        if (*p && *p != '\'')
            return 0;
        if (RBMode == RB_SHADOW && !x.ncall)
        {
            RBOp(&x, RC_SHADOWC | ((cond - entry) << 8), 0);
            RBOp(&x, t, 0);
        }
        RBOp(&x, RC_DOT | (t << 8), -1);
        RBKey(&x, base, libbit, after);
    }
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
}

static int RBCompileLoop(unsigned char *entry, unsigned char *base, uint32_t libbit, unsigned char *tok,
                         unsigned char *cmdl, uint16_t *code)
{
    rbcx_t x;
    unsigned char *p = cmdl, *cond = NULL;
    int flags, t, j;
    memset(&x, 0, sizeof(x));
    x.entry = entry;
    for (j = 0; j < C.ndo && C.dotab[j].loop != tok; j++)
        ;
    if (j == C.ndo)
        return 0; // its DO did not compile, or was not met first: text
    skipspace(p);
    if (C.dotab[j].cond)
    { // DO WHILE/UNTIL ... LOOP: the DO's condition, and nothing after LOOP
        if (*p && *p != '\'')
            return 0;
        cond = C.dotab[j].cond;
        flags = RL_ENTRY;
    }
    else if (*p == tokenWHILE || *p == tokenUNTIL)
    {
        flags = (*p == tokenUNTIL) ? RL_UNTIL : 0;
        cond = p + 1;
    }
    else if (*p == 0 || *p == '\'')
        flags = RL_ALWAYS;
    else
        return 0; // cmd_loop's syntax error
    RBOp(&x, RC_LOOPF, 0);
    if (cond)
    {
        p = cond;
        if ((t = RBEvaluate(&x, &p)) == 0)
            return 0;
        skipspace(p);
        if (*p && *p != '\'')
            return 0;
        if (RBMode == RB_SHADOW && !x.ncall)
        {
            RBOp(&x, RC_SHADOWCK, 0);
            RBOp(&x, t, 0);
            RBKey(&x, base, libbit, cond);
        }
        if (t == T_NBR)
            flags |= RL_NBR;
        RBOp(&x, RC_LOOPT | (flags << 8), -1);
    }
    else
        RBOp(&x, RC_LOOPT | (flags << 8), 0);
    RBOp(&x, RC_END, 0);
    return RBFinish(&x, code);
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
    if (cmd == 0)
        ncode = RBCompileCall(entry, tok, code + 1);
    if (cmd > 0)
    {
        CommandToken ct = commandtbl_decode(tok);
        int d;
        if (ct == RBTokLet)
            ncode = RBCompileLet(entry, cmdl, code + 1);
        else if (ct == RBTokIf)
            ncode = RBCompileIf(entry, base, libbit, tok, cmdl, code + 1);
        else if (ct == RBTokFor)
            ncode = RBCompileFor(entry, base, libbit, cmdl, next, code + 1);
        else if (ct == cmdDO)
            ncode = RBCompileDo(entry, base, libbit, cmdl, next, code + 1);
        else if (ct == cmdLOOP)
            ncode = RBCompileLoop(entry, base, libbit, tok, cmdl, code + 1);
        else if (ct == cmdNEXT)
            ncode = RBCompileNext(entry, code + 1);
        else if (ct == RBTokOption && (d = RBDefaultOption(cmdl)) >= 0)
            C.deftype = d;
        else if (ct == cmdSELECT_CASE)
            ncode = RBCompileSelect(entry, base, libbit, cmdl, next, code + 1);
        else if (ct == cmdCASE || ct == cmdCASE_ELSE || ct == cmdEND_SELECT)
            ncode = RBCompileCase(entry, base, libbit, cmdl, next, ct, code + 1);
        else if (ct == RBTokLocal)
            ncode = RBCompileLocal(entry, cmdl, code + 1);
        else if (ct == RBTokInc)
            ncode = RBCompileInc(entry, cmdl, code + 1);
        else if ((ncode = RBCompileSplice(entry, cmdl, ct, code + 1)) != 0)
            ;
        else
            ncode = RBCompileExit(entry, base, libbit, tok, cmdl, ct, code + 1);
    }
    w[n++] = RB_OP_STMT | (linestart ? RB_LINESTART : 0) | (ncode ? RB_COMPILED : 0) | (C.part ? RB_PART : 0);
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
        if (C.codelen & 2)
        { // a pad word, so that the addresses start on a 4-byte boundary (the code is a page's)
            int at = 4 + 2 * code[1];
            memmove(code + at + 1, code + at, (ncode + 1 - at) * 2);
            code[at] = 0;
            ncode++;
        }
        code[0] = ncode;
        RBEmitWords(code, ncode + 1);
    }
    C.stmts++;
}

/* A single-line IF's parts.  When its condition is true and it has no ELSE,
   cmd_if sends nextstmt to the byte after THEN; when false with an ELSE, to
   the byte after ELSE.  Neither starts an element, so without a record of
   their own each was a round trip to the text loop.  These records, marked
   RB_PART, sit after the IF's own in the stream and in the map; running on
   from a record skips them, so they are reached only by cmd_if's jump.  The
   positions are found as cmd_if finds them: THEN by a plain search from the
   IF's arguments, ELSE after the command the THEN part starts with.  A THEN
   part that is itself an IF takes the rest of the element (cmd_if's argc = 3)
   and has parts of its own. */
static void RBEmitPart(unsigned char *base, uint32_t libbit, unsigned char *entry);

static void RBEmitIfParts(unsigned char *base, uint32_t libbit, unsigned char *cmdl, unsigned char *next)
{
    unsigned char *then = cmdl, *q, *e;
    while (then < next && *then && *then != tokenTHEN)
        then++;
    if (then >= next || *then != tokenTHEN)
        return; // IF ... GOTO: cmd_if goes to its label itself
    q = then + 1;
    skipspace(q);
    if (*q == 0 || *q == '\'' || (*q >= '0' && *q <= '9'))
        return; // a multi-line IF, or THEN linenumber (cmd_if's findline)
    if (q[0] >= C_BASETOKEN && q[1] >= C_BASETOKEN && commandtbl_decode(q) == RBTokIf)
    {
        RBEmitPart(base, libbit, then + 1); // IF ... THEN IF ...: the rest is the THEN part
        return;
    }
    e = q + sizeof(CommandToken);
    while (e < next && *e && *e != tokenELSE)
        e++;
    if (e < next && *e == tokenELSE)
    { // with an ELSE: true runs the THEN part by execute_one_command, false goes after ELSE
        q = e + 1;
        skipspace(q);
        if (!(*q >= '0' && *q <= '9'))
            RBEmitPart(base, libbit, e + 1);
    }
    else
        RBEmitPart(base, libbit, then + 1);
}

// one part: a statement from entry to the end of the IF's element, as the
// text loop would meet it there
static void RBEmitPart(unsigned char *base, uint32_t libbit, unsigned char *entry)
{
    unsigned char *p = entry, *tok, *cmdl, *next;
    int cmd;
    skipspace(p);
    if (*p == 0 || *p == '\'')
        return;
    tok = p;
    if (p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN)
    {
        cmdl = next = p + sizeof(CommandToken);
        cmd = 1;
    }
    else
    {
        cmdl = next = p;
        cmd = 0;
    }
    skipspace(cmdl);
    skipelement(next);
    C.part = 1;
    RBEmitStmt(base, libbit, entry, 0, tok, cmd, cmdl, next);
    C.part = 0;
    if (cmd && commandtbl_decode(tok) == RBTokIf)
        RBEmitIfParts(base, libbit, cmdl, next);
}

// Walk one image as ExecuteProgram would, recording each statement.
/* A SUB or FUNCTION's locals.  When the walk reaches its header it lists
   the unit's parameters (a suffix or AS gives a type; BYVAL and BYREF are
   stepped over), a FUNCTION's own name (its result), and every LOCAL,
   STATIC and CONST in its body, so a name compiles inside the unit with the
   type it has there; any other name is a global.  A statement that meets
   the name before its LOCAL has run, or a local of another type, finds that
   out at its bind and runs as text.  A unit that cannot be listed (a header
   the survey cannot read, or more than RB_MAXULOCAL locals) compiles
   globals only, as P2 did. */
unsigned char *CheckByKeyword(unsigned char *p, int kind); // MMBasic.c

// the header's parameters and result: 0 if it could not be read
static int RBUnitHeader(unsigned char *p, int isfun)
{
    unsigned char *q, *name, *b;
    int suf, t;
    skipspace(p);
    if (!issymbol(*p))
        return 0;
    name = p;
    q = p + symbolsize(*p);
    suf = RBSuffix(&q);
    skipspace(q);
    if (*q == '(')
    {
        q++;
        skipspace(q);
        while (*q != ')')
        {
            unsigned char *v;
            int ps, pt = 0;
            if ((b = CheckByKeyword(q, 'V')) != NULL || (b = CheckByKeyword(q, 'R')) != NULL)
                q = b;
            skipspace(q);
            if (!issymbol(*q))
                return 0;
            v = q;
            q += symbolsize(*q);
            ps = RBSuffix(&q);
            skipspace(q);
            if (*q == '(')
            { // an array parameter, a()
                q++;
                skipspace(q);
                if (*q != ')')
                    return 0;
                q++;
                skipspace(q);
            }
            if (*q == tokenAS)
            {
                q++;
                skipspace(q);
                pt = RBTypeWord(&q);
                skipspace(q);
            }
            RBTypeName(v, ps ? ps : pt, 1);
            if (*q == ',')
            {
                q++;
                skipspace(q);
            }
            else if (*q != ')')
                return 0;
        }
        q++;
        skipspace(q);
    }
    if (isfun)
    {
        t = 0;
        if (*q == tokenAS)
        {
            q++;
            skipspace(q);
            t = RBTypeWord(&q);
        }
        RBTypeName(name, suf ? suf : t, 1);
    }
    return 1;
}

// the header at cmdl starts a unit whose body starts at next
static void RBUnitBegin(unsigned char *cmdl, unsigned char *next, int isfun)
{
    unsigned char *p = next, *q;
    CommandToken ct;
    C.unit = 1;
    C.nulocal = 0;
    RBCollect = 1;
    if (!RBUnitHeader(cmdl, isfun))
        C.nulocal = RB_MAXULOCAL + 1;
    while (C.nulocal <= RB_MAXULOCAL)
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
        if (*p && *p != '\'' && p[0] >= C_BASETOKEN && p[1] >= C_BASETOKEN)
        {
            ct = commandtbl_decode(p);
            q = p + sizeof(CommandToken);
            if (ct == RBTokEndSub || ct == RBTokEndFun || ct == cmdSUB || ct == cmdFUN)
                break;
            if (ct == RBTokLocal || ct == RBTokStatic)
                RBSurveyDim(q, 1);
            else if (ct == RBTokConst)
                RBSurveyConst(q, 1);
        }
        if (*p)
            skipelement(p);
        if ((p[0] == 0 && p[1] == 0) || (p[0] == 0xff && p[1] == 0xff))
            break;
    }
    RBCollect = 0;
}

static void RBWalk(unsigned char *base, uint32_t libbit)
{
    unsigned char *p = base, *entry, *tok, *cmdl, *next;
    int linestart, cmd;
    uint32_t endkey;
    uint16_t w[3];
    C.unit = 0;
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
            if (cmd > 0 && (commandtbl_decode(tok) == cmdSUB || commandtbl_decode(tok) == cmdFUN))
                RBUnitBegin(cmdl, next, commandtbl_decode(tok) == cmdFUN);
            RBEmitStmt(base, libbit, entry, linestart, tok, cmd, cmdl, next);
            if (cmd > 0 && commandtbl_decode(tok) == RBTokIf)
                RBEmitIfParts(base, libbit, cmdl, next);
            if (cmd > 0 && (commandtbl_decode(tok) == RBTokEndSub || commandtbl_decode(tok) == RBTokEndFun))
                C.unit = 0;
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
    C.ndo = 0;
    RBWalk(ProgMemory, 0);
    if (C.pass == 2)
        RBTabTo(C.nbprog - 1); // the program's last buckets end where the library's entries start
    if (LibPresent())
        RBWalk(LibMemory, RB_LIBBIT);
    if (C.pass == 2)
        RBTabTo(C.ntab - 1); // and the library's, and the sentinel, at the end of the map
}

// The survey's walk over one image: every DIM, LOCAL, STATIC and CONST.
static void RBSurveyImage(unsigned char *p)
{
    unsigned char *cmdl;
    CommandToken ct;
    int inunit = 0;
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

// The survey, over the program and the library: their names share canonical
// entries, and a global either declares is one variable.
static void RBSurvey(void)
{
    C.ntypes = SymCanonOf ? SymCanonCount : 0;
    C.types = C.ntypes ? GetTempMemory(C.ntypes) : NULL; // zeroed: no type known
    if (!C.types)
    {
        C.ntypes = 0;
        return;
    }
    RBSurveyImage(ProgMemory);
    if (LibPresent())
        RBSurveyImage(LibMemory);
}

// Compile the program (and the library) into the slot.
static void RBCompile(rbheader_t *h)
{
    uint32_t codeoff;
    RBComp = GetTempMemory(sizeof(struct rbcomp)); // (freed with the statement's temporary memory)
    memset(&C, 0, sizeof(C));
    W.full = 0;
    RBTokLet = GetCommandValue((unsigned char *)"Let");
    RBTokDim = GetCommandValue((unsigned char *)"Dim");
    RBTokLocal = GetCommandValue((unsigned char *)"Local");
    RBTokStatic = GetCommandValue((unsigned char *)"Static");
    RBTokConst = GetCommandValue((unsigned char *)"Const");
    RBTokOption = GetCommandValue((unsigned char *)"Option");
    RBTokInc = GetCommandValue((unsigned char *)"Inc");
    RBTokIf = GetCommandValue((unsigned char *)"If");
    RBTokFor = GetCommandValue((unsigned char *)"For");
    RBTokElse = GetCommandValue((unsigned char *)"Else");
    RBTokEndIf = GetCommandValue((unsigned char *)"EndIf");
    RBTokEnd_If = GetCommandValue((unsigned char *)"End If");
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
    RBNeed = codeoff + C.codelen;
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
static int RBCacheOK;                  // the stream is in PSRAM: records may cache their binds
static uint32_t RBNbProg, RBNTab;      // its program buckets, and all its entries
extern uint32_t core1stack[];
extern int TraceOn;
extern unsigned char *TraceBuff[TRACE_BUFF_SIZE];
extern int TraceBuffIndex;
extern uint32_t g_perf_usercmd_count;
extern uint32_t DefinedSubFunMem;       // MMBasic.c: a call's arguments are being processed
extern int DefinedSubFunLocalIndex;     // MMBasic.c: g_LocalIndex when it started
#define PERF_CMDTOKEN_MAX 1024 // as in MMBasic.c

// Clear every compiled record's bind-cache stamp: a stream used again may
// hold stamps from before a reboot, which restarted SymBindEvent.
static void RBClearCaches(void)
{
    const rbheader_t *h = (const rbheader_t *)RBSlotBase();
    uint16_t *r = (uint16_t *)(RBSlotBase() + h->codeoff), *end = r + h->codelen / 2;
    while (r < end)
    {
        if ((r[0] & 0xFF) == RB_OP_END)
            r += 3;
        else if ((r[3] & 0xFF) == RB_OP_NOP)
            r += 4;
        else if (r[0] & RB_COMPILED)
        {
            r[RB_HDR(r) + 2] = r[RB_HDR(r) + 3] = 0; // the stamp, after [n] [nbind]
            r += RB_HDR(r) + 1 + r[RB_HDR(r)];
        }
        else
            r += RB_HDR(r);
    }
}

static void RBMapStream(void)
{
    const rbheader_t *h = (const rbheader_t *)RBSlotBase();
    RBBase = RBSlotBase();
    RBMap = (const uint32_t *)(RBBase + h->mapoff);
    RBTab = (const uint16_t *)(RBBase + h->tabof);
    RBCacheOK = RBInPsram();
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
//
// The text loop calls CheckAbort and check_interrupt after every statement.
// They do real work only when an abort or an interrupt is pending, and
// otherwise on routinechecks' 100 us cadence (USB, touch, the cursor, the
// WiFi poll, which ProcessWeb also makes at least every ms).  The stream
// calls them when MMAbort or IntReady says so, at once, and otherwise every
// 100 us: a PC-sampled compiled loop spent 39% of its time in them.
#define RB_HOUSE_US 100
static uint32_t RBHouseAt; // time_us_32() when the stream last called them
#define RB_HOUSE_N 8 // the chain looks at abort, interrupts and the timer once in this many statements
static int RBHouseN;  // statements the chain runs before it next looks
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
        uint32_t now = time_us_32();
        if (MMAbort || IntReady.any || now - RBHouseAt >= RB_HOUSE_US)
        {
            RBHouseAt = now;
            CheckAbort();
            check_interrupt();
        }
    }
    return nextstmt != here;
}

// RBExec's call to a user SUB, as the text loop makes it.  In flash, as are
// the other cold parts of RunStream: its RAM is the stack's (see RBRun).
static __attribute__((noinline)) unsigned char *RBExecSub(const uint16_t *r, unsigned char *e)
{
    unsigned char *p = e + (r[3] >> 8), *end;
    int i;
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
    return end;
}

// TRACE ON: the line's number, as the text loop prints it (a line of the
// program's: the library's lines have none)
static __attribute__((noinline)) void RBTraceLine(unsigned char *entry)
{
    if (!(entry > ProgMemory && entry < ProgMemory + MAX_PROG_SIZE))
        return;
    inpbuf[0] = '[';
    IntToStr((char *)inpbuf + 1, CountLines(entry), 10);
    strcat((char *)inpbuf, "]");
    MMPrintString((char *)inpbuf);
    uSec(1000);
}

// Run the statement whose STMT record is r and whose text starts at e, as
// the text loop would.  Returns where the statement ends, the nextstmt it was
// given: if the handler leaves nextstmt there, the next record follows.
static inline __attribute__((always_inline)) unsigned char *RBExec(const uint16_t *r, unsigned char *e)
{
    unsigned char *p, *end;
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
        end = RBExecSub(r, e); // a call to a user SUB
    return end;
}

// OPTION COMPILE SHADOW: evaluate the right-hand side at p through the text
// evaluator, convert it as cmd_let would for type, and stop if the compiled
// value v differs from it in any bit.  Out of line: inlined, it put its
// buffers in RBRun's frame and its code in RAM.
static __attribute__((noinline)) void RBShadow(unsigned char *p, int type, const void *v)
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
    evaluate(p, &f, &i64, &str, &t, E_NOERROR); // the compiler has checked the end
    if (type == T_STR)
    { // the strings, byte for byte
        unsigned char *cs;
        memcpy(&c, v, sizeof(c));
        cs = (unsigned char *)(uint32_t)c.i;
        if (*cs == *str && memcmp(cs + 1, str + 1, *cs) == 0)
            return;
        memcpy(a, cs + 1, *cs < 38 ? *cs : 38);
        a[*cs < 38 ? *cs : 38] = 0;
        memcpy(b, str + 1, *str < 38 ? *str : 38);
        b[*str < 38 ? *str : 38] = 0;
        error("SHADOW: compiled \"$\", text \"$\"", a, b);
    }
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

// OPTION COMPILE SHADOW for IF's condition: its truth through the text
// evaluator, as cmd_if's getnumber gives it, against the compiled value's.
static __attribute__((noinline)) void RBShadowCond(unsigned char *p, int type, const void *v)
{
    MMFLOAT f;
    long long i64;
    unsigned char *str;
    int t = T_NBR, text, comp;
    union
    {
        long long i;
        MMFLOAT f;
    } c;
    evaluate(p, &f, &i64, &str, &t, E_NOERROR);
    text = ((t & T_INT) ? (MMFLOAT)i64 : f) != 0;
    memcpy(&c, v, sizeof(c));
    comp = type == T_NBR ? c.f != 0 : c.i != 0;
    if (text != comp)
        error("SHADOW: IF compiled %, text %", comp, text);
}

#define RB_INF 0x7FF0000000000000LL // +INFINITY's bits

// a cell of RBRun's stack, or a bound variable's value
union cell
{
    long long i;
    MMFLOAT f;
};

// RC_CALL's work: what DefinedSubFun does for a SUB, in its order (see
// RBCompileCall).  pc is at the op's operands, np the parameters, val the
// first value argument on the VM's stack, end the call statement's end.
// Returns 0, before anything has happened, if DefinedSubFun must do it.  In
// flash: the time is findvar's, and RBRun's RAM is the stack's.
// Each parameter of the call at def made by findvar as DefinedSubFun makes it,
// and given its argument: a variable of its type by reference (unless BYVAL),
// anything else converted.  CurrentLinePtr is left as DefinedSubFun leaves it:
// the definition until the first parameter is made, the caller after.
static void RBMakeParams(unsigned char *def, const uint16_t *pp, int np, union cell **slot, union cell *val, unsigned char *callers)
{
    unsigned char nm[MAXVARLEN + 8];
    union cell *src;
    struct s_vartbl *v;
    int i, at;
    CurrentLinePtr = def; // errors at the definition, until the first parameter is made (as DefinedSubFun has it)
    for (i = 0; i < np; i++, pp += 3)
    {
        memcpy(nm, def + (pp[1] >> 8), pp[1] & 0xFF);
        nm[pp[1] & 0xFF] = 0;
        findvar(nm, pp[2] | V_FIND | V_DIM_VAR | V_LOCAL | V_EMPTY_OK); // the parameter
        v = VREC(g_VarIndex);
        CurrentLinePtr = callers; // errors at the caller
        if (pp[0] & RP_VAR)
            src = slot[pp[0] >> 8];
        else if (pp[0] & RP_ELEM)
            src = (union cell *)(uint32_t)(val++)->i; // the element's address, as findvar gave it to DefinedSubFun
        else
            src = val++;
        at = (pp[0] & RP_INT) ? T_INT : T_NBR;
        if ((pp[0] & (RP_VAR | RP_ELEM)) && !(pp[0] & RP_BYVAL) && TypeMask(v->type) == at)
        { // a variable or an element of the parameter's type: by reference
            v->val.s = (unsigned char *)src;
            v->type |= T_PTR;
        }
        else if ((v->type & T_NBR) && at == T_NBR)
            v->val.f = src->f;
        else if ((v->type & T_NBR) && at == T_INT)
            v->val.f = src->i;
        else if ((v->type & T_INT) && at == T_INT)
            v->val.i = src->i;
        else if ((v->type & T_INT) && at == T_NBR)
            v->val.i = FloatToInt64(src->f);
        else
            error("Incompatible type"); // (a string parameter does not compile)
    }
}

static __attribute__((noinline)) int RBCallSub(const uint16_t *pc, int np, union cell **slot, union cell *val, unsigned char *end)
{
    int idx = pc[0];
    unsigned char *def = subfun[idx], *callers = CurrentLinePtr;
    const uint16_t *pp = pc + 3;
    // what would stop DefinedSubFun before it starts sends the call there
    if (gosubindex >= MAXGOSUB || ((pc[2] >> 8) && DefaultType != T_INT && DefaultType != T_NBR))
        return 0;
    RBRan++;
    RBCode++;
    if (g_option_profiling)
    {
        g_perf_usercmd_count++;
        if (g_perf_subcall_count && idx < MAXSUBFUN)
            g_perf_subcall_count[idx]++;
    }
    g_FunReturnArrayCount = 0;
    DefinedSubFunLocalIndex = g_LocalIndex;
    nextstmt = end; // where END SUB returns to
    errorstack[gosubindex] = callers;
    substack[gosubindex] = def; // for STATIC
    gosubstack[gosubindex++] = nextstmt;
    DefinedSubFunMem = 2; // an error from here on unwinds the call (no argument block)
    g_LocalIndex++;
#ifdef SUBPROFILE
    EnterLocalFrame();
    g_current_sub_idx = idx;
#endif
    RBMakeParams(def, pp, np, slot, val, callers);
    DefinedSubFunMem = 0;
    CurrentLinePtr = callers;
    nextstmt = def + (pc[2] & 0xFF); // the SUB's body
    return 1;
}

// RC_FCALL's work: what DefinedSubFun does for a FUNCTION called from
// getvalue, in its order; *res gets its value.  pc is at the op's operands:
// [idx] [values] [body | untyped << 8] [result's name] [its AS type] and the
// parameters.  The body runs in a nested ExecuteProgram, as DefinedSubFun
// runs it.  Returns 0, before anything has happened, if DefinedSubFun must
// do it.  In flash, as RBCallSub.
extern uint32_t stackwarn;           // MMBasic.c: 1K above the stack's floor
void StackNearLimit(uint32_t stack); // MMBasic.c: the overflow error, or a note for the prompt's warning
// RC_GUARD's check for a record's FUNCTION calls (see RBFinish), and RBCallFun's:
// OPTION DEFAULT is not what they need (bit 0: a number, 1: float, 2: integer);
// with no bits, RC_GUARD's other use: OPTION LEGACY (CMM1) is on.  In flash.
static __attribute__((noinline)) int RBDefaultBad(int b)
{
    if (!b)
        return CMM1;
    return ((b & 1) && DefaultType != T_INT && DefaultType != T_NBR) || ((b & 2) && DefaultType != T_NBR) ||
           ((b & 4) && DefaultType != T_INT);
}

static void RBRebind(const uint16_t *r, union cell **slot);
static __attribute__((noinline)) int RBCallFun(const uint16_t *pc, int np, union cell **slot, union cell *val, union cell *res,
                                               const uint16_t *r)
{
    int idx = pc[0], ftype, savetoken;
    // what can change the binds at the caller's level (as RBRun's gen)
    uint32_t g0 = g_LocalIndex < SYM_LEVELS ? SymBindGenG + SymLevelGen[g_LocalIndex] : 0, g1;
    unsigned char *def = subfun[idx], *callers = CurrentLinePtr, nm[MAXVARLEN + 8], *tp, *savecmdline, *savenext;
    uint32_t stack;
    // TestStackOverflow (MMBasic.c), which the text path's evaluator makes at every
    // value: a recursion through compiled calls never reaches it
    __asm volatile("MRS %0, msp" : "=r"(stack));
    if (stack < stackwarn)
        StackNearLimit(stack);
    if (gosubindex >= MAXGOSUB)
        return 0; // (the same at every call of the statement: only its first can find it so)
    if ((pc[2] >> 8) && RBDefaultBad(pc[2] >> 8))
        error("OPTION DEFAULT changed by a FUNCTION in this statement"); // (the record's guard held at its start)
    g_FunReturnArrayCount = 0;
    DefinedSubFunLocalIndex = g_LocalIndex;
    errorstack[gosubindex] = callers;
    substack[gosubindex] = def; // for STATIC
    gosubstack[gosubindex++] = NULL; // a FUNCTION: the end of ExecuteProgram returns to it
    DefinedSubFunMem = 2; // an error from here on unwinds the call (no argument block)
    g_LocalIndex++;
#ifdef SUBPROFILE
    EnterLocalFrame();
    g_current_sub_idx = idx;
#endif
    RBMakeParams(def, pc + 5, np, slot, val, callers);
    DefinedSubFunMem = 0;
    memcpy(nm, def + (pc[3] >> 8), pc[3] & 0xFF);
    nm[pc[3] & 0xFF] = 0;
    tp = findvar(nm, pc[4] | V_FIND | V_DIM_VAR | V_LOCAL | V_EMPTY_OK | V_FUNCT); // the result
    ftype = VREC(g_VarIndex)->type;
    savenext = nextstmt; // the globals the command that made the call uses
    savetoken = cmdtoken;
    savecmdline = cmdline;
    ExecuteProgram(def + (pc[2] & 0xFF)); // the body
    CurrentLinePtr = callers;
    cmdline = savecmdline;
    cmdtoken = savetoken;
    nextstmt = savenext;
    if (ftype & T_NBR)
        res->f = *(MMFLOAT *)tp;
    else
        res->i = *(long long int *)tp;
    ClearVars(g_LocalIndex--, true);
#ifdef SUBPROFILE
    LeaveLocalFrame();
#endif
    g_TempMemoryIsChanged = true;
    gosubindex--;
    g1 = g_LocalIndex < SYM_LEVELS ? SymBindGenG + SymLevelGen[g_LocalIndex] : 0;
    if (g1 != g0 || !g1)
        RBRebind(r, slot); // the call erased a variable or changed OPTION DEFAULT
    return 1;
}

// findvar with the array v found and its k indices (ints) at idx: the count
// of dimensions checked, each index against its bound, the element found as
// findvar finds it
static __attribute__((noinline)) union cell *RBElementAt(struct s_vartbl *v, union cell *idx, int k)
{
    int i, nbr, j;
    for (i = 0; i < MAXDIM && !DimIsEnd(RAW_DIM(*v, i)); i++)
        ;
    if (i != k)
        error("Array dimensions");
    for (i = 0; i < k; i++)
        if (idx[i].i > DimUpper(RAW_DIM(*v, i)) || idx[i].i < g_OptionBase)
            error("Index out of bounds");
    nbr = idx[0].i - g_OptionBase;
    j = 1;
    for (i = 1; i < k; i++)
    {
        j *= DimElements(RAW_DIM(*v, i - 1));
        nbr += (idx[i].i - g_OptionBase) * j;
    }
    return (union cell *)(v->val.s + nbr * 8);
}

// RC_RETURN: cmd_return, END SUB's and RETURN's; 0, before anything happens,
// if there is nothing to return to (the fallback raises the error)
// RC_FN: function id's work on the value v, by the helper its fun_ handler
// calls (Functions.c)
static __attribute__((noinline)) void RBFn(int id, union cell *v)
{
    switch (id)
    {
    case RF_SIN:
        v->f = FnSin(v->f);
        break;
    case RF_COS:
        v->f = FnCos(v->f);
        break;
    case RF_TAN:
        v->f = FnTan(v->f);
        break;
    case RF_ATN:
        v->f = FnAtn(v->f);
        break;
    case RF_SQR:
        v->f = FnSqr(v->f);
        break;
    case RF_EXP:
        v->f = FnExp(v->f);
        break;
    case RF_LOG:
        v->f = FnLog(v->f);
        break;
    case RF_DEG:
        v->f = FnDeg(v->f);
        break;
    case RF_RAD:
        v->f = FnRad(v->f);
        break;
    case RF_INT:
        v->i = FnInt(v->f);
        break;
    case RF_FIX:
        v->i = FnFix(v->f);
        break;
    case RF_ABSF:
        v->f = fabs(v->f); // fun_abs's float case
        break;
    case RF_ABSI:
        v->i = FnAbsI(v->i);
        break;
    case RF_SGNF:
        v->i = FnSgnF(v->f);
        break;
    case RF_RND:
        v->f = RndVal();
        break;
    default: // RF_SGNI: fun_sgn's integer case
        v->i = (v->i > 0LL) - (v->i < 0LL);
    }
}

/* The value splice (P5b).  RC_SPLICE runs the command whose record is r as
   RBExec runs a CMD record, but with cmdline at the spliced copy of its
   arguments in the code, and RBSplice at their values on the VM's stack,
   which getvalue reads through RBSpliceValue when it meets a T_VALUE. */
static union cell *RBSplice;
extern char CMM1; // Draw.c: OPTION LEGACY

unsigned char *RBSpliceValue(unsigned char *p, MMFLOAT *fa, long long int *ia, unsigned char **sa, int *ta)
{
    unsigned char k = p[1];
    if (RBSplice == NULL)
        SyntaxError(); // (never met in program text: tokenise makes control characters spaces)
    if (k >= 'a')
    {
        *fa = RBSplice[k - 'a'].f;
        *ta = T_NBR;
    }
    else if (k >= 'A')
    {
        *ia = RBSplice[k - 'A'].i;
        *ta = T_INT;
    }
    else
    { // P5c: a string, as getvalue has one (it copies none)
        *sa = (unsigned char *)(uint32_t)RBSplice[k - '0'].i;
        *ta = T_STR;
    }
    return p + 2;
}

/* P5c: RC_FSPLICE, a built-in function called as getvalue calls it (ep at a
   copy of its argument text, targ at its types, its answer in fret, iret or
   sret), with its arguments' values spliced; the answer replaces them on
   the VM's stack.  pc is at [n | token << 8], the text after it. */
static unsigned char RBFnText[2 * RB_MAXLIT + 2]; // (the copy: a handler may write into its argument text)
static __attribute__((noinline)) void RBFnSpliceRun(const uint16_t *pc, unsigned int words, union cell *vals)
{
    unsigned char tkn = pc[0] >> 8;
    union cell *save = RBSplice;
    int tmp;
    memcpy(RBFnText, pc + 1, 2 * words);
    ep = RBFnText;
    RBSplice = vals;
    tmp = targ = TypeMask(tokentype(tkn));
    tokenfunction(tkn)();
    RBSplice = save;
    if ((tmp & targ) == 0)
        error("Internal fault 2(sorry)");
    if (targ & T_STR)
        vals[0].i = (uint32_t)sret;
    else if (targ & T_INT)
        vals[0].i = iret;
    else
        vals[0].f = fret;
}


static __attribute__((noinline)) void RBSpliceCmd(const uint16_t *r, unsigned char *e, unsigned int nw, const uint16_t *txt, union cell *vals)
{
    cmdline = (unsigned char *)txt;
    nextstmt = e + (nw >> 8);
    cmdtoken = r[4];
    targ = T_CMD;
    CmdTokenPtr = e + (r[3] >> 8);
    RBSplice = vals;
    commandtbl[cmdtoken].fptr();
    RBSplice = NULL;
}

// RC_LOCAL: what cmd_dim does for LOCAL name {, name}, in its order, the
// names read where they are in the program (see RBCompileLocal).  Returns
// the code after the names' offsets, or NULL before anything has happened
// if a structure type is in the type's place.
static __attribute__((noinline)) const uint16_t *RBLocal(unsigned char *e, unsigned int nw, const uint16_t *off, unsigned int w)
{
    unsigned char *cmdl = e + (nw & 0xFF);
    int n = w >> 8, type, i;
    if (*cmdl == tokenAS)
        cmdl++;
    CheckIfTypeSpecified(cmdl, &type, true);
#ifdef STRUCTENABLED
    if (type & T_STRUCT)
        return NULL;
#endif
    for (i = 0; i < n; i++)
    {
        if (g_LocalIndex == 0)
            error("Invalid here");
        findvar(e + off[i], type | V_LOCAL | V_FIND | V_DIM_VAR | V_DIM_NEW);
        if (DimIsEmptyParam(RAW_DIM((*VREC(g_VarIndex)), 0)))
            error("Array dimensions");
    }
    return off + n;
}

// cmd_let's store into a string: its length against the variable's size (a
// BYREF parameter has its caller's), then the copy; for RC_INCS (P5c)
// cmd_inc's string case, the same check of the two together, then the
// concatenation
static __attribute__((noinline)) void RBStoreStr(unsigned int w, struct s_vartbl *v, unsigned char *s)
{
    int inc = (w & 0xFF) == RC_INCS;
    if ((inc ? *v->val.s : 0) + *s > v->size)
        error("String too long");
    if (inc)
        Mstrcat(v->val.s, s);
    else
        Mstrcpy(v->val.s, s);
}

// doexpr's call of operator o on the two strings at the top of the VM's
// stack: + leaves a string (in temporary memory), a comparison an integer
static __attribute__((noinline)) void RBOpStr(int o, union cell *sp)
{
    sarg1 = (unsigned char *)(uint32_t)sp[-2].i;
    sarg2 = (unsigned char *)(uint32_t)sp[-1].i;
    targ = T_STR;
    tokentbl[o].fptr();
    if (targ & T_STR)
        sp[-2].i = (uint32_t)sret;
    else
        sp[-2].i = iret;
}

// cmd_select's tests (RC_EQSEL, RC_RANGE, the selector's type in w's high
// byte) on the selector s: v[0] == s, or v[0] <= s <= v[1]
static __attribute__((noinline)) int RBSel(unsigned int w, const union cell *s, const union cell *v)
{
    if ((w & 0xFF) == RC_RANGE)
        return (w >> 8) == T_NBR ? (s->f >= v[0].f && s->f <= v[1].f) : (s->i >= v[0].i && s->i <= v[1].i);
    return (w >> 8) == T_NBR ? s->f == v[0].f : s->i == v[0].i;
}

static __attribute__((noinline)) int RBReturn(void)
{
    if (gosubindex == 0 || gosubstack[gosubindex - 1] == NULL)
        return 0;
    ClearVars(g_LocalIndex--, true); // delete any local variables
#ifdef SUBPROFILE
    LeaveLocalFrame(); // pop this GOSUB's local frame
#endif
    g_TempMemoryIsChanged = true;        // signal that temporary memory should be checked
    nextstmt = gosubstack[--gosubindex]; // return to the caller
    CurrentLinePtr = errorstack[gosubindex];
    return 1;
}

/* FOR's and DO's stack work, once a loop: in flash, as RBRun's RAM is the
   stack's (see the design's notes on the stack). */
// RC_FORP: cmd_for's stack work before its values, for the variable at vptr
static __attribute__((noinline)) void RBForPush(void *vptr, int vartype)
{
    int i;
    for (i = 0; i < g_forindex; i++)
        if (g_forstack[i].var == vptr && g_forstack[i].level == g_LocalIndex)
        { // the loop variable is already in the stack: remove it
            while (i < g_forindex - 1)
            {
                g_forstack[i] = g_forstack[i + 1];
                i++;
            }
            g_forindex--;
            break;
        }
    if (g_forindex == MAXFORLOOPS)
        error("Too many nested FOR loops");
    g_forstack[g_forindex].var = vptr;
    g_forstack[g_forindex].vartype = vartype; // the loop variable's type
    g_forstack[g_forindex].level = g_LocalIndex;
    g_forindex++; // incase functions use for loops
}

// RC_FORT: cmd_for after its values, TO at v[0] and STEP at v[1]; forptr is
// the FOR's nextstmt + 1 and pc the keys of its NEXT and after it.  True if
// the loop is done before it starts (nextstmt is then after its NEXT).
static __attribute__((noinline)) int RBForStart(union cell *v, unsigned char *forptr, const uint16_t *pc)
{
    struct s_forstack *fs;
    int test;
    g_forindex--;
    fs = &g_forstack[g_forindex];
    memcpy(&fs->stepvalue, &v[1], 8);
    memcpy(&fs->tovalue, &v[0], 8);
    fs->forptr = forptr;
    fs->nextptr = RBImage(pc[0] | ((uint32_t)pc[1] << 16));
    if (fs->vartype & T_INT)
        test = (fs->stepvalue.i >= 0 && *(long long int *)fs->var > fs->tovalue.i) || (fs->stepvalue.i < 0 && *(long long int *)fs->var < fs->tovalue.i);
    else
        test = (fs->stepvalue.f >= 0 && *(MMFLOAT *)fs->var > fs->tovalue.f) || (fs->stepvalue.f < 0 && *(MMFLOAT *)fs->var < fs->tovalue.f);
    if (test)
    {
        nextstmt = RBImage(pc[2] | ((uint32_t)pc[3] << 16));
        return 1;
    }
    g_forindex++;
    return 0;
}

// RC_DOP: cmd_do's stack work; doptr is the DO's nextstmt, evalptr its
// condition (NULL: none), pc the op's flags and LOOP's key
static __attribute__((noinline)) void RBDoPush(unsigned char *doptr, unsigned char *evalptr, const uint16_t *pc)
{
    struct s_dostack *ds;
    int i;
    for (i = 0; i < g_doindex; i++)
        if (g_dostack[i].doptr == doptr)
        { // this loop is already in the stack: remove it
            while (i < g_doindex - 1)
            {
                g_dostack[i] = g_dostack[i + 1];
                i++;
            }
            g_doindex--;
            break;
        }
    if (g_doindex == MAXDOLOOPS)
        error("Too many nested DO or WHILE loops");
    ds = &g_dostack[g_doindex];
    ds->evalptr = evalptr;
    ds->doptr = doptr;
    ds->level = g_LocalIndex;
    ds->untiltest = (pc[0] & RD_UNTIL) != 0;
    ds->loopptr = RBImage(pc[1] | ((uint32_t)pc[2] << 16));
    ds->fast_state = DOFAST_OFF; // the compiled LOOP, or cmd_loop on the text
    g_doindex++;
}

/* A record's binds: its nb entries at c, into slot, as RBRun makes them at
   its start.  0 if one does not hold, 2 if all do but an optional one
   (RB_BOPT), whose slot is then NULL, 1 if every one does.  Always inlined:
   RBRun's copy is in RAM, RBRebind's in flash. */
static inline __attribute__((always_inline)) int RBBindAll(const uint16_t *c, unsigned int nb, union cell **slot)
{
    unsigned int j;
    int res = 1;
    for (j = 0; j < nb; j++, c += 2)
    {
        unsigned int id = (c[0] & RC_IDMASK) + ((c[0] & RC_LIB) ? SymCanonLibBase : 0), sc;
        int k, i, suf = (0x2410 >> ((c[0] >> 10) & 0xC)) & 0xF; // RC_SUF bits: 0, T_NBR, T_INT, T_STR
        struct s_vartbl *v;
        if (id < SymCanonCount && SymCanonOf && (sc = SymCanonOf[id]) != 0)
            k = sc - 1;
        else if ((k = SymCanonById(id)) < 0)
            goto miss;
        i = SymL[k];
        if (i < 0 || VREC(i)->level != g_LocalIndex)
        { // not a local at this level: the global, if no text local can hide it
            i = SymG[k];
            if (i < 0 || (g_LocalIndex && SymTextLocals))
                goto miss; // not bound yet, or a text local may hide it: findvar decides
        }
        v = VREC(i);
        if (((c[1] & RB_BARRAY) ? !DimIsRealArray(RAW_DIM(*v, 0)) : !DimIsScalar(RAW_DIM(*v, 0))) ||
            (v->type & T_STRUCT) || (v->type & (T_INT | T_NBR | T_STR)) != (c[1] & (T_INT | T_NBR | T_STR)) ||
            (suf ? !(v->type & suf) : !(v->type & (DefaultType | T_IMPLIED))) ||
            ((c[0] & RC_TARGET) && (v->type & T_CONST)) || ((c[1] & RB_BCONST) && !(v->type & T_CONST)))
            goto miss;
        if (c[1] & (RB_BARRAY | T_STR))
        {
            slot[j] = (union cell *)v; // an array or a string: its variable, for its dimensions or size and its data
            continue;
        }
        // a T_PTR (a BYREF parameter, a STATIC) holds its data's address, as findvar returns it
        slot[j] = (v->type & T_PTR) ? (union cell *)v->val.s : (union cell *)&v->val;
        continue;
    miss:
        if (!(c[1] & RB_BOPT))
            return 0;
        slot[j] = NULL; // (RC_PARTCHK sends the record to its fallback if its part is taken)
        res = 2;
    }
    return res;
}

/* RBCallFun: a FUNCTION call moved the bind stamp (it erased a variable, or
   changed OPTION DEFAULT), so the statement's variables are bound again, as
   the text evaluator would find them after the call.  One that no longer
   holds cannot go to the fallback, which would make the call again. */
static __attribute__((noinline)) void RBRebind(const uint16_t *r, union cell **slot)
{ // (slot may be the record's bind cache: its stamp is then already out of date)
    const uint16_t *c = r + RB_HDR(r) + 1;
    unsigned int nb = *c++, j, had = 0;
    for (j = 0; j < nb; j++)
        if (slot[j] != NULL)
            had |= 1u << j; // (an optional bind unbound at the start may stay so)
    if (!RBBindAll(c + 2, nb, slot))
        error("A FUNCTION changed a variable of this statement");
    for (j = 0; j < nb; j++)
        if (((had >> j) & 1) && slot[j] == NULL)
            error("A FUNCTION changed a variable of this statement");
}

/* Bind a compiled statement's variables and run its code.  *rp and *ep are
   its record and its text.  Returns the end it gave nextstmt, or NULL if a
   bind failed and the fallback must run.

   The chain.  When the statement is done and the text loop's tail would have
   nothing to do - no abort, interrupt or housekeeping due, no temporary
   memory to clear, core 1's stack intact, no TRACE, no ON ERROR SKIP - and
   the next record (the one after, past any IF parts, or nextstmt's) is a
   compiled statement, RBRun does that tail and the next record's line
   bookkeeping as RunStream would, and runs it: a loop's body, and a call's,
   never leaves.

   Returns the record that comes next (RB_OUT: nextstmt is not in the
   stream), or NULL if a bind failed and the fallback must run: *rp and *ep
   are then the record it stopped on and its text. */
#define RB_OUT ((const uint16_t *)1)
static const uint16_t *RBRAM(RBRun)(const uint16_t **rp, unsigned char **ep)
{
    const uint16_t *r = *rp, *c, *pc, *t;
    unsigned char *e = *ep, *end;
    unsigned int nw; // cmdl | next << 8
    uint16_t *stamp, *cache;
    uint32_t gen; // the bind cache's stamp as it stands now
    union cell *slot[RB_MAXBIND], **slotp, st[RB_MAXDEPTH], *sp; // slotp: the binds' addresses
    unsigned int nb, j, w;
    int loopi;              // RC_LOOPF's stack entry
    unsigned char *clsave; // RC_CLSET: CurrentLinePtr before a CASE's line took its place
    unsigned char *nxcl, *nxend; // RC_NEXT, RC_CONTFOR: the NEXT's cmdline, and where it ends
again:
    clsave = NULL;
    c = r + RB_HDR(r) + 1;
    nw = r[RB_HDR(r) - 1];
    sp = st;
    nb = *c++;
    stamp = (uint16_t *)c;
    c += 2;
    cache = (uint16_t *)c + 2 * nb;
    if ((uint32_t)cache & 2)
        cache++; // the pad
    loopi = 0;
    // what can have changed the binds at this level (Symbols.h): the globals,
    // and this level's locals; 0, which never matches, past SYM_LEVELS
    gen = g_LocalIndex < SYM_LEVELS ? SymBindGenG + SymLevelGen[g_LocalIndex] : 0;
    if (RBCacheOK && (stamp[0] | ((uint32_t)stamp[1] << 16)) == gen && gen && !(g_LocalIndex && SymTextLocals))
        slotp = (union cell **)cache; // what the binds found last time, used where it is
    else
    {
    if ((j = RBBindAll(c, nb, slot)) == 0)
        goto fail;
    slotp = slot;
    if (RBCacheOK && j == 1) // (not with an optional bind unbound: the next run binds again)
    { // keep them, the addresses first and the stamp last
        for (j = 0; j < nb; j++)
            ((union cell **)cache)[j] = slot[j];
        stamp[0] = gen & 0xFFFF;
        stamp[1] = gen >> 16;
    }
    }
    // dispatch by computed goto through a table in RAM (the G3 prototype's
    // way): no bounds check, one indirect branch an op
    static void *const rbops[] __not_in_flash("rbops") = {
        [RC_END] = &&L_END,
        [RC_LDG] = &&L_LDG,
        [RC_STG] = &&L_STG,
        [RC_LK] = &&L_LK,
        [RC_CVIF] = &&L_CVIF,
        [RC_CVFI] = &&L_CVFI,
        [RC_CVIF2] = &&L_CVIF2,
        [RC_CVFI2] = &&L_CVFI2,
        [RC_SHADOW] = &&L_SHADOW,
        [RC_ADDF] = &&L_ADDF,
        [RC_ADDI] = &&L_ADDI,
        [RC_SUBF] = &&L_SUBF,
        [RC_SUBI] = &&L_SUBI,
        [RC_MULF] = &&L_MULF,
        [RC_MULI] = &&L_MULI,
        [RC_OPF] = &&L_OPF,
        [RC_OPI] = &&L_OPI,
        [RC_NEGF] = &&L_NEGF,
        [RC_NEGI] = &&L_NEGI,
        [RC_NOTF] = &&L_NOTF,
        [RC_NOTI] = &&L_NOTI,
        [RC_INV] = &&L_INV,
        [RC_JFF] = &&L_JFF,
        [RC_JFI] = &&L_JFI,
        [RC_JMP] = &&L_JMP,
        [RC_GOTO] = &&L_GOTO,
        [RC_FORP] = &&L_FORP,
        [RC_FORT] = &&L_FORT,
        [RC_DOP] = &&L_DOP,
        [RC_DOT] = &&L_DOT,
        [RC_LOOPF] = &&L_LOOPF,
        [RC_LOOPT] = &&L_LOOPT,
        [RC_SHADOWCK] = &&L_SHADOWCK,
        [RC_CALL] = &&L_CALL,
        [RC_NEXT] = &&L_NEXT,
        [RC_FCALL] = &&L_FCALL,
        [RC_CMPF] = &&L_CMPF,
        [RC_CMPI] = &&L_CMPI,
        [RC_BITI] = &&L_BITI,
        [RC_IDX] = &&L_IDX,
        [RC_LDEL] = &&L_LDEL,
        [RC_ADEL] = &&L_LDEL,
        [RC_STP] = &&L_STP,
        [RC_EXITFOR] = &&L_EXITFOR,
        [RC_EXITDO] = &&L_EXITDO,
        [RC_RETURN] = &&L_RETURN,
        [RC_ENDFUN] = &&L_ENDFUN,
        [RC_SEL] = &&L_SEL,
        [RC_EQSEL] = &&L_EQSEL,
        [RC_RANGE] = &&L_RANGE,
        [RC_CLSET] = &&L_CLSET,
        [RC_CGOTO] = &&L_CGOTO,
        [RC_LDS] = &&L_LDS,
        [RC_LKS] = &&L_LKS,
        [RC_OPS] = &&L_OPS,
        [RC_STS] = &&L_STS,
        [RC_CONTFOR] = &&L_CONTFOR,
        [RC_LOCAL] = &&L_LOCAL,
        [RC_GUARD] = &&L_GUARD,
        [RC_SPLICE] = &&L_SPLICE,
        [RC_FSPLICE] = &&L_FSPLICE,
        [RC_DUPLD] = &&L_DUPLD,
        [RC_PARTCHK] = &&L_PARTCHK,
        [RC_INCF] = &&L_INCF,
        [RC_INCS] = &&L_STS,
        [RC_FN] = &&L_FN,
        [RC_SHADOWC] = &&L_SHADOWC};
#define RBNEXT()                   \
    do                             \
    {                              \
        w = *pc++;                 \
        goto *rbops[w & 0xFF];     \
    } while (0)
    pc = cache + 2 * nb;
    RBNEXT();
    L_LDG:
            *sp++ = *slotp[w >> 8];
            RBNEXT();
    L_STG:
            *slotp[w >> 8] = *--sp;
            RBNEXT();
    L_LK:
            memcpy(sp++, pc, 8);
            pc += 4;
            RBNEXT();
    L_CVIF:
            sp[-1].f = (MMFLOAT)sp[-1].i;
            RBNEXT();
    L_CVFI:
            sp[-1].i = FloatToInt64(sp[-1].f);
            RBNEXT();
    L_CVIF2:
            sp[-2].f = (MMFLOAT)sp[-2].i;
            RBNEXT();
    L_CVFI2:
            sp[-2].i = FloatToInt64(sp[-2].f);
            RBNEXT();
    L_SHADOW:
            if (RBMode == RB_SHADOW)
                RBShadow(e + (w >> 8), *pc, &sp[-1]);
            pc++;
            RBNEXT();
    L_ADDF:
            sp--;
            sp[-1].f = sp[-1].f + sp[0].f;
            if (sp[-1].i == RB_INF) // r == INFINITY, on the bits
                StandardError(15);
            RBNEXT();
    L_ADDI:
            sp[-2].i = sp[-2].i + sp[-1].i;
            sp--;
            RBNEXT();
    L_SUBF:
            sp--;
            sp[-1].f = sp[-1].f - sp[0].f;
            if (sp[-1].i == RB_INF) // as op_subtract checks it
                StandardError(15);
            RBNEXT();
    L_SUBI:
            sp[-2].i = sp[-2].i - sp[-1].i;
            sp--;
            RBNEXT();
    L_MULF:
            sp--;
            sp[-1].f = sp[-1].f * sp[0].f;
            if (sp[-1].i == RB_INF)
                StandardError(15);
            RBNEXT();
    L_MULI:
            sp[-2].i = sp[-2].i * sp[-1].i;
            sp--;
            RBNEXT();
    L_OPF: // doexpr's call, on floats
            farg1 = sp[-2].f;
            farg2 = sp[-1].f;
            targ = T_NBR;
            goto opcall;
    L_OPI: // and on integers
            iarg1 = sp[-2].i;
            iarg2 = sp[-1].i;
            targ = T_INT;
    opcall:
            tokentbl[w >> 8].fptr();
            sp--;
            if (targ & T_NBR)
                sp[-1].f = fret;
            else
                sp[-1].i = iret;
            RBNEXT();
    L_NEGF:
            sp[-1].f = -sp[-1].f;
            RBNEXT();
    L_NEGI:
            sp[-1].i = -sp[-1].i;
            RBNEXT();
    L_NOTF:
            sp[-1].f = (sp[-1].i & 0x7FFFFFFFFFFFFFFFLL) ? 0 : 1; // (f != 0): NaN is not 0
            RBNEXT();
    L_CMPF: // compare() on floats: the sign of the difference, 0 for 0 and NaN; then the operator
        {
            union cell d;
            unsigned long long m;
            int r;
            d.f = sp[-2].f - sp[-1].f;
            m = (unsigned long long)d.i & 0x7FFFFFFFFFFFFFFFULL;
            r = (m == 0 || m > (unsigned long long)RB_INF) ? 0 : (d.i < 0 ? -1 : 1);
            goto cmp;
    L_CMPI: // compare() on integers: the difference, as op_ne and op_equal test them
            if ((w >> 8) >= RK_EQ)
                r = sp[-2].i != sp[-1].i;
            else
            {
                long long diff = sp[-2].i - sp[-1].i;
                r = diff < 0 ? -1 : diff > 0;
            }
            if ((w >> 8) == RK_EQ)
            {
                sp[-2].i = !r;
                sp--;
                RBNEXT();
            }
            if ((w >> 8) == RK_NE)
            {
                sp[-2].i = r;
                sp--;
                RBNEXT();
            }
        cmp:
            switch (w >> 8)
            {
            case RK_LT:
                r = r < 0;
                break;
            case RK_LTE:
                r = r <= 0;
                break;
            case RK_GT:
                r = r > 0;
                break;
            case RK_GTE:
                r = r >= 0;
                break;
            case RK_EQ:
                r = r == 0;
                break;
            default:
                r = r != 0;
            }
            sp[-2].i = r;
            sp--;
            RBNEXT();
        }
    L_IDX: // findvar straight after evaluating an index
        {
            long long int in;
            if ((w >> 8) == T_NBR)
                in = FloatToInt32(sp[-1].f);
            else
            {
                in = sp[-1].i;
                if (in != (int)in)
                    error("Index out of bounds"); // too big for any array: must not wrap to a small index
            }
            if (in < g_OptionBase)
                error("Dimensions");
            sp[-1].i = in;
            RBNEXT();
        }
    L_LDEL: // findvar with the array found: the count, the bounds, the element (RBElementAt)
        {
            struct s_vartbl *av = (struct s_vartbl *)slotp[w >> 8];
            union cell *el;
            if (*pc == 1 && !DimIsEnd(RAW_DIM(*av, 0)) && DimIsEnd(RAW_DIM(*av, 1)) &&
                sp[-1].i <= DimUpper(RAW_DIM(*av, 0)) && sp[-1].i >= g_OptionBase)
                el = (union cell *)(av->val.s + (sp[-1].i - g_OptionBase) * 8); // one dimension, in bounds
            else
                el = RBElementAt(av, sp - *pc, *pc);
            sp -= *pc++;
            if ((w & 0xFF) == RC_LDEL)
                *sp++ = *el;
            else
                (sp++)->i = (long long int)(uint32_t)el;
            RBNEXT();
        }
    L_EXITFOR: // cmd_exitfor
            if (g_forindex == 0)
                goto fail; // "No FOR loop is in effect": the fallback raises it
            nextstmt = g_forstack[--g_forindex].nextptr;
            goto exited;
    L_EXITDO: // cmd_exit
            if (g_doindex == 0)
                goto fail;
            nextstmt = g_dostack[--g_doindex].loopptr;
    exited: // past the NEXT or LOOP
            skipelement(nextstmt);
            goto ran;
    L_RETURN: // cmd_return (RBReturn)
            if (!RBReturn())
                goto fail; // "Nothing to return to"
            goto ran;
    L_ENDFUN: // cmd_endfun: the end of this run of ExecuteProgram
            if (gosubindex == 0 || gosubstack[gosubindex - 1] != NULL)
                goto fail;
            nextstmt = (unsigned char *)"\0\0\0";
            goto ran;
    L_SEL: // cmd_select's selector
            *sp++ = st[0];
            RBNEXT();
    L_RANGE: // cmd_select's range test (RBSel)
            sp--;
    L_EQSEL: // cmd_select: == or the range, on the selector's type (RBSel)
            sp[-1].i = RBSel(w, st, sp - 1);
            RBNEXT();
    L_CLSET: // cmd_select reports a CASE's errors against its line
            if (clsave == NULL)
                clsave = CurrentLinePtr;
            CurrentLinePtr = RBImage(pc[0] | ((uint32_t)pc[1] << 16));
            pc += 2;
            RBNEXT();
    L_CGOTO: // cmd_select's end: CurrentLinePtr back, and on to the key
            if (clsave != NULL)
                CurrentLinePtr = clsave;
            nextstmt = RBImage(pc[0] | ((uint32_t)pc[1] << 16));
            goto ran;
    L_FN: // a built-in function's work (RBFn)
            RBFn(w >> 8, sp - 1);
            RBNEXT();
    L_GUARD: // OPTION LEGACY (CMM1) is on, or OPTION DEFAULT is not what the FUNCTION calls need (RBFinish)
            if (RBDefaultBad(w >> 8))
                goto fail;
            RBNEXT();
    L_SPLICE: // the command's handler, its arguments' values spliced (RBSpliceCmd)
            sp -= pc[0];
            RBSpliceCmd(r, e, nw, pc + 1, sp);
            pc += 1 + (w >> 8);
            RBNEXT();
    L_FSPLICE: // a built-in function's handler, its arguments' values spliced (RBFnSpliceRun)
            sp -= pc[0] & 0xFF;
            RBFnSpliceRun(pc, w >> 8, sp);
            sp++;
            pc += 1 + (w >> 8);
            RBNEXT();
    L_PARTCHK: // an IF part's optional binds (RBPartCheck): one unbound sends the IF to its fallback
        {
            unsigned int m = *pc++;
            for (j = 0; m; j++, m >>= 1)
                if ((m & 1) && slotp[j] == NULL)
                    goto fail;
            RBNEXT();
        }
    L_DUPLD:
            *sp = *(union cell *)(uint32_t)sp[-1].i;
            sp++;
            RBNEXT();
    L_INCF:
            sp[-2].f = sp[-2].f + sp[-1].f;
            sp--;
            RBNEXT();
    L_LOCAL: // cmd_dim for LOCAL (RBLocal)
            if ((pc = RBLocal(e, nw, pc, w)) == NULL)
                goto fail;
            RBNEXT();
    L_LDS: // getvalue: a string variable's data, which it does not copy
            (sp++)->i = (uint32_t)((struct s_vartbl *)slotp[w >> 8])->val.s;
            RBNEXT();
    L_LKS:
            (sp++)->i = (uint32_t)pc;
            pc += w >> 8;
            RBNEXT();
    L_OPS: // doexpr's call on two strings (RBOpStr)
            RBOpStr(w >> 8, sp);
            sp--;
            RBNEXT();
    L_STS: // cmd_let's string store, and cmd_inc's (RBStoreStr)
            sp--;
            RBStoreStr(w, (struct s_vartbl *)slotp[w >> 8], (unsigned char *)(uint32_t)sp[0].i);
            RBNEXT();
    L_STP:
            sp -= 2;
            *(union cell *)(uint32_t)sp[0].i = sp[1];
            RBNEXT();
    L_BITI: // op_and, op_or, op_xor
            if ((w >> 8) == RK_AND)
                sp[-2].i = (long long int)((unsigned long long int)sp[-2].i & (unsigned long long int)sp[-1].i);
            else if ((w >> 8) == RK_OR)
                sp[-2].i = (long long int)((unsigned long long int)sp[-2].i | (unsigned long long int)sp[-1].i);
            else
                sp[-2].i = (long long int)((unsigned long long int)sp[-2].i ^ (unsigned long long int)sp[-1].i);
            sp--;
            RBNEXT();
    L_NOTI:
            sp[-1].i = (sp[-1].i != 0) ? 0 : 1;
            RBNEXT();
    L_INV:
            sp[-1].i = ~sp[-1].i;
            RBNEXT();
    L_JFF:
            sp--;
            if ((sp[0].i & 0x7FFFFFFFFFFFFFFFLL) == 0) // sp[0].f == 0, without a library compare
                pc += *pc;
            pc++;
            RBNEXT();
    L_JFI:
            sp--;
            if (sp[0].i == 0)
                pc += *pc;
            pc++;
            RBNEXT();
    L_JMP:
            pc += *pc + 1;
            RBNEXT();
    L_GOTO:
            nextstmt = RBImage(pc[0] | ((uint32_t)pc[1] << 16));
            goto ran; // the statement's end, which nextstmt is not: the executor looks it up
    L_FORP: // cmd_for before its values (RBForPush)
            RBForPush(slotp[w >> 8], *pc++);
            RBNEXT();
    L_FORT: // cmd_for after its values (RBForStart)
            sp -= 2;
            if (RBForStart(sp, e + (nw >> 8) + 1, pc))
            { // the loop is done before it starts: on after its NEXT
                goto ran;
            }
            pc += 4;
            RBNEXT();
    L_DOP: // cmd_do's stack work (RBDoPush)
            RBDoPush(e + (nw >> 8), (pc[0] & RD_COND) ? e + (w >> 8) : NULL, pc);
            pc += 3;
            RBNEXT();
    L_DOT: // the entry test: false goes after the LOOP
        {
            int c = ((w >> 8) == T_NBR ? sp[-1].i & 0x7FFFFFFFFFFFFFFFLL : sp[-1].i) != 0; // (f != 0 on the bits: NaN is not 0)
            sp--;
            if (g_dostack[g_doindex - 1].untiltest)
                c = !c;
            if (!c)
            {
                g_doindex--;
                nextstmt = RBImage(pc[0] | ((uint32_t)pc[1] << 16));
                goto ran;
            }
            pc += 2;
            RBNEXT();
        }
    L_LOOPF: // cmd_loop's search for its entry
        {
            unsigned char *cl = e + (nw & 0xFF), *q; // cmd_loop's cmdline
            for (loopi = 0; loopi < g_doindex; loopi++)
            {
                q = g_dostack[loopi].loopptr + sizeof(CommandToken);
                skipspace(q);
                if (q == cl)
                    break;
            }
            if (loopi == g_doindex)
                goto fail; // "LOOP without a matching DO": the fallback raises it
            RBNEXT();
        }
    L_LOOPT:
        {
            int f = w >> 8, tst;
            if (f & RL_ALWAYS)
                tst = 1;
            else
            {
                tst = ((f & RL_NBR) ? sp[-1].i & 0x7FFFFFFFFFFFFFFFLL : sp[-1].i) != 0; // (as RC_DOT)
                sp--;
                if ((f & RL_UNTIL) || ((f & RL_ENTRY) && g_dostack[loopi].untiltest))
                    tst = !tst;
            }
            if (tst)
            { // loop again
                nextstmt = g_dostack[loopi].doptr;
                goto ran;
            }
            g_doindex = loopi; // the loop has ended
            RBNEXT();
        }
    L_SHADOWCK:
            if (RBMode == RB_SHADOW)
                RBShadowCond(RBImage(pc[1] | ((uint32_t)pc[2] << 16)), pc[0], &sp[-1]);
            pc += 3;
            RBNEXT();
    L_SHADOWC:
            if (RBMode == RB_SHADOW)
                RBShadowCond(e + (w >> 8), *pc, &sp[-1]);
            pc++;
            RBNEXT();
    L_FCALL: // DefinedSubFun for a FUNCTION (RBCallFun): its value replaces the arguments
        {
            union cell res;
            if (!RBCallFun(pc, w >> 8, slotp, sp - pc[1], &res, r))
                goto fail;
            sp -= pc[1];
            *sp++ = res;
            pc += 5 + 3 * (w >> 8);
            RBNEXT();
        }
    L_CALL: // DefinedSubFun for a SUB (RBCallSub)
            if (!RBCallSub(pc, w >> 8, slotp, sp - pc[1], e + (nw >> 8)))
                goto fail;
            goto done;
    L_CONTFOR: // cmd_continue sends nextstmt to the loop's NEXT, whose cmd_next runs here
            if (g_forindex == 0)
                goto fail; // "No FOR loop is in effect"
            nxcl = g_forstack[g_forindex - 1].nextptr + sizeof(CommandToken);
            skipspace(nxcl);
            nxend = g_forstack[g_forindex - 1].nextptr;
            skipelement(nxend);
            goto next;
    L_NEXT: // cmd_next (Commands.c), line for line
            nxcl = e + (nw & 0xFF);
            nxend = e + (nw >> 8);
    next:
        {
            unsigned char *cl = nxcl, *q; // its cmdline
            int i, test;
            for (i = g_forindex - 1; i >= 0; i--)
            {
                q = g_forstack[i].nextptr + sizeof(CommandToken);
                skipspace(q);
                if (q == cl)
                    break;
            }
            if (i < 0)
                goto fail; // "Cannot find a matching FOR": the fallback raises it
            nextstmt = nxend;
        nextloop:
            if (g_forstack[i].vartype & T_INT)
            {
                *(long long int *)g_forstack[i].var += g_forstack[i].stepvalue.i;
                test = (g_forstack[i].stepvalue.i >= 0 && *(long long int *)g_forstack[i].var > g_forstack[i].tovalue.i) || (g_forstack[i].stepvalue.i < 0 && *(long long int *)g_forstack[i].var < g_forstack[i].tovalue.i);
            }
            else
            {
                *(MMFLOAT *)g_forstack[i].var += g_forstack[i].stepvalue.f;
                test = (g_forstack[i].stepvalue.f >= 0 && *(MMFLOAT *)g_forstack[i].var > g_forstack[i].tovalue.f) || (g_forstack[i].stepvalue.f < 0 && *(MMFLOAT *)g_forstack[i].var < g_forstack[i].tovalue.f);
            }
            if (test)
            { // the loop has ended: out of the stack, and any other loop this NEXT closes
                while (i < g_forindex - 1)
                {
                    g_forstack[i] = g_forstack[i + 1];
                    i++;
                }
                g_forindex--;
                for (i = g_forindex - 1; i >= 0; i--)
                {
                    q = g_forstack[i].nextptr + sizeof(CommandToken);
                    skipspace(q);
                    if (q == cl)
                        goto nextloop;
                }
            }
            else
                nextstmt = g_forstack[i].forptr; // back to the body
            goto ran;
        }
    L_END:
    nextstmt = e + (nw >> 8);
ran: // a statement run as compiled code (one copy of the counts, for RAM)
    RBRan++;
    RBCode++;
done:
    // the next record: the one after, past any IF parts, or nextstmt's
    end = e + (nw >> 8);
    if (nextstmt == end)
    {
        t = r;
        do
            t += RB_HDR(t) + ((t[0] & RB_COMPILED) ? 1 + t[RB_HDR(t)] : 0);
        while (t[0] & RB_PART);
    }
    else if ((t = RBFind(nextstmt)) == NULL)
        return RB_OUT;
    if ((t[0] & 0xFF) != RB_OP_END && (t[3] & 0xFF) != RB_OP_NOP && (t[0] & RB_COMPILED) &&
        OptionErrorSkip == 0 && !g_TempMemoryIsChanged && !TraceOn &&
        (--RBHouseN > 0 || (RBHouseN = RB_HOUSE_N,
#ifndef PICOMITEWEB
                            core1stack[0] == 0x12345678 &&
#endif
                            (OptionNoCheck || (!MMAbort && !IntReady.any && time_us_32() - RBHouseAt < RB_HOUSE_US)))))
    { // a compiled statement, and the tail has nothing to do (see RBTail): run it.  What
      // can change under the chain (ON ERROR SKIP, temporary memory, TRACE: a FUNCTION's
      // body may) is looked at every statement; an abort, an interrupt, core 1's stack
      // and the housekeeping timer once in RB_HOUSE_N (a few microseconds)
        {
            r = t;
            e = nextstmt = RBImage(RBKeyAt(r)); // the tail's nextstmt, which nothing moved
            if (r[0] & RB_LINESTART)
            { // RunStream's line bookkeeping (TRACE is off)
                CurrentLinePtr = e;
                TraceBuff[TraceBuffIndex] = e;
                if (++TraceBuffIndex >= TRACE_BUFF_SIZE)
                    TraceBuffIndex = 0;
            }
            goto again;
        }
    }
    return t;
fail:
    *rp = r;
    *ep = e;
    return NULL;
#undef RBNEXT
}

// RBExec while ON ERROR SKIP/IGNORE is in force: an error in the statement
// comes back here, as it does to the text loop's setjmp, and the statement
// counts as run.  Its own frame holds the jmp_buf's registers.  In flash:
// RBExec is folded into RunStream, which saves a frame on every FUNCTION
// level the text path nests, and this is its copy for the rare case.
static __attribute__((noinline)) unsigned char *RBExecSkip(const uint16_t *r, unsigned char *e)
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
            if (TraceOn)
                RBTraceLine(entry);
        }
        if ((r[3] & 0xFF) == RB_OP_NOP)
        { // no statement, so no tail after it either
            first = 1;
            r += 4;
            continue;
        }
        if ((r[0] & RB_COMPILED) && OptionErrorSkip == 0 && (t = RBRun(&r, &entry)) != NULL)
        { // compiled, and done: RBRun found the next record
            if (t != RB_OUT)
            {
                r = t;
                continue;
            }
            goto leave;
        }
        end = OptionErrorSkip == 0 ? RBExec(r, entry) : RBExecSkip(r, entry);
        // where next: the next record, the record of a jump's target, or the text loop
        if (nextstmt == end)
        { // the next record, past any IF parts (reached only by cmd_if's jump)
            do
                r += RB_HDR(r) + ((r[0] & RB_COMPILED) ? 1 + r[RB_HDR(r)] : 0);
            while (r[0] & RB_PART);
            continue;
        }
        if ((t = RBFind(nextstmt)) != NULL) // NULL too if the statement compiled the program again (RUN)
        {
            r = t;
            continue;
        }
    leave:
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
    if (SymOffLine != NULL)
    { // OPTION SYMBOLS OFF: there are no bindings for the stream's binds
        RBWhy = "OPTION SYMBOLS OFF";
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
        if (RBCacheOK)
            RBClearCaches();
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
        if (RBNeed > RBSlotSize())
        { // what it needed, of what there is
            strcat(out, " (");
            IntToStr(out + strlen(out), RBNeed, 10);
            strcat(out, " of ");
            IntToStr(out + strlen(out), RBSlotSize(), 10);
            strcat(out, " bytes)");
        }
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
        strcat(out, " SIZE "); // the stream's bytes, of the slot's
        IntToStr(out + strlen(out), ((const rbheader_t *)RBSlotBase())->codeoff + ((const rbheader_t *)RBSlotBase())->codelen, 10);
        strcat(out, " OF ");
        IntToStr(out + strlen(out), RBSlotSize(), 10);
    }
}
#endif // rp2350
/*  @endcond */
