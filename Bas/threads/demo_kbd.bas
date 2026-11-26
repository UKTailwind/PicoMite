' demo_kbd.bas - threads: a thread reads a line from the USB keyboard while the main program waits for it (THREAD WAIT)
' and a second thread counts. Before 5c72549 the USB host was not serviced while only threads ran, so the keys never
' arrived. Not one of the automatic tests: run it on a PicoMite with a USB keyboard and type a line.
Option Explicit
Dim Integer go = 1, n

Sub Reader
  Local a$
  Input "Type a line on the USB keyboard"; a$
  Print "The thread read: "; a$
  go = 0
End Sub

Sub Counter
  Do While go
    Inc n
  Loop
End Sub

Thread Start 2, Counter
Thread Start 1, Reader
Thread Wait 1
Thread Wait 2
Print "The counting thread counted"; n; " meanwhile"
