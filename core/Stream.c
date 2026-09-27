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
#define RB_VERSION 3        // the stream format
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

static void RBEmitWords(const uint16_t *w, int n)
{
    if (C.pass == 2)
        RBPut(&C.code, w, n * 2);
    C.codelen += n * 2;
}

static void RBEmitStmt(unsigned char *base, uint32_t libbit, unsigned char *entry, int linestart,
                       unsigned char *tok, int cmd, unsigned char *cmdl, unsigned char *next)
{
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
    w[n++] = RB_OP_STMT | (linestart ? RB_LINESTART : 0);
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
    RBWalk(ProgMemory, 0);
    if (C.pass == 2)
        RBTabTo(C.nbprog - 1); // the program's last buckets end where the library's entries start
    if (LibPresent())
        RBWalk(LibMemory, RB_LIBBIT);
    if (C.pass == 2)
        RBTabTo(C.ntab - 1); // and the library's, and the sentinel, at the end of the map
}

// Compile the program (and the library) into the slot.
static void RBCompile(rbheader_t *h)
{
    uint32_t codeoff;
    memset(&C, 0, sizeof(C));
    W.full = 0;
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
        end = OptionErrorSkip == 0 ? RBExec(r, entry) : RBExecSkip(r, entry);
        // where next: the next record, the record of a jump's target, or the text loop
        if (nextstmt == end)
        {
            r += (r[3] & 0xFF) == RB_OP_CMD ? 6 : 5;
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
    RBRan = RBMiss = 0;
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
    }
}
#endif // rp2350
/*  @endcond */
