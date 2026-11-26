' tqueue.bas - threads: the queue of 8.00.00b5's Threads User Manual ("A queue between threads"), which releases the
' hold before it waits and so runs on every version (the manual's queue now waits inside the hold: tcond.bas); its
' Put and Get as printed there, with a small queue so that it is often full as well as empty. One producer and one
' consumer: every message arrives, in order. Two producers and two consumers with yields between their operations:
' nothing is lost or taken twice. Both kinds of wait happen, and a busy thread runs while they wait.
Option Explicit
Const QSIZE = 4
Dim q$(QSIZE - 1)
Dim Integer head, tail, count
Dim Integer tests, pass, i, bad, got, sum, busy, spins, fullwaits, emptywaits, ended, stress
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

' ---- the manual's two SUBs, as printed (the two counters of waits are the test's) ----
Sub Put(m$)                         ' waits while the queue is full
  Local Integer done
  Do
    Thread Hold
    If count < QSIZE Then
      q$(tail) = m$
      tail = (tail + 1) Mod QSIZE
      Inc count
      Flag(1) = 1                   ' not empty: wakes a Get
      done = 1
    Else
      Flag(2) = 0                   ' full: cleared in the same step as the test
    EndIf
    Thread Release
    If done = 0 Then Inc fullwaits : Thread Wait Flag 2 ' until a Get makes room
  Loop Until done
End Sub

Sub Get(m$)                         ' waits while the queue is empty
  Local Integer done
  Do
    Thread Hold
    If count > 0 Then
      m$ = q$(head)
      head = (head + 1) Mod QSIZE
      Inc count, -1
      Flag(2) = 1                   ' not full: wakes a Put
      done = 1
    Else
      Flag(1) = 0                   ' empty: cleared in the same step as the test
    EndIf
    Thread Release
    If done = 0 Then Inc emptywaits : Thread Wait Flag 1 ' until a Put adds one
  Loop Until done
End Sub
' ----

Sub Producer(first%, last%)
  Local Integer k
  For k = first% To last%
    Put Str$(k)
    If stress Then Thread Yield
  Next
  Put ""                            ' the end
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

Sub Slow
  Local Integer k
  For k = 1 To 20
    Pause 10
    Put Str$(k)
  Next
  Put ""
End Sub

Sub Spin
  Do While busy
    Inc spins
  Loop
End Sub

' one producer, the main program the consumer: in order, nothing lost
Flag(1) = 0 : Flag(2) = 0
Thread Start 1, Producer, 1, 500
Do
  Get m$
  If m$ = "" Then Exit Do
  Inc got
  If Val(m$) <> got Then Inc bad
  If got Mod 7 = 0 Then Pause 1     ' (a slow consumer now and then: the queue fills)
Loop
Thread Wait 1
Check "one producer: all 500 messages arrived", got, 500
Check "in order", bad, 0
Check "the queue is empty at the end", count, 0
Check "Put waited while the queue was full", fullwaits > 0, 1

' a slow producer: Get waits while the queue is empty, and a busy thread runs meanwhile
got = 0 : emptywaits = 0 : busy = 1 : spins = 0
Thread Start 2, Spin
Thread Start 1, Slow
Do
  Get m$
  If m$ = "" Then Exit Do
  Inc got
Loop
busy = 0
Thread Wait 1 : Thread Wait 2
Check "a slow producer: all 20 arrived", got, 20
Check "Get waited while the queue was empty", emptywaits >= 15, 1
Check "and a busy thread ran meanwhile", spins > 50000, 1

' two producers, two consumers, with yields: every number once
For stress = 0 To 1
  got = 0 : sum = 0 : ended = 0 : head = 0 : tail = 0 : count = 0
  Thread Start 1, Producer, 1, 1000
  Thread Start 2, Producer, 1001, 2000
  Thread Start 3, Consumer
  Thread Start 4, Consumer
  For i = 1 To 4 : Thread Wait i : Next
  Check "two producers and two consumers" + Choice(stress, ", with yields", "") + ": 2000 messages", got, 2000
  Check "  the sum of their numbers", sum, 2001000
  Check "  both consumers ended", ended, 2
Next

' a flag already set: THREAD WAIT FLAG inside a hold goes on at once (tcond.bas tests the wait that lets go of it)
Thread Hold
Flag(5) = 1
On Error Skip
Thread Wait Flag 5
Thread Release
Check "THREAD WAIT FLAG inside THREAD HOLD, the flag already set", MM.Errno, 0
Print "tqueue:"; pass; " of"; tests; " passed"

