/***************************************************************************
CMM2 MMBasic
Thread.c

Threads for a BASIC program: THREAD START, EXIT, YIELD, STOP, PAUSE, RESUME, SLICE and SIZE (docs/Threads_Plan.md,
Stages 1 and 2).

Copyright 2011-2026 Geoff Graham, Peter Mather and Gerry Allardice.
See MMBasic.h for the licence.

A thread is a SUB running on a C stack of its own with its own copy of the interpreter's per-call state. The program
as it always ran is the main thread, number 0.

The running thread uses the interpreter's ordinary globals, where they always were. When it stops, what it has in
them is copied to its record (ContextSave), the parts of the stacks in use only; when it runs again they are copied
back (ContextLoad). The local variables' records are not copied and never move: each thread has a range of the local
slots (LocalBase to LocalLimit, MMBasic.c).

A thread is left only between two of its statements: the statement tail (check_interrupt, MM_Misc.c) calls
ThreadSchedule when INT_TICK_SLICE has been set: by the 1 ms tick when the running thread's time slice is up or a
sleeping thread's PAUSE has ended, by THREAD YIELD. So nothing of the firmware's C code has to be re-entrant. A statement
that calls a FUNCTION has its own frames on the thread's C stack while the FUNCTION's statements run, which is why
each thread has a stack: ThreadSwap changes the stack pointer.

In bank 2 (the linker script): nothing here is on a statement's path until a program starts a thread.

PicoMite (branch threads, docs/Threads_PicoMite_Review.md): this file is the CMM2's (CMM2V1.5 78d4172) with only what
the machine demands changed. Built only with USE_THREADS. The differences:
- A thread other than the main program runs on the process stack (PSP, CONTROL.SPSEL); the main program, every
  interrupt's handler and the housekeeping (routinechecks, on its own veneer) stay on the main stack (Peter, 26 Nov).
  ThreadSwap is written for each chip: ARMv8-M with its FPU and PSPLIM (RP2350), ARMv6-M (RP2040).
- What the CMM2 keeps in static RAM for the whole run (the routines' owners, MM.ERRMSG$'s home) is in the main
  program's record, made from the heap with the first thread: the RP2350 builds have 1 KB of RAM to spare.
- MMErrMsg is an array here, so a switch copies it; the stacks come from the SRAM heap (GetSRAMMemoryNull), never PSRAM.
- The CMM2's pages are the PicoMite's FRAMEBUFFER WRITE target (Stage P4), its frame count the scan-out's (P4).
***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>
#include "MMBasic_Includes.h"
#ifdef USE_THREADS                                                  // (the whole file: a build without threads has none of it)
#include "Hardware_Includes.h"
#include "Symbols.h"
#include "Thread.h"
#include "hardware/sync.h"
#include "DrawInternal.h"                                          // (FrameWait)

extern int g_localtop, g_hashlistpointer;                             // (MMBasic.c: the next local slot, the locals' list's top)
extern uint32_t DefinedSubFunMem;                                   // (a call whose arguments are being read)
extern int DefinedSubFunLocalIndex;
extern int g_StrTmpIndex;
#ifdef SUBPROFILE
extern uint64_t *g_subchild_us;                                     // (OPTION PROFILING's frame times: MMBasic.c)
#endif
#ifndef rp2350
#define GetSRAMMemoryNull GetMemoryNull                                    // (no PSRAM: the heap is SRAM)
#endif
#define TKNBUFSIZE STRINGSIZE                                       // (tknbuf's size here: MMBasic.c)
#define MAINLOCALS GetLocalVarHashSize()                            // the local region: OPTION LOCAL VARIABLES' (MMBasic.c)
volatile uint32_t FrameCount = 0;                                   // frames shown: to be counted by the scan-out (Stage P4)
static inline char *ProfileSwitch(char *line) { return line; }      // OPTION PROFILING per thread: Stage P5
#ifdef LOADER
extern volatile int LoaderActive;                                   // (Loader.c: a loadable program is running)
#else
#define LoaderActive 0
#endif

#define T_FREE 0
#define T_READY 1
#define T_DONE 2                                                    // ended: its blocks are freed from another thread's stack
#define T_SLEEP 3                                                   // in PAUSE until the microsecond clock reads 'wake'
#define T_WAIT 4                                                    // waiting for a lock, a thread or a flag, until 'wake' at most
#define ALIVE(t) ((t)->state == T_READY || (t)->state == T_SLEEP || (t)->state == T_WAIT)
#define W_LOCK 1                                                    // what a thread waits for (wkind), and which (warg)
#define W_THREAD 2
#define W_FLAG 3
#define W_FRAME 4                                                   // the frame after frame number warg
#define W_NET 5                                                     // the network, which another thread's WEB command has
#define NOLIMIT LLONG_MAX                                           // a wait with no time limit
#define CALLMAX 240                                                 // the longest call of a thread's SUB: a call is read with
                                                                    // a program line's room (makeargs, DefinedSubFun)
#define MAXTARGS 16                                                 // the arguments THREAD START can hand to a thread's SUB
struct s_targ {                                                     // one of them, evaluated by THREAD START
    int type;                                                       // T_INT, T_NBR or T_STR
    union { long long int i; MMFLOAT f; char *s; } v;               // (s: an MMBasic string, kept after the thread's record)
};
#define STACK_MARK 0x54485244                                       // the word at the bottom of a thread's stack
#define STACK_PAINT 0x5354414B                                      // the rest of it at the start, as StackPaint's (MMBasic.c)

struct s_thread {
    uint32_t *sp;                                                   // its stack pointer while it is not running (ThreadSwap)
    unsigned char state, id;
    unsigned char paused;                                           // THREAD PAUSE: passed over until THREAD RESUME
    long long wake;                                                 // T_SLEEP: when its PAUSE ends; T_WAIT: its time limit
    unsigned char wkind;                                            // T_WAIT: W_LOCK, W_THREAD or W_FLAG
    int warg;                                                       // and the lock, thread or flag
    int stacksize;
    char *stack;                                                    // its C stack (NULL: the main program's, the main stack)
    uint32_t floor;                                                 // the stack check's floor (StackFloor while it runs)
    int lbase, llimit;                                              // its local slots
    int16_t *syml;                                                  // its names' local bindings (SymL while it runs)
    // the interpreter's state, as ContextSave left it
    char *nextstmt, *cmdline, *CmdTokenPtr, *CurrentLinePtr;
    int cmdtoken, ExecDepth, g_LocalIndex;
    int gosubindex, g_forindex, g_doindex, g_hashlistpointer, g_localtop, g_Localvarcnt, g_StrTmpIndex, g_TempMemoryIsChanged;
    int OptionErrorSkip, ErrorSkipDepth, MMerrno, MMerrline;
    uint32_t DefinedSubFunMem;
    int DefinedSubFunLocalIndex, g_FunReturnArrayCount;
    int NextData, restorepointer, SymTextLocals;
    char *NextDataLine;
    unsigned int screwup;                                           // EXECUTE's time left (ScrewUpTimer), stopped while it waits
    union cell *splice;                                             // the compiler's spliced values (RBSplice, Stream.c)
    char *pending;                                                  // an interrupt routine another thread handed to this one
    char *profline;                                                 // OPTION PROFILING: the line it was running (ProfileSwitch)
    unsigned char doing;                                            // in PAUSE (1) or THREAD WAIT or LOCK (2): for the list, while
                                                                    // it is the one running (its state is then T_READY)
    unsigned char deaf;                                             // in a wait that takes no interrupt (Deaf)
    unsigned char prio;                                             // THREAD PRIORITY: the time slices in its turn (Turn)
    int pauseint, waitresume;                                       // PAUSE's and THREAD WAIT's state for an interrupt that
    long long pauseuntil, waitlimit;                                // cuts them short (PauseInterrupted, PauseUntil; WaitFor)
    int CurrentX, CurrentY, fcolour, bcolour, font, pixelmode;      // its print position, colours, font and PRINT @'s mode,
    unsigned char *writebuf;                                        // and its FRAMEBUFFER WRITE target (WriteBuf: ScreenSave)
    jmp_buf ErrNext;
    char errmsg[MAXERRMSG];                                         // MM.ERRMSG$ (copied: MMErrMsg is an array here)
    char *call;                                                     // the statement that calls its SUB: the name, its
                                                                    // arguments, then zeros (after the record: CallText)
    struct s_targ *args;                                            // the arguments' values, which the call asks for by
    int nargs;                                                      // MM.INFO(THREAD ARG n)
    char *tknbuf;                                                   // EXECUTE's line: a block of its own, made at its first EXECUTE
    /* Its own stacks, which the interpreter's pointers are set to while it runs (UseStacks): after the record, as
       many entries as its NEST and LOCALS give (ThreadStart). The main program's point at the interpreter's own
       arrays (g_forstack_main and the rest), of their full sizes. A switch copies none of them. */
    unsigned char **gosub, **errs, **subs;                          // (the PicoMite's stacks: unsigned char *)
    struct s_forstack *fors;
    struct s_dostack *dos;
    int nfor, ndo, ngosub, ntemp, ndata;                            // their entries (the interpreter's limits while it runs)
    struct s_hash *locals;                                          // its locals' list (after the record: as many as its
    int nlocals;                                                    // local slots, MAXLOCALLIST for the main program's)
    char **tmp;
    char *tmplevel;
    struct sa_data *data;
#ifdef SUBPROFILE
    int *fbstack, *fbsubidx;                                        // OPTION PROFILING's frames (MMBasic.c, EnterLocalFrame):
    uint64_t *fbentry, *fbchild;                                    // its own, their times (NULL while profiling is off),
    int fbsp, fbbase, fbcur;                                        // how deep, the frame and the SUB it is in, and when it
    uint64_t fbout;                                                 // was last switched out (ProfileFrames)
#endif
};

int ThreadCount = 0;
volatile int ThreadSliceLeft = 0;                                   // (0 while no thread exists: the tick does nothing)
int ThreadSliceLen = 1;
volatile unsigned int ThreadWakeMs = 0;
int ThreadHoldDepth = INT_MAX;
static int StackSize = THREAD_STACK, LocalSlots = THREAD_LOCALS;    // THREAD SIZE: what the next THREAD START gives
static int NestDepth = THREAD_NEST;
static int MainPriority = 1;                                        // the main program's, kept while it has no record
static struct s_thread *Thr[MAXTHREADS + 1];                        // by number: Thr[0] the main program's, made with the first thread
static struct s_thread *Cur = NULL;                                 // the one running (NULL: no thread has been started)
static int Stopping = 0;                                            // the program is ending: no more switches
extern uint64_t g_flag;                                             // (Commands.c: FLAG(n), which THREAD WAIT FLAG waits for)
static unsigned char LockOwner[MAXLOCKS + 1];                       // THREAD LOCK k's owner, as its thread number + 1; 0 free
static int HoldCount = 0, HoldBy = 0;                               // THREAD HOLD: its depth, and the thread that holds
static int NetOwner[2], NetBusy[2];                                 // [0] the network, [1] Bluetooth: the thread it belongs to
                                                                    // and the thread in a command of it, + 1; 0 none
