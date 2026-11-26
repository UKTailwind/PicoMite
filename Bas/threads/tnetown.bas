' tnetown.bas - threads, Stage 4b: the network belongs to one thread. Needs the Wi-Fi module joined to a network. The
' main program uses it first (setup), then a thread uses it and has it: a WEB command in the main program or another
' thread is then refused. When the thread ends the network is free again.
Option Explicit
Dim Integer tests, pass, done
Dim msg$

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub WebThread
  On Error Skip
  WEB OPEN TCP CLIENT "10.255.255.1", 80, 300
  msg$ = MM.ErrMsg$
  done = 1
  Do While done < 2 : Pause 10 : Loop
End Sub

Sub Other
  On Error Skip
  WEB OPEN TCP CLIENT "10.255.255.1", 80, 300
  msg$ = MM.ErrMsg$
  done = 3
End Sub

If MM.Info(IP Address) = "0.0.0.0" Or MM.Info(IP Address) = "" Then Print "tnetown: the Wi-Fi module is not joined" : End

On Error Skip
WEB OPEN TCP CLIENT "10.255.255.1", 80, 300
Check "the main program's WEB command before any thread: as before", Instr(MM.ErrMsg$, "belongs") = 0, 1
Thread Start 1, WebThread
Do While done = 0 : Pause 5 : Loop
Check "a thread's WEB command: as before", Instr(msg$, "belongs") = 0, 1
On Error Skip
WEB OPEN TCP CLIENT "10.255.255.1", 80, 300
Check "then the main program's is refused", MM.ErrMsg$ = "The network belongs to thread 1", 1
Thread Start 2, Other
Do While done < 3 : Pause 5 : Loop
Check "and another thread's", msg$ = "The network belongs to thread 1", 1
done = 2
Thread Wait 1
On Error Skip
WEB OPEN TCP CLIENT "10.255.255.1", 80, 300
Check "when the thread has ended the network is free", Instr(MM.ErrMsg$, "belongs") = 0, 1
Print "tnetown:"; pass; " of"; tests; " passed"
