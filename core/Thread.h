/***************************************************************************
CMM2 MMBasic
Thread.h

Threads for a BASIC program (Src/Thread.c, docs/Threads_Plan.md).

Copyright 2011-2026 Geoff Graham, Peter Mather and Gerry Allardice.
See MMBasic.h for the licence.
***************************************************************************/
#ifndef THREAD_H
#define THREAD_H

/* The PicoMite's (branch threads, docs/Threads_PicoMite_Review.md; Peter, 26 Nov: SIZE 8, LOCALS 24, NEST 8). A thread
   runs on the process stack, so its stack holds only its own frames and an exception's stacked registers, not the
   interrupts' handlers or the housekeeping: the guard is the deepest a command goes between two stack checks, to be
   measured (Stage P5). The CMM2's are 20 KB, 6 KB, 48 and 20. */
#define MAXTHREADS 8                                // the threads a program may start, numbered 1 to MAXTHREADS (the
                                                    // PicoMite's, Peter 26 Nov; the CMM2's is 16). The RP2040's too
                                                    // (Peter, 26 Nov, after its tests: the heap decides how many fit)
#define THREAD_STACK (8 * 1024)                     // a thread's C stack: two FUNCTION levels as text on either chip
                                                    // (a level is 1,280 bytes on a PC3, 1,416 on an RP2040: Stage P5)
#define THREAD_GUARD (3 * 1024)                     // the part of it below the stack check's floor: error()'s print
                                                    // needs more than 1.7 KB of it (a PC3, 26 Nov; Stage P5 measures)
#define THREAD_SIZE_MIN 6                           // THREAD SIZE's least, in K: the guard and a SUB's 2 KB to run in
#ifdef rp2350
#define THREAD_LOCALS 24                            // its local variable slots
#else
#define THREAD_LOCALS 16
#endif
#define THREAD_PRIORITY_MAX 10                      // THREAD PRIORITY: 1 (every thread's at RUN) to this
#ifdef rp2350
#define THREAD_NEST 8                               // how deep its FOR loops, DO loops and calls may each go (NEST)
#else
#define THREAD_NEST 6
#endif
#define MAXLOCKS 16                                 // THREAD LOCK's locks, numbered 1 to MAXLOCKS

extern int ThreadCount;                             // the threads started and not yet ended: 0, and nothing here is active
extern volatile int ThreadSliceLeft;                // milliseconds left of the running thread's slice (the tick, Timers.c)
extern int ThreadSliceLen;                          // THREAD SLICE
extern volatile unsigned int ThreadWakeMs;          // mSecTimer when the first sleeping thread is due (the tick); 0 none
/* Stage 4: a command's own wait (INPUT for a key, a frame's start, SYNC) lets the other threads run. No interrupt is
   taken in such a wait, as none ever was: while a thread is in one it is 'deaf', and its interrupts wait. */
extern volatile uint32_t FrameCount;                // frames shown: counted by the LTDC's interrupt (ltdc.c)
extern volatile unsigned char ThreadFrameWaiters;   // threads waiting for the next one, which that interrupt wakes by
extern volatile unsigned char ThreadFrameDue;       // this and IntSignal
void ThreadPoll(void);                              // a wait loop found nothing yet: the other threads have their turns
void ThreadFrameWait(void);                         // to the start of the next frame
void ThreadSleepDeaf(long long t);                  // to the microsecond clock's t, with no interrupt taken (SYNC)
/* Stage 4b: an interrupt routine runs in the thread that set it up (ThreadIntOwn, from GetIntAddress). IntScan asks
   IntMay of each source's routine before it takes the source: no while its thread cannot take it now, and the source
   is found again later. check_interrupt hands one found for another thread to it (ThreadIntHere). */