static const char *const NetName[2] = {"The network", "Bluetooth"};
volatile unsigned char ThreadFrameWaiters = 0, ThreadFrameDue = 0;
static int Deferred = 0;                                            // IntMay said no to a source: look again (Resignal)
#define MAXINTOWN 64
struct s_intown { char *routine; unsigned char id; };
static struct s_intown *IntOwner;                                   // which thread set each routine up: after the main
                                                                    // program's record (ThreadStart), not static RAM
static int IntOwners = 0;                                           // (none listed: the main program's)
extern int PauseInterrupted;                                        // (MM_Misc.c: PAUSE's, each thread's: ContextSave)
union cell;
#ifdef rp2350
extern union cell *RBSplice;                                        // (Stream.c: a spliced command's values, on its thread's stack)
#endif
static int WaitResume = 0;                                          // the running thread's wait was cut short by an interrupt,
static long long WaitLimit;                                         // and goes on to this time when its statement runs again

/*******************************************************************************************
 the switch
*******************************************************************************************/
/* The registers a C function must keep (r4-r11, and d8-d15 on the RP2350) and the return address pushed on the stack
   in use, its pointer kept in *from; then the stack 'to' is taken, and the same come back off it: the return is into
   the thread that was left there. The PicoMite's (Peter, 26 Nov): the main program runs on the main stack (MSP) as it
   always did, every other thread on the process stack (PSP, CONTROL.SPSEL): 'lim' is the thread's PSPLIM (its stack's
   hardware floor, the RP2350), 0 for the main program. So every interrupt's handler runs on the main stack, and a
   thread's stack holds only its own frames and an exception's stacked registers. r3 is pushed as well: an even number
   of words, so the main stack stays 8-byte aligned where the main program was left (ThreadRoutineChecks runs C code
   there). PSPLIM is 0 while the stack changes: a new thread's stack may be anywhere in the heap. Between two
   statements, never inside a handler. */
#ifdef rp2350
#define SWAP_WORDS 26                                               // d8-d15 (16), r3-r11 (9), lr
__attribute__((naked, noinline)) static void ThreadSwap(uint32_t **from, uint32_t *to, uint32_t lim) {
    __asm volatile(
        "push   {r3-r11, lr}        \n"
        "vpush  {d8-d15}            \n"
        "str    sp, [r0]            \n"
        "movs   r3, #0              \n"
        "msr    psplim, r3          \n"
        "cbz    r2, 1f              \n"
        "msr    psp, r1             \n"     /* a thread: the process stack */
        "mrs    r3, control         \n"
        "orr    r3, r3, #2          \n"
        "msr    control, r3         \n"
        "isb                        \n"
        "msr    psplim, r2          \n"
        "b      2f                  \n"
        "1:                         \n"
        "msr    msp, r1             \n"     /* the main program: the main stack */
        "mrs    r3, control         \n"
        "bic    r3, r3, #2          \n"
        "msr    control, r3         \n"
        "isb                        \n"
        "2:                         \n"
        "vpop   {d8-d15}            \n"
        "pop    {r3-r11, pc}        \n"
    );
}
#else
#define SWAP_WORDS 10                                               // r8-r11 (4), r3-r7 (5), lr
__attribute__((naked, noinline)) static void ThreadSwap(uint32_t **from, uint32_t *to, uint32_t lim) {
    __asm volatile(
        ".syntax unified             \n"     /* (GCC: Thumb-1 inline asm is divided syntax) */
        "push   {r3-r7, lr}         \n"
        "mov    r4, r8              \n"
        "mov    r5, r9              \n"
        "mov    r6, r10             \n"
        "mov    r7, r11             \n"
        "push   {r4-r7}             \n"
        "mov    r3, sp              \n"
        "str    r3, [r0]            \n"
        "movs   r0, #2              \n"
        "mrs    r3, control         \n"
        "cmp    r2, #0              \n"
        "beq    1f                  \n"
        "msr    psp, r1             \n"     /* a thread: the process stack */
        "orrs   r3, r0              \n"
        "msr    control, r3         \n"
        "isb                        \n"
        "b      2f                  \n"
        "1:                         \n"
        "msr    msp, r1             \n"     /* the main program: the main stack */
        "bics   r3, r0              \n"
        "msr    control, r3         \n"
        "isb                        \n"
        "2:                         \n"
        "pop    {r4-r7}             \n"
        "mov    r8, r4              \n"
        "mov    r9, r5              \n"
        "mov    r10, r6             \n"
        "mov    r11, r7             \n"
        "pop    {r3-r7, pc}         \n"
    );
}
#endif
// a thread's PSPLIM: its stack's floor, above the check word (0: the main program, which is on the main stack)
#define HWFLOOR(t) ((t)->stack == NULL ? 0u : (((uint32_t)(t)->stack + 8 + 7) & ~7u))

// THREAD_LANDING() (Thread.h): SP to MSP after a longjmp from a thread's stack; nothing if SPSEL is clear. Naked: it
// pushes nothing, so the SP the jump restored is the caller's whichever register holds it.
#ifdef rp2350
__attribute__((naked, noinline)) void ThreadLanding(void) {
    __asm volatile(
        "mrs    r0, control          \n"
        "tst    r0, #2               \n"
        "beq    1f                   \n"
        "mov    r1, sp               \n"
        "bic    r0, r0, #2           \n"
        "msr    control, r0          \n"
        "isb                         \n"
        "mov    sp, r1               \n"
        "movs   r0, #0               \n"
        "msr    psplim, r0           \n"
        "1:                          \n"
        "bx     lr                   \n"
    );
}
#else
__attribute__((naked, noinline)) void ThreadLanding(void) {
    __asm volatile(
        ".syntax unified             \n"     /* (GCC: Thumb-1 inline asm is divided syntax) */
        "mrs    r0, control          \n"
        "movs   r2, #2               \n"
        "tst    r0, r2               \n"
        "beq    1f                   \n"
        "mov    r1, sp               \n"
        "bics   r0, r2               \n"
        "msr    control, r0          \n"
        "isb                         \n"
        "mov    sp, r1               \n"
        "1:                          \n"
        "bx     lr                   \n"
    );
}
#endif

/* The housekeeping (routinechecks: USB, audio, the network, the GUI) run on the main stack while a thread is on its
   own (Peter, 26 Nov: interrupts and the housekeeping on the main stack), as the Loader runs a program's services. The
   main stack is where the main program was left (ThreadSwap, 8-byte aligned); what runs there now is below it. An
   error or Ctrl-C in it leaves by longjmp with SPSEL clear, on the main stack: the prompt's landing as from the main
   program. 1 if it ran it there; 0: this is the main stack already, and the caller runs it itself. */
__attribute__((naked, noinline)) int ThreadOnMainStack(void (*fn)(void)) {
    __asm volatile(
        ".syntax unified             \n"     /* (GCC: Thumb-1 inline asm is divided syntax) */
        "push   {r4, lr}            \n"
        "mrs    r4, control         \n"
        "movs   r1, #2              \n"
        "tst    r4, r1              \n"
        "beq    1f                  \n"     /* on the main stack already */
        "bics   r4, r1              \n"
        "msr    control, r4         \n"     /* SP is MSP from here */
        "isb                        \n"
        "blx    r0                  \n"
        "mrs    r4, control         \n"
        "movs   r1, #2              \n"
        "orrs   r4, r1              \n"
        "msr    control, r4         \n"     /* and PSP again */
        "isb                        \n"
        "movs   r0, #1              \n"
        "pop    {r4, pc}            \n"
        "1:                         \n"
        "movs   r0, #0              \n"
        "pop    {r4, pc}            \n"
    );
}

/* Each thread has its own print position, colours, font, PRINT @'s pixel mode and FRAMEBUFFER WRITE target: what
   PRINT, COLOUR, FONT and FRAMEBUFFER WRITE leave for the next statement. A thread starts with those of the
   thread that started it, and MODE gives every thread the new mode's (ThreadsScreenReset). One for the whole
   program, as they were until Stage 4, a thread's TEXT moved the place the main program's INPUT was echoing its
   line at, and one thread's COLOUR was the next thread's PRINT's (Peter, 25 Nov 2026: "Each thread will need to
   maintain some extra context"; the pages and the rest of the review the same day). */
static void ScreenSave(struct s_thread *t) {
    t->CurrentX = CurrentX; t->CurrentY = CurrentY;
    t->fcolour = gui_fcolour; t->bcolour = gui_bcolour;
    t->font = gui_font;
    t->pixelmode = PrintPixelMode;
    t->writebuf = (unsigned char *)WriteBuf;                        // (N, F, L, T or 2: which buffer it draws in)
}

static void ScreenLoad(struct s_thread *t) {
    CurrentX = t->CurrentX; CurrentY = t->CurrentY;
    gui_fcolour = t->fcolour; gui_bcolour = t->bcolour;
    PrintPixelMode = t->pixelmode;
    if(t->writebuf != WriteBuf) ThreadWriteTarget(t->writebuf);     // (only when it differs: FrameBuffer.c)
    if(FontTable[t->font >> 4] == NULL) t->font = (0 << 4) | 1;     // (its font's slot emptied: font 1)
    if(t->font != gui_font) SetFont(t->font);                       // (its size from the table: LOAD FONT may have changed it)
}

// MODE (setmode, ltdc.c) has set the screen up again: every thread has what the thread that ran it has now
void ThreadsScreenReset(void) {
    int i;
    if(Cur == NULL) return;
    for(i = 0; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && ALIVE(Thr[i])) ScreenSave(Thr[i]);
}

/* The thread, other than the running one, that writes to the buffer buf (its FRAMEBUFFER WRITE target), or -1. FRAMEBUFFER
   CLOSE refuses to free a buffer another thread draws in (the plan's section 9, Peter's decision 6, 26 Nov 2026); the
   running thread's own goes back to N there, as it always did. */
int ThreadPageInUse(unsigned char *buf) {
    int i;
    if(Cur == NULL || buf == NULL) return -1;
    for(i = 0; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && Thr[i] != Cur && ALIVE(Thr[i]) && Thr[i]->writebuf == buf) return i;
    return -1;
}

