' NEC IR Protocol Test - Address 123, Data 107
' Uses dual bitstream on same pin with AND logic for carrier gating
' Pin GP15 used for both carrier and envelope
'
' NEC Protocol:
' - 9ms leading pulse burst (38kHz carrier)
' - 4.5ms space
' - 8-bit address (LSB first)
' - 8-bit inverted address
' - 8-bit command
' - 8-bit inverted command
' - 562.5us final pulse
'
' Bit encoding:
' - Logical 0: 562.5us pulse + 562.5us space
' - Logical 1: 562.5us pulse + 1687.5us space
'
' 38kHz carrier: 26.3us period (13.15us on, 13.15us off)
'
' AND Logic modulation:
' - Carrier channel: continuous 38kHz toggling (state alternates HIGH/LOW)
' - Envelope channel: HIGH during pulses, LOW during spaces
' - Output = Carrier AND Envelope
' - When envelope is LOW, output stays LOW (no carrier)
' - When envelope is HIGH, output follows carrier (38kHz appears)

Const IR_PIN = 15
Const CARRIER_PERIOD = 13    ' microseconds (half period for 38kHz)
Const PULSE_562 = 562        ' microseconds
Const SPACE_562 = 562
Const SPACE_1687 = 1687
Const LEADER_PULSE = 9000
Const LEADER_SPACE = 4500

Const ADDRESS = 123
Const COMMAND = 107

' Build the envelope timing array
' Each entry is the time until the next state toggle
' Envelope starts LOW (disabled), first toggle enables carrier

Dim envelope(100) As Integer
Dim envIdx As Integer = 0

' Leader: 9ms pulse, 4.5ms space
envelope(envIdx) = 0 : envIdx = envIdx + 1              ' t=0: LOW->HIGH (enable carrier)
envelope(envIdx) = LEADER_PULSE : envIdx = envIdx + 1   ' t=9000: HIGH->LOW (disable for space)
envelope(envIdx) = LEADER_SPACE : envIdx = envIdx + 1   ' t=13500: LOW->HIGH (enable for bits)

' Helper to add a bit (pulse + space)
' Called while envelope is HIGH (carrier enabled)
' Need to: stay HIGH for pulse time, go LOW for space time, go HIGH for next pulse
Sub AddBit(bitVal As Integer)
  Local space As Integer
  If bitVal = 0 Then
    space = SPACE_562
  Else
    space = SPACE_1687
  EndIf
  ' After pulse duration, turn off carrier for space
  envelope(envIdx) = PULSE_562 : envIdx = envIdx + 1   ' HIGH->LOW after pulse
  ' After space duration, turn on carrier for next pulse
  envelope(envIdx) = space : envIdx = envIdx + 1       ' LOW->HIGH after space
End Sub

' Address bits (LSB first)
Dim i As Integer
For i = 0 To 7
  AddBit((ADDRESS >> i) And 1)
Next i

' Inverted address bits
Dim invAddr As Integer = (Not ADDRESS) And &HFF
For i = 0 To 7
  AddBit((invAddr >> i) And 1)
Next i

' Command bits (LSB first)
For i = 0 To 7
  AddBit((COMMAND >> i) And 1)
Next i

' Inverted command bits
Dim invCmd As Integer = (Not COMMAND) And &HFF
For i = 0 To 7
  AddBit((invCmd >> i) And 1)
Next i

' Final pulse: we're already HIGH from last space->HIGH transition
' Wait pulse time then go LOW to end
envelope(envIdx) = PULSE_562 : envIdx = envIdx + 1     ' HIGH->LOW (end final pulse)
' Envelope now LOW - carrier will be gated off
' envIdx should be even (started at 0, each bit adds 2, plus 3 for leader, plus 1 for final = even)

' Calculate total transmission time for carrier array
Dim totalTime As Integer = 0
For i = 0 To envIdx - 1
  totalTime = totalTime + envelope(i)
Next i

' Build carrier array (38kHz toggles for entire duration plus margin)
' Carrier starts LOW, first toggle at t=0 goes HIGH
' CARRIER_PERIOD is half-period (13us), so transitions needed = totalTime / CARRIER_PERIOD
Dim carrierCount As Integer = (totalTime \ CARRIER_PERIOD) + 50
Dim carrier(carrierCount) As Integer
carrier(0) = 0  ' First toggle at t=0 (LOW->HIGH)
For i = 1 To carrierCount - 1
  carrier(i) = CARRIER_PERIOD
Next i

Print "NEC IR Test with AND Logic"
Print "Address: "; ADDRESS; " (0x"; Hex$(ADDRESS, 2); ")"
Print "Command: "; COMMAND; " (0x"; Hex$(COMMAND, 2); ")"
Print "Envelope transitions: "; envIdx
Print "Carrier transitions: "; carrierCount
Print "Total time: "; totalTime; " us"
Print
Print "Transmitting on GP"; IR_PIN; "..."

' Configure pin
SetPin IR_PIN, DOUT
Pin(IR_PIN) = 0  ' Start LOW

' Use BITSTREAM with AND logic (parameter = 1)
' Channel 1: Carrier (38kHz continuous toggle)
' Channel 2: Envelope (gates the carrier on/off)
' AND logic: output HIGH only when BOTH carrier AND envelope are HIGH
BITSTREAM IR_PIN, carrierCount, carrier(), 0, IR_PIN, envIdx, envelope(), 0, 1

Print "Transmission complete!"
End
