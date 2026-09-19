' Play every converted sound effect in turn, so they can be heard and checked.
Option Console Serial
Option Explicit
Option Default None
Dim INTEGER packed(200), snd(84), i, f, ms
Dim STRING home, nm(19) LENGTH 12
home = MM.Info(Path)
If home = "NONE" Then home = "A:/"
nm(0)="PlateDown"  : nm(1)="PlateUp"   : nm(2)="GateDown"  : nm(3)="SpecialKey1"
nm(4)="SpecialKey2": nm(5)="Splat"     : nm(6)="MirrorCrack": nm(7)="LooseCrash"
nm(8)="GotKey"     : nm(9)="Footstep"  : nm(10)="RaisingExit": nm(11)="RaisingGate"
nm(12)="LowerGate" : nm(13)="SmackWall": nm(14)="Impaled"   : nm(15)="GateSlam"
nm(16)="FlashMsg"  : nm(17)="SwordClash1" : nm(18)="SwordClash2" : nm(19)="JawsClash"
Open home + "sounds.dat" For Input As #1
Memory Input #1, 80, packed()
Close #1
Memory Unpack packed(), snd(), 80, 8
Print "audio now: "; MM.Info$(Sound)
For i = 0 To 19
  f = snd(i*4) Or (snd(i*4+1) << 8)
  ms = snd(i*4+2) Or (snd(i*4+3) << 8)
  Print "  "; Str$(i); " "; nm(i); "  "; Str$(f); " Hz "; Str$(ms); " ms";
  Play Tone f, f, ms
  Print "   while playing: "; MM.Info$(Sound)
  Pause 500
Next i
Print "done"
Option Console Both
End