static void ContextSave(struct s_thread *t) {
    ScreenSave(t);
#ifdef SUBPROFILE
    t->fbsp = g_framebase_sp; t->fbbase = g_localframe_base; t->fbcur = g_current_sub_idx;
    t->fbout = time_us_64();
#endif
    t->nextstmt = nextstmt; t->cmdline = cmdline; t->CmdTokenPtr = CmdTokenPtr; t->CurrentLinePtr = CurrentLinePtr;
    t->cmdtoken = cmdtoken; t->ExecDepth = ExecDepth; t->g_LocalIndex = g_LocalIndex;
    t->gosubindex = gosubindex;                                     // (its stacks are its own: only how full each is)
    t->g_forindex = g_forindex;
    t->g_doindex = g_doindex;
    t->g_hashlistpointer = g_hashlistpointer;
    t->g_localtop = g_localtop; t->g_Localvarcnt = g_Localvarcnt;
    t->g_StrTmpIndex = g_StrTmpIndex; t->g_TempMemoryIsChanged = g_TempMemoryIsChanged;
    t->OptionErrorSkip = OptionErrorSkip; t->ErrorSkipDepth = ErrorSkipDepth;
    t->MMerrno = MMerrno; t->MMerrline = MMerrline;
    strncpy(t->errmsg, MMErrMsg, MAXERRMSG - 1);                    // (an array here: copied)
    t->errmsg[MAXERRMSG - 1] = 0;
    memcpy(t->ErrNext, ErrNext, sizeof(jmp_buf));
    t->DefinedSubFunMem = DefinedSubFunMem; t->DefinedSubFunLocalIndex = DefinedSubFunLocalIndex;
#ifdef rp2350
    t->splice = RBSplice;                                           // (a command that waits may be a spliced one: Stage 4)
#endif
    t->pauseint = PauseInterrupted; t->pauseuntil = PauseStart;     // (an interrupt may cut any thread's wait short: 4b)
    t->waitresume = WaitResume; t->waitlimit = WaitLimit;
    t->g_FunReturnArrayCount = g_FunReturnArrayCount;
    t->NextData = NextData; t->NextDataLine = NextDataLine; t->restorepointer = restorepointer;
    t->SymTextLocals = SymTextLocals;
    t->screwup = ScrewUpTimer;                                      // EXECUTE's second runs only while its thread does
    ScrewUpTimer = 0;
}

// the interpreter's stacks are the main program's again: its own arrays, of their full sizes
static void MainStacks(void) {
    g_forstack = g_forstack_main; ForLimit = MAXFORLOOPS;
    g_dostack = g_dostack_main; DoLimit = MAXDOLOOPS;
    gosubstack = gosubstack_main; errorstack = errorstack_main; substack = substack_main; GosubLimit = MAXGOSUB;
    datastore = datastore_main; RestoreLimit = MAXRESTORE;
    g_StrTmp = g_StrTmp_main; g_StrTmpLocalIndex = g_StrTmpLocalIndex_main; TempLimit = MAXTEMPSTRINGS;
    g_hashlist = g_hashlist_main;
#ifdef SUBPROFILE
    g_framebase_stack = g_framebase_stack_main; g_subidx_stack = g_subidx_stack_main;
    if(Thr[0] != NULL) { g_subentry_us = Thr[0]->fbentry; g_subchild_us = Thr[0]->fbchild; }
    g_framebase_sp = 0; g_localframe_base = 0; g_current_sub_idx = -1;
#endif
}

// what the variables' code needs of a thread's state: its locals, its loops (ClearVars trims them), its temporary
// memory, its bindings
static void ContextLoadVars(struct s_thread *t) {
    g_LocalIndex = t->g_LocalIndex;
    g_forstack = t->fors; ForLimit = t->nfor;
    g_forindex = t->g_forindex;
    g_dostack = t->dos; DoLimit = t->ndo;
    g_doindex = t->g_doindex;
    g_hashlist = t->locals;
    g_hashlistpointer = t->g_hashlistpointer;
    g_localtop = t->g_localtop; g_Localvarcnt = t->g_Localvarcnt;
    g_varcnt = g_Globalvarcnt + g_Localvarcnt;
    LocalBase = t->lbase; LocalLimit = t->llimit;
    g_StrTmp = t->tmp; g_StrTmpLocalIndex = t->tmplevel; TempLimit = t->ntemp;
    g_StrTmpIndex = t->g_StrTmpIndex; g_TempMemoryIsChanged = t->g_TempMemoryIsChanged;
    if(SymL != NULL) SymL = t->syml;
    SymTextLocals = t->SymTextLocals;
}

/* The compiler's records (Stream.c) keep the addresses their variables were found at, with a stamp: the sum of the last
   global event's number and the last local event's at the record's level (Symbols.h). A switch must make every stamp
   written before it stale, at every level. One new global event does not: with G moved on, G + L[l] for this thread at
   level l can equal the G + L[m] another thread stamped a record with at level m, when the two levels' own events
   differ by what G moved (it happened: tests/threads/tbind.bas). So every level is given a new event of its own, and
   the globals a newer one: each sum made from here on is greater than any made before, and the levels still differ. */
static void BindsStale(void) {
#ifdef rp2350                                                       // (the compiled route's stamps: the RP2350's)
    int l;
    for(l = 0; l < SYM_LEVELS; l++) SymLevelGen[l] = ++SymBindEvent;
#endif
    SymBindGlobalsChanged();
}

#ifdef SUBPROFILE
/* OPTION PROFILING's frames (MMBasic.c, EnterLocalFrame) are each thread's own, and a SUB's time its own thread's: the
   time the thread was switched out is added to the start of each SUB it is in, so that the threads that ran meanwhile
   are not charged to them (the CMM2's ProfileSwitch does the same for its lines). */
static void ProfileFrames(struct s_thread *t) {
    int i;
    g_framebase_stack = t->fbstack; g_subidx_stack = t->fbsubidx;
    g_subentry_us = t->fbentry; g_subchild_us = t->fbchild;
    g_framebase_sp = t->fbsp; g_localframe_base = t->fbbase; g_current_sub_idx = t->fbcur;
    if(t->fbentry != NULL) {
        uint64_t away = time_us_64() - t->fbout;
        for(i = 0; i < t->fbsp; i++) t->fbentry[i] += away;
    }
}
#endif

static void ContextLoad(struct s_thread *t) {
    ContextLoadVars(t);
    nextstmt = t->nextstmt; cmdline = t->cmdline; CmdTokenPtr = t->CmdTokenPtr; CurrentLinePtr = t->CurrentLinePtr;
    cmdtoken = t->cmdtoken; ExecDepth = t->ExecDepth;
    gosubstack = t->gosub; errorstack = t->errs; substack = t->subs; GosubLimit = t->ngosub;
#ifdef SUBPROFILE
    ProfileFrames(t);
#endif
    gosubindex = t->gosubindex;
    OptionErrorSkip = t->OptionErrorSkip; ErrorSkipDepth = t->ErrorSkipDepth;
    MMerrno = t->MMerrno; MMerrline = t->MMerrline;
    strcpy(MMErrMsg, t->errmsg);
    memcpy(ErrNext, t->ErrNext, sizeof(jmp_buf));
    DefinedSubFunMem = t->DefinedSubFunMem; DefinedSubFunLocalIndex = t->DefinedSubFunLocalIndex;
#ifdef rp2350
    RBSplice = t->splice;
#endif
    PauseInterrupted = t->pauseint; PauseStart = t->pauseuntil;
    WaitResume = t->waitresume; WaitLimit = t->waitlimit;
    ScreenLoad(t);
    g_FunReturnArrayCount = t->g_FunReturnArrayCount;
    datastore = t->data; RestoreLimit = t->ndata;
    NextData = t->NextData; NextDataLine = t->NextDataLine; restorepointer = t->restorepointer;
    StackFloor = t->floor;                                          // (from the record, never saved: PIXEL FILL turns the
    stackwarn = t->floor + 1024;                                    // check off for its length, and an error may leave it so)
    ScrewUpTimer = t->screwup;
    BindsStale();                                                   // a compiled record binds again the first time it runs in
}                                                                   // this thread: no stamp written before the switch matches

// the main program's limit of local slots: the lowest any thread has taken
static void MainLimit(void) {
    int i, lim = MAINLOCALS;
    for(i = 1; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && ALIVE(Thr[i]) && Thr[i]->lbase < lim) lim = Thr[i]->lbase;
    Thr[0]->llimit = lim;
    if(Cur == Thr[0]) LocalLimit = lim;
}

// the last thread has ended and the main program runs on alone: nothing of this is active again
static void ThreadsOff(void) {
    LocalLimit = MAINLOCALS;                                        // (MMErrMsg is an array: it keeps its message)
    FreeMemory(Thr[0]);
    Thr[0] = Cur = NULL;
    Stopping = 0;
    ThreadSliceLeft = 0;
    ThreadWakeMs = 0;
}

// the blocks of the threads that have ended, freed by the thread that runs next (not from their own stacks)
#ifdef THREAD_STACK_REPORT
/* A measuring build's only (Stage P5: cmake ... -DTHREAD_STACK_REPORT): as a thread's stack is freed, the most of it
   the thread used, so that a thread that ends at an error is measured too */
static int StackUsed(struct s_thread *t);
static void StackReport(struct s_thread *t) {
    char b[64];
    sprintf(b, "[stack] thread %d used %d of %d\r\n", (int)t->id, StackUsed(t), t->stacksize);
    MMPrintString(b);
}
#define STACK_REPORT(t) StackReport(t)
#else
#define STACK_REPORT(t)
#endif

static void ThreadReap(void) {
    int i, any = 0;
    for(i = 1; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && Thr[i]->state == T_DONE) {
            if(*(uint32_t *)Thr[i]->stack != STACK_MARK) {          // its stack went past its guard
                int n = i;
                Thr[i]->state = T_FREE;                             // (not reported twice)
                error("Thread % overflowed its stack", n);
            }
            STACK_REPORT(Thr[i]);
            FreeMemory(Thr[i]->stack);
            if(Thr[i]->tknbuf != NULL) FreeMemory(Thr[i]->tknbuf);
            FreeMemory(Thr[i]);
            Thr[i] = NULL;
            any = 1;
        }
    if(!any) return;
    MainLimit();
    if(ThreadCount == 0 && Cur == Thr[0]) ThreadsOff();
}

// what a thread waits for is so: the lock is free, the thread has ended, the flag is set
static int WaitOver(int kind, int arg) {
    if(kind == W_LOCK) return LockOwner[arg] == 0;
    if(kind == W_THREAD) return Thr[arg] == NULL || !ALIVE(Thr[arg]);
    if(kind == W_FRAME) return FrameCount != (uint32_t)arg;
    if(kind == W_NET) return NetBusy[arg] == 0;
    return (int)((g_flag >> arg) & 1);
}

