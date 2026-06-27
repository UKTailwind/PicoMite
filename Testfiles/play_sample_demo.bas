' ============================================================
' PLAY SAMPLE Demo - Independent Stereo ADSR Envelopes
' ============================================================
' This demo creates single-cycle waveforms and plays them
' using PLAY SAMPLE with the extended stereo syntax:
'
' Basic (same envelope both channels):
'   PLAY SAMPLE left%(), right%(), freq, A, D, S, R [,intr]
'
' Extended (independent L/R frequency and ADSR):
'   PLAY SAMPLE left%(), right%(),
'     freq_L, A_L, D_L, S_L, R_L,
'     freq_R, A_R, D_R, S_R, R_R [,intr]
'
' The left channel carries the melody with a piano ADSR.
' The right channel carries a harmony with a string ADSR
' (slow attack, high sustain, long release).
'
' PLAY RELEASE triggers both channels' release phase.
' PLAY STOP   stops immediately.
' ============================================================

Option Base 0
Const CYCLE_LEN = 256       ' samples per single cycle
Const AMPLITUDE = 32000     ' peak amplitude (16-bit signed)

Dim INTEGER left%(CYCLE_LEN - 1)
Dim INTEGER right%(CYCLE_LEN - 1)

' --- Build a single-cycle sine wave ---
For i% = 0 To CYCLE_LEN - 1
  left%(i%)  = Int(Sin(2 * Pi * i% / CYCLE_LEN) * AMPLITUDE)
  right%(i%) = left%(i%)
Next i%

' ============================================================
' Example 1: Basic mono playback (same ADSR both channels)
' ============================================================
Print "Example 1: Clean 440 Hz sine, 2 seconds..."
Print "  Attack=0  Decay=0  Sustain=100%  Release=0"
Play Sample left%(), right%(), 440, 0, 0, 100, 0
Pause 2000
Play Stop
Print "Done."
Print

' ============================================================
' Example 2: Piano-style envelope at middle C
' ============================================================
Print "Example 2: Piano tone at 262 Hz (middle C)..."
Print "  Attack=5ms  Decay=300ms  Sustain=20%  Release=500ms"
Play Sample left%(), right%(), 262, 5, 300, 20, 500
Pause 3000
Play Release
Pause 600
Print "Done."
Print

' ============================================================
' Example 3: Independent stereo - left piano, right string
'            Both play the same pitch (A4 = 440 Hz)
' ============================================================
' Demonstrates independent ADSR per channel at one pitch.
' Left  = Piano : fast attack, moderate decay, low sustain
' Right = String: slow attack, gentle decay, high sustain
Print "Example 3: Same pitch, different envelopes (stereo)..."
Print "  LEFT  (piano):  freq=440  A=5  D=300  S=20  R=500"
Print "  RIGHT (string): freq=440  A=150  D=200  S=75  R=800"
Play Sample left%(), right%(), 440, 5, 300, 20, 500, 440, 150, 200, 75, 800
Pause 3000
Play Release
Pause 900
Print "Done."
Print

' ============================================================
' Example 4: Stereo interval - left root, right fifth above
'            Left = C4 piano, Right = G4 string
' ============================================================
Print "Example 4: Stereo fifth - C4 piano (L) + G4 string (R)..."
Print "  LEFT  (piano):  freq=261.6  A=5  D=300  S=20  R=500"
Print "  RIGHT (string): freq=392.0  A=150  D=200  S=75  R=800"
Play Sample left%(), right%(), 261.6, 5, 300, 20, 500, 392.0, 150, 200, 75, 800
Pause 3000
Play Release
Pause 900
Print "Done."
Print

' ============================================================
' Example 5: Chromatic scale - piano left, string third right
' ============================================================
' The left channel plays a chromatic scale with a piano ADSR.
' The right channel plays a major third above with a string
' ADSR (the interval ratio is 5/4 = 1.2599).
Print "Example 5: Chromatic scale with stereo harmony..."
Restore scale_data
For note% = 1 To 13
  Read freq!
  harm! = freq! * 1.2599     ' major third above
  Print "  L="; freq!; "Hz  R="; harm!; "Hz"
  ' Left=piano(A=5,D=150,S=30,R=300) Right=string(A=120,D=150,S=75,R=500)
  Play Sample left%(), right%(), freq!, 5, 150, 30, 300, harm!, 120, 150, 75, 500
  Pause 500
  Play Release
  Pause 550
