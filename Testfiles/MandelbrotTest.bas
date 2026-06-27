' MandelbrotTest.bas
' Demonstrates all features of the MANDELBROT command
'
' Requires a configured display (SPI LCD, VGA, etc.)
' Press Ctrl-C at any time to abort a draw in progress

' Start with a clean slate
CLS

' --- 1. Basic draw (default 64 iterations) ---
Print "Drawing Mandelbrot set (default view)..."
Timer = 0
Mandelbrot Draw
Print "Done in " Str$(Timer) "ms"
Pause 2000

' --- 2. Draw with higher iteration count ---
CLS
Print "Redrawing with 128 iterations..."
Mandelbrot Reset
Timer = 0
Mandelbrot Draw 128
Print "Done in " Str$(Timer) "ms"
Pause 2000

' --- 3. Bare command (no subcommand, 64 iters) ---
CLS
Print "Bare MANDELBROT command..."
Mandelbrot Reset
Timer = 0
Mandelbrot 64
Print "Done in " Str$(Timer) "ms"
Pause 2000

' --- 4. Centre on a point of interest ---
'   Re-centre on approx (3/4 width, 1/2 height) - near the
'   boundary of the main cardioid, then zoom in
CLS
Print "Centre on interesting region..."
Mandelbrot Reset
Mandelbrot Draw
Pause 1000
Mandelbrot Centre MM.HRes*3\4, MM.VRes\2
Print "Centred and redrawn"
Pause 2000

' --- 5. Zoom in sequence ---
CLS
Print "Zooming in x2, x2, x2..."
Mandelbrot Reset
Mandelbrot Draw
Pause 1000

Mandelbrot Zoom 2
Print "Zoom x2"
Pause 1000

Mandelbrot Zoom 2
Print "Zoom x4 total"
Pause 1000

Mandelbrot Zoom 2
Print "Zoom x8 total"
Pause 2000

' --- 6. Zoom out ---
CLS
Print "Zooming back out..."
Mandelbrot Zoom 0.25
Print "Zoom x2 total (zoomed out x4)"
Pause 2000

' --- 7. Pan around ---
CLS
Print "Panning right 50 pixels..."
Mandelbrot Reset
Mandelbrot Draw
Pause 1000

Mandelbrot Pan 50, 0
Print "Panned right"
Pause 1000

Print "Panning down 30 pixels..."
Mandelbrot Pan 0, 30
Print "Panned down"
Pause 1000

Print "Panning left and up..."
Mandelbrot Pan -80, -30
Print "Panned left+up"
Pause 2000

' --- 8. Combined: centre, zoom, pan workflow ---
CLS
Print "Interactive-style exploration..."
Mandelbrot Reset
Mandelbrot Draw
Pause 1000

' Centre on the seahorse valley (approx left-centre of set)
Mandelbrot Centre MM.HRes\4, MM.VRes\2
Pause 1000

' Zoom in deep
Mandelbrot Zoom 4
Pause 1000
Mandelbrot Zoom 4
Pause 1000
Mandelbrot Zoom 2
Print "Deep zoom into seahorse valley"
Pause 2000

' Increase iterations for detail at this depth
Mandelbrot Draw 256
Print "Re-rendered with 256 iterations"
Pause 2000

' --- 9. Reset back to default ---
CLS
Print "Resetting to default view..."
Mandelbrot Reset
Mandelbrot Draw
Print "Back to default"
Pause 2000

CLS
Print "Mandelbrot demo complete!"
End