// IntMay held a source back: the next statement tail looks for it again
static void Resignal(void) {
    if(!Deferred) return;
    Deferred = 0;
    IntSignal();
}

// a thread has ended: its locks are free, and its hold is over
static void Disown(int id) {
    int k;
    for(k = 1; k <= MAXLOCKS; k++)
        if(LockOwner[k] == id + 1) LockOwner[k] = 0;
    if(HoldCount && HoldBy == id) HoldCount = 0;
    for(k = 0; k < 2; k++) {                                        // (the network and Bluetooth are free again)
        if(NetOwner[k] == id + 1) NetOwner[k] = 0;
        if(NetBusy[k] == id + 1) NetBusy[k] = 0;
    }
    for(k = 0; k < IntOwners; k++)                                  // (its interrupts are the main program's from now on)
        if(IntOwner[k].id == id) IntOwner[k].id = 0;
    Resignal();
}

// The running thread goes into (1) or comes out of (0) a wait that takes no interrupt: INPUT, a frame wait, SYNC. Into
// it, it looks once for the other threads' interrupts, since no statement tail may run until one of them does (a
// thread in THREAD WAIT FLAG while the main program is in INPUT); out of it, its own that waited are looked for again.
static void ScanForOthers(void);
static void Deaf(int on) {
    Cur->deaf = on;
    if(on) ScanForOthers();
    else Resignal();
}

/* The thread to run next, made ready if it was asleep: a thread another handed an interrupt to; else the
   sleeping thread whose PAUSE ended first, so that a timed thread is on time among busy ones; else the next ready one
   in number order after the one running (which is itself the last tried). With none of those it is the main program,
   asleep or not: its PAUSE loop is the idle loop, as it always was (CheckAbort, the interrupts, the GUI's work). */
static struct s_thread *NextReady(void) {
    struct s_thread *t, *due = NULL;
    long long now = time_us_64();
    int i;
    for(i = 0; i <= MAXTHREADS; i++) {
        t = Thr[i];
        if(t != NULL && t != Cur && t->pending != NULL && ALIVE(t) && !t->paused && !t->deaf) {
            t->state = T_READY;
            return t;
        }
    }
    for(i = 0; i <= MAXTHREADS; i++) {
        t = Thr[i];
        if(t == NULL || t->paused) continue;
        if(t->state == T_WAIT && t->wkind == W_FRAME && FrameCount != (uint32_t)t->warg) {   // its frame has started: first
            t->state = T_READY;
            return t;
        }
        if(!(t->state == T_SLEEP || t->state == T_WAIT) || t->wake > now) continue;
        if(due == NULL || t->wake < due->wake) due = t;
    }
    if(due != NULL) {
        due->state = T_READY;
        return due;
    }
    for(i = 1; i <= MAXTHREADS + 1; i++) {
        t = Thr[(Cur->id + i) % (MAXTHREADS + 1)];
        if(t == NULL || t->paused) continue;
        if(t->state == T_READY) return t;
        if(t->state == T_WAIT && WaitOver(t->wkind, t->warg)) {     // what it waited for is so: its statement goes on
            t->state = T_READY;
            return t;
        }
    }
    Thr[0]->state = T_READY;
    return Thr[0];
}

// the tick is told when the first sleeping thread is due (ThreadWakeMs, in mSecTimer's count), to end the running
// thread's slice then
static void ArmWake(void) {
    long long now, first = 0;
    unsigned int ms;
    int i;
    for(i = 0; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && !Thr[i]->paused && (Thr[i]->state == T_SLEEP || (Thr[i]->state == T_WAIT && Thr[i]->wake != NOLIMIT))
           && (first == 0 || Thr[i]->wake < first)) first = Thr[i]->wake;
    if(first == 0) {
        ThreadWakeMs = 0;
        return;
    }
    now = time_us_64();
    ms = (unsigned int)mSecTimer + (first > now ? (unsigned int)((first - now + 999) / 1000) : 0);
    ThreadWakeMs = ms ? ms : 1;
}

// no switch now: an interrupt routine is running (it is atomic, and its saved state is single), BASIC is
// running from inside a CSUB or a module's call, a loadable program has the machine, the program is ending, or the
// running thread is between THREAD HOLD and THREAD RELEASE
static int Held(void) {
    if(ExecDepth < ThreadHoldDepth) ThreadHoldDepth = INT_MAX;      // (a call that held has ended: Thread.h)
    return Stopping || InterruptReturn != NULL || LoaderActive || ThreadHoldDepth != INT_MAX || HoldCount;
}

/* THREAD PRIORITY (Peter, 25 Nov: "KISS principle is crucial for this"). The threads take turns in number order as
   they always did, and a thread's priority is how many time slices its turn is: 5 beside 1 runs five slices to the
   other's one. The count runs down as it runs, and it has a whole turn again when it next comes round, so no thread
   waits longer than one round. While another thread waits for a lock this one holds, its turn is as long as the
   longest of theirs: a thread of a low priority does not keep one of a high priority waiting at its own pace. */
static int Turn(struct s_thread *t) {
    int i, n = t->prio;
    for(i = 0; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && Thr[i]->state == T_WAIT && Thr[i]->wkind == W_LOCK && Thr[i]->prio > n
           && LockOwner[Thr[i]->warg] == t->id + 1) n = Thr[i]->prio;
    return n;
}

static void ThreadSwitchTo(struct s_thread *to) {
    struct s_thread *from = Cur;
    from->profline = ProfileSwitch(to->profline);                   // (OPTION PROFILING: each line's time its own thread's)
    if(from->stack != NULL && *(uint32_t *)from->stack != STACK_MARK) error("Thread % overflowed its stack", (int)from->id);
    ContextSave(from);
    ContextLoad(to);
    Cur = to;
    ThreadSliceLeft = ThreadSliceLen * Turn(to);                    // a whole turn for it
    ArmWake();
    Resignal();                                                     // (an interrupt held back may be this one's)
    ThreadSwap(&from->sp, to->sp, HWFLOOR(to));                     // back here when 'from' next runs
    ThreadReap();
}

// the running thread has ended: the next one runs, and frees this one's blocks
__attribute__((noreturn)) static void ThreadFinish(void) {
    struct s_thread *old = Cur, *to;
    old->state = T_DONE;
    ThreadCount--;
    Disown(old->id);
    ScrewUpTimer = 0;
    to = NextReady();                                               // (the main program at least)
    ProfileSwitch(to->profline);
    ContextLoad(to);
    Cur = to;
    ThreadSliceLeft = ThreadSliceLen * Turn(to);
    ArmWake();
    ThreadSwap(&old->sp, to->sp, HWFLOOR(to));
    for(;;) {}                                                      // (nothing switches to a thread that has ended)
}

// a new thread's first run, on its own stack: its SUB called as a statement of its own, so that the call is made as
// any other (DefinedSubFun), and END SUB returns to the end of that statement, which ends this run of the interpreter
static void ThreadMain(void) {
    ThreadReap();
    ExecuteProgram(Cur->call);
    ThreadFinish();
}

/*******************************************************************************************
 what the rest of the firmware calls
*******************************************************************************************/
// The statement tail, when INT_TICK_SLICE was set (check_interrupt): the running thread's slice is up, a sleeping
// thread is due, or THREAD YIELD or THREAD PAUSE asked. The next thread that can run does, and this returns when this
// thread's turn comes round again.
void ThreadSchedule(void) {
    struct s_thread *to;
    if(Cur == NULL || Held()) return;
    if(Cur->pending != NULL) return;                                // (the PicoMite's: an interrupt another thread handed
                                                                    // to this one runs first, from this same statement tail;
                                                                    // switching on would put it a whole round late: ttick)
    to = NextReady();
    if(to != Cur) ThreadSwitchTo(to);
}

/* PAUSE's wait (PauseFor, MM_Misc.c): the other threads run until the microsecond clock reads t. Back when this
   thread is next chosen: at t; for the main program also when an interrupt waits for it, or when no thread can run,
   so that its PAUSE loop goes round as it always did. The caller's loop tests the time, and calls again. */
void ThreadSleepUntil(long long t) {
    struct s_thread *to;
    if(Cur == NULL || Held()) return;
    Cur->wake = t;
    Cur->state = T_SLEEP;
    to = NextReady();
    if(to != Cur) ThreadSwitchTo(to);
    Cur->state = T_READY;
}

// the running thread waits: the others run until what it waits for is so, its time is up, or (the main program) an
// interrupt is due or nothing else can run. The caller's loop looks, and calls again.
static void WaitSwitch(int kind, int arg, long long limit) {
    struct s_thread *to;
    Cur->wkind = kind;
    Cur->warg = arg;
    Cur->wake = limit;
    Cur->state = T_WAIT;
    to = NextReady();
    if(to != Cur) ThreadSwitchTo(to);
    Cur->state = T_READY;
}

/* THREAD LOCK and THREAD WAIT: the running thread waits until the thing is so (1), or for us microseconds (0); no
   limit if us is less than 0. A thread takes its interrupts while it waits, as it does in PAUSE: -1 is 'an
   interrupt is due', the command returns at once, the routine runs, and the statement ('token') runs again and goes
   on to the same time. Where no other thread can run (an interrupt routine, a hold, BASIC called from a CSUB) a wait
   could never end, and is an error. */
// PAUSE and THREAD WAIT's loops: what the running thread is doing, for the list after Ctrl-C (0 nothing)
void ThreadDoing(int d) {
    if(Cur != NULL) Cur->doing = d;
}

static int WaitFor(int kind, int arg, long long us, char *token) {
    if(WaitOver(kind, arg)) {
        if(InterruptReturn == NULL) WaitResume = 0;
        return 1;
    }
    if(InterruptReturn != NULL) error("Cannot wait in an interrupt routine");
    if(ExecDepth < ThreadHoldDepth) ThreadHoldDepth = INT_MAX;      // (a call that held has ended: as Held)
    if(HoldCount || ThreadHoldDepth != INT_MAX) error("Cannot wait while the threads are held");
    if(!WaitResume) WaitLimit = us < 0 ? NOLIMIT : time_us_64() + us;
    WaitResume = 0;
    if(Cur != NULL) {                                               // (for the list after Ctrl-C, while this one runs)
        Cur->wkind = kind;
        Cur->warg = arg;
        Cur->doing = 2;
    }
    for(;;) {
        if(WaitOver(kind, arg)) { ThreadDoing(0); return 1; }
        if(time_us_64() >= WaitLimit) { ThreadDoing(0); return 0; }
        CheckAbort();
        if(check_interrupt()) {
            InterruptReturn = token;                                // this statement again after the routine
            WaitResume = 1;
            ThreadDoing(0);
            return -1;
        }
        if(ThreadCount) WaitSwitch(kind, arg, WaitLimit);
    }
}

