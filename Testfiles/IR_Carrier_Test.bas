' IR_Carrier_Test.bas - Test IR carrier modulation using BITSTREAM with AND logic
' This demonstrates modulating a 38kHz carrier with an envelope signal
' Using the new logic parameter: 0=XOR, 1=AND, 2=OR

Option Base 0
Option Explicit

Const IR_PIN = 1           ' Output pin (GP0)
Const CARRIER_FREQ = 38000 ' 38kHz IR carrier
Const CARRIER_PERIOD = 1000000/CARRIER_FREQ  ' ~26.3us period
Const CARRIER_HIGH = CARRIER_PERIOD/3        ' ~8.8us (33% duty cycle)
Const CARRIER_LOW = CARRIER_PERIOD - CARRIER_HIGH ' ~17.5us

Dim Integer i

' Envelope timing (e.g., NEC protocol preamble)
Dim Float env(4)     ' 2 transitions for one ON pulse
env(0) = 0           ' Start immediately (go HIGH = enable carrier)
env(1) = 9000        ' After 9ms, go LOW (disable carrier)

' Carrier timing - enough cycles for 9ms burst
Dim Integer num_carrier_cycles
num_carrier_cycles = 9000 / CARRIER_PERIOD + 10  ' Add some extra cycles
Print "Carrier cycles for 9ms burst: "; num_carrier_cycles

' Create carrier array with 2 entries per cycle (HIGH then LOW transition)
Dim Float carrier(num_carrier_cycles * 2)

' Build carrier timing array
' Start with time 0 (first HIGH edge)
carrier(0) = 0
For i = 1 To num_carrier_cycles * 2 - 1
  If (i Mod 2) = 1 Then
    ' Odd index: falling edge after HIGH time
    carrier(i) = carrier(i-1) + CARRIER_HIGH
  Else
    ' Even index: rising edge after LOW time
    carrier(i) = carrier(i-1) + CARRIER_LOW
  EndIf
Next i

Print "Carrier array built with "; num_carrier_cycles * 2; " transitions"
Print "Total carrier time: "; carrier(num_carrier_cycles * 2 - 1); " us"
Print
Print "Testing BITSTREAM with AND logic for IR modulation"
Print "Pin "; IR_PIN; ", carrier array, envelope array"
Print

' Channel 1: Carrier (fast toggling)
' Channel 2: Envelope (slow ON/OFF)
' AND logic: Output is HIGH only when BOTH channels are HIGH
' This gates the carrier with the envelope

' Mode 0 = push-pull for both channels (simulated for same-pin)
' Logic 1 = AND

Print "Press any key to transmit 9ms IR burst..."
Do While Inkey$ = "" : Loop

' Both channels control the same pin
' Carrier toggles fast, Envelope enables/disables
' With AND logic: pin is HIGH only when carrier is HIGH AND envelope is HIGH
Device BITSTREAM IR_PIN, num_carrier_cycles * 2, carrier(), 0, IR_PIN, 2, env(), 0, 1

Print "Transmission complete!"
Print
Print "Expected output on scope:"
Print "- 9ms burst of 38kHz carrier"
Print "- Carrier duty cycle: ~33%"
Print "- Carrier gated by envelope (AND logic)"
End
