' SdCopyBTest.bas - SD-card I/O straight after a background framebuffer copy
'
' Needs a board whose SD card shares the system SPI with the display, for
' example OPTION RESET PICO-RESTOUCH-LCD-3.5, with a card in the slot.
'
' FRAMEBUFFER COPY F,N,B returns as soon as core 1 has been told to burst the
' panel.  The next SD-card operation used to drive the same SPI mid-burst and
' fail with an I/O error.  Each pass does a copy and then, with no pause, a
' file read and a LOAD BMP.  Expect "fails 0" on a fixed build and a non-zero
' count on 6.03.02b12 and earlier.
'
' The BMP is written by this program (a 64 x 64 24-bit file) rather than by
' SAVE IMAGE, so the test does not depend on the panel being readable.
'
Option Explicit
Option Default None
Const PASSES = 20
Const DATFILE = "B:/copyb.dat"
Const BMPFILE = "B:/copyb.bmp"
Const BW = 64, BH = 64
Dim Integer n, i, x, y, fails, readfails, bmpfails, bytes
Dim s$, row$

Sub Put32(v As Integer)
  Print #1, Chr$(v And 255); Chr$((v >> 8) And 255); Chr$((v >> 16) And 255); Chr$((v >> 24) And 255);
End Sub
Sub Put16(v As Integer)
  Print #1, Chr$(v And 255); Chr$((v >> 8) And 255);
End Sub

CLS
FrameBuffer Create
FrameBuffer Write F
For i = 0 To 40
  Box i * 8, i * 5, 60, 40, 1, RGB(i * 6, 255 - i * 6, 128), RGB(255 - i * 6, i * 3, i * 6)
Next i
FrameBuffer Write N

' the data the passes read back: 16 KB, so the read outlasts any burst tail
Open DATFILE For Output As #1
For i = 1 To 64
  Print #1, String$(255, Chr$(65 + (i Mod 26)))
Next i
Close #1

' a 24-bit BMP, bottom-up, rows are BW*3 bytes = a multiple of 4 already
Open BMPFILE For Output As #1
Print #1, "BM";
Put32 54 + BW * BH * 3 : Put32 0 : Put32 54
Put32 40 : Put32 BW : Put32 BH : Put16 1 : Put16 24
Put32 0 : Put32 BW * BH * 3 : Put32 2835 : Put32 2835 : Put32 0 : Put32 0
For y = 0 To BH - 1
  row$ = ""
  For x = 0 To BW - 1
    row$ = row$ + Chr$(x * 4 And 255) + Chr$(y * 4 And 255) + Chr$(128)
  Next x
  Print #1, row$;
Next y
Close #1

fails = 0 : readfails = 0 : bmpfails = 0
For n = 1 To PASSES
  FrameBuffer Copy F, N, B
  On Error Ignore
  bytes = 0
  Open DATFILE For Input As #1
  If MM.ErrNo = 0 Then
    Do While Not Eof(#1)
      s$ = Input$(255, #1)
      bytes = bytes + Len(s$)
    Loop
    Close #1
  EndIf
  If MM.ErrNo <> 0 Or bytes < 16000 Then
    readfails = readfails + 1
    Print "pass"; n; " read: "; MM.ErrMsg$; " bytes"; bytes
  EndIf
  On Error Abort

  FrameBuffer Copy F, N, B
  On Error Ignore
  Load BMP BMPFILE, 10, 10
  If MM.ErrNo <> 0 Then
    bmpfails = bmpfails + 1
    Print "pass"; n; " bmp: "; MM.ErrMsg$
  EndIf
  On Error Abort
Next n

FrameBuffer Close
fails = readfails + bmpfails
Print "read fails"; readfails; " bmp fails"; bmpfails
Print "fails"; fails
If fails = 0 Then Print "PASS" Else Print "FAIL"
