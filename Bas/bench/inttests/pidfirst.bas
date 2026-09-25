' pidfirst.bas - when do the first PID callbacks arrive after MATH PID START?
' T = 0.5 s, so they should come at about 500, 1000 and 1500 ms
Dim p!(13), t!(2), n% = 0
p!(0) = 1 : p!(8) = 0.5
p!(4) = -100 : p!(5) = 100 : p!(6) = -100 : p!(7) = 100
Math Pid Init 1, p!(), cb
Timer = 0
Math Pid Start 1
Do While n% < 3 : Loop
Math Pid Stop 1
Print "PIDCB"; t!(0); t!(1); t!(2)
End

Sub cb
  Local o! = Math(Pid 1, 0, 0)
  t!(n%) = Timer
  Inc n%
End Sub
