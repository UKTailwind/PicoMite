' tinput.bas - threads, Stage 4 (docs/Threads_Plan.md): INPUT and LINE INPUT let the other threads run while the line
' is typed. tinput.in types the lines at the prompts ending in "?". The main program in INPUT with two threads busy
' and a SETTICK going (no interrupt is taken in INPUT, as none ever was: it comes when the line is in); INPUT in a
' thread; LINE INPUT in a thread while another uses short strings (what findvar found last is not the line's); a
' line typed while a thread draws text elsewhere keeps its place (TEXT moves the print position, which is shared).
Option Explicit
Dim Integer tests, pass, busy, ticks, before, ran, t, k, h, v, n(2)
Dim a$, b$, c$, e5$ Length 5, f5$ Length 5

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
  Local Integer me = MM.Info(Thread)
  Do While busy
    Inc n(me)
  Loop
End Sub

Sub Tick
  Inc ticks
End Sub

Sub Asker
  Local w$, v%
  Input "a thread's two"; w$, v%
  b$ = w$
  k = v%
End Sub

Sub Liner
  Local l$
  Line Input "a thread's line? ", l$
  c$ = l$
End Sub

Sub Texter
  Do While busy = 3
    Text MM.HRes - 1, 0, "12:34:56", "RT"
    Pause 3
  Loop
End Sub

Sub Shorts
  f5$ = "five5"
  Do While busy = 2
    e5$ = f5$
  Loop
End Sub

busy = 1
Thread Start 1, Spin
Thread Start 2, Spin
SetTick 10, Tick
Pause 35
before = n(1) + n(2)
t = ticks
Input "the main program's"; a$
t = ticks - t
ran = n(1) + n(2) - before
SetTick 0, 0
Print
Check "the main program in INPUT: its line", a$ = "first", 1
Check "the busy threads ran while it was typed", ran > 10000, 1
Check "no interrupt was taken in INPUT, and the one that waited came after it", t >= 1 And t <= 2, 1

before = n(1) + n(2)
Thread Start 3, Asker
Thread Wait 3
ran = n(1) + n(2) - before
Print
Check "INPUT in a thread: its string", b$ = "second", 1
Check "and its number", k, 22
Check "the busy threads ran while it was typed", ran > 10000, 1

busy = 2
Thread Start 4, Shorts
Thread Start 3, Liner
Thread Wait 3
Print
Check "LINE INPUT in a thread, another using strings of five", c$ = "a line that is longer than five", 1

busy = 3
Thread Wait 4
Thread Start 4, Texter
Print @(0, 300);
Input "with a thread drawing text"; a$
h = MM.Info(HPos) : v = MM.Info(VPos)
Print
Check "its line", a$ = "fourth", 1
Check "the line typed kept its place: the print position after it", h = 0 And v = 300 + MM.Info(FontHeight), 1
busy = 0
Print "tinput:"; pass; " of"; tests; " passed"
