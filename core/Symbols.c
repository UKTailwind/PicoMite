/***********************************************************************************************************************
PicoMite MMBasic

Symbols.c

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
/**
 * @file Symbols.c
 * @author Geoff Graham, Peter Mather
 * @brief Symbols: names stored in a saved program as 2 or 3 byte tokens (see Symbols.h)
 */
/**
 * @cond
 * The following section will be excluded from the documentation.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

// the few routines run for every name or statement live in RAM, except on the
// RP2040 WebMite, whose RAM has no room for them
#if defined(PICOMITEWEB) && !defined(rp2350)
#define SYMRAM(f) f
#define SYMRAMDATA
#else
#define SYMRAM(f) __not_in_flash_func(f)
#define SYMRAMDATA __not_in_flash("data")
#endif

const symtab_t *SymTabProg = NULL;
const symtab_t *SymTabLib = NULL;
int SymMode = SYM_OFF;
int SymRawBlock = 0;
int SymEnabled = 1;
int SymLongest = 0;
static int SymLongestLib = 0; // the longest spelling in the library's table
int SymLibSave = 0;

// the payload alphabet: 0-9 A-Z a-z _ are the digits 0-62, anything else is 0xFF
#define XX 0xFF
SYMRAMDATA const unsigned char symdigit[128] = {
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, XX, XX, XX, XX, XX, XX,
    XX, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, XX, XX, XX, XX, 62,
    XX, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, XX, XX, XX, XX, XX};
#undef XX
static const char symchars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";

/********************************************************************************************************************************************
 reading symbols
********************************************************************************************************************************************/

// The spelling of the symbol at p, or NULL if the symbol is not in its table
static inline const unsigned char *SymLookup(const unsigned char *p, int *len)
{
    const symtab_t *t = (p[0] & 2) ? SymTabLib : SymTabProg;
    unsigned int id;
    if (p[0] & 1)
        id = symdigit[p[1] & 0x7f];
    else
        id = SYM_NSHORT + symdigit[p[1] & 0x7f] * SYM_NSHORT + symdigit[p[2] & 0x7f];
    if (t == NULL || id >= t->count)
        return NULL;
    const unsigned char *n = (const unsigned char *)t + t->names + ((const uint16_t *)(t + 1))[id];
    *len = n[0];
    return n + 1;
}

// A symbol that its table cannot supply (see SymSpelling() in Symbols.h)
void SymDamaged(void)
{
    error("Program damaged: symbol table");
}

#ifndef rp2350
const unsigned char *SYMRAM(SymSpelling)(const unsigned char *p, int *len)
{
    return SymSpellingInline(p, len);
}
#endif

// true if the name at p (symbol or text) is text, ignoring case
int SymNameEqual(const unsigned char *p, const char *text)
{
    const unsigned char *s;
    int len;
    NameView((unsigned char *)p, &s, &len);
    if (len == 0 || (int)strlen(text) != len)
        return 0;
    while (len--)
        if (mytoupper(*s++) != mytoupper(*text++))
            return 0;
    return 1;
}

// true if any of the n bytes at p is a symbol
int SymAny(const unsigned char *p, int n)
{
    while (n-- > 0)
        if (issymbol(*p++))
            return 1;
    return 0;
}

// Search the statement at p for the name at name (symbol or text, with an
// optional type suffix), ignoring case.  Only whole names count.  If name
// has a type suffix the name found must have the same one; if it has none,
// any suffix will do.
int SymFindName(unsigned char *p, unsigned char *name)
{
    const unsigned char *ns, *s;
    int nl, l;
    unsigned char *ne = NameView(name, &ns, &nl), *start = p;
    unsigned char suffix = (*ne == '$' || *ne == '%' || *ne == '!') ? *ne : 0;
    if (nl == 0)
        return 0;
    while (*p)
    {
        if (issymbol(*p) || (isnamestart(*p) && (p == start || !isnamechar(p[-1]))))
        {
            unsigned char *e = NameView(p, &s, &l);
            if (l == nl)
            {
                int k = 0;
                while (k < l && mytoupper(s[k]) == mytoupper(ns[k]))
                    k++;
                if (k == l && (!suffix || *e == suffix))
                    return 1;
            }
            p = (e > p) ? e : p + 1;
            continue;
        }
        p++;
    }
    return 0;
}

