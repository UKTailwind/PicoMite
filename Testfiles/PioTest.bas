' PicoMite BASIC PIO regression, firmware 9f9bc51 / API 20.
' Hardware test overwrites the selected PIO instruction memory.
' Selects the first idle PIO block using BASIC PIO(IDLE).
' No GPIO wiring. BASIC INTEGER is 64 bits, DMA words are 32 bits.
Option Explicit
Option Default Integer
Option Base 0

Dim Integer p%, sm%, i%, failures%, passes%, got%
Dim Integer fhits%, dhits%, t%, ok%, phase%, bits%, n%
Dim Integer tx%(7), rx%(7), prog%(7), rt%, rr%

Print "BASIC PIO regression - 9f9bc51"
Chk "PINCTRL", PIO(PINCTRL 3,2,7,GP11,GP12,GP13,GP14), &H6875B1AE
Chk "EXECCTRL", PIO(EXECCTRL GP11,3,27,1,1), &H6B01B180
Chk "SHIFTCTRL", PIO(SHIFTCTRL 14,29,1,1,1,0,1,0), &H7AE70000

p% = -1
For i% = 0 To 2
  ok% = PIO(IDLE i%)
  Print "PIO";i%;": idle=";ok%
  If p% = -1 And ok% Then p% = i%
Next i%
If p% = -1 Then
  Print "SKIP: no available idle PIO block"
  GoTo Summary
EndIf

sm% = 0
Print "WARNING: clearing PIO"; p%; " instruction RAM!"
PIO CLEAR p%
PIO ASSEMBLE p%, ".PROGRAM REGTEST"
PIO ASSEMBLE p%, "PULL BLOCK"
PIO ASSEMBLE p%, "MOV ISR, OSR"
PIO ASSEMBLE p%, "PUSH BLOCK"
PIO ASSEMBLE p%, "JMP 0"
PIO ASSEMBLE p%, ".END PROGRAM"
Chk "ASSEMBLE / NEXT LINE", PIO(NEXT LINE p%), 4

' Load the same four-word loopback via BASIC PIO PROGRAM / PROGRAM LINE.
' PIO PROGRAM reads 32 16-bit instructions from 8 INTEGERs.
For i% = 0 To 7 : prog%(i%) = 0 : Next i%
prog%(0) = &H00008020A0C780A0
PIO PROGRAM p%,prog%()
PIO PROGRAM LINE p%,3,0
ResetSM
PIO SYNC p%,1
PIO WRITE p%,sm%,1,&H11223344
t% = Timer
Do While PIO(FLEVEL p%,sm%,RX) = 0 And Timer - t% < 1000
Loop
PIO READ p%,sm%,1,got%
Chk "PROGRAM/LINE/SYNC", got%, &H11223344

' INIT MACHINE / EXECUTE go through the original BASIC command path.
PIO STOP p%,sm%
PIO INIT MACHINE p%,sm%,15000000
PIO EXECUTE p%,sm%,0
PIO START p%,sm%
PIO WRITE p%,sm%,1,&H55667788
t% = Timer
Do While PIO(FLEVEL p%,sm%,RX) = 0 And Timer - t% < 1000
Loop
PIO READ p%,sm%,1,got%
Chk "INIT MACHINE / EXECUTE", got%, &H55667788

' Restore assembly route for the remainder.
Print "PHASE: reassemble"
PIO CLEAR p%
PIO ASSEMBLE p%, ".PROGRAM REGTEST2"
PIO ASSEMBLE p%, "PULL BLOCK"
PIO ASSEMBLE p%, "MOV ISR, OSR"
PIO ASSEMBLE p%, "PUSH BLOCK"
PIO ASSEMBLE p%, "JMP 0"
PIO ASSEMBLE p%, ".END PROGRAM"

For i% = 0 To 7
  tx%(i%) = &H7000000 + i%
  rx%(i%) = -1
Next i%
' Distinct final DMA word: the last upper half is nonzero.
tx%(7) = &H0700000007000007
Print "PHASE: ring buffers"
PIO MAKE RING BUFFER rt%,256
PIO MAKE RING BUFFER rr%,256
rt%(0) = &H11220001
rt%(1) = &H11220002

