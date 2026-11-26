' twait.bas - threads, Stage 3: THREAD WAIT n and THREAD WAIT FLAG f, with and without a time limit. A producer and a
' consumer hand twenty items over through two flags; the main program waits for a flag an interrupt routine sets.
Option Explicit
Dim Integer tests, pass, done, item, sum, got, ticks
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

Sub Sleeper
  Pause 120
  done = 1
End Sub

Sub Producer
  Local Integer k
  For k = 1 To 20
    item = k
    Flag(2) = 0
    Flag(1) = 1                         ' an item is there
    Thread Wait Flag 2                  ' until it has been taken
  Next
End Sub

Sub Consumer
  Local Integer k
  For k = 1 To 20
    Thread Wait Flag 1
    sum = sum + item
    Inc got
    Flag(1) = 0
    Flag(2) = 1
  Next
End Sub

Sub Tick
  Inc ticks
  If ticks = 5 Then Flag(7) = 1
End Sub

Sub Late
  Thread Wait Flag 9                    ' a thread waiting for a flag an interrupt routine sets
  done = 2
End Sub

Flags = 0
t0 = Timer
Thread Start 1, Sleeper
Thread Wait 1
t1 = Timer - t0
Check "THREAD WAIT n: back when the thread has ended", done, 1
Check "after its 120 ms", t1 >= 120 And t1 < 128, 1
t0 = Timer
Thread Wait 1
Check "a thread that is not running: no wait", Timer - t0 < 2, 1

done = 0
t0 = Timer
Thread Start 1, Sleeper
Thread Wait 1, 50
t1 = Timer - t0
Check "with a limit of 50 ms: back then", t1 >= 50 And t1 < 56, 1
Check "the thread still running", MM.Info(Thread 1) <> 0 And done = 0, 1
Thread Wait 1

Thread Start 2, Consumer
Thread Start 3, Producer
Thread Wait 3, 2000
Thread Wait 2, 2000
Check "producer and consumer: twenty items", got, 20
Check "their sum", sum, 210

t0 = Timer
Thread Wait Flag 5, 40
t1 = Timer - t0
Check "THREAD WAIT FLAG with a limit: back after 40 ms", t1 >= 40 And t1 < 46, 1
Check "the flag still 0", Flag(5), 0

t0 = Timer
SetTick 10, Tick
Thread Wait Flag 7                      ' the fifth tick sets it
t1 = Timer - t0
SetTick 0, 0
Check "the main program waits for a flag an interrupt sets", ticks, 5
Check "and is back at the fifth tick", t1 >= 49 And t1 < 56, 1

done = 0
Thread Start 4, Late
Pause 20
Check "a thread waits", done, 0
Check "a thread that waits is 4", MM.Info(Thread 4), 4
Flag(9) = 1
Pause 5
Check "and goes on when the flag is set", done, 2

On Error Skip
Thread Wait 0
Check "THREAD WAIT 0 is refused", MM.Errno <> 0, 1
Print "twait:"; pass; " of"; tests; " passed"