// The table that belongs to the image at base, or NULL if it has none.  The
// table starts on the word after the 0xFFFFFFFF marker that ends the
// program text, which is where the CSUB records start when there is no table.
const symtab_t *SymFindTable(const unsigned char *image)
{
    const unsigned char *p = image, *limit = image + MAX_PROG_SIZE;
    const symtab_t *t;
    if (image == NULL)
        return NULL;
    // step from line to line, using each line's skip byte when it can be trusted
    while (p < limit && *p == T_NEWLINE)
    {
        unsigned int skip = p[1];
        if (skip >= 3 && skip <= 0xFD && p + skip < limit && p[skip - 1] == 0 && (p[skip] == T_NEWLINE || p[skip] == 0))
            p += skip;
        else
        {
            p += T_NEWLINE_HDR;
            while (p + 1 < limit && !(p[0] == 0 && (p[1] == T_NEWLINE || p[1] == 0)))
                p++;
            p++;
        }
    }
    // the program ends with zeros, then the 0xFFFFFFFF marker
    while (p < limit && *p == 0)
        p++;
    if (p + 4 + sizeof(symtab_t) > limit || *p != 0xFF)
        return NULL;
    p++;
    t = (const symtab_t *)(((uint32_t)p + 3) & ~3);
    if ((const unsigned char *)t + sizeof(symtab_t) > limit || t->magic != SYM_MAGIC ||
        t->textlen != (uint32_t)((const unsigned char *)t - image) || (const unsigned char *)t + t->size > limit ||
        t->names != sizeof(symtab_t) + 2 * t->count || t->count > SYM_MAXIDS)
        return NULL;
    return t;
}

// the longest spelling in table t
static int SymLongestIn(const symtab_t *t)
{
    int longest = 0;
    if (t != NULL)
    {
        const unsigned char *n = (const unsigned char *)t + t->names;
        for (int i = 0; i < t->count; i++)
        {
            if (n[0] > longest)
                longest = n[0];
            n += n[0] + 1;
        }
    }
    return longest;
}

// Point SymTabProg at the table of the program image at base
void SymSetProgram(const unsigned char *image)
{
    SymTabProg = SymFindTable(image);
    SymLongest = SymLongestIn(SymTabProg);
    if (SymLongest < SymLongestLib)
        SymLongest = SymLongestLib;
}

// Point SymTabLib at the table of the library image (NULL: no library)
void SymSetLibrary(const unsigned char *image)
{
    SymTabLib = SymFindTable(image);
    SymLongestLib = SymLongestIn(SymTabLib);
    if (SymLongest < SymLongestLib)
        SymLongest = SymLongestLib;
}

// Copy n bytes of program text to dst, spelling out every symbol.  Returns
// the number of bytes written, or -1 if they would not fit in cap bytes.  A
// symbol that its table cannot supply is written as '?'.
int SymExpand(unsigned char *dst, const unsigned char *src, int n, int cap)
{
    const unsigned char *end = src + n, *s;
    int len, o = 0;
    while (src < end)
    {
        if (issymbol(*src))
        {
            if ((s = SymLookup(src, &len)) == NULL)
            {
                s = (const unsigned char *)"?";
                len = 1;
            }
            if (o + len > cap)
                return -1;
            memcpy(dst + o, s, len);
            o += len;
            src += symbolsize(*src);
        }
        else
        {
            if (o + 1 > cap)
                return -1;
            dst[o++] = *src++;
        }
    }
    return o;
}

// The arguments of a command that has not learnt about symbols.  p is the
// first byte after the command token; if the statement holds any symbol a
// temporary copy with every symbol spelt out is returned, otherwise p.  The
// copy lasts until the end of the statement, like any temporary memory.
unsigned char *SYMRAM(SymExpandStatement)(unsigned char *p)
{
    unsigned char *q = p, *buf;
    int n;
    while (*q && !issymbol(*q))
        q++;
    if (*q == 0)
        return p;
    while (*q)
        q++;
    buf = GetTempMemory(STRINGSIZE + 4);
    n = SymExpand(buf, p, q - p, STRINGSIZE);
    if (n < 0)
        error("Line is too long");
    buf[n] = buf[n + 1] = buf[n + 2] = 0; // a statement ends with a zero, a program with two
    return buf;
}

/********************************************************************************************************************************************
 bindings (see Symbols.h)
********************************************************************************************************************************************/
uint16_t *SymCanonOf = NULL;
unsigned int SymCanonLibBase = 0;
int16_t *SymG = NULL, *SymL = NULL, *SymS = NULL;
symcold_t *SymCold = NULL;
unsigned int SymCanonCount = 0;
int16_t *SymLShadow = NULL;
uint16_t *SymLCanon = NULL;
unsigned int SymLSlots = 0;
int SymTextLocals = 0;
int SymBindHeap = 0;
unsigned char *SymOffLine = NULL;
uint32_t SymBindEvent = 1; // see Symbols.h
uint32_t SymBindGenG = 1;
#ifdef rp2350
uint32_t SymLevelGen[SYM_LEVELS];
_Static_assert(SYM_LEVELS > MAXGOSUB + 8, "SYM_LEVELS must exceed the SUB and GOSUB levels");
// a local made or freed in slot, at its level (see Symbols.h)
#define SymLocalEvent(slot)                          \
    do                                               \
    {                                                \
        unsigned int l_ = VREC(slot)->level;         \
        if (l_ < SYM_LEVELS)                         \
            SymLevelGen[l_] = ++SymBindEvent;        \
    } while (0)