' FIFO and BASIC RX interrupt. The handler MUST drain RX FIFO.
Print "PHASE: FIFO interrupt"
ResetSM
fhits% = 0
PIO INTERRUPT p%,sm%,FifoEvent
PIO WRITE p%,sm%,1,&H13579BDF
t% = Timer
Do While fhits% = 0 And Timer - t% < 1000
Loop
PIO INTERRUPT p%,sm%,0
Chk "FIFO interrupt", (fhits% > 0) And 1, 1
Chk "FIFO read in IRQ", got%, &H13579BDF
' Check ordinary PIO READ independently of the interrupt.
PIO WRITE p%,sm%,1,&H2468ACE0
t% = Timer
Do While PIO(FLEVEL p%,sm%,RX) = 0 And Timer - t% < 1000
Loop
PIO READ p%,sm%,1,got%
Chk "FIFO read", got%, &H2468ACE0
PIO READ p%,sm%,1,got%
Chk "FIFO empty returns -1", got%, -1
Chk "FLEVEL RX empty", PIO(FLEVEL p%,sm%,RX), 0
Chk "FSTAT RXEMPTY", (PIO(FSTAT p%) >> (8 + sm%)) And 1, 1
Print "FDEBUG = &H"; Hex$(PIO(FDEBUG p%))

' Single-shot RX+TX, all supported DMA widths: 8/16/32 bits.
' A BASIC INTEGER holds 8 bytes; each width copies 16 * bits / 64 ints.
For phase% = 0 To 2
  Select Case phase%
    Case 0 : bits% = 8
    Case 1 : bits% = 16
    Case 2 : bits% = 32
  End Select
  n% = 16 * bits% \ 64
  ResetSM
  For i% = 0 To 7 : rx%(i%) = -1 : Next i%
  PIO DMA RX p%,sm%,16,rx%(),,bits%
  PIO DMA TX p%,sm%,16,tx%(),,bits%
  t% = Timer
  Do While rx%(n% - 1) <> tx%(n% - 1) And Timer - t% < 1000
  Loop
  Chk "DMA RX pointer", (PIO(DMA RX POINTER) <> 0) And 1, 1
  Chk "DMA TX pointer", (PIO(DMA TX POINTER) <> 0) And 1, 1
  PIO DMA TX OFF
  PIO DMA RX OFF
  ok% = 1
  For i% = 0 To n% - 1
    If rx%(i%) <> tx%(i%) Then
      Print "  one-shot ";bits%;" mismatch [";i%;"] got=";rx%(i%);" want=";tx%(i%)
      ok% = 0
      Exit For
    EndIf
  Next i%
  If n% < 8 Then
    If rx%(n%) <> -1 Then ok% = 0
  EndIf
  Chk "DMA one-shot "+Str$(bits%)+" bit", ok%, 1
Next phase%

' RX finite ring: 16 DMA words into four 32-bit slots = two INTEGERs.
ResetSM
For i% = 0 To 7 : rr%(i%) = -1 : Next i%
PIO DMA RX p%,sm%,16,rr%(),,32,4
PIO DMA TX p%,sm%,16,tx%()
t% = Timer
Do While (rr%(0) <> tx%(6) Or rr%(1) <> tx%(7)) And Timer - t% < 1000
Loop
PIO DMA TX OFF
PIO DMA RX OFF
Chk "DMA finite RX ring [0]", rr%(0), tx%(6)
Chk "DMA finite RX ring [1]", rr%(1), tx%(7)
Chk "DMA finite RX ring guard", rr%(2), -1

' RX fixed destination: 16 32-bit words all overwrite first 32 bits.
ResetSM
For i% = 0 To 7 : rr%(i%) = -1 : Next i%
PIO DMA RX p%,sm%,16,rr%(),,32,1
PIO DMA TX p%,sm%,16,tx%()
t% = Timer
Do While (rr%(0) And &HFFFFFFFF) <> &H07000000 And Timer - t% < 1000
Loop
PIO DMA TX OFF
PIO DMA RX OFF
Chk "DMA RX fixed addr", rr%(0) And &HFFFFFFFF, &H07000000
Chk "DMA RX fixed guard", rr%(1), -1

