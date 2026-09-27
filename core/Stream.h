/*
 * @cond
 * The following section will be excluded from the documentation.
 */
/* *********************************************************************************************************************
PicoMite MMBasic

Stream.h

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
 * Route B: a compiled statement stream, derived from the tokenised program
 * and run in place of its text (docs/Interpreter_RouteB_Design.html).  The
 * text stays the program: LIST, EDIT, SAVE, errors and TRACE work from it as
 * before, and a program whose stream is missing, stale or refused runs as text.
 *
 * P1a: the development switch and the slot that will hold the stream.
 * P1b: the stamp, and writing the stream into its slot.
 * P1c: the records and the statement map.
 * P1d: the executor, running fallback records.
 */
#ifndef __STREAM_H
#define __STREAM_H

#define RB_OFF 0
#define RB_ON 1
#define RB_SHADOW 2
extern int RBMode; // OPTION COMPILE ON | OFF | SHADOW: a development switch, not saved

int RBStreamSlotRam(void);           // 1: the stream is in a RAM (PSRAM) slot, 0: in a flash slot
int RBStreamSlot(void);              // that slot's number: RAM slot 4, or flash slot 2
void RBGuardSlot(int ram, int slot); // refuse a command on the slot that holds the stream

// P1b: the stamp.  PrepareProgram(true) compares a CRC of the program and
// library with the one the stream was compiled from, and compiles again only
// when they differ.
extern int RBLive;             // a stream matching the program is in the slot
void RBPrepare(void);          // at the end of a successful PrepareProgram(true)
void RBStatus(char *out);      // MM.INFO(COMPILE): what the last RUN did

// P1d: the executor.  ExecuteProgram hands over to it while a live stream
// holds the statement it is about to run.  It runs from RAM on the RP2350.
// On the RP2040 it is still in flash, where it and the handlers it calls
// (findvar, cmd_next) evict each other from the 16 KB two-way XIP cache
// (pixart 29% slower than text); RAM for it there means crossing the 4 KB
// page below AllMemory, i.e. 4 KB less heap and program memory.
int RBInImage(unsigned char *p);             // p lies in the program or library image
unsigned char *RunStream(unsigned char *p);  // run from p; returns where the text loop carries on

#endif /* __STREAM_H */
/*  @endcond */