#else
#define SymLocalEvent(slot) // (the RP2040 has no compiled records to tell)
#endif
static uint16_t *SymCanonHead = NULL; // hash chain heads
extern struct s_hash g_hashlist[MAXLOCALLIST]; // (as MMBasic.c defines it)
extern int g_hashlistpointer;
int GetLocalVarHashSize(void);
static unsigned int SymCanonMask, SymNCanon;
static void *SymHotBlock = NULL, *SymColdBlock = NULL;

// the spelling of symbol id (a library id when it is SymCanonLibBase or more)
static const unsigned char *SymIdSpelling(unsigned int id, int *len)
{
    const symtab_t *t = SymTabProg;
    if (id >= SymCanonLibBase)
    {
        t = SymTabLib;
        id -= SymCanonLibBase;
    }
    const unsigned char *n = (const unsigned char *)t + t->names + ((const uint16_t *)(t + 1))[id];
    *len = n[0];
    return n + 1;
}

// The first read of the symbol at p: find the canonical entry of its name,
// or make one.  Returns its index, or -1.
int SymCanonNew(const unsigned char *p)
{
    const unsigned char *s, *t;
    int len, tlen, k;
    unsigned int id, n, bucket;
    uint32_t h = FNV_offset_basis;
    if ((s = SymLookup(p, &len)) == NULL)
        return -1;
    id = symdigit[p[1] & 0x7f];
    if (!(p[0] & 1))
        id = SYM_NSHORT + id * SYM_NSHORT + symdigit[p[2] & 0x7f];
    if (p[0] & 2)
        id += SymCanonLibBase;
    if (id >= SymCanonCount)
        return -1;
    for (k = 0; k < len; k++)
    {
        h ^= mytoupper(s[k]);
        h *= FNV_prime;
    }
    bucket = h & SymCanonMask;
    for (n = SymCanonHead[bucket]; n; n = SymCold[n - 1].next)
    {
        t = SymIdSpelling(SymCold[n - 1].id, &tlen);
        if (tlen != len)
            continue;
        for (k = 0; k < len && mytoupper(s[k]) == mytoupper(t[k]); k++)
            ;
        if (k == len)
        {
            SymCanonOf[id] = n; // another spelling of a name already seen
            return n - 1;
        }
    }
    if (SymNCanon >= SymCanonCount)
        return -1;
    n = SymNCanon;
    SymG[n] = SymL[n] = -1;
    SymS[n] = SYM_UNBOUND;
    SymCold[n].labind = NULL;
    SymCold[n].flags = memchr(s, '.', len) ? SYMC_DOT : 0;
    SymCold[n].id = id;
    SymCold[n].next = SymCanonHead[bucket];
    SymCanonHead[bucket] = ++SymNCanon;
    SymCanonOf[id] = SymNCanon;
    return n;
}

// The canonical entry of symbol id (as in SymCanonOf: a library id when it is
// SymCanonLibBase or more), made if this is the first use of its spelling:
// SymCanonAt for a caller that has the id but not the symbol's bytes (Route
// B's binds, core/Stream.c).  -1 if there are no bindings.
int SymCanonById(unsigned int id)
{
    unsigned char b[3], lib = 0;
    if (SymCanonOf == NULL || id >= SymCanonCount)
        return -1;
    if (SymCanonOf[id])
        return SymCanonOf[id] - 1;
    if (id >= SymCanonLibBase)
    {
        id -= SymCanonLibBase;
        lib = SYM_LIB_LONG - SYM_PROG_LONG;
    }
    if (id < SYM_NSHORT)
    {
        b[0] = SYM_PROG_SHORT + lib;
        b[1] = symchars[id];
    }
    else
    {
        b[0] = SYM_PROG_LONG + lib;
        b[1] = symchars[(id - SYM_NSHORT) / SYM_NSHORT];
        b[2] = symchars[(id - SYM_NSHORT) % SYM_NSHORT];
    }
    return SymCanonNew(b);
}

// the index in commandtbl of the command called name, or -1
static int SymCommandIndex(const char *name)
{
    for (int j = 0; j < CommandTableSize - 1; j++)
        if (str_equal((const unsigned char *)name, commandtbl[j].name))
            return j;
    return -1;
}

