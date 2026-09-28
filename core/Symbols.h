/*
 * @cond
 * The following section will be excluded from the documentation.
 */
/* *********************************************************************************************************************
PicoMite MMBasic

Symbols.h

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
 * Symbols: every identifier in a saved program is stored as a 2 or 3 byte
 * symbol instead of its text.
 *
 *   introducer byte  0x04 program long   0x05 program short
 *                    0x06 library long   0x07 library short
 *   payload          one (short) or two (long) characters from the
 *                    63-character alphabet 0-9 A-Z a-z _
 *
 * A short symbol names one of the 63 most used spellings, a long one any of
 * the next 3,969.  Each spelling of a name (Foo, FOO) has its own id so LIST
 * prints the program exactly as typed.  The spellings travel with the image
 * in a table written after the program text (see symtab_t), so flash slots,
 * RAM slots and saved images carry it with them.
 *
 * tokenise() turns control characters into spaces, so 0x04-0x07 can never
 * come from source text, and every payload byte is a name character, clear
 * of the bytes that scanners act on (0, quotes, brackets, commas, comment
 * marks and tokens).  Code that has not learnt about symbols sees 0x04 as
 * "not a name" and raises a syntax error rather than giving a wrong answer.
 *
 * Images saved by older firmware have no table and no symbols; they run on
 * the text path exactly as before.
 */
#ifndef SYMBOLS_H
#define SYMBOLS_H
#include <stdint.h>

#define SYM_PROG_LONG 0x04
#define SYM_PROG_SHORT 0x05
#define SYM_LIB_LONG 0x06
#define SYM_LIB_SHORT 0x07
#define SYM_NSHORT 63                                  // spellings that get a short symbol
#define SYM_MAXIDS (SYM_NSHORT + SYM_NSHORT * SYM_NSHORT) // spellings one image can hold
#define SYM_MAXLEN 255                                 // longest spelling the table can hold
#define SYM_MAGIC 0x314D5953u                          // "SYM1"

#define issymbol(c) ((((unsigned char)(c)) & 0xFC) == 0x04)
#define symbolsize(c) ((((unsigned char)(c)) & 1) ? 2 : 3)

// The table that follows a program image.  It starts on the word after the
// 0xFFFFFFFF marker that ends the program text, and the CSUB/font records
// follow it.
typedef struct
{
    uint32_t magic;   // SYM_MAGIC
    uint32_t textlen; // offset of this table from the start of its image (a self-check)
    uint32_t size;    // bytes in the table, header included, a multiple of 4
    uint16_t count;   // number of spellings
    uint16_t names;   // offset of the names area from the start of the table
    // uint16_t offs[count]: each spelling's offset in the names area
    // names area: one [length][spelling] record per id
} symtab_t;

extern const symtab_t *SymTabProg; // table of the program being run or listed (NULL = none)
extern const symtab_t *SymTabLib;  // table of the library (NULL = none)
extern const unsigned char symdigit[128];

// save-time state used by tokenise()
#define SYM_OFF 0
#define SYM_COUNT 1
#define SYM_EMIT 2
extern int SymMode;     // SYM_OFF: names stay text
extern int SymRawBlock; // inside a CSUB or DefineFont block: names stay text
extern int SymEnabled;  // OPTION SYMBOLS: 0 = save programs as text
extern int SymLongest;  // the longest spelling in the program's and the library's tables
extern int SymLibSave;  // a library is being saved: its names become library symbols

void SymInit(void);
void SymDamaged(void);
const symtab_t *SymFindTable(const unsigned char *image);
void SymSetProgram(const unsigned char *image);
void SymSetLibrary(const unsigned char *image);
int SymExpand(unsigned char *dst, const unsigned char *src, int n, int cap);
unsigned char *SymExpandStatement(unsigned char *p);
int SymAwareCommand(int cmd);
int SymAwareFunction(unsigned char tkn);
int SymNameEqual(const unsigned char *p, const char *text);
int SymFindName(unsigned char *p, unsigned char *name);
int SymAny(const unsigned char *p, int n);

