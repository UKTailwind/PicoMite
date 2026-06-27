' ============================================================================
' https_test.bas - thorough test of the PicoMite WEBRP2350 TLS functionality.
'
' Tests every code path added in the TLS work:
'   - WEB OPEN TLS CLIENT (plain, no-verify path)
'   - Multiple sequential TLS sessions (PCB / mbedtls cleanup)
'   - Implicit close on re-open (the prior-pcb leak fix)
'   - Larger responses (bigger than one TCP segment)
'   - WEB TLS CA  (peer-cert verification REQUIRED)
'   - WEB TLS NOVERIFY (revert to encrypted-but-unauthenticated)
'   - Negative case: verification correctly rejects a non-chaining cert
'   - WEB MQTT round-trip on port 1883 (plain) and port 8883 (TLS / MQTTS)
'
' Prerequisites:
'   OPTION WIFI "<ssid>", "<password>"        ' associate
'   (optional) "ca.pem"                       ' bundle covering Amazon + ISRG
'                                              (use fetch_ca.py:
'                                               python fetch_ca.py isrg-x1 amazon-r1)
'   (optional) "ca-mismatch.pem"              ' bundle that does NOT cover
'                                              httpbin.org (e.g. just digicert-g2,
'                                              built with:
'                                               python fetch_ca.py digicert-g2
'                                               then rename to ca-mismatch.pem)
'
' Tests requiring extra files report "skipped" if the file isn't present.
' ============================================================================

Option Explicit
Option Default Integer

Dim pass = 0, total = 0, skipped = 0

' ----------------------------------------------------------------------------
' Helpers
' ----------------------------------------------------------------------------

Sub Report(name$, ok)
  total = total + 1
  If ok Then
    pass = pass + 1
    Print "  [PASS] "; name$
  Else
    Print "  [FAIL] "; name$
  EndIf
End Sub

Sub Skip(name$, why$)
  total = total + 1
  skipped = skipped + 1
  Print "  [SKIP] "; name$; "  ("; why$; ")"
End Sub

Sub Banner(title$)
  Print
  Print String$(60, "=")
  Print "== "; title$
  Print String$(60, "=")
End Sub

Function FileExists(fname$) As Integer
  FileExists = (Dir$(fname$) <> "")
End Function

' ----------------------------------------------------------------------------
' Build a minimal HTTP/1.0 request once; reused across tests
' ----------------------------------------------------------------------------
Dim CRLF$ = Chr$(13) + Chr$(10)

Function BuildGet$(path$, host$) As String
  ' Local — not Dim! Inside a Function, Dim creates a variable that
  ' persists across calls and a second call errors with "already declared".
  Local r$
  r$ =     "GET " + path$ + " HTTP/1.0"          + CRLF$
  r$ = r$ + "Host: " + host$                      + CRLF$
  r$ = r$ + "User-Agent: PicoMite-TLS-test/1.1"   + CRLF$
  r$ = r$ + "Connection: close"                   + CRLF$
  r$ = r$ +                                         CRLF$
  BuildGet$ = r$
End Function

' Quick check that a response actually looks like an HTTP response.
' Use LINSTR (longstring INSTR) rather than LGetStr$ + INSTR, because
' LGetStr$ returns a regular BASIC string which is capped at MAXSTRLEN
' (255 bytes) — asking for more than that triggers "Number out of bounds".
' LINSTR searches the entire longstring with no length limit.
Function ResponseOK(rx%(), expectStatus$) As Integer
  ResponseOK = (LINSTR(rx%(), "HTTP/") = 1) And (LINSTR(rx%(), expectStatus$) > 0)
End Function

' --- MQTT round-trip helper -------------------------------------------------
' Subscribe-then-publish to a unique topic on the given broker/port and
' verify the published message comes back through our subscription. This
' exercises the full MQTT protocol stack: CONNECT, SUBSCRIBE, PUBLISH,
' incoming PUBLISH callback, UNSUBSCRIBE, DISCONNECT. Use port 8883 to
' exercise the same path over TLS.

' Global flag set by the OnMqtt interrupt — used to detect that the
' subscribed topic delivered a message back to us.
Dim mqttGot = 0

Sub OnMqtt
  mqttGot = 1
End Sub

