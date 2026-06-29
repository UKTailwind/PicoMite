' ---------------------------------------------------------------
' GUI SLIDER test  -  drag a thumb to set an analog value
' Assumes a touch LCD already configured via OPTION (e.g. ILI9341).
' Needs OPTION CONTROLS nn set (once, persisted) - e.g. OPTION CONTROLS 20
' ---------------------------------------------------------------
Option Explicit

Dim string shown = "", s

CLS

' Orientation is taken from the shape: wider than tall = horizontal,
' taller than wide = vertical (with the maximum at the top).

' #ref,  x,   y,   w,   h,  fc,          bc,         min, max, inc
GUI SLIDER #1, 40,  40, 220, 30, RGB(cyan),   RGB(black),  0,  100      ' continuous 0..100
GUI SLIDER #2, 40, 110, 220, 30, RGB(yellow), RGB(black),  0,  10,  1   ' snaps to whole steps
GUI SLIDER #3, 280, 40,  30, 180, RGB(green),  RGB(black),  0, 255       ' vertical

GUI DISPLAYBOX #4, 40, 170, 220, 26, RGB(white), RGB(black)

Do
  s = "H1=" + Str$(CtrlVal(#1), 0, 0) + "  Step=" + Str$(CtrlVal(#2), 0, 0) + "  V=" + Str$(CtrlVal(#3), 0, 0)
  If s <> shown Then           ' update only when something moved
    shown = s
    CtrlVal(#4) = s
  EndIf
Loop
