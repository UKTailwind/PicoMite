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

int RBMode = RB_OFF;

// A board with PSRAM keeps the stream in RAM slot 4, written at memory speed;
// one without keeps it in flash slot 2 (slot 3 holds the library and slot 1
// is left for the user).
int RBStreamSlotRam(void)
{
#ifdef rp2350
    return PSRAMsize != 0;
#else
    return 0;
#endif
}

int RBStreamSlot(void)
{
    return RBStreamSlotRam() ? 4 : 2;
}

// While OPTION COMPILE is on, the stream's slot is not the user's: saving,
// loading, erasing or running it would destroy the stream or run it as a
// program.
void RBGuardSlot(int ram, int slot)
{
    if (RBMode == RB_OFF || ram != RBStreamSlotRam() || slot != RBStreamSlot())
        return;
    if (ram)
        error("RAM slot % holds the compiled program: OPTION COMPILE OFF first", slot);
    else
        error("Flash slot % holds the compiled program: OPTION COMPILE OFF first", slot);
}

/* ---------------------------------------------------------------------------
   P1b: the stamp and the slot.

   A slot holds one stream.  Its first 256-byte page is the header, written
   last, so a stream whose writing was interrupted has no valid header and
   reads as stale.
   --------------------------------------------------------------------------- */
#define RB_MAGIC 0x31304252 // "RB01"
#define RB_VERSION 2        // the stream format
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
    uint32_t hdrcrc;   // CRC32 of everything above
} rbheader_t;

int RBLive = 0;
static int RBCompiles = 0;
static int RBReused = 0;
static uint32_t RBRan = 0;  // statements run from the stream since RUN
static uint32_t RBMiss = 0; // map lookups the cache did not answer (a binary search each)
static const char *RBWhy = NULL; // why the last RUN ran as text

// A stream is tied to the firmware that wrote it: a record holds command
// token numbers, which another build may number differently.
static uint32_t RBBuildId(void)
{
    return ((uint32_t)RB_VERSION << 24) ^ ((uint32_t)CommandTableSize << 12) ^ (uint32_t)TokenTableSize ^ (uint32_t)MAX_PROG_SIZE;
}

static uint8_t *RBSlotBase(void)
{
#ifdef rp2350
    if (RBStreamSlotRam())
        return (uint8_t *)PSRAMblock + (RBStreamSlot() - 1) * MAX_PROG_SIZE;
#endif
    return (uint8_t *)(flash_target_contents + (RBStreamSlot() - 1) * MAX_PROG_SIZE);
}

// the flash offset of the slot, for safe_flash_range_erase/program
static uint32_t RBSlotFlashOffset(void)
{
    return FLASH_TARGET_OFFSET + FLASH_ERASE_SIZE + SAVEDVARS_FLASH_SIZE + (RBStreamSlot() - 1) * MAX_PROG_SIZE;
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
    if (RBStreamSlotRam())
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
    if (!RBStreamSlotRam())
    {
        w->page = GetTempMemory(RB_PAGE);
        memset(w->page, 0xFF, RB_PAGE);
    }
}

static void RBPut(rbwriter_t *w, const void *data, uint32_t n)
{
    const uint8_t *d = data;
    if (RBStreamSlotRam())
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
    if (!RBStreamSlotRam() && w->pos % RB_PAGE)
        RBFlashPage(w->pos - w->pos % RB_PAGE, w->page); // the rest of the page is 0xFF
}

static void RBWriteEnd(rbheader_t *h)
{
    uint8_t *hp;
    h->hdrcrc = lfs_crc(0xffffffff, h, offsetof(rbheader_t, hdrcrc));
    if (RBStreamSlotRam())
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

   Slot layout: header page, map (8 bytes a statement), code from the next
   page boundary.  A first pass counts, so every part's place is known before
   the second writes.
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

static struct
{
    int pass;         // 1: count, 2: write
    uint32_t stmts;   // records
    uint32_t codelen; // bytes of code
    rbwriter_t map, code;
    int toolong; // an offset did not fit its byte: the program runs as text
} C;

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
    w[0] = RB_OP_END;
    w[1] = endkey & 0xFFFF;
    w[2] = endkey >> 16;
    RBEmitWords(w, 3);
}