Sub TestMqttRoundtrip(broker$, port As Integer, label$)
  ' Make topic + payload unique per call so a stuck retained message from
  ' a previous test (or someone else on the public broker) can't pass us.
  Local Integer stamp = Timer Mod 1000000
  Local topic$ = "picomite-test/" + Str$(stamp)
  Local msg$ = "hello-" + Str$(stamp)
  Local Integer t

  mqttGot = 0
  On Error Skip 1
  Web Mqtt Connect broker$, port, "", "", OnMqtt
  If MM.ErrNo <> 0 Then
    Report label$ + " connect", 0
    Print "    error: "; MM.ErrMsg$
    Exit Sub
  EndIf
  Report label$ + " connect", 1

  On Error Skip 1
  Web Mqtt Subscribe topic$
  If MM.ErrNo <> 0 Then
    Report label$ + " subscribe", 0
    Web Mqtt Close
    Exit Sub
  EndIf
  Report label$ + " subscribe", 1

  ' Tiny grace period so the SUBACK lands before our PUBLISH, otherwise
  ' the broker may not echo back the message we subscribed for.
  Pause 200

  On Error Skip 1
  Web Mqtt Publish topic$, msg$
  If MM.ErrNo <> 0 Then
    Report label$ + " publish", 0
    Print "    error: "; MM.ErrMsg$
    Web Mqtt Close
    Exit Sub
  EndIf
  Report label$ + " publish", 1

  ' Wait up to 5 seconds for round-trip
  t = Timer + 5000
  Do While Timer < t And mqttGot = 0
    Pause 50
  Loop

  If mqttGot Then
    Report label$ + " round-trip delivered", 1
    Report label$ + " topic matches",   MM.Topic$ = topic$
    Report label$ + " payload matches", MM.Message$ = msg$
  Else
    Report label$ + " round-trip delivered", 0
  EndIf

  On Error Skip 1
  Web Mqtt Unsubscribe topic$
  Web Mqtt Close
End Sub

' Print first n bytes of a longstring (capped to fit a BASIC string) for
' diagnosing weird responses. Replaces control chars with '.' so the print
' stays single-line.
Sub DumpHead(rx%(), n)
  Local m = n
  If m > 64 Then m = 64                ' fit in MAXSTRLEN safely
  If m > rx%(0) Then m = rx%(0)
  If m <= 0 Then
    Print "    (empty)"
    Exit Sub
  EndIf
  Local s$ = LGetStr$(rx%(), 1, m)
  Local out$, j, c
  For j = 1 To Len(s$)
    c = Asc(Mid$(s$, j, 1))
    If c < 32 Or c > 126 Then
      out$ = out$ + "."
    Else
      out$ = out$ + Chr$(c)
    EndIf
  Next j
  Print "    Head: ["; out$; "] ("; rx%(0); " total)"
End Sub

' ============================================================================
' Pre-flight
' ============================================================================
Banner "Pre-flight"

Dim ip$ = MM.Info(IP Address)
Print "  Local IP:     "; ip$
If ip$ = "0.0.0.0" Or ip$ = "" Then
  Print "  Wi-Fi not connected. Configure with OPTION WIFI, reboot, retry."
  End
EndIf
Report "Wi-Fi associated", 1

' NTP. Some tests (verified TLS) require real time for cert expiry checks.
' Soft-fail if NTP is unavailable; later tests will adapt.
' WEB NTP syntax: offset_hours, server$, timeout_ms — empty arg slots are
' not accepted, you must specify the offset (0 for UTC).
Dim ntpOK = 0
On Error Skip 1
Web Ntp 0, "time.cloudflare.com", 10000
If MM.ErrNo = 0 Then ntpOK = 1
If ntpOK Then
  Print "  System time:  "; Time$; " UTC, "; Date$
  Report "NTP sync", 1
Else
  Print "  NTP failed:   "; MM.ErrMsg$
  Report "NTP sync (verified TLS tests will be skipped)", 0
EndIf

' ============================================================================
' Test 1: Plain TLS handshake (no CA loaded — encrypted but unauthenticated)
' ============================================================================
Banner "Test 1: Plain TLS handshake"

Dim rx1%(256)
Dim req1$ = BuildGet$("/get", "httpbin.org")

On Error Skip 1
Dim t1 = Timer
Web Open Tls Client "httpbin.org", 443, 15000
If MM.ErrNo = 0 Then
  Print "  Handshake: "; Timer - t1; " ms"
  Report "TLS handshake completes", 1
  On Error Skip 1
  Web Tcp Client Request req1$, rx1%(), 10000
  If MM.ErrNo = 0 Then
    Report "GET /get returns HTTP 200", ResponseOK(rx1%(), "200 OK")
    Report "Response contains 'httpbin.org'", LINSTR(rx1%(), "httpbin.org") > 0
  Else
    Report "GET /get (request failed: " + MM.ErrMsg$ + ")", 0
  EndIf
  Web Close Tcp Client
Else
  Report "TLS handshake (failed: " + MM.ErrMsg$ + ")", 0
EndIf

' ============================================================================
' Test 2: Three sequential TLS sessions (PCB / mbedtls cleanup)
' ============================================================================
Banner "Test 2: Three sequential TLS sessions"

