' Test Str$() with struct string members
Option Base 0
Option Explicit

Type TTest
  name As String Length 20
End Type

Dim t As TTest
Dim s$
Dim i%

i% = 2

' Test Str$() directly
s$ = Str$(i%)
Print "Str$(2) = '"; s$; "' length="; Len(s$)

' Test concatenation
s$ = "Rect" + Str$(i%)
Print "'Rect' + Str$(2) = '"; s$; "'"

' Test assignment to struct member
t.name = "Rect" + Str$(i%)
Print "t.name = '"; t.name; "'"

' Test with explicit space
t.name = "Rect " + Str$(i%)
Print "t.name (with space) = '"; t.name; "'"

' Test direct assignment
t.name = "Test String"
Print "t.name direct = '"; t.name; "'"

End