/* A command's wait loop has found nothing yet (INPUT with no key, ADC START with its samples not all in): the other
   threads have their turns, and this returns when this thread's comes round, to look again. With no other thread
   ready it returns at once. The command takes no interrupt while it waits, as it never did. */
void ThreadPoll(void) {
    struct s_thread *to;
    if(Cur == NULL || Held()) return;
    Deaf(1);
    to = NextReady();
    if(to != Cur) ThreadSwitchTo(to);
    Deaf(0);
}

/* FRAMEBUFFER WAIT, PAGE COPY ...,B and the others that wait for a frame's start. With no thread it is the wait it
   always was, on the flag the LTDC's interrupt sets. With threads the others run meanwhile, and the wait is on that
   interrupt's count of frames: each waiter clears the flag, so of two that wait at once one could miss its frame.
   The interrupt ends the running thread's slice when a thread waits, and the waiter runs first. */
void ThreadFrameWait(void) {
    struct s_thread *to;
    uint32_t f;
#ifdef PICOMITEVGA
    if(Cur == NULL || Held()) {                                     // (the PicoMite's: the loop it always was, DrawInternal.h)
        FrameWaitLoop();
        return;
    }
#else
    if(1) {                                                         // (a panel: no frame interrupt counts its refreshes;
        FrameWaitLoop();                                            // the wait as it was, holding the threads)
        return;
    }
#endif
    f = FrameCount;
    while(FrameCount == f) {
        CheckAbort();
        Deaf(1);
        Cur->wkind = W_FRAME;
        Cur->warg = (int)f;
        Cur->wake = NOLIMIT;
        Cur->state = T_WAIT;
        ThreadFrameWaiters++;
        to = NextReady();
        if(to != Cur) ThreadSwitchTo(to);
        ThreadFrameWaiters--;
        Cur->state = T_READY;
        Deaf(0);
    }
}

// SYNC: the other threads run until the microsecond clock reads t; back then, or sooner (the caller's loop looks)
void ThreadSleepDeaf(long long t) {
    Deaf(1);
    ThreadSleepUntil(t);
    Deaf(0);
}

/* The network (Stage 4b). Peter: "Only one thread should handle web activity. Perhaps we need some sort of lock that
   ensures that. Then yielding will be completely safe"; agreed the same day: the network belongs to one thread. The
   main program's WEB commands before any thread has used the network do not claim it (WEB CONNECT, WEB NTP at the
   start); the first other thread to use it has it until that thread ends, and a WEB command in any other thread,
   the main program's included, is the error "The network belongs to thread n". The interrupt routines its thread
   set up (WEB TCP INTERRUPT, MQTT's, UDP's) run in that thread, so they may use it. Only the one thread is ever in
   the network's code, so its one timer (Timer4) and its clients' one state each are that thread's, and its waits can
   let the others run (NetYield, Src/net/WebMite_cmm2.c). The one overlap, a thread's first WEB command while the
   main program's is under way, waits for it as THREAD LOCK does (0: an interrupt first, the statement runs again).
   With no thread, nothing. Bluetooth (k 1) is as the network (k 0): its scan, its serial connection and its keyboard
   each keep one state, so it belongs to one thread too (BLUETOOTH, cmd_bluetooth_threads). */
int ThreadNetEnter(int k) {
    int me, r;
    if(Cur == NULL) return 1;
    me = Cur->id + 1;
    if(NetOwner[k] && NetOwner[k] != me) error("$ belongs to thread %", NetName[k], NetOwner[k] - 1);
    if(NetBusy[k] && NetBusy[k] != me) {
        if(InterruptReturn != NULL || Held()) error("$ is in use by thread %", NetName[k], NetBusy[k] - 1);
        r = WaitFor(W_NET, k, -1, CmdTokenPtr);
        if(r <= 0) return 0;                                        // (an interrupt first)
        if(NetOwner[k] && NetOwner[k] != me) error("$ belongs to thread %", NetName[k], NetOwner[k] - 1);
    } else if(InterruptReturn == NULL) WaitResume = 0;
    if(me != 1 && NetOwner[k] == 0) NetOwner[k] = me;               // (the first thread other than the main program)
    NetBusy[k] = me;
    return 1;
}

void ThreadNetLeave(int k) {
    if(Cur != NULL && NetBusy[k] == Cur->id + 1) NetBusy[k] = 0;
}

// error(), before it leaves the command: the network's or Bluetooth's command over, if this thread was in one
void ThreadErrorRelease(void) {
    int k;
    if(Cur == NULL) return;
    for(k = 0; k < 2; k++)
        if(NetBusy[k] == Cur->id + 1) NetBusy[k] = 0;
}

/* A command's own wait of ms milliseconds (TEMPR's conversion): the other threads run meanwhile. No interrupt is
   taken, as none was in MM_Delay. With no thread, MM_Delay as it was. */
void ThreadDelay(int ms) {
    long long t;
    if(Cur == NULL || ThreadCount == 0 || Held()) {
        uSec(ms * 1000);
        return;
    }
    t = time_us_64() + (long long)ms * 1000;
    while(time_us_64() < t) {
        CheckAbort();
        ThreadSleepDeaf(t);
    }
}

/* Interrupts and threads (Stage 4b). An interrupt routine runs in the thread that set it up (SETTICK, SETPIN, ON
   KEY and the rest, each through GetIntAddress), as its own context: a SUB with its locals, which runs to its end
   with no switch. One set up by the main program, before the threads, or by a thread that has ended runs in the
   main program, as all of them did until now. The same routine set up by two threads is the last one's.
   IntScan asks IntMay of each source before it takes it: yes for the running thread's own; for another thread's
   only if that thread can take it now (not in a wait that takes none, not paused, none handed to it already) and
   this one can switch to it. Otherwise the source is left as it is and found again (Resignal): no tick, key or edge
   is lost. check_interrupt runs one of the running thread's own (ThreadIntHere), and hands another's to it, which
   then runs (its PAUSE, THREAD WAIT or statement tail takes it). */
static struct s_thread *IntOwnerOf(char *routine) {
    int k;
    for(k = 0; k < IntOwners; k++)
        if(IntOwner[k].routine == routine) {
            struct s_thread *t = Thr[IntOwner[k].id];
            return (t != NULL && ALIVE(t)) ? t : Thr[0];
        }
    return Thr[0];
}

void ThreadIntOwn(char *routine) {
    int k;
    if(Cur == NULL || routine == NULL) return;
    for(k = 0; k < IntOwners; k++)
        if(IntOwner[k].routine == routine) break;
    if(k == IntOwners) {
        if(Cur->id == 0) return;                                    // (not listed: the main program's)
        if(IntOwners == MAXINTOWN) error("Too many interrupt routines set up in threads");
        IntOwners++;
        IntOwner[k].routine = routine;
    }
    IntOwner[k].id = Cur->id;
}

int IntMay(char *routine) {
    struct s_thread *o;
    if(Cur == NULL) return 1;
    o = IntOwnerOf(routine);
    if(o == Cur && !Cur->deaf) return 1;
    if(!o->paused && !o->deaf && o->pending == NULL && !Held()) return 1;
    Deferred = 1;
    return 0;
}

// Deaf(1): one interrupt found due for another thread is handed to it (IntMay: it can take it now), and the scheduler
// runs it first (NextReady). This thread's own are held back: it takes none in this wait.
static void ScanForOthers(void) {
    char *r;
    struct s_thread *o;
    if(!IntReady.any || Held()) return;
    if((r = IntScan()) == NULL) return;                             // (MM_Misc.c: found and taken, not run)
    o = IntOwnerOf(r);
    o->pending = r;
    if(o->state == T_SLEEP || o->state == T_WAIT) o->state = T_READY;
}

int ThreadIntHere(char *routine) {
    struct s_thread *o;
    if(Cur == NULL) return 1;
    o = IntOwnerOf(routine);
    if(o == Cur) return 1;
    o->pending = routine;                                           // (IntMay: it can take it, and this can switch)
    o->state = T_READY;                                             // (out of its PAUSE or wait, which looks again)
    ThreadSwitchTo(o);
    return 0;
}

char *ThreadIntTake(void) {
    char *r;
    if(Cur == NULL || Cur->pending == NULL) return NULL;
    r = Cur->pending;
    Cur->pending = NULL;
    return r;
}

int ThreadIntWaiting(void) {
    return Cur != NULL && Cur->pending != NULL;
}

// MM.INFO(THREAD n): 0 not running, 1 running or ready to, 2 in PAUSE, 3 paused by THREAD PAUSE, 4 waiting for a lock,
// a thread or a flag
int ThreadState(int n) {
    if(n == 0) return 1;
    if(Thr[n] == NULL || !ALIVE(Thr[n])) return 0;
    return Thr[n]->paused ? 3 : (Thr[n]->state == T_SLEEP ? 2 : (Thr[n]->state == T_WAIT ? 4 : 1));
}

int ThreadLockOwner(int k) {
    return (int)LockOwner[k] - 1;
}

// the number of the program line p is in: 0 if it is not in the program
static int LineOf(char *p) {                                        // (the PicoMite's: as an error's line)
    if(p == NULL || (unsigned char *)p < ProgMemory || (unsigned char *)p >= ProgMemory + MAX_PROG_SIZE) return 0;
    return CountLines((unsigned char *)p) + 1;                      // (counted from 0, as error() shows it + 1)
}

// the list's text: printed, or kept at ListAt (to ListEnd) for the prompt's landing to print after the error's message
static char *ListBuf = NULL, *ListAt, *ListEnd;
static void ListText(const char *s) {
    if(ListBuf == NULL) {
        MMPrintString((char *)s);
        return;
    }
    while(*s && ListAt < ListEnd) *ListAt++ = *s++;
    *ListAt = 0;
}

static void ListNumber(const char *before, int n) {
    char b[16];
    ListText(before);
    IntToStr(b, n, 10);
    ListText(b);
}

// the most of its stack a thread has used: from the top down to the deepest word its paint no longer holds
static int StackUsed(struct s_thread *t) {
    uint32_t *p;
    for(p = (uint32_t *)t->stack + 1; (char *)p < t->stack + t->stacksize && *p == STACK_PAINT; p++) {}
    return (int)(t->stack + t->stacksize - (char *)p);
}

// a thread's stack for THREAD LIST: the most it has used, and its size
static void ListStack(struct s_thread *t) {
    if(t->stack == NULL) return;                                    // (the main program's, the main stack: Stage P5)
    ListNumber(", stack used at most ", StackUsed(t));
    ListNumber(" of ", t->stacksize);
}

