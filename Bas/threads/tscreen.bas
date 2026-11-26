' tscreen.bas - threads, Stage 4: each thread has its own print position, colours and font, and starts with those of
' the thread that started it. (They were one for the program: a thread's TEXT then moved the place the main program's
' INPUT was echoing its line at.)
Option Explicit
Dim Integer tests, pass, phase, fc, bc, fn, hp, vp, tfc, tbc, tfn, thp, tvp

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub Mine                                ' the running thread's own, as it finds them
  tfc = MM.Info(FColour) : tbc = MM.Info(BColour) : tfn = MM.Info(Font) : thp = MM.Info(HPos) : tvp = MM.Info(VPos)
End Sub

Sub Painter                             ' changes every one of its own, and stays
  Colour RGB(red), RGB(blue)
  Font 2
  Print @(200, 300) "x";
  Text 400, 10, "text elsewhere"
  phase = 1
  Do While phase < 2 : Pause 5 : Loop
  Mine
  phase = 3
End Sub

Sub Heir
  Mine
End Sub

Colour RGB(white), RGB(black)
Font 1
Print @(0, 100);
Thread Start 1, Painter
Do While phase < 1 : Pause 5 : Loop
fc = MM.Info(FColour) : bc = MM.Info(BColour) : fn = MM.Info(Font) : hp = MM.Info(HPos) : vp = MM.Info(VPos)
Colour RGB(green)                       ' and the main program changes its own while the thread waits
Print @(40, 200);
phase = 2
Do While phase < 3 : Pause 5 : Loop
phase = 0
Dim Integer pfc = tfc, pbc = tbc, pfn = tfn, php = thp, pvp = tvp     ' what the first thread had kept
Thread Start 2, Heir                    ' with the main program's green, font 1 and place
Thread Wait 2
Colour RGB(white), RGB(black)
Print @(0, 320);
Check "a thread's COLOUR is not the main program's", fc = RGB(white) And bc = RGB(black), 1
Check "nor its FONT", fn, 1
Check "nor its print position, after its PRINT and its TEXT", hp = 0 And vp = 100, 1
Check "the thread kept its own colours while the main program changed its own", pfc = RGB(red) And pbc = RGB(blue), 1
Check "and its font", pfn, 2
Check "and its print position, where its own TEXT left it", php > 400 And pvp = 10, 1
Check "a new thread starts with its starter's colours", tfc = RGB(green) And tbc = RGB(black), 1
Check "its font", tfn, 1
Check "and its print position", thp = 40 And tvp = 200, 1
Print "tscreen:"; pass; " of"; tests; " passed"
