' s5t1 - symbols smoke test: names in every role, printed results
Option Explicit
Option Default Integer
Dim integer a, b%, Count, total
Dim float f!, g
Dim string s$, Name$ length 20
Dim arr(10), m(3,3)
Dim string sa$(5) length 10
Const MAXV = 7
Type pt
  x As integer
  ename As string length 8
End Type
Dim pts(2) As pt
Dim p1 As pt
a = 1: b% = 2: Count = 3: COUNT = COUNT + 1
Print "a b count", a, b%, count, MAXV
For a = 1 To 5: total = total + a: Next a
Print "total", total
For a = 1 To 3
  For b% = 1 To 3
    m(a, b%) = a * 10 + b%
  Next b%
Next a
Print "m", m(2,3), m(3,1)
arr(3) = &H1F + &B101 + &O17
Print "hex", arr(3), &hff
s$ = "Hello": Name$ = LCase$(s$) + UCase$(" x")
Print Name$, Left$(s$, 2), Right$(s$, 3), Mid$(s$, 2, 2), Len(s$)
Print Max(3, 9, 2), Min(4, 1), Hex$(255), Bin$(5)
f! = 1.5E2: g = 2.5e-1
Print "floats", f!, g, Sqr(16), Int(f! / 7)
Print "rgb", Hex$(RGB(RED)), Hex$(RGB(0, 128, 255))
p1.x = 5: pts(1).x = 7: pts(1).ename = "ex"
Print "type", p1.x, pts(1).x, pts(1).ename
Do While Count < 10: Count = Count + 2: Loop
Print "do", Count
Do: Count = Count - 3: Loop Until Count < 0
Print "until", Count
Select Case total
  Case Is > 100: Print "big"
  Case 10 To 20: Print "mid"
  Case Else: Print "else"
End Select
If total = 15 Then Print "if1" Else Print "else1"
If total = 15 Then show "then sub"
If total <> 15 Then
  Print "wrong"
ElseIf total = 15 Then
  Print "elseif"
EndIf
GoSub lab1
Print "back from gosub"
Print "sq", Square(7), Square(Square(2))
Addup 3, 4, total
Print "addup", total
Print "static", Counter(), Counter(), Counter()
Restore mydata
Read a, s$, b%
Print "read", a, s$, b%
sa$(2) = "two": Print sa$(2)
On 2 GoSub l1, l2, l3
Print "on done"
Local_test
Print "str", Str$(3.25), Val("12") + 1, Chr$(65), Asc("B")
Print "instr", Instr("abcdef", "cd"), Field$("a,b,c", 2, ",")
Print "done"
End
lab1:
  Print "in gosub"
  Return
l1: Print "l1": Return
l2: Print "l2": Return
l3: Print "l3": Return
mydata:
Data 42, HelloWorld, 7
Sub show msg$
  Print "show:"; msg$
End Sub
Function Square(x)
  Square = x * x
End Function
Sub Addup(p, q, ByRef r)
  r = p + q
End Sub
Function Counter()
  Static n
  n = n + 1
  Counter = n
End Function
Sub Local_test
  Local integer a, total = 99
  a = 5
  Print "local", a, total
End Sub