// THREAD LIST, and the prompt's landing after an error or Ctrl-C: each thread, the line it is at and what it is doing
// (and for THREAD LIST its stack: withstack)
static void ThreadList(int withstack) {
    struct s_thread *t;
    int i, line;
    if(Cur == NULL) {
        ListText("No threads\r\n");
        return;
    }
    for(i = 0; i <= MAXTHREADS; i++) {
        t = Thr[i];
        if(t == NULL || !ALIVE(t)) continue;
        if(i) ListNumber("Thread ", i); else ListText("Main program");
        line = LineOf(t == Cur ? (char *)CurrentLinePtr : t->CurrentLinePtr);
        if(line) ListNumber(": line ", line); else ListText(":");
        int sleeping = t->state == T_SLEEP || (t == Cur && t->doing == 1);
        int waiting = t->state == T_WAIT || (t == Cur && t->doing == 2);
        if(t->paused) ListText(", paused");
        else if(sleeping) ListText(", in PAUSE");
        else if(waiting && t->wkind == W_LOCK) {
            ListNumber(", waiting for lock ", t->warg);
            if(LockOwner[t->warg] == 1) ListText(" (the main program has it)");
            else if(LockOwner[t->warg]) { ListNumber(" (thread ", LockOwner[t->warg] - 1); ListText(" has it)"); }
        } else if(waiting && t->wkind == W_THREAD) ListNumber(", waiting for thread ", t->warg);
        else if(waiting && t->wkind == W_FRAME) ListText(", waiting for the next frame");
        else if(waiting && t->wkind == W_NET) {
            ListText(t->warg ? ", waiting for Bluetooth" : ", waiting for the network");
            ListNumber(" (thread ", NetBusy[t->warg] - 1);
            ListText(" has it)");
        }
        else if(waiting) ListNumber(", waiting for flag ", t->warg);
        else if(t == Cur) ListText(", running");
        else ListText(", ready");
        if(t->prio != 1 || Turn(t) != 1) {                           // (a program that sets none reads as it did)
            ListNumber(", priority ", t->prio);
            if(Turn(t) != t->prio) { ListNumber(" (", Turn(t)); ListText(" while its lock is waited for)"); }
        }
        if(withstack) ListStack(t);
        ListText("\r\n");
    }
}

/* The prompt's landing (main.c), first thing: a program with threads has stopped at an error or Ctrl-C (not at END,
   which stops the threads itself). Where each thread was is written down before ThreadsEnd frees the records, and
   printed by ThreadsStoppedShow after the error's own message. With no memory for it there is no list. */
void ThreadsStopped(void) {
    if(ListBuf != NULL) FreeMemory(ListBuf);
    ListBuf = NULL;
    if(Cur == NULL || ThreadCount == 0 || Stopping) return;
    if((ListBuf = GetMemoryNull(1536)) == NULL) return;
    ListAt = ListBuf;
    ListEnd = ListBuf + 1535;
    ThreadList(0);
}

// MEMORY: the threads' stacks and records, in K, and how many there are (0: none)
int ThreadMemory(int *kb) {
    int i, n = 0, b = 0;
    *kb = 0;
    if(Cur == NULL) return 0;
    for(i = 1; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL && ALIVE(Thr[i])) {
            n++;
            b += MemSize(Thr[i]) + MemSize(Thr[i]->stack) + (Thr[i]->tknbuf != NULL ? MemSize(Thr[i]->tknbuf) : 0);
        }
    b += MemSize(Thr[0]);
    *kb = (b + 512) / 1024;
    return n;
}

void ThreadsStoppedShow(void) {
    char *b = ListBuf;
    if(b == NULL) return;
    ListBuf = NULL;
    MMPrintString(b);
    FreeMemory(b);
}

int ThreadNumber(void) {
    return Cur == NULL ? 0 : Cur->id;
}

void ThreadMainOnly(void) {
    if(Cur != NULL && Cur->id != 0) error("Not allowed in a thread");
}

void ThreadsNone(void) {
    if(ThreadCount) error("Not allowed while a thread is running");
}

void ThreadsStop(void) {
    Stopping = 1;
}

char *ThreadTokenBuffer(void) {
    if(Cur == NULL || Cur->id == 0) return tknbuf;
    if(Cur->tknbuf == NULL) Cur->tknbuf = GetMemory(TKNBUFSIZE);    // (its own, from its first EXECUTE to its end)
    return Cur->tknbuf;
}

void ThreadStackLimits(void) {
    if(Cur == NULL) return;
    StackFloor = Cur->floor;
    stackwarn = Cur->floor + 1024;
}

static int OnMainStack(void) {
    uint32_t c;
    __asm volatile("MRS %0, control" : "=r"(c));
    return !(c & 2);                                                // (SPSEL clear: a thread's is the process stack)
}

/* The program has stopped (an error, END, Ctrl-C: the prompt's landing, main.c) or is being replaced (RUN, CHAIN:
   ClearRuntime): every thread ends. Each one's locals and temporary memory are freed through the interpreter's own
   routines, with its state in the globals for the moment, then the blocks go. What is left is a program with no
   threads, no locals and empty stacks; the statement being run (nextstmt and the rest) is not touched. Called on the
   main stack only: a thread cannot free the stack it is running on. 1 if there were threads. */
int ThreadsEnd(void) {
    int i, eno = MMerrno, eline = MMerrline;
    StackSize = THREAD_STACK;                                       // (THREAD SIZE and SLICE are the program's)
    LocalSlots = THREAD_LOCALS;
    NestDepth = THREAD_NEST;
    MainPriority = 1;
    ThreadSliceLen = 1;
    ThreadHoldDepth = INT_MAX;
    memset(LockOwner, 0, sizeof(LockOwner));                        // (the locks, the hold and a wait are the program's too)
    HoldCount = 0;
    memset(NetOwner, 0, sizeof(NetOwner));
    memset(NetBusy, 0, sizeof(NetBusy));
    WaitResume = 0;
    IntOwners = 0;
    Deferred = 0;
    ThreadFrameWaiters = 0;
    ThreadFrameDue = 0;
    if(Cur == NULL) return 0;
    if(!OnMainStack()) error("Not allowed in a thread");
    ContextSave(Cur);
    if(Cur != Thr[0]) ScreenLoad(Thr[0]);                           // (the main program's print position, colours and font)
    for(i = 0; i <= MAXTHREADS; i++) {
        if(Thr[i] == NULL) continue;
        if(ALIVE(Thr[i])) {
            ContextLoadVars(Thr[i]);
            ClearVars(1, true);
            g_LocalIndex = 0;
            ClearTempMemory();
        }
    }
    MainStacks();
    LocalBase = 0; LocalLimit = MAINLOCALS;
    g_localtop = 0; g_hashlistpointer = 0; g_Localvarcnt = 0;
    g_StrTmpIndex = 0; restorepointer = 0;
    g_varcnt = g_Globalvarcnt;
    g_LocalIndex = 0; gosubindex = g_forindex = g_doindex = 0;
    if(SymL != NULL) SymL = Thr[0]->syml;
    SymTextLocals = 0;
    StackFloor = Thr[0]->floor;
    stackwarn = Thr[0]->floor + 1024;
    MMerrno = eno; MMerrline = eline;                               // (MMErrMsg, an array, keeps its message)
    for(i = 1; i <= MAXTHREADS; i++)
        if(Thr[i] != NULL) {
            STACK_REPORT(Thr[i]);
            FreeMemory(Thr[i]->stack);
            if(Thr[i]->tknbuf != NULL) FreeMemory(Thr[i]->tknbuf);
            FreeMemory(Thr[i]);
            Thr[i] = NULL;
        }
    FreeMemory(Thr[0]);
    Thr[0] = Cur = NULL;
    ThreadCount = 0;
    Stopping = 0;
    ThreadSliceLeft = 0;
    ThreadWakeMs = 0;
    return 1;
}

/*******************************************************************************************
 THREAD
*******************************************************************************************/
#define PUT(c) do { if(o >= end) error("Arguments too long for a thread"); *o++ = (c); } while(0)
/* The statement a new thread starts with, written at out (CALLMAX bytes): its SUB's name and its arguments. Each
   argument of THREAD START (argv[4], argv[6]...) is evaluated here, in the thread that starts the new one, and kept in
   ta[]. In the call it is written MM.INFO(THREAD ARG n), which gives the new thread that value back, exactly, when
   its call is made: the call takes it as any call takes an expression, by value, and DefinedSubFun is as it was. (The
   values cannot be written into the call as constants: a call is read with a program line's room, 255 characters,
   and one string can be that long.) A whole array (a()) is written as its name: the new thread finds the same global
   by it, so a global it must be. The call's length; *nta the number of values kept. */
static int CallText(char *out, struct s_targ *ta, int *nta, char *sub, int len, int argc, unsigned char **argv) {
    char *o = out, *end = out + CALLMAX - 4, *a, *b, nb[12];
    unsigned char *s;
    const char *w;
    MMFLOAT f;
    long long int i64;
    int i, j, t, n = 0, info = GetTokenValue("MM.Info(");
    for(j = 0; j < len; j++) PUT(sub[j]);
    for(i = 4; i < argc; i += 2) {
        PUT(i == 4 ? ' ' : ',');
        a = argv[i];
        skipspace(a);
        if(*a == 0) continue;                                       // (left out: the SUB's own default)
        if(isnamestart(*a) && *skipvar(a, false) == 0 && (b = strchr(a, '(')) != NULL && FindSubFun(a, 1) < 0) {
            b++;
            skipspace(b);
            if(*b == ')') {                                         // a whole array
                findvar(a, V_FIND | V_EMPTY_OK | V_NOFIND_ERR);
                if(VREC(g_VarIndex)->level != 0 || (VREC(g_VarIndex)->type & T_PTR))
                    error("Only a global array can be passed to a thread");
                while(*a) PUT(*a++);
                continue;
            }
        }
        if(n == MAXTARGS) error("Too many arguments for a thread");
        t = T_NOTYPE;
        s = NULL;
        evaluate(a, &f, &i64, &s, &t, false);
        if(t & T_INT) {
            ta[n].type = T_INT;
            ta[n].v.i = i64;
        } else if(t & T_NBR) {
            ta[n].type = T_NBR;
            ta[n].v.f = f;
        } else if(t & T_STR) {
            ta[n].type = T_STR;
            ta[n].v.s = GetTempMemory(STRINGSIZE);                  // (evaluate's own place is the next argument's too)
            Mstrcpy(ta[n].v.s, s);
        } else
            error("Invalid argument for a thread");
        n++;
        PUT(info);
        for(w = "THREAD ARG "; *w; w++) PUT(*w);
        IntToStr(nb, n, 10);
        for(j = 0; nb[j]; j++) PUT(nb[j]);
        PUT(')');
    }
    *nta = n;
    return o - out;
}

