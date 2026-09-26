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
#include "hardware/flash.h" // FLASH_SECTOR_SIZE

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
#define RB_VERSION 1        // the stream format
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
   own handler.  A comment gets no record, because the text loop runs nothing
   for it.  END closes each image.  All offsets in a record are from the
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
    RB_OP_END       // key (2 words) of the image's end
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
    if (cmd)
    {
        w[n++] = RB_OP_CMD | ((tok - entry) << 8);
        w[n++] = commandtbl_decode(tok);
    }
    else
        w[n++] = RB_OP_SUBCALL | ((tok - entry) << 8);
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
            { // a comment: the text loop runs nothing
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
            if (cmd >= 0)
                RBEmitStmt(base, libbit, entry, linestart, tok, cmd, cmdl, next);
            p = next;
        }
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

void RBPrepare(void)
{
    rbheader_t h;
    const rbheader_t *old = (const rbheader_t *)RBSlotBase();
    RBLive = 0;
    RBWhy = NULL;
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
    h.progcrc = lfs_crc(0xffffffff, ProgMemory, MAX_PROG_SIZE);
    h.libcrc = LibPresent() ? lfs_crc(0xffffffff, LibMemory, MAX_PROG_SIZE) : 0;
    if (old->magic == h.magic && old->version == h.version && old->size == h.size && old->build == h.build &&
        old->progcrc == h.progcrc && old->libcrc == h.libcrc &&
        old->hdrcrc == lfs_crc(0xffffffff, old, offsetof(rbheader_t, hdrcrc)))
    {
        RBReused++;
        RBLive = 1;
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
    }
}
/*  @endcond */
