
' ======================================================================
'  Sound effects.  Each entry is
'      DATA channel, waveform, volume        waveform 0=square 1=triangle
'      DATA freq, frames, freq, frames, ... , -1, 0        2=sine 3=noise
'  A frequency of 0 is a rest.  One frame is about 33 ms.  Volume is
'  0 to 25 per channel (PLAY SOUND caps it at 100 / MAXSOUNDS).
' ======================================================================
sfxdata:
' egg collected - quick rising sparkle
DATA 0, 0, 10
DATA 659,1, 880,1, 1047,1, 1319,1, 1568,1, 2093,2, -1,0
' grain collected
DATA 0, 0, 9
DATA 1568,2, 1047,2, -1,0
' jump
DATA 0, 0, 8
DATA 262,1, 349,1, 440,1, 523,1, 659,1, -1,0
' caught - long fall away
DATA 0, 0, 13
DATA 880,2, 784,2, 698,2, 622,2, 587,2, 523,2, 466,2, 415,2, 370,2
DATA 330,2, 294,2, 262,2, 233,2, 208,2, 185,2, 165,3, 147,3, -1,0
' hen cluck
DATA 2, 1, 7
DATA 1245,1, 740,1, 988,1, 0,1, -1,0
' footfall
DATA 1, 0, 4
DATA 147,1, 0,1, -1,0
' bonus countdown tick
DATA 0, 0, 8
DATA 2093,1, 0,1, -1,0
' extra life
DATA 0, 0, 12
DATA 1047,2, 1319,2, 1568,2, 2093,2, 1568,2, 2093,4, -1,0
' duck quack
DATA 2, 1, 8
DATA 330,3, 262,3, 311,3, 247,4, -1,0
' landing thud
DATA 3, 3, 5
DATA 300,2, 0,1, -1,0

' ---------------------------------------------------------------- tunes
'  DATA frequency, milliseconds ... terminated by -1, 0
tune0:
DATA 523,70, 659,70, 784,70, 1047,140, 0,40, 784,70, 1047,220, 0,1, -1,0
tune1:
DATA 784,80, 988,80, 1175,80, 1568,80, 1976,260, 0,1, -1,0
tune2:
DATA 523,180, 494,180, 440,180, 392,180, 330,520, 0,1, -1,0


' ----------------------------------------------------- hens per floor
'  How many of the five hen squares each floor uses on cycles 1 and 3.
'  Cycle 2 uses none, cycles 4 and 5 use all five.
hendata:
DATA 3, 4, 4, 4, 3, 3, 3, 3