void ThreadIntOwn(char *routine);                   // GetIntAddress: routine's interrupts are the running thread's
int IntMay(char *routine);                          // IntScan: may routine's interrupt be taken now
int ThreadIntHere(char *routine);                   // check_interrupt: 1 run it here; 0 handed to its thread
char *ThreadIntTake(void);                          // check_interrupt: one handed to the running thread, or NULL
int ThreadIntWaiting(void);                         // ... is there one
void ThreadIntDone(void);                           // IRETURN: a routine handed out of turn has ended (Thread.c)
void ThreadsScreenReset(void);                      // MODE: every thread has the new mode's screen state
int ThreadPageInUse(unsigned char *buf);            // FRAMEBUFFER CLOSE: the other thread that writes to buf, or -1
void ThreadWriteTarget(unsigned char *buf);         // a switch: the thread's FRAMEBUFFER WRITE target (FrameBuffer.c)
int ThreadNetEnter(int k);                          // WEB (k 0), BLUETOOTH (k 1): it is this thread's for the command (0:
void ThreadNetLeave(int k);                         // an interrupt is due first, the statement runs again after it)
void ThreadErrorRelease(void);                      // error(): what the running thread held for its command is let go
void ThreadDelay(int ms);                           // a wait in a command that lets the other threads run (TEMPR)
/* No thread switch while BASIC runs from inside a CSUB or a module's call (its own data is not a thread's, and a
   module's call has one stack): ThreadHoldDepth is the ExecDepth such BASIC runs at, or more. The caller brackets its
   call with the two macros. An error that leaves the call by longjmp leaves the depth set: the scheduler clears it
   the first time it is asked at a shallower depth, which only happens once the call is over. */
extern int ThreadHoldDepth;
#define THREAD_HOLD_BEGIN() int threadhold_ = ThreadHoldDepth; if(ExecDepth + 1 < ThreadHoldDepth) ThreadHoldDepth = ExecDepth + 1
#define THREAD_HOLD_END() ThreadHoldDepth = threadhold_
void cmd_thread(void);
void ThreadSchedule(void);                          // the statement tail's call: another thread's turn (check_interrupt)
void ThreadSleepUntil(long long t);                 // PAUSE: the other threads run until the microsecond clock reads t
int ThreadState(int n);                             // MM.INFO(THREAD n)
int ThreadLockOwner(int k);                         // MM.INFO(THREAD LOCK k): the thread that has lock k, -1 none
int ThreadArg(int n, long long int *i, MMFLOAT *f, char *s);    // MM.INFO(THREAD ARG n), for a thread's own call
void ThreadsStopped(void);                          // the prompt's landing: the list of threads after an error or Ctrl-C,
void ThreadsStoppedShow(void);                      // written down, and then printed after the error's message
int ThreadMemory(int *kb);                          // MEMORY: the threads running, and their memory in K
void ThreadDoing(int d);                            // PAUSE's loop: 1 in it, 0 out (the list after Ctrl-C)
int ThreadsEnd(void);                               // the program has stopped or is being replaced: 1 if it had threads
void ThreadsStop(void);                             // END: no other thread runs from here on
int ThreadNumber(void);                             // the running thread's number: 0 the main program
void ThreadMainOnly(void);                          // an error in any thread but the main program
void ThreadsNone(void);                             // an error while the program has a thread
char *ThreadTokenBuffer(void);                      // EXECUTE's line: tknbuf for the main program, a thread's own
void ThreadStackLimits(void);                       // the stack check's limits as the running thread's (after PIXEL FILL)
int ThreadOnMainStack(void (*fn)(void));            // fn run on the main stack if a thread's is in use: 1; 0 it is not

/* The prompt's landing after an error or Ctrl-C (PicoMite.c), first thing: a longjmp made while a thread ran on the
   process stack wrote its stack pointer to PSP, and clearing SPSEL alone would bring back the main stack's old value,
   below live frames. So the value is moved: read SP, clear SPSEL, write it to MSP (and PSPLIM off, the RP2350). A naked
   function (Thread.c), not inline asm: in an ordinary function on the RP2040 GCC assembles that in divided syntax. */
void ThreadLanding(void);
#define THREAD_LANDING() ThreadLanding()

#endif