// Is the statement at p OPTION SYMBOLS OFF?
int SymIsOffStatement(const unsigned char *p)
{
    static int opt = -2;
    unsigned char *q;
    if (opt == -2)
        opt = SymCommandIndex("Option");
    if (opt < 0 || commandtbl_at(p) != opt)
        return false;
    q = (unsigned char *)p + sizeof(CommandToken);
    skipspace(q);
    if ((q = checkstring(q, (unsigned char *)"SYMBOLS")) == NULL)
        return false;
    return checkstring(q, (unsigned char *)"OFF") != NULL;
}

// Called by PrepareProgram with the program's first line.  OPTION SYMBOLS OFF
// on a line of its own at the top of the program - after nothing but blank and
// comment lines and a LIBRARY LOAD, in either order - runs the program without
// the bindings: they take 3-5 KB of heap that a program written for an older
// MMBasic may need.  Returns that line, or NULL.  (LIBRARY LOAD's own "first
// statement" test steps over the line in the same way.)
unsigned char *SymFindOff(unsigned char *q)
{
    static int lib = -2;
    if (lib == -2)
        lib = SymCommandIndex("Library");
    while (*q == T_NEWLINE)
    {
        unsigned char *line = q, *body = q + T_NEWLINE_HDR;
        if (*body == T_LINENBR)
            body += 3;
        while (*body == ' ')
            body++;
        if (*body != 0 && *body != 39) // (39: a comment)
        {
            if (SymIsOffStatement(body))
                return line;
            unsigned char *a = body + sizeof(CommandToken);
            skipspace(a);
            if (!(lib >= 0 && commandtbl_at(body) == lib && checkstring(a, (unsigned char *)"LOAD")))
                return NULL; // real code first
        }
        q = body;
        while (*q)
            q++;
        q++;
    }
    return NULL;
}

// Called by PrepareProgram: start the program's bindings afresh.  They are
// sized to the program's and the library's symbols and live in the BASIC
// heap; the part every lookup reads is in SRAM, the rest in PSRAM when there
// is some.  Without the memory the program simply runs without them.
void SymBindInit(void)
{
    unsigned int count, hsize = 64, slots;
    SymBindFree();
    SymCanonLibBase = SymTabProg != NULL ? SymTabProg->count : 0;
    count = SymCanonLibBase + (SymTabLib != NULL ? SymTabLib->count : 0);
    if (count == 0 || SymOffLine != NULL)
        return; // (OPTION SYMBOLS OFF: see SymFindOff)
    while (hsize < count)
        hsize <<= 1;
    slots = GetLocalVarHashSize() > MAXLOCALVARS ? GetLocalVarHashSize() : MAXLOCALVARS;
    int hot = count * 4 * sizeof(int16_t) + slots * 2 * sizeof(int16_t);
#ifdef rp2350
    // A program with many names is usually one that fills the SRAM heap, and
    // what it would lose to the bindings spills its own data to PSRAM (Elite:
    // +20% for 9 KB).  So only a small hot block takes SRAM when there is PSRAM.
    SymHotBlock = (PSRAMsize && hot > 4096) ? GetPSMemoryNull(hot) : NULL;
    if (SymHotBlock == NULL)
#endif
        SymHotBlock = GetMemoryNull(hot);
    if (SymHotBlock == NULL)
        return;
    int cold = count * sizeof(symcold_t) + hsize * sizeof(uint16_t);
#ifdef rp2350
    SymColdBlock = PSRAMsize ? GetPSMemoryNull(cold) : NULL;
    if (SymColdBlock == NULL)
#endif
        SymColdBlock = GetMemoryNull(cold);
    if (SymColdBlock == NULL)
    {
        SymBindFree();
        return;
    }
    // (both blocks come zeroed)
    SymCanonOf = (uint16_t *)SymHotBlock;
    SymG = (int16_t *)(SymCanonOf + count);
    SymL = SymG + count;
    SymS = SymL + count;
    SymLShadow = SymS + count;
    SymLCanon = (uint16_t *)(SymLShadow + slots);
    SymLSlots = slots;
    SymCold = (symcold_t *)SymColdBlock;
    SymCanonHead = (uint16_t *)(SymCold + count);
    SymCanonCount = count;
    SymBindHeap = hot + cold;
    SymCanonMask = hsize - 1;
    SymNCanon = 0;
    // locals alive now (left by an error, or made before these bindings)
    // have no entry: count them as made from text until they go
    for (int i = 0; i < g_hashlistpointer; i++)
        if (g_hashlist[i].level > 0 && g_hashlist[i].hash >= 0)
        {
            if ((unsigned)g_hashlist[i].hash < SymLSlots)
                SymLCanon[g_hashlist[i].hash] = SYM_LTEXT;
            SymTextLocals++;
        }
}

