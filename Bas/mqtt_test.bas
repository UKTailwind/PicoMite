' ---------------------------------------------------------------------------
' mqtt_test.bas  -  MQTT round-trip test against a public external broker
' ---------------------------------------------------------------------------
' Purpose: prove the WebMite MQTT client works end-to-end (CONNECT, SUBSCRIBE,
' PUBLISH, incoming message, UNSUBSCRIBE, CLOSE) against an anonymous broker
' on plain port 1883 (no TLS). If this passes but your LOCAL broker fails,
' the problem is with the local broker's configuration / permissions
' (anonymous access, ACLs, listener binding, firewall) - not the MMBasic code.
'
' The broker below is public and allows anonymous connections on port 1883.
' Swap Broker$ for "test.mosquitto.org" if hivemq is unreachable.
' ---------------------------------------------------------------------------

Const Broker$ = "broker.hivemq.com"      ' public, anonymous, plain MQTT
Const Port    = 1883                     ' plain (non-TLS) MQTT port

Dim mqttGot = 0                          ' set by the OnMqtt interrupt

Sub OnMqtt
  mqttGot = 1
End Sub

' Unique topic + payload so a stale retained message can't fool us
Dim Integer stamp = Timer Mod 1000000
Dim topic$ = "picomite-test/" + Str$(stamp)
Dim msg$   = "hello-" + Str$(stamp)
Dim Integer t

Print
Print "MQTT external-broker test"
Print "-------------------------"
Print "Local IP   : "; MM.Info$(IP Address)
Print "Broker     : "; Broker$; " : "; Str$(Port)
Print "Topic      : "; topic$
Print "Payload    : "; msg$
Print

' --- CONNECT (anonymous: empty user + empty password) ----------------------
mqttGot = 0
On Error Skip 1
Web Mqtt Connect Broker$, Port, "", "", OnMqtt
If MM.ErrNo <> 0 Then
  Print "CONNECT   : FAIL - "; MM.ErrMsg$
  Print
  Print "Could not reach the external broker. Check WiFi / DNS / internet."
  End
EndIf
Print "CONNECT   : OK"

' --- SUBSCRIBE -------------------------------------------------------------
On Error Skip 1
Web Mqtt Subscribe topic$
If MM.ErrNo <> 0 Then
  Print "SUBSCRIBE : FAIL - "; MM.ErrMsg$
  Web Mqtt Close
  End
EndIf
Print "SUBSCRIBE : OK"

' Let the SUBACK land before we publish
Pause 200

' --- PUBLISH ---------------------------------------------------------------
On Error Skip 1
Web Mqtt Publish topic$, msg$
If MM.ErrNo <> 0 Then
  Print "PUBLISH   : FAIL - "; MM.ErrMsg$
  Web Mqtt Close
  End
EndIf
Print "PUBLISH   : OK"

' --- WAIT FOR ROUND-TRIP ---------------------------------------------------
t = Timer + 5000
Do While Timer < t And mqttGot = 0
  Pause 50
Loop

Print
If mqttGot Then
  Print "ROUND-TRIP: OK - message came back"
  Print "  topic   : "; MM.Topic$;   " ("; Choice(MM.Topic$   = topic$, "match", "MISMATCH"); ")"
  Print "  payload : "; MM.Message$; " ("; Choice(MM.Message$ = msg$,   "match", "MISMATCH"); ")"
  Print
  Print "PASS - external MQTT works. If your LOCAL broker fails, it is a"
  Print "       broker permission/config issue, not the firmware."
Else
  Print "ROUND-TRIP: FAIL - no message received within 5 s"
  Print "  Broker accepted CONNECT/SUBSCRIBE/PUBLISH but did not echo back."
EndIf

On Error Skip 1
Web Mqtt Unsubscribe topic$
Web Mqtt Close
Print
Print "Done."
End
