' Test program to check STATIC variables when structures are passed to functions
Option DEFAULT INTEGER
Option BASE 0

' Define a simple structure
Type MyStruct
  x As integer
  y As integer
End Type

' Create and initialize a structure instance
Dim s As MyStruct
s.x = 10
s.y = 20

' Call the function multiple times to test STATIC behavior
Print "Testing STATIC variable with structure parameter:"
Print "Call 1: result = "; TestFunc(s)
Print "Call 2: result = "; TestFunc(s)
Print "Call 3: result = "; TestFunc(s)
Print "Call 4: result = "; TestFunc(s)

Print
Print "Expected: counter should increment each call (1, 2, 3, 4)"
Print "If counter resets or corrupts, there's a bug"

End

' Function that takes a structure and has a STATIC variable
Function TestFunc(st As MyStruct)
  Static INTEGER counter = 0
  Local INTEGER result

  counter = counter + 1
  result = st.x + st.y + counter

  Print "  Inside function: counter="; counter; " st.x="; st.x; " st.y="; st.y

  TestFunc = result
End Function