// InitHeap has wiped the heap the bindings lived in
void SymBindForget(void)
{
    SubLayoutForget(); // the parameter lists read (MMBasic.c) live with the bindings
    SymBindGlobalsChanged();
    SymHotBlock = SymColdBlock = NULL;
    SymCanonOf = SymCanonHead = SymLCanon = NULL;
    SymG = SymL = SymS = SymLShadow = NULL;
    SymCold = NULL;
    SymCanonCount = SymNCanon = SymLSlots = SymCanonLibBase = 0;
    SymTextLocals = 0;
    SymBindHeap = 0;
}

void SymBindFree(void)
{
    if (SymHotBlock != NULL)
        FreeMemorySafe(&SymHotBlock);
    if (SymColdBlock != NULL)
        FreeMemorySafe(&SymColdBlock);
    SubLayoutFree();
    SymBindForget();
}

// ClearVars(0): every variable has gone
void SymBindReset(void)
{
    SymBindGlobalsChanged();
    for (unsigned int i = 0; i < SymNCanon; i++)
        SymG[i] = SymL[i] = -1;
    if (SymLCanon != NULL)
        memset(SymLCanon, 0, SymLSlots * sizeof(uint16_t));
    SymTextLocals = 0;
}

// findvar made a local in slot, for canonical entry k (-1: its name was text).
// A slot past the shadow arrays (OPTION LOCAL VARIABLES raised since) is
// counted as a text local too.
void SYMRAM(SymLocalMade)(int slot, int k)
{
    SymLocalEvent(slot);
    if (SymLCanon == NULL)
        return;
    if ((unsigned)slot >= SymLSlots)
        SymTextLocals++;
    else if (k >= 0)
    {
        SymLShadow[slot] = SymL[k];
        SymL[k] = slot;
        SymLCanon[slot] = k + 1;
    }
    else
    {
        SymLCanon[slot] = SYM_LTEXT;
        SymTextLocals++;
    }
}

// ClearVars is freeing the local in slot (newest first)
void SYMRAM(SymLocalFreed)(int slot)
{
    unsigned int c;
    SymLocalEvent(slot); // (its level is still set: ClearVars clears the entry after)
    if (SymLCanon == NULL)
        return;
    if ((unsigned)slot >= SymLSlots)
    {
        SymTextLocals--;
        return;
    }
    c = SymLCanon[slot];
    if (c == SYM_LTEXT)
        SymTextLocals--;
    else if (c)
        SymL[c - 1] = SymLShadow[slot];
    SymLCanon[slot] = 0;
}

// erase(): the global in this slot has gone
void SymBindForgetSlot(int slot)
{
    SymBindGlobalsChanged();
    for (unsigned int i = 0; i < SymNCanon; i++)
        if (SymG[i] == slot)
            SymG[i] = -1;
}

/********************************************************************************************************************************************
 commands that read symbols themselves

 Every other command is given its arguments with the symbols spelt out, so
 it sees exactly the text it saw before symbols existed.  A command belongs
 here when it must see the program itself - it keeps a pointer into its own
 statement or searches the program from there - or, once its argument
 parsing has been checked, for speed.
********************************************************************************************************************************************/
static const char *const SymAwareNames[] = {
    "Let", "If", "ElseIf", "Else If", "Else", "EndIf", "End If",
    "For", "Next", "Do", "While", "Loop", "Exit For", "Exit Do", "Exit Sub", "Exit Function", "Exit",
    "Continue", "Select Case", "Case", "Case Else", "End Select",
    "Sub", "Function", "End Sub", "End Function", "CSub", "End CSub",
    "DefineFont", "End DefineFont", "Rem", "/*", "*/", "Data",
    "Return", "IReturn", "Print", "Inc", "GoTo", "GoSub", "Dim", "Local", "Static", "Const",
    // graphics (Draw.c, Blit.c, Sprite.c): arguments read through the evaluator and getargaddress()
    "Pixel", "Line", "Box", "RBox", "Circle", "Triangle", "Arc", "Bezier", "CLS",
    "Colour", "Color", "Text", "Font", "Blit", "Sprite", "Refresh",
#ifdef STRUCTENABLED
    "Type", "End Type",
#endif
    NULL};
static uint8_t SymAwareBits[(1024 + 7) / 8];

int SYMRAM(SymAwareCommand)(int cmd)
{
    return (unsigned)cmd < 1024 && (SymAwareBits[cmd >> 3] & (1 << (cmd & 7)));
}