// MM.INFO(THREAD ARG n): argument n of the THREAD START that started the running thread (its type; a string copied to s)
int ThreadArg(int n, long long int *i, MMFLOAT *f, char *s) {
    struct s_targ *a;
    if(Cur == NULL || n < 1 || n > Cur->nargs) error("Thread argument % does not exist", n);
    a = &Cur->args[n - 1];
    if(a->type == T_INT) *i = a->v.i;
    else if(a->type == T_NBR) *f = a->v.f;
    else Mstrcpy(s, a->v.s);
    return a->type;
}

// THREAD START n, sub [, argument ...]
static void ThreadStart(int n, int StackSize, int LocalSlots, int Nest, int Priority, char *name, int argc, unsigned char **argv) {
    struct s_thread *t;
    char *q;
    int nfor = Nest, ndo = Nest, ngosub = Nest + 1, ntemp = 64 + 16 * Nest, ndata = Nest < MAXRESTORE ? Nest : MAXRESTORE;
    struct s_targ ta[MAXTARGS];
    char *stack, sub[MAXVARLEN + 1], call[CALLMAX], *strs;
    int i, len = 0, base, maintop, bindings, calllen, nta, strbytes = 0;
    uint32_t *sp;
    if(CurrentLinePtr == NULL) error("Invalid at the command prompt");
    skipspace(name);
    if(!isnamestart(*name)) error("Expected the name of a SUB");
    while(isnamechar(name[len])) {
        if(len == MAXVARLEN) error("Variable name too long");
        sub[len] = name[len];
        len++;
    }
    sub[len] = 0;
    checkend(name + len);
    if(Thr[n] != NULL && ALIVE(Thr[n])) error("Thread % is running", n);
    if((i = FindSubFun(sub, 0)) < 0 || commandtbl_at(subfun[i]) != cmdSUB) {
        for(i = 0; sub[i]; i++) sub[i] = mytoupper(sub[i]);           // (the PicoMite keeps a name as typed; its
        error("$ is not a SUB", sub);                               // messages give it in capitals)
    }
    calllen = CallText(call, ta, &nta, sub, len, argc, argv);       // (an argument may call a FUNCTION, in which the
    for(i = 0; i < nta; i++)                                        // threads take their turns: the checks again after it)
        if(ta[i].type == T_STR) strbytes += (unsigned char)*ta[i].v.s + 1;
    if(Thr[n] != NULL && ALIVE(Thr[n])) error("Thread % is running", n);

    if(Cur != NULL) ThreadReap();                                   // (a number that has just ended is free to use again;
                                                                    // with the last of them gone, Cur is NULL after it)
    if(Cur == NULL) {                                               // the first: the main program's record
        if((t = GetSRAMMemoryNull(sizeof(struct s_thread) + MAXINTOWN * sizeof(struct s_intown))) == NULL)
            error("Not enough memory for a thread");
        IntOwner = (struct s_intown *)(t + 1);                      // (in the heap, not static RAM: the PicoMite's)
        IntOwners = 0;
        t->locals = g_hashlist_main; t->nlocals = MAXLOCALLIST;     // (its stacks: the interpreter's own arrays)
        t->fors = g_forstack_main; t->nfor = MAXFORLOOPS;
        t->dos = g_dostack_main; t->ndo = MAXDOLOOPS;
        t->gosub = gosubstack_main; t->errs = errorstack_main; t->subs = substack_main; t->ngosub = MAXGOSUB;
        t->tmp = g_StrTmp_main; t->tmplevel = g_StrTmpLocalIndex_main; t->ntemp = MAXTEMPSTRINGS;
        t->data = datastore_main; t->ndata = MAXRESTORE;
#ifdef SUBPROFILE
        t->fbstack = g_framebase_stack_main; t->fbsubidx = g_subidx_stack_main;
        t->fbentry = g_subentry_us; t->fbchild = g_subchild_us;
#endif
        t->prio = MainPriority;
        t->state = T_READY;
        t->id = 0;
        t->floor = StackFloor;
        t->llimit = MAINLOCALS;
        t->syml = SymL;
        Thr[0] = Cur = t;
        Stopping = 0;
    }

    // its local slots: the highest range free, above everything the main program has in use. Packed (the PicoMite's,
    // Peter 26 Nov: "drop the rounding"; the CMM2 starts each range on a multiple of 16): below a range it meets
    maintop = (Cur == Thr[0]) ? g_localtop : Thr[0]->g_localtop;
    base = MAINLOCALS - LocalSlots;
    while(base >= maintop) {
        for(i = 1; i <= MAXTHREADS; i++)
            if(Thr[i] != NULL && base < Thr[i]->llimit && base + LocalSlots > Thr[i]->lbase) break;
        if(i > MAXTHREADS) break;
        base = Thr[i]->lbase - LocalSlots;                          // (the next try: just below the range it met)
    }
    if(base < maintop) {
        if(ThreadCount == 0) ThreadsOff();
        error("Not enough local variables for a thread");
    }

    // its stack and its record, with its names' bindings after it
    bindings = SymL != NULL ? SymCanonCount * sizeof(int16_t) : 0;
#ifdef SUBPROFILE
    int fbn = g_perf_subtime_us != NULL ? ngosub : 0;               // (OPTION PROFILING's frames' times: only while it is on)
#define FRAMEBYTES (2 * fbn * sizeof(uint64_t) + 2 * ngosub * sizeof(int))
#else
#define FRAMEBYTES 0
#endif
    stack = GetSRAMMemoryNull(StackSize);                           // (SRAM: PSRAM cannot be read while the flash is written)
    /* After the record, in the order of their alignment: its arguments (8-byte values), its FOR and DO stacks, its DATA
       saves, its GOSUB stack's three lists, its temporary memory's list, its locals' list, its names' bindings, the
       temporary memory's levels, its call and its arguments' strings. NEST gives the stacks' entries: a FOR, a DO and
       a SUB, FUNCTION or GOSUB each that deep (one more call for an interrupt routine), 64 + 16 a level of temporary
       memory, and as many DATA saves. */
    t = stack != NULL ? GetSRAMMemoryNull(sizeof(struct s_thread) + nta * sizeof(struct s_targ)
                                      + (nfor + 1) * sizeof(struct s_forstack) + ndo * sizeof(struct s_dostack)
                                      + ndata * sizeof(struct sa_data) + 3 * ngosub * sizeof(char *) + ntemp * sizeof(char *)
                                      + LocalSlots * sizeof(struct s_hash) + bindings + ntemp + calllen + 4 + strbytes
                                      + FRAMEBYTES) : NULL;
    if(t == NULL) {
        if(stack != NULL) FreeMemory(stack);
        if(ThreadCount == 0) ThreadsOff();
        error("Not enough memory for a thread");
    }
    t->state = T_READY;
    t->id = n;
    t->stack = stack;
    t->stacksize = StackSize;
    *(uint32_t *)stack = STACK_MARK;
    for(sp = (uint32_t *)stack + 1; (char *)sp < stack + StackSize - 128; sp++) *sp = STACK_PAINT;   // (THREAD LIST's depth)
    t->floor = (uint32_t)stack + THREAD_GUARD;
    t->lbase = t->g_localtop = base;
    t->llimit = base + LocalSlots;
#ifdef SUBPROFILE
    t->fbentry = fbn ? (uint64_t *)((struct s_targ *)(t + 1) + nta) : NULL;
    t->fbchild = fbn ? t->fbentry + fbn : NULL;
    t->fors = (struct s_forstack *)((uint64_t *)((struct s_targ *)(t + 1) + nta) + 2 * fbn); t->nfor = nfor;
#else
    t->fors = (struct s_forstack *)((struct s_targ *)(t + 1) + nta); t->nfor = nfor;
#endif
    t->dos = (struct s_dostack *)(t->fors + nfor + 1); t->ndo = ndo;
    t->data = (struct sa_data *)(t->dos + ndo); t->ndata = ndata;
    t->gosub = (unsigned char **)(t->data + ndata); t->errs = t->gosub + ngosub; t->subs = t->errs + ngosub; t->ngosub = ngosub;
#ifdef SUBPROFILE
    t->fbstack = (int *)(t->subs + ngosub); t->fbsubidx = t->fbstack + ngosub;
    t->fbcur = -1;                                                  // (in no SUB: GetMemory zeroed the rest)
    t->tmp = (char **)(t->fbsubidx + ngosub); t->ntemp = ntemp;
#else
    t->tmp = (char **)(t->subs + ngosub); t->ntemp = ntemp;
#endif
    t->locals = (struct s_hash *)(t->tmp + ntemp); t->nlocals = LocalSlots;
    t->prio = Priority;
    q = (char *)(t->locals + LocalSlots);
    if(bindings) {
        t->syml = (int16_t *)q;
        memset(t->syml, 0xFF, bindings);                            // (-1: no name has a local)
    }
    t->tmplevel = (char *)(q + bindings);
    t->CurrentLinePtr = CurrentLinePtr;                             // its SUB's call is reported at this line
    t->NextDataLine = (char *)ProgMemory;
    ScreenSave(t);                                                  // (the print position, colours and font of its starter)
    t->args = (struct s_targ *)(t + 1);
    t->nargs = nta;
    t->call = (char *)t->tmplevel + ntemp;
    memcpy(t->call, call, calllen);                                 // (the zeros after it end the statement and the "program")
    strs = t->call + calllen + 4;
    for(i = 0; i < nta; i++) {
        t->args[i] = ta[i];
        if(ta[i].type != T_STR) continue;
        Mstrcpy(strs, ta[i].v.s);
        t->args[i].v.s = strs;
        strs += (unsigned char)*strs + 1;
    }

    // its stack as ThreadSwap leaves one: d8-d15, r4-r11, then where to go on, ThreadMain, with the stack 8-byte aligned
    // there as a function's is on entry
    sp = (uint32_t *)(((uint32_t)stack + StackSize) & ~7u) - SWAP_WORDS;
    memset(sp, 0, (SWAP_WORDS - 1) * sizeof(uint32_t));
    sp[SWAP_WORDS - 1] = (uint32_t)ThreadMain;
    t->sp = sp;

    Thr[n] = t;
    ThreadCount++;
    MainLimit();
    if(ThreadSliceLeft == 0) ThreadSliceLeft = ThreadSliceLen * Turn(Cur);   // the tick counts the slices from now
}

// the statement tail is to call ThreadSchedule (check_interrupt): THREAD YIELD, and a thread that has paused itself
static void AskSwitch(void) {
    uint32_t s = save_and_disable_interrupts();
    IntReady.tick |= INT_TICK_SLICE;
    restore_interrupts(s);
}

