' jsontest_orig.bas - the original small, known-good JSON-over-HTTPS test.
'
' This is the version that returned ~6 KB and parsed cleanly. Kept separate so
' there is always a minimal, reliable request to fall back to - open-meteo (like
' most free APIs) can rate-limit repeated requests in a short window, so if a
' bigger query returns "No response from server", wait a bit and run this.
'
' Target  : WEBRP2350 firmware (Pico 2 W) - TLS required for this host.
'
' JSON$ path note: a path that ENDS in an array index returns "" (the parser
' always finishes with a field lookup), so read scalars through named leaves.

OPTION EXPLICIT

CONST HOST$ = "api.open-meteo.com"
CONST PORT  = 443
CONST CRLF$ = CHR$(13) + CHR$(10)

' ~32 KB receive buffer (a long string is an integer array, 8 bytes/element).
DIM buff%(32000 \ 8)              ' raw HTTP response (headers + body)
DIM js%(32000 \ 8)                ' JSON body only, after header stripping

DIM query$, ip$, status$
DIM hdr%, blen%

' --- check we are on the network -------------------------------------------
ip$ = MM.INFO(IP ADDRESS)
PRINT "IP address : " ip$
IF ip$ = "0.0.0.0" OR ip$ = "" THEN
  PRINT "Not connected to WiFi - set OPTION WIFI ssid, password and reboot."
  END
END IF

' --- build a single small HTTP/1.0 request ---------------------------------
' forecast_days defaults to 7; three hourly variables plus current_weather.
query$ =          "GET /v1/forecast?latitude=51.5074&longitude=-0.1278"
query$ = query$ + "&hourly=temperature_2m,relative_humidity_2m,wind_speed_10m"
query$ = query$ + "&current_weather=true HTTP/1.0" + CRLF$
query$ = query$ + "Host: " + HOST$ + CRLF$
query$ = query$ + "Connection: close" + CRLF$ + CRLF$

' --- fetch -----------------------------------------------------------------
PRINT "Connecting to " HOST$ " ..."
WEB OPEN TLS CLIENT HOST$, PORT            ' encrypted, unverified (fine for public data)
WEB TCP CLIENT REQUEST query$, buff%(), 15000
WEB CLOSE TCP CLIENT

PRINT "Received   : " LLen(buff%()) " bytes (headers + body)"
IF LLen(buff%()) = 0 THEN PRINT "Empty response - aborting." : END

' --- check the status line --------------------------------------------------
status$ = LGetStr$(buff%(), 1, 15)         ' e.g. "HTTP/1.0 200 OK"
PRINT "Status     : " status$
IF INSTR(status$, " 200") = 0 THEN
  PRINT "Server did not return 200 - aborting."
  END
END IF

' --- strip the HTTP headers: copy everything after the blank line -----------
hdr% = LInStr(buff%(), CRLF$ + CRLF$)      ' 1-based position of the CRLFCRLF
IF hdr% = 0 THEN PRINT "No header/body separator found - aborting." : END
LongString MID js%(), buff%(), hdr% + 4    ' body starts 4 bytes after that
blen% = LLen(js%())
PRINT "JSON body  : " blen% " bytes"
PRINT

' --- parse it ---------------------------------------------------------------
PRINT "--- location ---"
PRINT "latitude   : " JSON$(js%(), "latitude")
PRINT "longitude  : " JSON$(js%(), "longitude")
PRINT "elevation  : " JSON$(js%(), "elevation")
PRINT "timezone   : " JSON$(js%(), "timezone")
PRINT "gen time ms: " JSON$(js%(), "generationtime_ms")
PRINT
PRINT "--- units (nested object) ---"
PRINT "temp unit  : " JSON$(js%(), "hourly_units.temperature_2m")
PRINT "wind unit  : " JSON$(js%(), "hourly_units.wind_speed_10m")
PRINT
PRINT "--- current weather (nested object) ---"
PRINT "temperature: " JSON$(js%(), "current_weather.temperature") " " JSON$(js%(), "hourly_units.temperature_2m")
PRINT "wind speed : " JSON$(js%(), "current_weather.windspeed")
PRINT "wind dir   : " JSON$(js%(), "current_weather.winddirection")
PRINT "weathercode: " JSON$(js%(), "current_weather.weathercode")
PRINT "obs time   : " JSON$(js%(), "current_weather.time")
PRINT
PRINT "Done - parsed a " blen% "-byte JSON document without a reboot."
END
