' tnet_p.bas - threads, Stage P4b: the network's waits and TEMPR() let the other threads run (the PicoMite's version
' of tnet.bas: TEMPR() on GP0, which has nothing on it but a link to GP1, an input, for GP6). Needs the Wi-Fi module
' joined to a network. A thread counts while the main program opens a TCP connection to an address that does not
' answer (10.255.255.1, 2 s limit) and reads TEMPR() on a pin with no sensor; then the other way round, a thread
' waiting on the network while the main program counts.
Option Explicit
Dim Integer tests, pass, busy, n, before, ran, done
Dim Float t0, t1, tt
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

Sub Spin
  Do While busy
    Inc n
  Loop
End Sub

Sub Opener
  On Error Skip
  WEB OPEN TCP CLIENT "10.255.255.1", 80, 2000
  msg$ = MM.ErrMsg$
  done = 1
End Sub

If MM.Info(IP Address) = "0.0.0.0" Or MM.Info(IP Address) = "" Then Print "tnet: the Wi-Fi module is not joined" : End

busy = 1
Thread Start 1, Spin
Pause 20
before = n : t0 = Timer
On Error Skip
WEB OPEN TCP CLIENT "10.255.255.1", 80, 2000
t1 = Timer - t0 : ran = n - before
Check "an address that does not answer: an error after the 2 s limit", MM.Errno <> 0 And t1 >= 1900 And t1 < 2600, 1
Check "the thread ran all the while", ran > 100000, 1

before = n : t0 = Timer
tt = TEMPR(GP0)
t1 = Timer - t0 : ran = n - before
Check "TEMPR() with no sensor", tt, 1000
Check "the thread ran during its conversion time", ran > 10000, 1

busy = 0
Thread Wait 1
n = 0 : done = 0
Thread Start 2, Opener
t0 = Timer
Do While done = 0
  Inc n
Loop
t1 = Timer - t0
Check "a thread waiting on the network: an error after its limit", msg$ <> "" And t1 >= 1900 And t1 < 2600, 1
Check "the main program ran all the while", n > 100000, 1
Print "tnet:"; pass; " of"; tests; " passed"
