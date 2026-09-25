Sub k1(n%, s%)
  Local i%
  s% = 0
  For i% = 1 To n%
    s% = s% + (i% And 7) * 3
    If s% > 1000000 Then s% = s% - 1000000
  Next i%
End Sub

Sub k2(a%(), n%)
  Local i%, j%, v%
  For i% = 1 To n% - 1
    v% = a%(i%)
    j% = i% - 1
    Do While j% >= 0
      If a%(j%) <= v% Then Exit Do
      a%(j% + 1) = a%(j%)
      j% = j% - 1
    Loop
    a%(j% + 1) = v%
  Next i%
End Sub

Sub k3(w, h, xd, yd, rOfs, iOfs, cRe, cIm, mit, ck)
  Local X, Y, CX, CY, Zr, Zi, COUNT, new_Zr, new_Zi
  For X = 0 To (w - 1)
    CX = X * xd + rOfs
    For Y = 0 To (h - 1)
      CY = Y * yd + iOfs
      Zr = CX
      Zi = CY
      COUNT = 0
      Do While ((COUNT <= mit) And ((Zr * Zr + Zi * Zi) < 4))
        new_Zr = Zr * Zr - Zi * Zi + cRe
        new_Zi = 2 * Zr * Zi + cIm
        Zr = new_Zr
        Zi = new_Zi
        COUNT = COUNT + 1
      Loop
      ck = ck + COUNT
    Next Y
  Next X
End Sub

Sub findleap(jday, leapsecond)
  Local i As integer
  If (jday <= jdleap(1)) Then
    leapsecond = leapsec(1)
    Exit Sub
  End If
  If (jday >= jdleap(28)) Then
    leapsecond = leapsec(28)
    Exit Sub
  End If
  For i = 1 To 27
    If (jday >= jdleap(i) And jday < jdleap(i + 1)) Then
      leapsecond = leapsec(i)
      Exit Sub
    End If
  Next i
End Sub

Sub k4(n%, tot)
  Local k%, jd, ls
  tot = 0
  For k% = 1 To n%
    jd = 2441000 + (k% Mod 17000)
    findleap jd, ls
    tot = tot + ls
  Next k%
End Sub