Next note%
Print "Scale complete."
Print

' ============================================================
' Example 6: Twinkle Twinkle Little Star
'   Left channel  = melody (piano ADSR)
'   Right channel = harmony a third above (string ADSR)
' ============================================================
' Piano ADSR:  Attack=5ms   Decay=150ms  Sustain=30%  Release=200ms
' String ADSR: Attack=150ms Decay=200ms  Sustain=75%  Release=800ms
'
' Melody in C major with harmony a diatonic third above:
'   C->E  D->F  E->G  F->A  G->B  A->C5
Print "Example 6: Twinkle Twinkle - piano melody (L) + string harmony (R)..."
Restore twinkle_harmony
For note% = 1 To 42
  Read mfreq!, hfreq!, dur%
  If mfreq! = 0 Then
    ' Rest
    Pause dur%
  Else
    Print "  melody="; mfreq!; "Hz  harmony="; hfreq!; "Hz  dur="; dur%; "ms"
    Play Sample left%(), right%(), mfreq!, 5, 150, 30, 200, hfreq!, 150, 200, 75, 800
    Pause dur%
    Play Release
    Pause 250
  EndIf
Next note%
Print "Tune complete."
End

done:
  ' Interrupt handler for end of release
  SamplePlaying% = 0
  IReturn

' Frequencies for one chromatic octave (A4 to A5)
scale_data:
Data 440.0, 466.2, 493.9, 523.3, 554.4, 587.3, 622.3
Data 659.3, 698.5, 740.0, 784.0, 830.6, 880.0

' Twinkle Twinkle Little Star with diatonic third harmony
' Each triple: melody freq, harmony freq, duration (ms)
' C4=261.6 D4=293.7 E4=329.6 F4=349.2 G4=392.0 A4=440.0 B4=493.9 C5=523.3
'
' Line 1: C C G G A A G-
' Harmony: E E B B C5 C5 B
twinkle_harmony:
Data 261.6, 329.6, 350, 261.6, 329.6, 350
Data 392.0, 493.9, 350, 392.0, 493.9, 350
Data 440.0, 523.3, 350, 440.0, 523.3, 350
Data 392.0, 493.9, 700
' Line 2: F F E E D D C-
' Harmony: A A G G F F E
Data 349.2, 440.0, 350, 349.2, 440.0, 350
Data 329.6, 392.0, 350, 329.6, 392.0, 350
Data 293.7, 349.2, 350, 293.7, 349.2, 350
Data 261.6, 329.6, 700
' Line 3: G G F F E E D-
' Harmony: B B A A G G F
Data 392.0, 493.9, 350, 392.0, 493.9, 350
Data 349.2, 440.0, 350, 349.2, 440.0, 350
Data 329.6, 392.0, 350, 329.6, 392.0, 350
Data 293.7, 349.2, 700
' Line 4: G G F F E E D-
' Harmony: B B A A G G F
Data 392.0, 493.9, 350, 392.0, 493.9, 350
Data 349.2, 440.0, 350, 349.2, 440.0, 350
Data 329.6, 392.0, 350, 329.6, 392.0, 350
Data 293.7, 349.2, 700
' Line 5: C C G G A A G-
' Harmony: E E B B C5 C5 B
Data 261.6, 329.6, 350, 261.6, 329.6, 350
Data 392.0, 493.9, 350, 392.0, 493.9, 350
Data 440.0, 523.3, 350, 440.0, 523.3, 350
Data 392.0, 493.9, 700
' Line 6: F F E E D D C-
' Harmony: A A G G F F E
Data 349.2, 440.0, 350, 349.2, 440.0, 350
Data 329.6, 392.0, 350, 329.6, 392.0, 350
Data 293.7, 349.2, 350, 293.7, 349.2, 350
Data 261.6, 329.6, 700