' Buffer is declared once outside the loop — MMBasic doesn't allow Dim of
' an already-declared variable. WEB TCP CLIENT REQUEST resets rx2(0) on
' each call, so reusing the array is fine.
Dim rx2%(256)
Dim i, allOK = 1
For i = 1 To 3
  On Error Skip 1
  Web Open Tls Client "httpbin.org", 443, 15000
  If MM.ErrNo <> 0 Then
    Print "  Round "; i; ": open failed: "; MM.ErrMsg$
    allOK = 0
  Else
    On Error Skip 1
    Web Tcp Client Request BuildGet$("/get", "httpbin.org"), rx2%(), 10000
    If MM.ErrNo <> 0 Then
      Print "  Round "; i; ": request errored: "; MM.ErrMsg$
      allOK = 0
    ElseIf Not ResponseOK(rx2%(), "200 OK") Then
      Print "  Round "; i; ": bad response shape"
      DumpHead rx2%(), 60
      allOK = 0
    Else
      Print "  Round "; i; ": OK ("; rx2%(0); " bytes)"
    EndIf
    Web Close Tcp Client
  EndIf
  Pause 200      ' let lwIP fully drain pending FIN/ACKs before next open
Next i
Report "3 sequential opens succeed", allOK

' ============================================================================
' Test 3: Implicit close on re-open (no explicit Close between Opens)
' ============================================================================
' This exercises the fix that auto-closes the prior TCP_CLIENT when a new
' WEB OPEN TLS CLIENT runs without an intervening WEB CLOSE TCP CLIENT.
Banner "Test 3: Implicit close on re-open"

Dim rx3%(256)
On Error Skip 1
Web Open Tls Client "httpbin.org", 443, 15000
Dim openA = (MM.ErrNo = 0)
' Don't close — open again immediately. The previous PCB should be cleaned up
' automatically; otherwise we'd leak a TCP/TLS PCB per loop iteration and
' eventually exhaust the pool.
On Error Skip 1
Web Open Tls Client "httpbin.org", 443, 15000
Dim openB = (MM.ErrNo = 0)
If openB Then
  On Error Skip 1
  Web Tcp Client Request BuildGet$("/get", "httpbin.org"), rx3%(), 10000
  Web Close Tcp Client
EndIf
Report "First Open without explicit Close", openA
Report "Second Open auto-cleans the first", openB

' ============================================================================
' Test 4: Larger response — httpbin /bytes/1500 returns 1500 random bytes
' ============================================================================
' Exercises pbuf chaining on the receive path. The body crosses TCP segment
' boundaries (MSS is 1460). The integer array buffer is sized to hold it.
Banner "Test 4: Larger response (~1.5 KB body)"

Dim rx4%(512)    ' 512 ints = 4 KB capacity, plenty for headers + 1500 bytes
On Error Skip 1
Web Open Tls Client "httpbin.org", 443, 15000
If MM.ErrNo = 0 Then
  Dim t4 = Timer
  On Error Skip 1
  Web Tcp Client Request BuildGet$("/bytes/1500", "httpbin.org"), rx4%(), 15000
  If MM.ErrNo = 0 Then
    Print "  Received: "; rx4%(0); " bytes in "; Timer - t4; " ms"
    Report "Response > 1500 bytes received", rx4%(0) > 1500
    Report "HTTP 200 returned", ResponseOK(rx4%(), "200 OK")
  Else
    Report "Larger response request", 0
  EndIf
  Web Close Tcp Client
Else
  Report "Larger response open", 0
EndIf

' ============================================================================
' Test 5: Load CA bundle and do an authenticated TLS handshake
' ============================================================================
Banner "Test 5: CA load + verified TLS"

If Not ntpOK Then
  Skip "Verified TLS handshake", "NTP not synced (cert expiry can't verify)"
ElseIf Not FileExists("ca.pem") Then
  Skip "WEB TLS CA loads bundle", "ca.pem not on device"
  Skip "Verified TLS handshake", "ca.pem not on device"
Else
  On Error Skip 1
  Web Tls Ca "ca.pem"
  If MM.ErrNo = 0 Then
    Report "WEB TLS CA loads bundle", 1
    Dim rx5%(256)
    On Error Skip 1
    Web Open Tls Client "httpbin.org", 443, 15000
    If MM.ErrNo = 0 Then
      Report "Verified TLS handshake (cert chains to bundle)", 1
      On Error Skip 1
      Web Tcp Client Request BuildGet$("/get", "httpbin.org"), rx5%(), 10000
      Report "Verified request returns 200", MM.ErrNo = 0 And ResponseOK(rx5%(), "200 OK")
      Web Close Tcp Client
    Else
      Print "  Handshake error: "; MM.ErrMsg$
      Report "Verified TLS handshake", 0
    EndIf
  Else
    Report "WEB TLS CA loads bundle (parse failed: " + MM.ErrMsg$ + ")", 0
    Skip "Verified TLS handshake", "CA not loaded"
  EndIf