// Functions whose arguments may hold symbols: every function in Functions.c
// reads its arguments through the evaluator (the letter that tokenise()
// writes after SChange$( TopBottom( and ~( stays text).  Any other function
// is given its arguments with the symbols spelt out.
static const char *const SymAwareFunNames[] = {
    "Abs(", "ACos(", "Asc(", "ASin(", "Atan2(", "Atn(", "base$(", "Bin$(", "Bin2str$(", "Bit(",
    "Bound(", "Byte(", "Call(", "Chr$(", "Cint(", "Cos(", "Deg(", "Eval(", "Exp(", "Field$(",
    "Fix(", "Flag(", "Hex$(", "Instr(", "Int(", "LCase$(", "Left$(", "Len(", "Log(", "Max(",
    "TopBottom(", "Mid$(", "MID$(", "Min(", "Rad(", "Right$(", "Rnd(", "SChange$(", "Sgn(", "Sin(",
    "Space$(", "Sqr(", "Str$(", "Str2bin(", "String$(", "Tab(", "Tan(", "Choice(", "~(", "Trim$(",
    "UCase$(", "Val(", NULL};
static uint8_t SymAwareFunBits[(256 + 7) / 8];

int SYMRAM(SymAwareFunction)(unsigned char tkn)
{
    return SymAwareFunBits[tkn >> 3] & (1 << (tkn & 7));
}

void SymInit(void)
{
    memset(SymAwareBits, 0, sizeof(SymAwareBits));
    for (int i = 0; SymAwareNames[i] != NULL; i++)
    {
        for (int j = 0; j < CommandTableSize - 1 && j < 1024; j++)
            if (str_equal((unsigned char *)SymAwareNames[i], commandtbl[j].name))
                SymAwareBits[j >> 3] |= 1 << (j & 7);
    }
    memset(SymAwareFunBits, 0, sizeof(SymAwareFunBits));
    for (int i = 0; SymAwareFunNames[i] != NULL; i++)
    {
        for (int j = 0; j < TokenTableSize - 1 && j + C_BASETOKEN < 256; j++)
            if (str_equal((unsigned char *)SymAwareFunNames[i], tokentbl[j].name))
                SymAwareFunBits[(j + C_BASETOKEN) >> 3] |= 1 << ((j + C_BASETOKEN) & 7);
    }
    SymTabProg = SymTabLib = NULL;
    SymMode = SYM_OFF;
    SymRawBlock = 0;
}

/********************************************************************************************************************************************
 saving: collecting the names of a program and writing its table

 The program is tokenised twice.  The first pass (SYM_COUNT) records every
 spelling and how often it is used; SymRank() then gives the 63 most used
 spellings the short symbols; the second pass (SYM_EMIT) writes the program
 with its symbols, and SymTableWrite() writes the table after it.

 The collector lives in one block of temporary memory: a hash table of
 SYMHASH chain heads, the entries growing up from after it and the
 spellings growing down from the end.  If the block fills up the program is
 simply saved as text, as it would have been before symbols existed.
********************************************************************************************************************************************/
#define SYMHASH 256
typedef struct
{
    uint16_t next;  // next entry in this hash chain + 1, 0 = none
    uint16_t count; // uses of this spelling
    uint16_t id;    // symbol id, 0xFFFF until ranked
    uint16_t name;  // offset of [length][spelling] in the block
} symentry_t;

static unsigned char *symblk = NULL;
static int symblkowned;        // symblk is temporary memory of our own (not borrowed from the source's buffer)
static unsigned char *symsrc;  // the source being saved (SymBegin's src), for SymRetry
static int symblksize, symentries, symnametop, symoverflow, symshort, symnamebytes;
static uint16_t symtop[SYM_NSHORT]; // the entries that get short symbols, most used first

extern char *g_StrTmp[MAXTEMPSTRINGS];
extern char g_StrTmpLocalIndex[MAXTEMPSTRINGS];
extern int g_StrTmpIndex;

#define SYMHEADS ((uint16_t *)symblk)
#define SYMENTRY ((symentry_t *)(symblk + SYMHASH * sizeof(uint16_t)))

// The space in src's heap block after its text (LOAD, EDIT and the like take
// nearly the whole heap for the source text and fill only part of it): where
// it starts, and *size, at most 32 KB, or 0 if there is less than 4 KB.
static unsigned char *SymAfterSource(unsigned char *src, int *size)
{
    unsigned char *start = (unsigned char *)(((uint32_t)(src + strlen((char *)src) + 1) + 3) & ~3);
    int n = MemRemaining(src) - (start - src);
    *size = n < 4096 ? 0 : (n > 32768 ? 32768 : n);
    return start;
}