// the running thread ends here, whatever SUBs and FUNCTIONs it is in: its locals at every level, its loops and its
// temporary memory go, and the next thread runs
__attribute__((noreturn)) static void ThreadEndNow(void) {
    ClearVars(1, true);
    g_LocalIndex = 0;
    gosubindex = g_forindex = g_doindex = 0;
    ClearTempMemory();
    ThreadFinish();
}

// THREAD STOP n: thread n ends where it stands. What it held is freed through the interpreter's own routines, with
// its state in the globals for the moment (as ThreadsEnd), and its blocks go: this is not its stack.
static void ThreadStop(int n) {
    struct s_thread *t = Thr[n];
    if(t == NULL || !ALIVE(t)) return;                              // (not running: nothing to stop)
    if(t == Cur) ThreadEndNow();
    ContextSave(Cur);
    ContextLoadVars(t);
    ClearVars(1, true);
    g_LocalIndex = 0;
    ClearTempMemory();
    ContextLoad(Cur);
    if(t->state == T_WAIT && t->wkind == W_FRAME) ThreadFrameWaiters--;
    t->state = T_DONE;
    ThreadCount--;
    Disown(n);
    ThreadReap();
}

// the thread THREAD PAUSE and RESUME name: it must be running
static struct s_thread *Named(char *p) {
    int n = getint(p, 1, MAXTHREADS);
    if(Thr[n] == NULL || !ALIVE(Thr[n])) error("Thread % is not running", n);
    return Thr[n];
}

/* THREAD START's first argument: the thread's number, then SIZE kb (its stack), LOCALS m (its local slots) and NEST d
   (how deep its FOR loops, its DO loops and its calls may each go), in any order, each once, for this thread only;
   THREAD SIZE's otherwise. The same limits as THREAD SIZE's. */
static int OptionValue(char **pp, int lo, int hi) {
    MMFLOAT f;
    long long int i;
    unsigned char *s;
    int ty = T_NBR | T_INT;
    *pp = evaluate(*pp, &f, &i, &s, &ty, true);                    // (true: it may be followed by SIZE or LOCALS)
    if(ty & T_NBR) i = FloatToInt64(f);
    else if(!(ty & T_INT)) error("Expected a number");
    if(i < lo || i > hi) error("% is invalid (valid is % to %)", (int)i, lo, hi);
    return (int)i;
}

static char *StartOptions(char *p, int *n, int *size, int *slots, int *nest, int *prio) {
    char *q;
    int gotsize = 0, gotslots = 0, gotnest = 0, gotprio = 0;
    *n = OptionValue(&p, 1, MAXTHREADS);
    for(;;) {
        skipspace(p);
        if(*p == 0) return p;
        if(!gotsize && (q = checkstring(p, "SIZE")) != NULL) {
            p = q;
            *size = OptionValue(&p, THREAD_SIZE_MIN, 512) * 1024;     // (the guard and room to run in)
            gotsize = 1;
        } else if(!gotslots && (q = checkstring(p, "LOCALS")) != NULL) {
            p = q;
            *slots = OptionValue(&p, 8, MAINLOCALS / 2) + 1;          // (+1: findvar keeps one slot free: m locals)
            gotslots = 1;
        } else if(!gotnest && (q = checkstring(p, "NEST")) != NULL) {
            p = q;
            *nest = OptionValue(&p, 4, MAXGOSUB);
            gotnest = 1;
        } else if(!gotprio && (q = checkstring(p, "PRIORITY")) != NULL) {
            p = q;
            *prio = OptionValue(&p, 1, THREAD_PRIORITY_MAX);
            gotprio = 1;
        } else
            error("Syntax");
    }
}

// The PicoMite's makeargs makes no empty argument before a leading separator (THREAD SIZE , 48 gives "," then "48"),
// where the CMM2's does: it is put back, so that the CMM2's tests of argc and of *argv[0] hold (argv has room: max)
static void LeadingEmpty(unsigned char **argv, int *n, int max) {
    int i;
    if(*n == 0 || *n >= max || argv[0][0] != ',' || argv[0][1] != 0) return;
    for(i = *n; i > 0; i--) argv[i] = argv[i - 1];
    argv[0] = (unsigned char *)"";
    (*n)++;
}

// THREAD LOCK's and THREAD WAIT's time limit, in milliseconds: as microseconds, -1 for none
static long long TimeLimit(int argc, unsigned char **argv) {
    MMFLOAT ms;
    if(argc != 3) return -1;
    ms = getnumber(argv[2]);
    if(ms < 0) error("Number out of bounds");
    return (long long)(ms * 1000);
}

void cmd_thread(void) {
    unsigned char *p, *q, *token = CmdTokenPtr;                     // (this statement, to run again after an interrupt)
    int k, me = ThreadNumber();
    if((p = checkstring(cmdline, "YIELD")) != NULL) {               // (first: the one a loop is made of)
        checkend(p);
        if(InterruptReturn != NULL) error("Invalid in an interrupt routine");
        if(ThreadCount) AskSwitch();                                // the statement tail makes the switch (ThreadSchedule)
    } else if((p = checkstring(cmdline, "START")) != NULL) {
        int n, size = StackSize, slots = LocalSlots, nest = NestDepth, prio = 1;
        getargs(&p, MAX_ARG_COUNT * 2 - 1, ",");
        if(argc < 3 || !(argc & 1)) error("Syntax");
        q = StartOptions(argv[0], &n, &size, &slots, &nest, &prio);
        ThreadStart(n, size, slots, nest, prio, argv[2], argc, argv);
    } else if((p = checkstring(cmdline, "LOCK")) != NULL) {
        getargs(&p, 3, ",");
        if(!(argc == 1 || argc == 3)) error("Syntax");
        k = getint(argv[0], 1, MAXLOCKS);
        if(LockOwner[k] == me + 1) error("Lock % is already held by this thread", k);
        switch(WaitFor(W_LOCK, k, TimeLimit(argc, argv), token)) {
            case 1: LockOwner[k] = me + 1; break;                   // (no statement ends between the test and this)
            case 0: error("Lock % timed out", k);
        }
    } else if((p = checkstring(cmdline, "UNLOCK")) != NULL) {
        k = getint(p, 1, MAXLOCKS);
        if(LockOwner[k] != me + 1) error("Lock % is not held by this thread", k);
        LockOwner[k] = 0;
    } else if((p = checkstring(cmdline, "HOLD")) != NULL) {
        checkend(p);
        HoldCount++;                                                // (no other thread runs from here: Held)
        HoldBy = me;
    } else if((p = checkstring(cmdline, "RELEASE")) != NULL) {
        checkend(p);
        if(HoldCount == 0) error("The threads are not held");
        HoldCount--;
        Resignal();
    } else if((p = checkstring(cmdline, "WAIT")) != NULL) {
        int kind = W_THREAD;
        if((q = checkstring(p, "FLAG")) != NULL) {
            kind = W_FLAG;
            p = q;
        }
        getargs(&p, 3, ",");
        if(!(argc == 1 || argc == 3)) error("Syntax");
        k = (kind == W_FLAG) ? getint(argv[0], 0, 63) : getint(argv[0], 1, MAXTHREADS);
        if(kind == W_THREAD && k == me) error("A thread cannot wait for itself");
        /* Inside THREAD HOLD the hold is let go while the thread waits, and taken again when the wait ends, as a
           condition variable's wait lets go of its mutex: the test that found the queue empty, the flag's clearing and
           the start of the wait are one step, and the statements after the wait run held again (a Back Shed user's
           request, 26 Nov 2026; the CMM2's a2db2ec). Only THREAD HOLD's own hold: one made by C code that runs BASIC
           (ThreadHoldDepth) still refuses the wait, and so does an interrupt routine. A wait that ends at once keeps the
           hold throughout. On an interrupt for this thread WaitFor returns with the statement to run again after the
           routine: the hold is taken again here first, so the routine runs held, and the statement lets it go again. */
        int held = 0;
        if(HoldCount && ThreadHoldDepth == INT_MAX && InterruptReturn == NULL && !WaitOver(kind, k)) {
            held = HoldCount;
            HoldCount = 0;
            Resignal();                                             // (the other threads' interrupts held back)
        }
        WaitFor(kind, k, TimeLimit(argc, argv), token);             // (at its time limit the program looks for itself)
        if(held) {
            HoldCount = held;
            HoldBy = me;
        }
    } else if((p = checkstring(cmdline, "LIST")) != NULL) {
        checkend(p);
        ThreadList(1);
    } else if((p = checkstring(cmdline, "EXIT")) != NULL) {
        checkend(p);
        if(Cur == NULL || Cur->id == 0) error("Not in a thread");
        ThreadEndNow();
    } else if((p = checkstring(cmdline, "STOP")) != NULL) {
        ThreadStop(getint(p, 1, MAXTHREADS));
    } else if((p = checkstring(cmdline, "PAUSE")) != NULL) {
        struct s_thread *t = Named(p);
        t->paused = 1;
        if(t == Cur) AskSwitch();                                   // (it stops at the end of this statement)
    } else if((p = checkstring(cmdline, "RESUME")) != NULL) {
        Named(p)->paused = 0;
        Resignal();                                                 // (its interrupts held back)
    } else if((p = checkstring(cmdline, "PRIORITY")) != NULL) {   // of a running thread, or the main program (0): from
        int pr;                                                     // its next turn
        getargs(&p, 3, ",");
        if(argc != 3) error("Syntax");
        k = getint(argv[0], 0, MAXTHREADS);
        pr = getint(argv[2], 1, THREAD_PRIORITY_MAX);
        if(k == 0) {
            MainPriority = pr;
            if(Thr[0] != NULL) Thr[0]->prio = pr;
        } else {
            if(Thr[k] == NULL || !ALIVE(Thr[k])) error("Thread % is not running", k);
            Thr[k]->prio = pr;
        }
    } else if((p = checkstring(cmdline, "SLICE")) != NULL) {
        ThreadSliceLen = getint(p, 1, 100);
        if(ThreadSliceLeft) ThreadSliceLeft = ThreadSliceLen * Turn(Cur);
    } else if((p = checkstring(cmdline, "SIZE")) != NULL) {
        getargs(&p, 5, ",");
        LeadingEmpty(argv, &argc, 5);
        if(!(argc == 1 || argc == 3 || argc == 5)) error("Syntax");
        if(*argv[0]) StackSize = getint(argv[0], THREAD_SIZE_MIN, 512) * 1024;   // (the guard and room to run in)
        if(argc >= 3 && *argv[2]) LocalSlots = getint(argv[2], 8, MAINLOCALS / 2) + 1;   // (as THREAD START's LOCALS: m locals)
        if(argc == 5) NestDepth = getint(argv[4], 4, MAXGOSUB);     // (as its NEST)
    } else
        error("Syntax");
}
#endif /* USE_THREADS */
