' PS2 Keyboard Scan Code Transmitter
' Uses dual bitstream with two pins in open-collector mode
' Simulates PS2 device-to-host transmission
'
' PS2 Protocol (device to host):
' - Clock: 10-16kHz, open-collector
' - Data: open-collector, changes when clock is HIGH
' - Data sampled on falling edge of clock
'
' Frame format (11 bits):
' 1. Start bit: DATA = 0
' 2. Data bits 0-7: LSB first
' 3. Parity bit: Odd parity
' 4. Stop bit: DATA = 1

Const CLK_PIN = 19    ' PS2 Clock pin
Const DAT_PIN = 20    ' PS2 Data pin

' PS2 timing (microseconds)
Const CLK_LOW = 40    ' Clock low time
Const CLK_HIGH = 40   ' Clock high time
Const INIT_DELAY = 50 ' Initial delay before first clock

' Initialize pins (call once at startup)
Sub PS2_Init
  SetPin CLK_PIN, DOUT
  SetPin DAT_PIN, DOUT
End Sub

' Transmit a PS2 scan code
' scanCode: 8-bit scan code to send
Sub PS2_Send(scanCode As Integer)
  Local bits(10) As Integer
  Local clkTiming(30) As Integer
  Local datTiming(30) As Integer
  Local clkIdx As Integer = 0
  Local datIdx As Integer = 0
  Local currentData As Integer = 1
  Local dataTime As Integer = 0
  Local bitVal As Integer
  Local i As Integer
  Local parity As Integer
  Local count As Integer = 0
  
  ' Calculate parity (odd parity)
  For i = 0 To 7
    If (scanCode >> i) And 1 Then count = count + 1
  Next i
  parity = (count + 1) And 1
  
  ' Build the 11-bit frame
  bits(0) = 0  ' Start bit
  For i = 0 To 7
    bits(i + 1) = (scanCode >> i) And 1
  Next i
  bits(9) = parity
  bits(10) = 1  ' Stop bit
  
  ' First bit - data changes before first clock falling edge
  bitVal = bits(0)
  If bitVal <> currentData Then
    datTiming(datIdx) = INIT_DELAY - 5 : datIdx = datIdx + 1
    currentData = bitVal
    dataTime = 5
  Else
    dataTime = INIT_DELAY
  EndIf
  
  ' First clock transitions
  clkTiming(clkIdx) = INIT_DELAY : clkIdx = clkIdx + 1
  clkTiming(clkIdx) = CLK_LOW : clkIdx = clkIdx + 1
  
  ' Process remaining bits (1-10)
  For i = 1 To 10
    bitVal = bits(i)
    
    If bitVal <> currentData Then
      datTiming(datIdx) = dataTime + CLK_LOW + 5 : datIdx = datIdx + 1
      currentData = bitVal
      dataTime = CLK_HIGH - 5
    Else
      dataTime = dataTime + CLK_LOW + CLK_HIGH
    EndIf
    
    clkTiming(clkIdx) = CLK_HIGH : clkIdx = clkIdx + 1
    clkTiming(clkIdx) = CLK_LOW : clkIdx = clkIdx + 1
  Next i
  
  ' Return data to HIGH if needed
  If currentData = 0 Then
    datTiming(datIdx) = dataTime + CLK_LOW + CLK_HIGH : datIdx = datIdx + 1
  EndIf
  
  ' Ensure even number of data transitions
  If (datIdx And 1) = 1 Then
    datTiming(datIdx) = 50 : datIdx = datIdx + 1
  EndIf
  
  ' Transmit
  Device BITSTREAM CLK_PIN, clkIdx, clkTiming(), 1, DAT_PIN, datIdx, datTiming(), 1
End Sub

' ============ Test Program ============

Print "PS2 Scan Code Transmitter"
Print "Initializing..."
PS2_Init
Pause 1000

Print "Sending 'A' (0x1C)..."
PS2_Send(&H1C)
Print "Done!"
Pause 500

Print "Sending 'B' (0x32)..."
PS2_Send(&H32)
Print "Done!"
Pause 500

Print "Sending 'C' (0x21)..."
PS2_Send(&H21)
Print "Done!"

Print
Print "All transmissions complete!"
End