static void RBWalkAll(void)
{
    C.stmts = 0;
    C.codelen = 0;
    RBWalk(ProgMemory, 0);
    if (LibPresent())
        RBWalk(LibMemory, RB_LIBBIT);
}

// Compile the program (and the library) into the slot.
static void RBCompile(rbheader_t *h)
{
    uint32_t codeoff;
    memset(&C, 0, sizeof(C));
    W.full = 0;
    C.pass = 1;
    RBWalkAll();
    h->stmts = C.stmts;
    h->mapoff = RB_PAGE;
    h->maplen = C.stmts * 8;
    codeoff = (h->mapoff + h->maplen + RB_PAGE - 1) & ~(RB_PAGE - 1);
    h->codeoff = codeoff;
    h->codelen = C.codelen;
    if (C.toolong || codeoff + C.codelen > MAX_PROG_SIZE)
    {
        W.full = 1;
        return;
    }
    RBWriteBegin(codeoff + C.codelen);
    RBWriterStart(&C.map, h->mapoff);
    RBWriterStart(&C.code, codeoff);
    C.pass = 2;
    RBWalkAll();
    RBFlush(&C.map);
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

   The executor's position is kept in globals, not registers, so the longjmp
   that ON ERROR SKIP takes back into it cannot lose it.  A FUNCTION called in
   a statement runs its body in a nested ExecuteProgram and so a nested
   RunStream, which saves its caller's globals on entry and restores them
   when it returns.
   --------------------------------------------------------------------------- */
// The executor runs for every statement, so it lives in RAM, as ExecuteProgram
// does: from flash it and the handlers it calls evict each other from the XIP
// cache (RP2040 pixart: 29% slower than text, most of it in RBTail/RBExec
// fetches and in findvar lines they pushed out)
#ifdef rp2350 // the RP2040 has no page of RAM to spare below the heap (see Stream.h)
#define RBRAM(f) __not_in_flash_func(f)
#else
#define RBRAM(f) f
#endif
// Map lookups remembered, so a loop driven by a fallback NEXT finds its
// target here.  Exile misses on a quarter of its statements with 64 entries
// and on 15% with 512: its jump targets outnumber any cache RAM allows, so
// the miss path itself has to get cheaper (docs/Interpreter_RouteB_Design.html).
#define RB_CACHE_BITS 6
#define RB_CACHE (1 << RB_CACHE_BITS)

static struct
{
    uint32_t key;
    const uint16_t *rec;
} RBCache[RB_CACHE];

static const uint8_t *RBBase;          // the live stream's slot
static const uint32_t *RBMap;          // its statement map: key, record offset
static uint32_t RBMapLen;              // entries in the map
static const uint16_t *volatile RBRec; // the executor's position: a STMT or END record
static unsigned char *volatile RBEntry; // the statement's entry in the text
static unsigned char *volatile RBEnd;   // where the statement ends (the nextstmt it was given)
static volatile int RBFirst;            // entering: the text loop has run the tail
static volatile int RBSaveLocal;        // g_LocalIndex before the statement, for ON ERROR SKIP
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
    RBMapLen = h->stmts;
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
    uint32_t key, lo, hi;
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
        return RBCache[c].rec;
    RBMiss++;
    lo = 0;
    hi = RBMapLen;
    while (lo < hi)
    {
        uint32_t mid = (lo + hi) >> 1;
        if (RBMap[mid * 2] < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo >= RBMapLen || RBMap[lo * 2] != key)
        return NULL;
    RBCache[c].key = key;
    RBCache[c].rec = (const uint16_t *)(RBBase + RBMap[lo * 2 + 1]);
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

// Run the statement whose STMT record is r, as the text loop would.
static void RBRAM(RBExec)(const uint16_t *r)
{
    unsigned char *e = RBEntry, *p;
    int i;
    RBRan++;
    if ((r[3] & 0xFF) == RB_OP_CMD)
    {
        p = e + (r[3] >> 8);
        cmdline = e + (r[5] & 0xFF);
        nextstmt = RBEnd = e + (r[5] >> 8);
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
        nextstmt = RBEnd = e + (r[4] >> 8);
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
}

unsigned char *RBRAM(RunStream)(unsigned char *p)
{
    const uint16_t *save_rec = RBRec, *t;
    unsigned char *save_entry = RBEntry, *save_end = RBEnd, *ret;
    int save_first = RBFirst, save_local = RBSaveLocal;
    const uint16_t *r;
    t = RBFind(p);
    if (t == NULL)
        return p;
    RBRec = t;
    RBFirst = 1;
    for (;;)
    {
        r = RBRec;
        if ((r[0] & 0xFF) == RB_OP_END)
        { // the end of an image: the text loop sees it and stops
            ret = RBImage(RBKeyAt(r));
            if (!RBFirst && RBTail(ret))
            {
                if ((t = RBFind(nextstmt)) != NULL)
                {
                    RBRec = t;
                    RBFirst = 1;
                    continue;
                }
                ret = nextstmt;
            }
            break;
        }
        // STMT: the tail of the statement before, then this one's head
        RBEntry = RBImage(RBKeyAt(r));
        if (!RBFirst && RBTail(RBEntry))
        { // an interrupt: go to its handler
            if ((t = RBFind(nextstmt)) != NULL)
            {
                RBRec = t;
                RBFirst = 1;
                continue;
            }
            ret = nextstmt;
            break;
        }
        RBFirst = 0;
        if (r[0] & RB_LINESTART)
        {
            CurrentLinePtr = RBEntry; // the line's T_NEWLINE, for errors
            TraceBuff[TraceBuffIndex] = RBEntry;
            if (++TraceBuffIndex >= TRACE_BUFF_SIZE)
                TraceBuffIndex = 0;
            if (TraceOn && RBEntry > ProgMemory && RBEntry < ProgMemory + MAX_PROG_SIZE)
            {
                inpbuf[0] = '[';
                IntToStr((char *)inpbuf + 1, CountLines(RBEntry), 10);
                strcat((char *)inpbuf, "]");
                MMPrintString((char *)inpbuf);
                uSec(1000);
            }
        }
        if ((r[3] & 0xFF) == RB_OP_NOP)
        { // no statement, so no tail after it either
            RBFirst = 1;
            RBRec = r + 4;
            continue;
        }
        RBSaveLocal = g_LocalIndex;
        if (OptionErrorSkip == 0)
            RBExec(r);
        else if (setjmp(ErrNext) == 0)
            RBExec((const uint16_t *)RBRec);
        else
        { // ON ERROR SKIP/IGNORE caught an error in the statement
            g_LocalIndex = RBSaveLocal;
            ClearTempMemory();
        }
        r = RBRec;
        // where next: the next record, the record of a jump's target, or the text loop
        if (!RBLive)
        { // the statement compiled the program again (RUN): the text loop picks up
            ret = nextstmt;
            break;
        }
        if (nextstmt == RBEnd)
        {
            RBRec = r + ((r[3] & 0xFF) == RB_OP_CMD ? 6 : 5);
            continue;
        }
        if ((t = RBFind(nextstmt)) != NULL)
        {
            RBRec = t;
            continue;
        }
        ret = nextstmt; // leaving the stream, after this statement's tail
        if (RBTail(ret) && (t = RBFind(nextstmt)) != NULL)
        {
            RBRec = t;
            RBFirst = 1;
            continue;
        }
        ret = nextstmt;
        break;
    }
    RBRec = save_rec;
    RBEntry = save_entry;
    RBEnd = save_end;
    RBFirst = save_first;
    RBSaveLocal = save_local;
    return ret;
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
/*  @endcond */
