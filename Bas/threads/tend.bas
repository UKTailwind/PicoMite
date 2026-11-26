' tend.bas - threads, Stage 1: END in a thread ends the program, SUB MM.END running first, in that thread.
Option Explicit
Sub MM.End
  Print "MM.END runs in thread"; MM.Info(Thread)
End Sub
Sub Ender
  Print "thread"; MM.Info(Thread); " ends the program"
  End
  Print "not reached"
End Sub
Thread Slice 100                      ' (the order is what is tested: with 1 ms slices a PRINT can end one)
Thread Start 2, Ender
Do
  Thread Yield
  Print "not reached: the main program"
Loop
