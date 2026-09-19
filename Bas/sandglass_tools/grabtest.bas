' grabtest.bas - Phase 4: the ledge-grab rule, exhaustively.
'
' The demo only proved the grab was reachable, not that it decides correctly:
' it passed the gates twice and caught nothing, which is consistent with there
' being no ledge there and equally consistent with the rule being wrong.  A
' negative result proves very little, so this walks every combination instead.
'
' A ledge is catchable when there is clear air above him and a solid floor above
' and in front - the exposed edge.  Three exceptions:
'   a panel without a floor counts as clear in general, but not when he reaches
'     the way its floorpiece faces;
'   a panel with a floor can only be caught from one side;
'   a floor that has already sprung loose cannot be caught at all.

Option Console Serial
Option Explicit
Option Default None

Const TYPES = 30
Const SOLID_BLOCK = 20
Const PANEL_NO_FLOOR = 12
Const PANEL_WITH_FLOOR = 7

Const EXP_CAUGHT = 279
Const EXP_REFUSED = 1521
Const EXP_SUM = 8879441

Dim INTEGER noFloor(31), cFace
Dim INTEGER above, aboveinf, facingLeft, r, ck, nCaught, nRefused, i
Dim FLOAT t0

For i = 0 To 31 : noFloor(i) = 0 : Next i
noFloor(0)=1 : noFloor(9)=1 : noFloor(12)=1 : noFloor(20)=1
noFloor(26)=1 : noFloor(27)=1 : noFloor(28)=1 : noFloor(29)=1

Print "--- ledge grab rule"
ck = 0 : nCaught = 0 : nRefused = 0
t0 = Timer
For above = 0 To TYPES - 1
  For aboveinf = 0 To TYPES - 1
    For facingLeft = 1 To 0 Step -1
      If facingLeft Then cFace = &HFF Else cFace = 0
      r = CanGrab(above, aboveinf)
      If r Then nCaught = nCaught + 1 Else nRefused = nRefused + 1
      ck = (ck * 31 + r) And &HFFFFFF
    Next facingLeft
  Next aboveinf
Next above

Print "  catchable "; Str$(nCaught); "  expected "; Str$(EXP_CAUGHT)
Print "  refused   "; Str$(nRefused); "  expected "; Str$(EXP_REFUSED)
Print "  checksum  "; Str$(ck); "  expected "; Str$(EXP_SUM)
Print "  time      "; Str$(Timer - t0, 0, 1); " ms for 1800 combinations"
Print
If nCaught = EXP_CAUGHT And nRefused = EXP_REFUSED And ck = EXP_SUM Then
  Print "PASS - the grab rule agrees with the host reference exactly"
Else
  Print "FAIL - the engine and the reference disagree"
End If
Option Console Both
End

'-----------------------------------------------------------------------------
' This is the same routine the engine runs, kept identical on purpose.
Function CanGrab(above As INTEGER, aboveinf As INTEGER) As INTEGER
  CanGrab = 0
  If above = SOLID_BLOCK Then Exit Function
  If above = PANEL_NO_FLOOR And (cFace And &H80) = 0 Then Exit Function
  If noFloor(above) = 0 Then Exit Function
  If noFloor(aboveinf) Then Exit Function
  If aboveinf = PANEL_WITH_FLOOR And (cFace And &H80) <> 0 Then Exit Function
  CanGrab = 1
End Function