// saving
int SymBegin(unsigned char *src);
void SymEnd(void);
void SymCount(unsigned char *pm);
void SymRank(void);
unsigned char *SymName(unsigned char *op, const unsigned char *name, int len);
int SymTableSize(void);
void SymTableWrite(void (*put)(unsigned char), uint32_t textlen);

// The spelling of the symbol at p (in its image's table) and its length.  A
// symbol that its table cannot supply means the image and its table do not
// belong together.
static inline __attribute__((always_inline)) const unsigned char *SymSpellingInline(const unsigned char *p, int *len)
{
    const symtab_t *t = (p[0] & 2) ? SymTabLib : SymTabProg;
    unsigned int id = symdigit[p[1] & 0x7f];
    if (!(p[0] & 1))
        id = SYM_NSHORT + id * SYM_NSHORT + symdigit[p[2] & 0x7f];
    if (__builtin_expect(t == NULL || id >= t->count, 0))
        SymDamaged();
    const unsigned char *n = (const unsigned char *)t + t->names + ((const uint16_t *)(t + 1))[id];
    *len = n[0];
    return n + 1;
}
#ifdef rp2350
#define SymSpelling SymSpellingInline
#else
const unsigned char *SymSpelling(const unsigned char *p, int *len); // one copy, to save flash
#endif

// The spelling of the name at p, symbol or text.  Sets *s and *len and
// returns the byte after the name in the program.  *len is 0 if there is no
// name at p.
static inline __attribute__((always_inline)) unsigned char *NameView(unsigned char *p, const unsigned char **s, int *len)
{
    if (issymbol(*p))
    {
        *s = SymSpelling(p, len);
        return p + symbolsize(*p);
    }
    unsigned char *q = p;
    if (isnamestart(*q))
        while (isnamechar(*q))
            q++;
    *s = p;
    *len = q - p;
    return q;
}

// Copy the name at p (symbol or text) into buf, at most MAXVARLEN characters
// and zero terminated.  Sets *n to the number copied and returns the byte
// after the name (for text, after the characters copied).
static inline unsigned char *CopyName(unsigned char *p, unsigned char *buf, int *n)
{
    int m = 0;
    if (issymbol(*p))
    {
        const unsigned char *s = SymSpelling(p, &m);
        if (m > MAXVARLEN)
            m = MAXVARLEN;
        memcpy(buf, s, m);
        p += symbolsize(*p);
    }
    else
        while (isnamechar(*p) && m < MAXVARLEN)
            buf[m++] = *p++;
    buf[m] = 0;
    *n = m;
    return p;
}

// A name starts at p: a symbol, or text beginning with a letter or underscore
#define isnamestartsym(c) (isnamestart(c) || issymbol(c))

/* Bindings (S5/S6).  Each distinct name (case ignored) read through one of
   the program's symbols gets a canonical entry, made the first time one of
   its spellings is read.  The entry remembers the global variable, the SUB
   or FUNCTION and the label of that name once they have been found, and the
   newest local variable of that name, so lookups skip the name search.

   Locals form a shadow stack per name: making a local saves the entry's
   local binding in SymLShadow[slot] and points it at the new slot; ClearVars
   frees locals newest first and puts each binding back.  So the binding is
   always the newest live local of its name, and a local is visible only at
   its own level, which a local at any other level cannot be.

   A binding is cleared where the thing it points to goes away: ClearVars(0)
   and erase() for globals, ClearVars(level) for locals; the SUB and label
   bindings last as long as the program (PrepareProgram starts afresh).  A
   local made from text (EXECUTE, a library saved as text, the prompt) has no
   entry; while any is alive a global binding is not trusted inside a SUB.

   Library symbols share the entries: their ids follow the program's
   (SymCanonLibBase + the library id), so a name is one entry wherever it is
   read.

   What every lookup reads (SymCanonOf, SymG, SymL, SymS and the local
   shadows) is kept in SRAM, about 8 bytes a name; the rest goes to PSRAM
   when there is some, so the bindings take as little as possible of the
   program's own SRAM heap. */
