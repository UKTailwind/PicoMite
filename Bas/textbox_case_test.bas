' ---------------------------------------------------------------
' GUI TEXTBOX lower-case test
' Assumes a touch LCD already configured via OPTION (e.g. ILI9341)
' and OPTION CONTROLS nn set (once, persisted) - e.g. OPTION CONTROLS 20
'
' Steps:
'   1. Tap the text box  -> the pop-up keyboard appears in UPPER case
'   2. Tap the up-arrow (shift) key, bottom-left of the middle row
'        PASS : the letter keys change to a s d f ... q w e r ...
'        FAIL : the keys change to ! @ # $ ... 1 2 3 4 ...  (the &12 page)
'   3. Type a few letters, tap Ent - the typed string prints on the console
' ---------------------------------------------------------------
Option Explicit

Dim last$ = Chr$(1)
Dim s$

CLS

' #ref, x,  y,   w,   h,   fc,         bc
GUI TEXTBOX #1, 20, 20, 240, 34, RGB(white), RGB(blue)
GUI CAPTION #2, "Tap box, then the up-arrow (shift) key", 20, 70, LT, RGB(white), RGB(black)
GUI CAPTION #3, "PASS = a s d ...   FAIL = ! @ # ...", 20, 90, LT, RGB(yellow), RGB(black)

Print "Type into the text box, then press Ent."
Print "Watch for lower-case letters after pressing shift."

Do
  s$ = CtrlVal(#1)
  If s$ <> last$ Then
    last$ = s$
    Print "TEXTBOX = [" + s$ + "]"
  EndIf
  Pause 50
Loop
