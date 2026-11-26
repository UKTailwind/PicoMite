' tmutex.bas - threads: THREAD LOCK used as the mutex of the Threads User Manual's "A lock is a mutex" (Sharing Data
' Safely). A named mutex inside a SUB, the try (a time limit of 0), a mutex kept across a PAUSE while a thread that
' does not want it runs on, the Enter and Leave SUBs that let a thread take its own mutex again, two busy threads that
' both get their share, and a mutex and interrupt routines: the thread's own routine runs inside its locked statements
' and inside its hold, and a routine may take a free mutex but not wait for one.
Option Explicit
Const mxTotal = 1, mxLog = 2, mxPool = 3, mxTick = 4
Dim Integer tests, pass, i, total, busy, go, spins, held, ticks, inside, cnt(2), depth(16)
Dim Float t0, t1
Dim e$

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Deposit(amount%)                    ' the manual's SUB: the mutex is taken and given back in one place
  Local Integer v
  Thread Lock mxTotal
  v = total
  Thread Yield                          ' (the switch the clock makes sooner or later)
  total = v + amount%
  Thread Unlock mxTotal
End Sub

Sub Saver
  Local Integer k
  For k = 1 To 300 : Deposit 2 : Next
End Sub

Sub Keeper(ms%)                         ' has mxLog for ms%, in a PAUSE
  Thread Lock mxLog
  busy = 1
  Pause ms%
  Thread Unlock mxLog
End Sub

Sub Spin
  Do While go
    Inc spins
  Loop
End Sub

Sub Enter(k%)                           ' the manual's pair: a mutex the same thread may take again
  If MM.Info(Thread Lock k%) <> MM.Info(Thread) Then Thread Lock k%
  Inc depth(k%)
End Sub

Sub Leave(k%)
  Inc depth(k%), -1
  If depth(k%) = 0 Then Thread Unlock k%
End Sub

Sub Inner
  Enter mxPool
  Inc total
  Leave mxPool
End Sub

Sub Outer
  Local Integer k
  For k = 1 To 200
    Enter mxPool
    Inner                               ' takes mxPool again
    Inner
    Leave mxPool
  Next
End Sub

Sub Greedy(me%, giveway%)                 ' takes the mutex over and over with no wait of its own
  Local Integer v
  Do While go
    Thread Lock mxTotal
    v = total : v = v + 1 : total = v
    Inc cnt(me%)
    Thread Unlock mxTotal
    If giveway% Then Thread Yield
  Loop
End Sub

Sub Ticker                              ' a thread with a SETTICK of its own, which comes inside its locked statements
  Local Float t
  SetTick 5, Tick, 1
  Thread Lock mxTick
  inside = 1
  t = Timer
  Do While Timer - t < 30 : Loop
  inside = 0
  Thread Unlock mxTick
  Thread Hold
  inside = 2
  t = Timer
  Do While Timer - t < 30 : Loop
  inside = 0
  Thread Release
  SetTick 0, 0, 1
End Sub

Sub Tick
  If inside = 1 Then
    Inc ticks
    If e$ = "" Then
      On Error Skip
      Thread Lock mxTick                ' its own thread has it
      e$ = MM.ErrMsg$
    EndIf
  EndIf
  If inside = 2 Then Inc held
End Sub

Sub Taker                               ' a routine of the main program's: a free mutex, and one another thread has
  On Error Skip
  Thread Lock mxLog
  e$ = MM.ErrMsg$
  If MM.Info(Thread Lock mxLog) = MM.Info(Thread) Then Thread Unlock mxLog
  inside = 9
End Sub

' a named mutex inside a SUB
For i = 1 To 4 : Thread Start i, Saver : Next
For i = 1 To 4 : Thread Wait i : Next
Check "four threads through one SUB with a mutex: every deposit is there", total, 2400

' the try: a time limit of 0
On Error Skip
Thread Lock mxLog, 0
Check "a try of a free mutex takes it", MM.Errno = 0 And MM.Info(Thread Lock mxLog) = 0, 1
Thread Unlock mxLog
busy = 0
Thread Start 5, Keeper, 200
Do While busy = 0 : Loop
t0 = Timer
On Error Skip
Thread Lock mxLog, 0
t1 = Timer - t0
Check "a try of one in use is the error, at once", MM.ErrMsg$ = "Lock 2 timed out" And t1 < 2, 1
On Error Clear

' kept across a PAUSE: a thread that does not want it runs on, one that does waits
go = 1 : spins = 0
Thread Start 6, Spin
Thread Lock mxLog
t1 = Timer - t0
go = 0
Thread Unlock mxLog
Check "a mutex kept across a PAUSE: the one that wants it waits for it", t1 >= 195 And t1 < 215, 1
Check "and a thread that does not runs meanwhile", spins > 5000, 1
Thread Wait 6

' the same thread again: Enter and Leave
total = 0
For i = 1 To 3 : Thread Start i, Outer : Next
For i = 1 To 3 : Thread Wait i : Next
Check "Enter and Leave: a thread takes its own mutex again", total, 1200
Check "and it is free at the end", MM.Info(Thread Lock mxPool) = -1 And depth(mxPool) = 0, 1

' two busy threads
total = 0 : cnt(1) = 0 : cnt(2) = 0 : go = 1
Thread Start 1, Greedy, 1, 0
Thread Start 2, Greedy, 2, 0
Pause 400
go = 0
Thread Wait 1 : Thread Wait 2
Check "two busy threads: every count is there", total, cnt(1) + cnt(2)
Check "and both had a share", cnt(1) > total / 10 And cnt(2) > total / 10, 1
total = 0 : cnt(1) = 0 : cnt(2) = 0 : go = 1
Thread Start 1, Greedy, 1, 1
Thread Start 2, Greedy, 2, 1
Pause 400
go = 0
Thread Wait 1 : Thread Wait 2
Check "with THREAD YIELD after the unlock they take turns", Abs(cnt(1) - cnt(2)) < total / 50 And total = cnt(1) + cnt(2), 1

' interrupt routines
e$ = "" : ticks = 0 : held = 0
Thread Start 1, Ticker
Thread Wait 1
Check "a thread's own routine runs inside its locked statements", ticks >= 3, 1
Check "where the mutex is the thread's already", e$ = "Lock 4 is already held by this thread", 1
Check "and inside its hold", held >= 3, 1
e$ = "?" : inside = 0
SetTick 10, Taker, 2
Do While inside = 0 : Loop
SetTick 0, 0, 2
Check "a routine takes a free mutex", e$ = "", 1
busy = 0
Thread Start 5, Keeper, 100
Do While busy = 0 : Loop
e$ = "?" : inside = 0
SetTick 10, Taker, 2
Do While inside = 0 : Loop
SetTick 0, 0, 2
Check "but cannot wait for one", e$ = "Cannot wait in an interrupt routine", 1
Thread Wait 5
Print "tmutex:"; pass; " of"; tests; " passed"