EndIf

' ============================================================================
' Test 6: Negative case — wrong CA must REJECT the handshake
' ============================================================================
' If you generate a bundle that DOESN'T include the AWS roots (e.g.
'   python fetch_ca.py digicert-g2 -o ca-mismatch.pem
' ) then httpbin.org's cert won't chain to anything in it. Verification
' must fail — this proves the verify path is actually active rather than
' silently passing.
Banner "Test 6: Verification rejects unmatched cert"

If Not ntpOK Then
  Skip "Wrong-CA handshake rejected", "NTP not synced"
ElseIf Not FileExists("ca-mismatch.pem") Then
  Skip "Wrong-CA handshake rejected", "ca-mismatch.pem not on device"
Else
  On Error Skip 1
  Web Tls Ca "ca-mismatch.pem"
  If MM.ErrNo = 0 Then
    On Error Skip 1
    Web Open Tls Client "httpbin.org", 443, 10000
    ' We EXPECT this to fail — pass means MM.ErrNo is non-zero.
    Report "Wrong-CA handshake correctly rejected", MM.ErrNo <> 0
    If MM.ErrNo = 0 Then Web Close Tcp Client
  Else
    Report "Load mismatch CA bundle", 0
  EndIf
EndIf

' ============================================================================
' Test 7: WEB TLS NOVERIFY reverts to encrypted-but-unauthenticated
' ============================================================================
' After any CA-loaded test, NOVERIFY should drop the bundle and let us
' connect to anything (no chain validation) again.
Banner "Test 7: WEB TLS NOVERIFY revert"

On Error Skip 1
Web Tls Noverify
Report "WEB TLS NOVERIFY accepts", MM.ErrNo = 0

Dim rx7%(256)
On Error Skip 1
Web Open Tls Client "httpbin.org", 443, 15000
If MM.ErrNo = 0 Then
  On Error Skip 1
  Web Tcp Client Request BuildGet$("/get", "httpbin.org"), rx7%(), 10000
  Report "Post-NOVERIFY handshake succeeds", MM.ErrNo = 0 And ResponseOK(rx7%(), "200 OK")
  Web Close Tcp Client
Else
  Report "Post-NOVERIFY handshake (failed: " + MM.ErrMsg$ + ")", 0
EndIf

' ============================================================================
' Test 8: Plain TCP still works (didn't break the non-TLS path)
' ============================================================================
' Sanity check: our altcp refactor shouldn't have broken plain WEB OPEN
' TCP CLIENT. Use a no-TLS endpoint that's reliable. httpbin offers port 80.
Banner "Test 8: Plain TCP path unaffected"

Dim rx8%(256)
On Error Skip 1
Web Open Tcp Client "httpbin.org", 80, 10000
If MM.ErrNo = 0 Then
  On Error Skip 1
  Web Tcp Client Request BuildGet$("/get", "httpbin.org"), rx8%(), 10000
  Report "Plain TCP request still works", MM.ErrNo = 0 And ResponseOK(rx8%(), "200 OK")
  Web Close Tcp Client
Else
  Report "Plain TCP open (failed: " + MM.ErrMsg$ + ")", 0
EndIf

' ============================================================================
' Test 9: MQTT round-trip on plain port 1883
' ============================================================================
' Public broker broker.hivemq.com accepts anonymous connections and echoes
' back published messages to any subscriber on the same topic. We subscribe,
' publish, and verify the message comes back through our OnMqtt interrupt.
Banner "Test 9: MQTT plain (port 1883)"
TestMqttRoundtrip "broker.hivemq.com", 1883, "MQTT/1883"

' ============================================================================
' Test 10: MQTTS round-trip on TLS port 8883
' ============================================================================
' Same broker, same flow, but transported over TLS. Test 7 has already run
' WEB TLS NOVERIFY so TLS verification is off — the handshake is encrypted
' but not authenticated. Verifying MQTTS against a real CA needs a bundle
' covering the broker's cert authority (hivemq uses Let's Encrypt → ISRG
' Root X1) — load ca.pem with WEB TLS CA between tests if you want to
' exercise that.
Banner "Test 10: MQTTS over TLS (port 8883)"
TestMqttRoundtrip "broker.hivemq.com", 8883, "MQTTS/8883"

' ============================================================================
' Summary
' ============================================================================
Banner "Summary"
Print "  Total:     "; total
Print "  Passed:    "; pass
Print "  Failed:    "; total - pass - skipped
Print "  Skipped:   "; skipped
Print
If pass + skipped = total Then
  Print "  RESULT: all executed tests passed"
Else
  Print "  RESULT: there are failures - inspect the log above"
EndIf
End
