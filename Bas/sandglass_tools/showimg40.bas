' showimg.bas - put a saved still on the screen with the game palette.
Option Explicit
Option Default None
Dim INTEGER i
Dim STRING home
home = MM.Info(Path)
If home = "NONE" Then home = "A:/"
Mode 2
Load Image home + "shot40.bmp"
Map(0) = 0
Map(1) = 9071429
Map(2) = 11569754
Map(3) = 12886646
Map(4) = 2379903
Map(5) = 5147844
Map(6) = 9423344
Map(7) = 0
Map(8) = 8006176
Map(9) = 11882554
Map(10) = 14715487
Map(11) = 0
Map(12) = 7031386
Map(13) = 10123914
Map(14) = 14732758
Map(15) = 16777215
Map Set
Do : Loop
