' doppler.bas: PLAY MODSAMPLE's LOOP, rate and pan, with PLAY MODSAMPLE CHANGE: cars going by
' doppler.mod, beside it, is a short tune made for this demo with two looped effect samples:
' 10, an engine, and 11, a car horn. First the engine is put on the left, in the centre and on
' the right. Then each sample passes you at 20, 40 and 60 m/s, 10 m away, left to right and
' right to left in turn: its pitch falls as it goes by, its volume follows the distance and its
' pan the direction it is heard from. Ctrl-C ends it.
' It needs PLAY MODSAMPLE's LOOP, CHANGE and pan.
Const sound = 343, closest = 10, rate = 16000
p$ = MM.Info(PATH) : If p$ = "NONE" Then p$ = ""
Play ModFile p$ + "doppler.mod"
Pause 2000

' the pan alone: the engine standing still, moved between the speakers (an empty argument is kept)
Play ModSample 10, 1, 40, rate, -100, LOOP
Print "The engine on the left"  : Pause 2000
Play ModSample Change 1, , , 0
Print "in the centre"           : Pause 2000
Play ModSample Change 1, , , 100
Print "on the right"            : Pause 2000
Play ModSample Stop 1
Pause 3000

' the cars
dir = 1
For s = 10 To 11
  For v = 20 To 60 Step 20
    If dir > 0 Then d$ = "left to right" Else d$ = "right to left"
    Print "Sample"; s; " passing at"; v; " m/s ("; v * 3.6; " km/h), "; d$
    CarPasses s, v, dir
    dir = -dir
    Pause 3000                              ' a gap: the music alone
  Next
Next
Play Stop

' one pass, from 4 s before the closest point to 4 s after it: dir 1 left to right, -1 right to
' left. The rate, volume and pan are changed every 20 ms.
Sub CarPasses s, v, dir
  Local t, x, d, away
  Play ModSample s, 1, 0, rate, -100 * dir, LOOP
  For t = -4 To 4 Step 0.02
    x = v * t : d = Sqr(x * x + closest * closest)
    away = v * x / d                        ' the speed away from you: + going, - coming
    Play ModSample Change 1, Min(64, 2400 / d), rate * sound / (sound + away), 100 * dir * x / d
    Pause 20
  Next
  Play ModSample Stop 1
End Sub
