' ShareClient.bas - Simultaneous Host + Client test (Board B)
' Tests bidirectional 4-bit MEMORY SHARE between two PicoMites.
'
' Board B (this program):        Board A (ShareHost.bas):
'   CLIENT on GP0-GP3 + GP22 <--  HOST  on GP0-GP3 + GP22   (A sends to B)
'   HOST  on GP4-GP7 + GP26  -->  CLIENT on GP4-GP7 + GP26   (B sends to A)
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

' RX buffer: 100 integers (800 bytes) received from Board A
Dim rx%(99)
Dim rxaddr% = Peek(VarAddr rx%())

' TX buffer: 100 integers (800 bytes) sent to Board A
Dim tx%(99)
Dim txaddr% = Peek(VarAddr tx%())

Print "BOARD B: Simultaneous Host + Client (4-bit)"
Print "RX buffer at &h" Hex$(rxaddr%) " (800 bytes)"
Print "TX buffer at &h" Hex$(txaddr%) " (800 bytes)"
Print

' Fill TX with initial data (offset by 1000 so we can distinguish from Board A)
Dim i%
For i% = 0 To 99
  tx%(i%) = 1000 + i%
Next

' Clear RX
For i% = 0 To 99
  rx%(i%) = -1
Next

' Start CLIENT: PIO 1, data GP0, clock GP22, 800 bytes, 4-bit
Memory Share Client 1, GP0, GP22, rxaddr%, 800, 4

' Start HOST: PIO 1, data GP4, clock GP26, 800 bytes, div 10, 4-bit
Memory Share Host 1, GP4, GP26, txaddr%, 800, 10, 4

Print "Client on PIO1/SM1: GP0-GP3 data, GP22 clock (receiving)"
Print "Host on PIO1/SM0: GP4-GP7 data, GP26 clock (sending)"
Print "Press any key to stop"
Print

Dim counter% = 0
Do
  ' Update TX buffer
  For i% = 0 To 99
    tx%(i%) = 1000 + counter% + i%
  Next
  Print "TX: counter=" Str$(1000 + counter%);
  Print "  RX: [0]=" Hex$(rx%(0));
  Print " [50]=" Hex$(rx%(50));
  Print " [99]=" Hex$(rx%(99))
  counter% = counter% + 100
  Pause 500
Loop Until Inkey$ <> ""

Memory Share Stop
Print "Share stopped."
