' verify.bas - checks two statements made in MMBasic_Performance_Guide:
' a FOR loop's limit is fixed when the FOR runs, and SORT against a Shell sort
' written in BASIC on 4000 numbers.
Option Console Serial
Dim Float n, i, cnt, t0, tmp, k
Dim gp%, j%
n = 5 : cnt = 0
For i = 1 To n
  n = 2 : Inc cnt
Next
Print "FORLIMIT "; cnt
Dim Float a(3999), c(3999)
For j% = 0 To 3999 : a(j%) = ((j% * 7919) Mod 4000) + 0.5 : Next
c() = a()
t0 = Timer
Sort c()
Print "SORT4000 "; Timer - t0; " "; c(0); " "; c(3999)
c() = a()
t0 = Timer
gp% = 2000
Do While gp% > 0
  For j% = gp% To 3999
    tmp = c(j%) : k = j%
    Do While k >= gp%
      If c(k - gp%) <= tmp Then Exit Do
      c(k) = c(k - gp%) : k = k - gp%
    Loop
    c(k) = tmp
  Next
  gp% = gp% \ 2
Loop
Print "SHELL4000 "; Timer - t0; " "; c(0); " "; c(3999)
Option Console Both
End
