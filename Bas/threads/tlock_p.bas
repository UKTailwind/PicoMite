' tlock.bas - threads, Stage 3 (docs/Threads_Plan.md): THREAD LOCK and UNLOCK, THREAD HOLD and RELEASE. Four threads
' add to a total with a read and a write that a switch comes between: wrong with nothing, right with a lock, right
' with a hold. A lock's timeout, a lock whose owner ends or is stopped, and what is refused.
Option Console Serial ' (the PicoMite: a PRINT to the screen as well can scroll it, some milliseconds on a PC3, inside the timed waits)
Option Explicit
Dim Integer tests, pass, i, total, how, busy
Dim Float t0, t1

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Adder
  Local Integer k, v
  For k = 1 To 300
    If how = 1 Then Thread Lock 1
    If how = 2 Then Thread Hold
    v = total
    Thread Yield                        ' the switch the clock makes sooner or later (none inside a hold)
    total = v + 1
    If how = 1 Then Thread Unlock 1
    If how = 2 Then Thread Release
  Next
End Sub

Sub RunFour
  total = 0
  For i = 1 To 4 : Thread Start i, Adder : Next
  For i = 1 To 4 : Thread Wait i : Next
End Sub

Sub Keeper                              ' has lock 2 for 300 ms
  Thread Lock 2
  busy = 1
  Pause 300
  Thread Unlock 2
End Sub

Sub Leaver                              ' ends with lock 3 its own
  Thread Lock 3
End Sub

Sub Sitter                              ' has lock 4 and never gives it up
  Thread Lock 4
  busy = 1
  Do : Pause 1000 : Loop
End Sub

Sub Tick
  If MM.Info(Thread Lock 2) = 5 Then Inc total
End Sub

how = 0 : RunFour
Check "no lock: counts are lost", total < 1200, 1
how = 1 : RunFour
Check "THREAD LOCK: every count is there", total, 1200
Check "the lock is free afterwards", MM.Info(Thread Lock 1), -1
how = 2 : RunFour
Check "THREAD HOLD: every count is there", total, 1200

busy = 0
Thread Start 5, Keeper
Do While busy = 0 : Loop
Check "MM.INFO(THREAD LOCK k) is the owner", MM.Info(Thread Lock 2), 5
t0 = Timer
On Error Skip
Thread Lock 2, 50
t1 = Timer - t0
Check "a lock in use: the time limit is an error", MM.ErrMsg$ = "Lock 2 timed out", 1
Check "after 50 ms", t1 >= 50 And t1 < 56, 1
On Error Clear
total = 0
SetTick 20, Tick
Thread Lock 2                           ' no limit: until the owner unlocks, with interrupts taken meanwhile
SetTick 0, 0
t1 = Timer - t0
Check "with no limit it waits for the owner", t1 >= 295 And t1 < 310, 1
Check "the main program has it", MM.Info(Thread Lock 2), 0
Check "interrupts ran while it waited", total >= 10, 1
Thread Unlock 2

Thread Start 5, Leaver
Thread Wait 5
Check "a lock whose owner has ended is free", MM.Info(Thread Lock 3), -1
busy = 0
Thread Start 5, Sitter
Do While busy = 0 : Loop
Thread Stop 5
Check "a lock whose owner is stopped is free", MM.Info(Thread Lock 4), -1
Thread Lock 4, 10
Check "and can be taken", MM.Info(Thread Lock 4), 0

On Error Skip
Thread Lock 4
Check "a lock taken twice is refused", MM.ErrMsg$ = "Lock 4 is already held by this thread", 1
Thread Unlock 4
On Error Skip
Thread Unlock 4
Check "UNLOCK of a lock not held is refused", MM.ErrMsg$ = "Lock 4 is not held by this thread", 1
On Error Skip
Thread Release
Check "RELEASE with no HOLD is refused", MM.ErrMsg$ = "The threads are not held", 1
busy = 0
Thread Start 5, Sitter
Do While busy = 0 : Loop
Thread Hold
On Error Skip
Thread Lock 4, 10
Check "a wait inside a hold is refused", MM.ErrMsg$ = "Cannot wait while the threads are held", 1
Thread Release
Thread Stop 5
Print "tlock:"; pass; " of"; tests; " passed"
