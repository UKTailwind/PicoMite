' oneshot.bas - ONESHOT GP2 rising -> 100 ms pulse on GP3, with no wiring.
' The rising edge comes from switching floating GP2's pad from pull-down to
' pull-up. GP2's bits in PROC0_INTE0 (&H800 = rising edge) must be set while
' ONESHOT runs and clear after ONESHOT DISABLE. RP2350 addresses.
Const PAD2 = &H40038000 + 4 + 4 * 2
Const INTE0 = &H40028000 + &H248
Const SIOOUT = &HD0000010
ONESHOT GP2, POSITIVE, GP3, 0, 100000
Print "INTE start "; Hex$(Peek(Word INTE0) And &HF00)
Print "OUT before"; (Peek(Word SIOOUT) >> 3) And 1
Poke Word PAD2 + &H3000, 4
Poke Word PAD2 + &H2000, 8
Pause 20
Print "OUT during"; (Peek(Word SIOOUT) >> 3) And 1
Pause 150
Print "OUT after"; (Peek(Word SIOOUT) >> 3) And 1
ONESHOT DISABLE
Print "INTE disabled "; Hex$(Peek(Word INTE0) And &HF00)
Poke Word PAD2 + &H3000, 8
Poke Word PAD2 + &H2000, 4
End
