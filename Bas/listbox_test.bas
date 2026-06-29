' ---------------------------------------------------------------
' GUI LISTBOX test  -  pick an item from a scrollable popup list
' Assumes a touch LCD already configured via OPTION (e.g. ILI9341).
' Needs OPTION CONTROLS nn set (once, persisted) - e.g. OPTION CONTROLS 20
' ---------------------------------------------------------------
Option Explicit

Dim integer i, lastsel = -2

' The backing array MUST stay in scope for the life of the control,
' so declare it at the program (module) level - never inside a SUB.
' 12 items (index 0..11) vs maxrows 5 -> the popup has to scroll.
Dim items$(11) Length 20
For i = 0 To 11
  items$(i) = "Item " + Str$(i + 1)
Next i

CLS

' #ref, array$(),  x,  y,   w,  h,  fc,         bc,        maxrows
GUI LISTBOX    #1, items$(), 40, 30, 200, 26, RGB(white), RGB(blue), 5
GUI CAPTION    #2, "You picked:", 40, 80, LT, RGB(white), RGB(black)
GUI DISPLAYBOX #3, 40, 100, 200, 26, RGB(green), RGB(black)

' Show the initial selection
CtrlVal(#3) = items$(CtrlVal(#1))

Do
  If CtrlVal(#1) <> lastsel Then          ' selection changed?
    lastsel = CtrlVal(#1)
    If lastsel >= 0 Then
      CtrlVal(#3) = items$(lastsel)       ' index -> item text (program owns the array)
      Print "Selected index "; lastsel; " = "; items$(lastsel)
    Else
      CtrlVal(#3) = ""
    EndIf
  EndIf
Loop
