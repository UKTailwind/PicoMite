' demo_net.bas - threads: real network work in a thread while the main program counts. WEB NTP, then an HTTPS GET
' (httpbin.org, TLS handshake on the thread's own 8 KB stack), each in thread 1 with the default SIZE.
Option Explicit
Dim Integer n, done, rx%(2000)
Dim e$, crlf$ = Chr$(13) + Chr$(10)
Dim Float t0

Sub Ntp
  On Error Skip
  Web NTP 0
  If MM.Errno Then e$ = "ntp: " + MM.ErrMsg$
  done = 1
End Sub

Sub Get
  On Error Skip
  Web Open Tls Client "httpbin.org", 443, 15000
  If MM.Errno Then e$ = "open: " + MM.ErrMsg$ : done = 1 : Exit Sub
  On Error Skip
  Web Tcp Client Request "GET /get HTTP/1.1" + crlf$ + "Host: httpbin.org" + crlf$ + "Connection: close" + crlf$ + crlf$, rx%(), 10000
  If MM.Errno Then e$ = "request: " + MM.ErrMsg$
  Web Close Tcp Client
  done = 1
End Sub

n = 0 : done = 0 : e$ = "" : t0 = Timer
Thread Start 1, Ntp
Do While done = 0 : Inc n : Loop
Thread Wait 1
Print "NTP in a thread: "; Int(Timer - t0); " ms, the main program counted"; n; ", time "; Time$; " "; e$

n = 0 : done = 0 : e$ = "" : t0 = Timer
Thread Start 1, Get
Do While done = 0 : Inc n : Loop
Thread Wait 1
Print "HTTPS GET in a thread: "; Int(Timer - t0); " ms, the main program counted"; n; ", reply"; LLen(rx%()); " bytes, has httpbin.org:"; LInStr(rx%(), "httpbin.org") > 0; " "; e$
