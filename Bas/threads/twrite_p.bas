' twrite_p.bas - threads, Stage P4: each thread has its own FRAMEBUFFER WRITE target (the plan's section 9, decision
' 6). A thread draws in F while the main program draws in N at the same place, each switching to the other between
' its drawing and its reading back; a thread starts with its starter's target; FRAMEBUFFER CLOSE refuses a buffer
' another thread writes to, and closes it once that thread has ended. MODE 2, and MODE 1 back at the end. (The
' pixels are at the foot of the screen: the console's lines are at its head.)
Option Explicit
Dim Integer tests, pass, go, got1, got2, k, bad1, badm
Dim e$

Sub Check(n$, got, want)
  tests = tests + 1
  If got = want Then
    pass = pass + 1
    Print "ok  "; n$
  Else
    Print "FAIL "; n$; " got"; got; " want"; want
  EndIf
End Sub

Sub InF
  Local Integer j
  FrameBuffer Write F
  For j = 0 To 49
    Pixel j, 230, RGB(red)
    Thread Yield
    If Pixel(j, 230) <> RGB(red) Then Inc bad1
  Next
  Do While go : Pause 5 : Loop          ' (still writing to F while the main program tries to close it)
End Sub

Sub Inherit
  got2 = Pixel(300, 230)                    ' the layer's, its starter's target
  FrameBuffer Write N
  Pixel 300, 230, RGB(blue)
End Sub

Mode 2
FrameBuffer Create
FrameBuffer Layer
go = 1
Thread Start 1, InF
For k = 0 To 49
  Pixel k, 230, RGB(green)
  Thread Yield
  If Pixel(k, 230) <> RGB(green) Then Inc badm
Next
Check "a thread's pixels are in its own buffer, F", bad1, 0
Check "and the main program's in N, at the same places", badm, 0
FrameBuffer Write F
Check "F has the thread's", Pixel(20, 230), RGB(red)
FrameBuffer Write N
Check "N has the main program's", Pixel(20, 230), RGB(green)

On Error Skip
FrameBuffer Close F
e$ = MM.ErrMsg$
Check "FRAMEBUFFER CLOSE F is refused while thread 1 writes to it", Instr(e$, "in use by thread 1") > 0, 1
On Error Skip
FrameBuffer Close
e$ = MM.ErrMsg$
Check "and so is FRAMEBUFFER CLOSE", Instr(e$, "in use by thread 1") > 0, 1
go = 0
Thread Wait 1

FrameBuffer Write L
Pixel 300, 230, RGB(yellow)
Thread Start 2, Inherit
Thread Wait 2
Check "a thread starts with its starter's target (L)", got2, RGB(yellow)
Check "and its FRAMEBUFFER WRITE N is its own", Pixel(300, 230), RGB(yellow)
FrameBuffer Write N
Check "it drew in N", Pixel(300, 230), RGB(blue)

On Error Clear
On Error Skip
FrameBuffer Close F
Check "FRAMEBUFFER CLOSE F once the thread has ended", MM.ErrNo, 0
FrameBuffer Close
Mode 1
Print "twrite:"; pass; " of"; tests; " passed"
