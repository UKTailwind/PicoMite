' tcond_p.bas - threads: THREAD WAIT inside THREAD HOLD lets go of the hold while it waits and takes it again when the
' wait ends (a condition variable's wait). The queue of the Threads User Manual in that form; the other threads run
' during the wait and not after it until THREAD RELEASE; nested holds come back whole; a time limit, a flag already
' set, THREAD WAIT for a thread, an interrupt routine during the wait (which runs held), and the other waits, which a
' hold still refuses.
' (The PicoMite's version of tcond.bas: the other threads' spins during the 100 ms wait over 2000, for the CMM2's
' 10000. An RP2040 (PICOMIN, 200 MHz) counts about 6300.)
Option Explicit
Const QSIZE = 4
Dim q$(QSIZE - 1)
Dim Integer head, tail, count
Dim Integer tests, pass, i, got, bad, sum, ended, stress, busy, spins, s0, s1, ticks, setter
Dim Float t0, t1
Dim m$

Sub Check(n$, gotv, want)
  tests = tests + 1
  If gotv = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; gotv; " want"; want
  EndIf
End Sub

' ---- the manual's queue, in this form ----
Sub Put(m$)                         ' waits while the queue is full
  Thread Hold
  Do While count = QSIZE
    Flag(2) = 0                     ' full: wait for a Get to make room
    Thread Wait Flag 2              ' (lets go of the hold while it waits)
  Loop
  q$(tail) = m$
  tail = (tail + 1) Mod QSIZE
  Inc count
  Flag(1) = 1                       ' not empty: wakes a Get
  Thread Release
End Sub

Sub Get(m$)                         ' waits while the queue is empty
  Thread Hold
  Do While count = 0
    Flag(1) = 0                     ' empty: wait for a Put
    Thread Wait Flag 1
  Loop
  m$ = q$(head)
  head = (head + 1) Mod QSIZE
  Inc count, -1
  Flag(2) = 1                       ' not full: wakes a Put
  Thread Release
End Sub
' ----

Sub Producer(first%, last%)
  Local Integer k
  For k = first% To last%
    Put Str$(k)
    If stress Then Thread Yield
  Next
  Put ""
End Sub

Sub Consumer
  Local x$
  Do
    Get x$
    If x$ = "" Then Exit Do
    Inc got
    Inc sum, Val(x$)
    If stress Then Thread Yield
  Loop
  Inc ended
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

Sub SetLater(f%, ms%)               ' sets FLAG(f%) after ms%, holding the threads itself meanwhile once
  Pause ms%
  Thread Hold                       ' (allowed: the waiting thread let go of its hold)
  setter = 1
  Thread Release
  Flag(f%) = 1
End Sub

Sub Worker(ms%)
  Pause ms%
End Sub

Sub Tick
  Inc ticks
End Sub

Sub Keeper                          ' has lock 2 until busy goes to 0
  Thread Lock 2
  Do While busy : Pause 5 : Loop
  Thread Unlock 2
End Sub

' the queue: one producer, the main program the consumer
Flag(1) = 0 : Flag(2) = 0
Thread Start 1, Producer, 1, 500
Do
  Get m$
  If m$ = "" Then Exit Do
  Inc got
  If Val(m$) <> got Then Inc bad
  If got Mod 7 = 0 Then Pause 1
Loop
Thread Wait 1
Check "the queue: all 500 messages, in order", got = 500 And bad = 0, 1
Check "  and it is empty at the end", count, 0

For stress = 0 To 1
  got = 0 : sum = 0 : ended = 0 : head = 0 : tail = 0 : count = 0
  Thread Start 1, Producer, 1, 1000
  Thread Start 2, Producer, 1001, 2000
  Thread Start 3, Consumer
  Thread Start 4, Consumer
  For i = 1 To 4 : Thread Wait i : Next
  Check "two producers and two consumers" + Choice(stress, ", with yields", "") + ": 2000 messages, sum right", got = 2000 And sum = 2001000 And ended = 2, 1
Next

' the hold is let go during the wait, and taken again after it
busy = 1 : spins = 0 : setter = 0
Thread Start 5, Spin
Flag(3) = 0
Thread Start 1, SetLater, 3, 100
Thread Hold
s0 = spins
t0 = Timer
Thread Wait Flag 3
t1 = Timer - t0
s1 = spins
For i = 1 To 20000 : Next          ' (about 20 ms of statements, held)
Check "the other threads ran during the wait", s1 - s0 > 2000, 1
Check "  another thread held and released meanwhile", setter, 1
Check "  the wait lasted until the flag was set", t1 >= 98 And t1 < 120, 1
Check "  and after it they did not, until THREAD RELEASE", spins, s1
Thread Release
Thread Wait 1

' nested holds come back whole
Flag(3) = 0
Thread Start 1, SetLater, 3, 20
Thread Hold
Thread Hold
Thread Wait Flag 3
Thread Release
s0 = spins
For i = 1 To 5000 : Next
Check "two holds: still held after one RELEASE", spins, s0
Thread Release
On Error Skip
Thread Release
Check "  and free after the second", MM.ErrMsg$ = "The threads are not held", 1
Thread Wait 1

' a time limit: the wait ends, held again, with the flag still 0
Flag(4) = 0
Thread Hold
t0 = Timer
Thread Wait Flag 4, 50
t1 = Timer - t0
s0 = spins
For i = 1 To 5000 : Next
Check "a time limit: back after 50 ms", t1 >= 49 And t1 < 60, 1
Check "  held again, and the flag still 0", spins = s0 And Flag(4) = 0, 1
Thread Release

' a flag already set: no wait, and the hold is kept throughout
Flag(4) = 1
Thread Hold
s0 = spins
Thread Wait Flag 4
For i = 1 To 5000 : Next
Check "a flag already set: no other thread ran", spins, s0
Thread Release

' THREAD WAIT for a thread, inside a hold
Thread Start 1, Worker, 30
Thread Hold
t0 = Timer
Thread Wait 1
t1 = Timer - t0
s0 = spins
For i = 1 To 5000 : Next
Check "THREAD WAIT n in a hold: the thread ran to its end", MM.Info(Thread 1) = 0 And t1 >= 28, 1
Check "  held again afterwards", spins, s0
Thread Release

' an interrupt routine of this thread during the wait runs, and runs held
ticks = 0
Flag(3) = 0
Thread Start 1, SetLater, 3, 100
SetTick 20, Tick, 1
Thread Hold
Thread Wait Flag 3
SetTick 0, 0, 1
s0 = spins
For i = 1 To 5000 : Next
Check "SETTICK's routine ran during the wait", ticks >= 3, 1
Check "  held again after the wait", spins, s0
Thread Release
Thread Wait 1

' PAUSE inside a hold holds every thread for its time; THREAD LOCK of a lock another thread has is refused
Thread Hold
s0 = spins
t0 = Timer
Pause 30
t1 = Timer - t0
Check "PAUSE inside a hold: it pauses, and no other thread runs", t1 >= 29 And spins = s0, 1
Thread Release
Thread Start 6, Keeper
Do While MM.Info(Thread Lock 2) <> 6 : Thread Yield : Loop
Thread Hold
On Error Skip
Thread Lock 2
Check "THREAD LOCK of a lock in use, inside a hold, is refused", MM.ErrMsg$ = "Cannot wait while the threads are held", 1
Thread Release
busy = 0
Thread Wait 5 : Thread Wait 6
Print "tcond:"; pass; " of"; tests; " passed"
