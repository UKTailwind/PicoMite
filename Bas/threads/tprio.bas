' tprio.bas - threads: THREAD PRIORITY. A thread's turn is as many time slices as its priority (1 to 10; 1 unless
' THREAD START n PRIORITY p or THREAD PRIORITY n, p says otherwise; n = 0 is the main program). Two busy threads share
' the processor in the ratio of their priorities; the priority can be changed while they run; the main program's
' counts as any other's; and the owner of a lock that a thread of a higher priority waits for has that thread's turn
' until it unlocks. The figures are not printed (show = 1 prints them): only whether each is in its range.
Option Explicit
Dim Integer tests, pass, show = 0
Dim Integer c(9), halt, hold, i, w, a, b, x1, x4
Dim Float r, t0, ta, tb

Sub Check(n$, ok)
  tests = tests + 1
  If ok Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$
  EndIf
End Sub

Sub Count
  Local Integer me = MM.Info(Thread)
  Do While halt = 0
    c(me) = c(me) + 1
  Loop
End Sub

Sub Measure(ms)                         ' the two counters over ms milliseconds, the main program asleep
  Pause 30
  c(1) = 0 : c(2) = 0
  Pause ms
  a = c(1) : b = c(2)
  r = a / b
  If show Then Print "  "; a; b; " ratio "; Str$(r, 0, 2)
End Sub

Sub Holder                              ' takes lock 1, works for w loops, gives it up
  Local Integer k
  Thread Lock 1
  hold = 1
  For k = 1 To w : Next
  Thread Unlock 1
End Sub

Sub Waiter
  Thread Lock 1
  Thread Unlock 1
End Sub

' work that takes about 50 ms of the processor in this compile mode
t0 = Timer : For i = 1 To 20000 : Next : ta = Timer - t0
w = 20000 * 50 / ta

Thread Start 1, Count
Thread Start 2, Count
Measure 400
Check "equal priorities share equally", r > 0.8 And r < 1.25
Thread Priority 1, 5
Measure 400
Check "priority 5 beside 1: five slices to one", r > 4 And r < 6
Thread Priority 1, 1 : Thread Priority 2, 5
Measure 400
Check "changed over while they run: one to five", r > 1 / 6 And r < 1 / 4
halt = 1 : Thread Wait 1 : Thread Wait 2

halt = 0
Thread Start 1 Priority 3, Count
Thread Start 2, Count
Measure 400
Check "THREAD START n PRIORITY 3: three to one", r > 2.4 And r < 3.6
halt = 1 : Thread Wait 1 : Thread Wait 2

' the main program's priority: a thread beside a busy main program has half the time at 1 to 1, a fifth at 4 to 1
halt = 0
Thread Start 1, Count
Pause 30
c(1) = 0 : t0 = Timer : Do While Timer - t0 < 400 : Loop : x1 = c(1)
Thread Priority 0, 4
Pause 30
c(1) = 0 : t0 = Timer : Do While Timer - t0 < 400 : Loop : x4 = c(1)
Thread Priority 0, 1
r = x4 / x1
If show Then Print "  "; x1; x4; " ratio "; Str$(r, 0, 2)
Check "the main program at priority 4: the thread has a fifth, not a half", r > 0.3 And r < 0.5
halt = 1 : Thread Wait 1

' a lock: the holder at priority 1 beside a busy thread at 9 has a tenth of the time, until a thread at 9 waits for
' its lock: then it has a turn of 9 too, half the time
halt = 0 : hold = 0
Thread Start 2 Priority 9, Count
Thread Start 3, Holder
Do While hold = 0 : Pause 1 : Loop
t0 = Timer : Thread Wait 3 : ta = Timer - t0
hold = 0
Thread Start 3, Holder
Do While hold = 0 : Pause 1 : Loop
Thread Start 4 Priority 9, Waiter
t0 = Timer : Thread Wait 3 : tb = Timer - t0
Thread Wait 4
halt = 1 : Thread Wait 2
If show Then Print "  the holder's work: "; Str$(ta, 0, 0); " ms alone, "; Str$(tb, 0, 0); " ms with a waiter at 9"
Check "a lock's holder has its waiter's turn: its work takes less than half the time", tb < ta / 2

' the limits
On Error Skip
Thread Start 5 Priority 0, Waiter
Check "PRIORITY 0 is refused", Instr(MM.ErrMsg$, "valid is 1 to 10") > 0
On Error Skip
Thread Start 5 Priority 11, Waiter
Check "PRIORITY 11 is refused", Instr(MM.ErrMsg$, "valid is 1 to 10") > 0
On Error Skip
Thread Priority 7, 2
Check "THREAD PRIORITY of a thread that is not running is refused", MM.ErrMsg$ = "Thread 7 is not running"
On Error Skip
Thread Priority 0, 11
Check "THREAD PRIORITY 0, 11 is refused", MM.Errno <> 0
Print "tprio:"; pass; " of"; tests; " passed"