' TX cyclic ring, with separate finite RX.
ResetSM
For i% = 0 To 7 : rx%(i%) = -1 : Next i%
PIO DMA RX p%,sm%,16,rx%()
PIO DMA TX p%,sm%,0,rt%(),,32,4
t% = Timer
Do While rx%(7) <> rt%(1) And Timer - t% < 1000
Loop
PIO DMA TX OFF
PIO DMA RX OFF
ok% = -1
For i% = 0 To 7
  If rx%(i%) <> rt%(i% Mod 2) Then
    Print "  TX ring[";i%;"] got=";rx%(i%);" want=";rt%(i% Mod 2)
    ok% = 0
    Exit For
  EndIf
Next i%
Chk "DMA cyclic TX ring", ok%, -1

' TX cyclic retrigger (count=size), no address ring.
ResetSM
For i% = 0 To 7 : rx%(i%) = -1 : Next i%
PIO DMA RX p%,sm%,16,rx%()
PIO DMA TX p%,sm%,4,rt%(),,32,4
t% = Timer
Do While rx%(7) <> rt%(1) And Timer - t% < 1000
Loop
PIO DMA TX OFF
PIO DMA RX OFF
ok% = -1
For i% = 0 To 7
  If rx%(i%) <> rt%(i% Mod 2) Then
    Print "  TX retrigger[";i%;"] got=";rx%(i%);" want=";rt%(i% Mod 2)
    ok% = 0
    Exit For
  EndIf
Next i%
Chk "DMA cyclic TX retrigger", ok%, -1

' Cyclic RX: latest four 32-bit words overwrite first two INTEGERs.
ResetSM
For i% = 0 To 7 : rr%(i%) = -1 : Next i%
PIO DMA RX p%,sm%,0,rr%(),,32,4
PIO DMA TX p%,sm%,16,tx%()
t% = Timer
Do While (rr%(0) <> tx%(6) Or rr%(1) <> tx%(7)) And Timer - t% < 1000
Loop
PIO DMA RX OFF
PIO DMA TX OFF
Chk "DMA cyclic RX [0]", rr%(0), tx%(6)
Chk "DMA cyclic RX [1]", rr%(1), tx%(7)
Chk "DMA cyclic RX guard", rr%(2), -1

' Last: BASIC DMA-completion callback (RX only).
ResetSM
For i% = 0 To 7 : rx%(i%) = -1 : Next i%
dhits% = 0
PIO DMA RX p%,sm%,16,rx%(),DmaEvent
PIO DMA TX p%,sm%,16,tx%()
t% = Timer
Do While dhits% = 0 And Timer - t% < 1000
Loop
PIO DMA TX OFF
PIO DMA RX OFF
Chk "DMA RX interrupt", dhits%, 1
ok% = -1
For i% = 0 To 7
  If rx%(i%) <> tx%(i%) Then
    Print "  RX callback[";i%;"] got=";rx%(i%);" want=";tx%(i%)
    ok% = 0
    Exit For
  EndIf
Next i%
Chk "DMA RX callback data", ok%, -1

PIO STOP p%,sm%
PIO CLEAR p%
Summary:
Print
Print "BASIC PIO: "; passes%; " PASS, "; failures%; " FAIL"
If failures% = 0 Then Print "ALL TESTS PASSED"
End

Sub ResetSM
  PIO STOP p%,sm%
  PIO CONFIGURE p%,sm%,15000000
  PIO START p%,sm%
End Sub

Sub FifoEvent
  PIO READ p%,sm%,1,got%
  Inc fhits%
End Sub

Sub DmaEvent
  Inc dhits%
End Sub

Sub Chk label$,actual%,expected%
  If actual% = expected% Then
    Print "PASS ";label$
    Inc passes%
  Else
    Print "FAIL ";label$;" got=";actual%;" expected=";expected%
    Inc failures%
  EndIf
End Sub
