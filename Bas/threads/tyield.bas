' tyield.bas - threads, Stage 1 (docs/Threads_Plan.md): two threads and the main program take turns with THREAD YIELD.
' A thread runs until it yields or ends, and the next in number order runs: the order of the lines is fixed. (With
' a time slice far longer than a turn: at the usual 1 ms a PRINT's own time ends a slice, and the order with it.)
Option Explicit
Dim Integer k
Thread Slice 100

Sub Ping
  Local Integer i
  For i = 1 To 5
    Print "ping"; i; " in thread"; MM.Info(Thread)
    Thread Yield
  Next
End Sub

Sub Pong
  Local Integer i
  For i = 1 To 3
    Print "pong"; i; " in thread"; MM.Info(Thread)
    Thread Yield
  Next
  Print "pong ends"
End Sub

Print "the main program is thread"; MM.Info(Thread)
Thread Yield                          ' with no thread started: nothing
Thread Start 1, Ping
Thread Start 2, Pong
Print "started: nothing runs until the main program yields"
For k = 1 To 8
  Print "main"; k
  Thread Yield
Next
Print "done, in thread"; MM.Info(Thread)
