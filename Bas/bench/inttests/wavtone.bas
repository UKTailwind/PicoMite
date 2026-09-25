' wavtone.bas - PLAY TONE's end-of-play interrupt fires once, when the
' 200 ms tone ends. Needs audio configured (the PC3 has it).
'   WAV  interrupts, and whether it came between 150 and 400 ms
Dim w%, t
Play Tone 440, 440, 200, WavEnd
Timer = 0
Do While w% = 0 And Timer < 2000 : Loop
t = Timer
Pause 100
Print "WAV"; w%; t > 150 And t < 400
End

Sub WavEnd
  Inc w%
End Sub