typedef struct
{
    unsigned char *labind; // the label's line, NULL = not looked up yet
    uint16_t id;           // a symbol id spelling this name
    uint16_t next;         // next entry in the same hash chain + 1, 0 = none
    uint16_t flags;        // SYMC_DOT: the name holds a '.' (it may be a structure member path)
                           // SYMC_NOSUB: no SUB or FUNCTION has the name (checked once, P6 F3)
} symcold_t;
#define SYM_UNBOUND (-2)
#define SYMC_DOT 1
#define SYMC_NOSUB 2
#define SYM_LTEXT 0xFFFF // SymLCanon: a local made from text

extern uint16_t *SymCanonOf; // canonical entry + 1 of each symbol id, 0 = not seen yet
extern unsigned int SymCanonLibBase; // library symbol id 0 in SymCanonOf: the program's count
extern int16_t *SymG;        // per entry: g_vartbl slot of the global of the name, -1 = not bound
extern int16_t *SymL;        // per entry: g_vartbl slot of the newest live local of the name, -1 = none
extern int16_t *SymS;        // per entry: subfun[] index, -1 = none, SYM_UNBOUND = not looked up yet
extern symcold_t *SymCold;   // per entry: the rest
extern unsigned int SymCanonCount;
extern int16_t *SymLShadow; // per local slot: the local binding its local hid
extern uint16_t *SymLCanon; // per local slot: canonical entry + 1, SYM_LTEXT, or 0 = not tracked
extern unsigned int SymLSlots; // slots the two arrays above cover
extern int SymTextLocals;   // live locals made from text
// What a name binds to at a level changes only with an event that can change
// a global's binding - new bindings (SymBindInit), every variable gone
// (SymBindReset), one gone (erase), DefaultType changed (OPTION DEFAULT, RUN)
// - or with a local made or freed at that level: a deeper level's locals (a
// call's, an interrupt's, a GOSUB's) are gone before the level runs again.
// Every event takes the next value of SymBindEvent: SymBindGenG holds the
// last global event's, SymLevelGen[l] the last local event's at level l (P6
// F4).  Route B's compiled records reuse what their binds found at level l
// while SymBindGenG + SymLevelGen[l] is unchanged (a sum, which still changes
// after the count wraps, where the larger of the two would not); at a level
// from SYM_LEVELS on they bind every time.  (Route B is the RP2350's: the
// RP2040 keeps no per-level counts, whose RAM would come off its stack.)
#define SYM_LEVELS 80 // more than MAXGOSUB levels of SUBs and GOSUBs, with interrupts
extern uint32_t SymBindEvent;
extern uint32_t SymBindGenG;
#ifdef rp2350
extern uint32_t SymLevelGen[SYM_LEVELS];
#endif
#define SymBindGlobalsChanged() (SymBindGenG = ++SymBindEvent)
int SymCanonNew(const unsigned char *p);
int SymCanonById(unsigned int id);
void SymBindInit(void);
void SymBindFree(void);
void SymBindForget(void);
void SymBindReset(void);
void SymBindForgetSlot(int slot);
void SymLocalMade(int slot, int k);
void SymLocalFreed(int slot);

// The canonical entry of the symbol at p, or -1 if there are no bindings
// (no memory for them).
static inline __attribute__((always_inline)) int SymCanonAt(const unsigned char *p)
{
    unsigned int id, c;
    if (SymCanonOf == NULL)
        return -1;
    id = symdigit[p[1] & 0x7f];
    if (!(p[0] & 1))
        id = SYM_NSHORT + id * SYM_NSHORT + symdigit[p[2] & 0x7f];
    if (p[0] & 2)
        id += SymCanonLibBase;
    if (id >= SymCanonCount)
        return -1;
    c = SymCanonOf[id];
    if (c)
        return c - 1;
    return SymCanonNew(p); // the first use of this spelling
}

// The program symbol id of the symbol at p (a program symbol)
static inline __attribute__((always_inline)) unsigned int SymIdAt(const unsigned char *p)
{
    unsigned int id = symdigit[p[1] & 0x7f];
    if (!(p[0] & 1))
        id = SYM_NSHORT + id * SYM_NSHORT + symdigit[p[2] & 0x7f];
    return id;
}

#endif
/*  @endcond */
