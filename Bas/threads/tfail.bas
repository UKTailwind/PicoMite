' tfail.bas - threads, Stage 1: an error in a thread stops the program, and its message names the thread.
Option Explicit
Sub Bad
  Thread Yield
  Error "this thread has failed"
End Sub
Thread Start 3, Bad
Print "the main program waits"
Do : Thread Yield : Loop
