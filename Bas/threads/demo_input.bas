' demo_input.bas - threads: four balls and a clock are threads, and they keep going while the main program waits in
' INPUT for a line to be typed. Not one of the automatic tests: something to run and watch. The manual's "Balls and a
' clock while the user types" (CMM2 docs/threads/text/09_examples.md); on the PicoMite in MODE 3 (MODE 1 on HDMI has
' one colour for each 8x8 tile).
Option Explicit
Dim Integer i, go = 1
Dim a$
Dim Integer col(4) = (0, RGB(red), RGB(green), RGB(yellow), RGB(cyan))

Sub Ball(x As Float, y As Float, dx As Float, dy As Float, c As Integer)
  Do While go
    Circle x, y, 5, 1, 1, 0, 0                    ' rub the old one out
    x = x + dx : y = y + dy
    If x < 10 Or x > MM.HRes - 10 Then dx = -dx
    If y < 130 Or y > MM.VRes - 10 Then dy = -dy
    Circle x, y, 5, 1, 1, c, c
    Pause 20
  Loop
End Sub

Sub Clock
  Do While go
    Text MM.HRes - 1, 0, Time$, "RT", 1, 1, RGB(white), RGB(blue)
    Pause 250
  Loop
End Sub

Mode 3
For i = 1 To 4
  Thread Start i Size 12 Locals 8, Ball, 100 * i, 150 + 60 * i, 2 + i, 1 + i, col(i)
Next
Thread Start 5 Size 12 Locals 8, Clock
Do
  Print @(0, 40) Space$(70);
  Print @(0, 40);
  Input "Type something (Q to quit)"; a$
  Print @(0, 70) "You typed: "; a$; Space$(60)
Loop Until UCase$(a$) = "Q"
go = 0
End