// Start collecting for the program source at src.  Returns false if symbols
// are switched off or there is not the memory to collect them, in which case
// the program is saved as text.
int SymBegin(unsigned char *src)
{
    static const int sizes[] = {32768, 16384, 8192};
    SymMode = SYM_OFF;
    SymRawBlock = 0;
    SymLibSave = 0;
    symblk = NULL;
    symblkowned = false;
    symsrc = src;
    if (!SymEnabled)
        return false;
    for (int i = 0; i < 3 && symblk == NULL && g_StrTmpIndex < MAXTEMPSTRINGS; i++)
    {
        symblk = GetMemoryNull(sizes[i]);
        symblksize = sizes[i];
    }
    if (symblk != NULL)
    {
        // register it as temporary memory so that an error part way through a save frees it
        g_StrTmpLocalIndex[g_StrTmpIndex] = g_LocalIndex;
        g_StrTmp[g_StrTmpIndex++] = (char *)symblk;
        g_TempMemoryIsChanged = true;
        symblkowned = true;
    }
    else if (src != NULL)
    {
        // no block of the heap: borrow what is left after the source text
        int size;
        unsigned char *start = SymAfterSource(src, &size);
        if (size == 0)
            return false;
        symblk = start;
        symblksize = size;
    }
    else
        return false;
    memset(symblk, 0, SYMHASH * sizeof(uint16_t));
    symentries = symoverflow = symshort = symnamebytes = 0;
    symnametop = symblksize;
    SymMode = SYM_COUNT;
    return true;
}

// After the counting pass: if the names did not fit in the block taken from
// the heap but there is more room after the source text, change to that and
// return true for the count to be made again (once: the second count is in
// the borrowed space).  On an RP2040 a large program's text can leave only the
// 8 KB block in the heap beside it, too small for 500 names.
int MIPS16 SymRetry(void)
{
    unsigned char *start;
    int size;
    if (SymMode != SYM_COUNT || !symoverflow || !symblkowned || symsrc == NULL)
        return false;
    start = SymAfterSource(symsrc, &size);
    if (size <= symblksize)
        return false;
    ClearSpecificTempMemory(symblk);
    symblkowned = false;
    symblk = start;
    symblksize = size;
    memset(symblk, 0, SYMHASH * sizeof(uint16_t));
    symentries = symoverflow = symshort = symnamebytes = 0;
    symnametop = symblksize;
    return true;
}

void SymEnd(void)
{
    SymMode = SYM_OFF;
    SymRawBlock = 0;
    SymLibSave = 0;
    if (symblk != NULL && symblkowned)
        ClearSpecificTempMemory(symblk);
    symblk = NULL;
    symblkowned = false;
}

// After the counting pass: give the most used spellings the short symbols
// and number the rest in the order they were first seen.  If the names did
// not fit, symbols are switched off and the program is saved as text.
void SymRank(void)
{
    int i, j, k;
    symentry_t *e = SYMENTRY;
    if (symoverflow || symentries == 0)
    {
        SymMode = SYM_OFF;
        return;
    }
    for (i = 0, k = 0; i < symentries; i++)
    {
        // insert entry i into the list of the most used, keeping the first seen ahead on a tie
        for (j = k; j > 0 && e[symtop[j - 1]].count < e[i].count; j--)
            if (j < SYM_NSHORT)
                symtop[j] = symtop[j - 1];
        if (j < SYM_NSHORT)
        {
            symtop[j] = i;
            if (k < SYM_NSHORT)
                k++;
        }
    }
    symshort = k;
    for (i = 0; i < symentries; i++)
        e[i].id = 0xFFFF;
    for (i = 0; i < symshort; i++)
        e[symtop[i]].id = i;
    for (i = 0, j = symshort; i < symentries; i++)
        if (e[i].id == 0xFFFF)
            e[i].id = j++;
    symnamebytes = 0;
    for (i = 0; i < symentries; i++)
        symnamebytes += symblk[e[i].name] + 1;
    SymMode = SYM_EMIT;
    SymRawBlock = 0;
}

// The counting pass: read the program source the way the savers do and
// tokenise every line, which records its names.  The output is discarded.
void SymCount(unsigned char *pm)
{
    unsigned char *p, prevchar = 0;
    int continuation = false, n;
    multi = false;
    SymRawBlock = false;
    while (*pm)
    {
        if (continuation)
            p = &inpbuf[strlen((char *)inpbuf)];
        else
            p = inpbuf;
        continuation = false;
        while (!(*pm == 0 || *pm == '\r' || (*pm == '\n' && prevchar != '\r')))
        {
            if (*pm == TAB)
            {
                do
                {
                    *p++ = ' ';
                } while ((p - inpbuf) % 2 && (p - inpbuf) < MAXSTRLEN);
            }
            else if (isprint((uint8_t)*pm))
                *p++ = *pm;
            prevchar = *pm++;
            if ((p - inpbuf) >= MAXSTRLEN)
                goto done; // the save itself will report the line as too long
        }
        if (*pm)
            prevchar = *pm++;
        *p = 0;
        if (*inpbuf == 0 && (*pm == 0 || (!isprint((uint8_t)*pm) && pm[1] == 0)))
            break;
        n = strlen((char *)inpbuf);
        if (Option.continuation && n >= 2 && inpbuf[n - 1] == Option.continuation && inpbuf[n - 2] == ' ')
        {
            continuation = true;
            inpbuf[n - 2] = 0;
            continue;
        }
        tokenise(false);
    }
done:
    multi = false;
    SymRawBlock = false;
}

