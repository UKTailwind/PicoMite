' Dual Bitstream Test - Open Collector Mode
' Test two pins with open-collector (mode=1)

Const PIN1 = 19
Const PIN2 = 20

' Timing arrays (microseconds)
Dim timing1(10) As Integer
Dim timing2(10) As Integer

' Pin 1: 4 transitions at 100us intervals
timing1(0) = 100
timing1(1) = 100
timing1(2) = 100
timing1(3) = 100

' Pin 2: 2 transitions at 200us intervals
timing2(0) = 200
timing2(1) = 200

Print "Dual Bitstream Test - Open Collector"
Print "Pin "; PIN1; ": 4 @ 100us"
Print "Pin "; PIN2; ": 2 @ 200us"
Print

SetPin PIN1, DOUT
SetPin PIN2, DOUT
Pause 1000

Print "Running..."
Device BITSTREAM PIN1, 4, timing1(), 1, PIN2, 2, timing2(), 1

Print "Done!"
Print "Pin "; PIN1; " = "; Pin(PIN1)
Print "Pin "; PIN2; " = "; Pin(PIN2)
End
