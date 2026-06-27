' ShareHost.bas - Simultaneous Host + Client test (Board A)
' Tests bidirectional 4-bit MEMORY SHARE between two PicoMites.
'
' Board A (this program):        Board B (ShareClient.bas):
'   HOST  on GP0-GP3 + GP22  -->  CLIENT on GP0-GP3 + GP22   (A sends to B)
'   CLIENT on GP4-GP7 + GP26 <--  HOST  on GP4-GP7 + GP26   (B sends to A)
'
' Wiring between Board A and Board B:
'   A GP0  --- B GP0     (data bit 0, channel A->B)
'   A GP1  --- B GP1     (data bit 1, channel A->B)
'   A GP2  --- B GP2     (data bit 2, channel A->B)
'   A GP3  --- B GP3     (data bit 3, channel A->B)
'   A GP22 --- B GP22    (clock, channel A->B)
'   A GP4  --- B GP4     (data bit 0, channel B->A)
'   A GP5  --- B GP5     (data bit 1, channel B->A)
'   A GP6  --- B GP6     (data bit 2, channel B->A)
'   A GP7  --- B GP7     (data bit 3, channel B->A)
'   A GP26 --- B GP26    (clock, channel B->A)
'   GND    --- GND

Option Base 0
On Error Skip
Memory Share Stop

' TX buffer: 100 integers (800 bytes) sent to Board B
Dim tx%(99)
Dim txaddr% = Peek(VarAddr tx%())

' RX buffer: 100 integers (800 bytes) received from Board B
Dim rx%(99)
Dim rxaddr% = Peek(VarAddr rx%())

Print "BOARD A: Simultaneous Host + Client (4-bit)"
Print "TX buffer at &h" Hex$(txaddr%) " (800 bytes)"
Print "RX buffer at &h" Hex$(rxaddr%) " (800 bytes)"
Print

' Fill TX with initial data
Dim i%
For i% = 0 To 99
  tx%(i%) = i%
Next

' Clear RX
For i% = 0 To 99
  rx%(i%) = -1
Next

' Start HOST: PIO 1, data GP0, clock GP22, 800 bytes, div 10, 4-bit
Memory Share Host 1, GP0, GP22, txaddr%, 800, 10, 4

' Start CLIENT: PIO 1, data GP4, clock GP26, 800 bytes, 4-bit
Memory Share Client 1, GP4, GP26, rxaddr%, 800, 4

Print "Host on PIO1/SM0: GP0-GP3 data, GP22 clock (sending)"
Print "Client on PIO1/SM1: GP4-GP7 data, GP26 clock (receiving)"
Print "Press any key to stop"
Print

Dim counter% = 0
Do
  ' Update TX buffer
  For i% = 0 To 99
    tx%(i%) = counter% + i%
  Next
  Print "TX: counter=" Str$(counter%);
  Print "  RX: [0]=" Hex$(rx%(0));
  Print " [50]=" Hex$(rx%(50));
  Print " [99]=" Hex$(rx%(99))
  counter% = counter% + 100
  Pause 500
Loop Until Inkey$ <> ""

Memory Share Stop
Print "Share stopped."