// Called by tokenise() for each name in a program being saved.  Counts the
// name, or writes its symbol (a library symbol while a library is saved), and
// returns the new output pointer.  Anything that cannot be a symbol is written
// as text, which always works.
unsigned char *SymName(unsigned char *op, const unsigned char *name, int len)
{
    uint32_t h = FNV_offset_basis;
    int i, n;
    symentry_t *e = SYMENTRY;
    if (SymMode == SYM_OFF || symblk == NULL || len == 0 || len > SYM_MAXLEN || symoverflow)
        goto astext;
    for (i = 0; i < len; i++)
    {
        h ^= name[i];
        h *= FNV_prime;
    }
    h &= SYMHASH - 1;
    for (n = SYMHEADS[h]; n; n = e[n - 1].next)
    {
        unsigned char *s = symblk + e[n - 1].name;
        if (s[0] == len && memcmp(s + 1, name, len) == 0)
            break;
    }
    if (SymMode == SYM_COUNT)
    {
        if (n)
        {
            if (e[n - 1].count < 0xFFFF)
                e[n - 1].count++;
        }
        else
        {
            // room for one more entry below the spellings?
            if (symentries >= SYM_MAXIDS ||
                SYMHASH * sizeof(uint16_t) + (symentries + 1) * sizeof(symentry_t) + len + 1 > (unsigned)symnametop)
            {
                symoverflow = 1;
                goto astext;
            }
            symnametop -= len + 1;
            symblk[symnametop] = len;
            memcpy(symblk + symnametop + 1, name, len);
            e[symentries].name = symnametop;
            e[symentries].count = 1;
            e[symentries].id = 0xFFFF;
            e[symentries].next = SYMHEADS[h];
            SYMHEADS[h] = ++symentries;
        }
        goto astext; // the counting pass writes text as before
    }
    // SYM_EMIT
    if (n == 0 || e[n - 1].id == 0xFFFF)
        goto astext;
    i = e[n - 1].id;
    if (i < SYM_NSHORT)
    {
        *op++ = SymLibSave ? SYM_LIB_SHORT : SYM_PROG_SHORT;
        *op++ = symchars[i];
    }
    else
    {
        i -= SYM_NSHORT;
        *op++ = SymLibSave ? SYM_LIB_LONG : SYM_PROG_LONG;
        *op++ = symchars[i / SYM_NSHORT];
        *op++ = symchars[i % SYM_NSHORT];
    }
    return op;
astext:
    memcpy(op, name, len);
    return op + len;
}

// bytes the table will take, 0 if there is none
int SymTableSize(void)
{
    if (SymMode != SYM_EMIT)
        return 0;
    return (sizeof(symtab_t) + 2 * symentries + symnamebytes + 3) & ~3;
}

// Write the table through put().  textlen is its offset from the start of
// the image.
void SymTableWrite(void (*put)(unsigned char), uint32_t textlen)
{
    symtab_t h;
    symentry_t *e = SYMENTRY;
    unsigned char *b = (unsigned char *)&h;
    int i, k, n, off, size = SymTableSize();
    if (size == 0)
        return;
    h.magic = SYM_MAGIC;
    h.textlen = textlen;
    h.size = size;
    h.count = symentries;
    h.names = sizeof(symtab_t) + 2 * symentries;
    for (i = 0; i < (int)sizeof(symtab_t); i++)
        put(b[i]);
    // offsets, in id order.  The long ids follow entry order so one walk of
    // the entries lists them; the short ones come first from symtop[].
    off = 0;
    for (i = 0; i < symshort; i++)
    {
        put(off & 0xFF);
        put(off >> 8);
        off += symblk[e[symtop[i]].name] + 1;
    }
    for (k = 0; k < symentries; k++)
        if (e[k].id >= symshort)
        {
            put(off & 0xFF);
            put(off >> 8);
            off += symblk[e[k].name] + 1;
        }
    // the spellings, in the same order
    for (i = 0; i < symshort; i++)
    {
        unsigned char *s = symblk + e[symtop[i]].name;
        for (n = 0; n <= s[0]; n++)
            put(s[n]);
    }
    for (k = 0; k < symentries; k++)
        if (e[k].id >= symshort)
        {
            unsigned char *s = symblk + e[k].name;
            for (n = 0; n <= s[0]; n++)
                put(s[n]);
        }
    for (i = sizeof(symtab_t) + 2 * symentries + symnamebytes; i < size; i++)
        put(0);
}
/*  @endcond */
