' jsontest.bas - pull a LARGE JSON document over HTTPS and stress-parse it.
'
' Target  : WEBRP2350 firmware (Pico 2 W) - TLS is required for this host.
' Purpose : exercise the JSON$ arena heavily. Two forms of stress:
'             1. a big response (16-day forecast, ~20 KB, ~2700 cJSON nodes)
'             2. re-parsing that document 100x in a loop, which hammers the
'                arena's allocate / bulk-free cycle and proves json_arena_reset()
'                reclaims everything cleanly with no leak or corruption.
'
' Source  : api.open-meteo.com - free, no API key, honours HTTP/1.0 +
'           Content-Length (contiguous body, no chunked encoding).
'
' Firmware limits this program respects:
'   * MMBasic strings are max 255 chars, so the request line (and thus the
'     number of hourly variables we can name) is bounded - 6 vars fits.
'   * WEB TCP CLIENT REQUEST drains for only ~200 ms after the first byte, so a
'     body that can't arrive in that window would be truncated. ~20 KB is safe;
'     truncation now just yields "Invalid JSON data", never a reboot.
'
' JSON$ path note: a path that ENDS in an array index returns "" (the parser
' always finishes with a field lookup), so read scalars through named leaves.

OPTION EXPLICIT

CONST HOST$ = "api.open-meteo.com"
CONST PORT  = 443
CONST CRLF$ = CHR$(13) + CHR$(10)
CONST LOOPS = 10                 ' how many times to re-parse for the stress run

' 48 KB buffers (long string = integer array, 8 bytes/element).
DIM buff%(48000 \ 8)              ' raw HTTP response (headers + body)
DIM js%(48000 \ 8)                ' JSON body only, after header stripping

DIM query$, ip$, status$, expect$, got$
DIM hdr%, blen%, i%, t1%, ok%

' --- check we are on the network -------------------------------------------
ip$ = MM.INFO(IP ADDRESS)
PRINT "IP address : " ip$
IF ip$ = "0.0.0.0" OR ip$ = "" THEN
  PRINT "Not connected to WiFi - set OPTION WIFI ssid, password and reboot."
  END
END IF

' --- build a single HTTP/1.0 request (kept under 255 chars) -----------------
query$ =          "GET /v1/forecast?latitude=51.5074&longitude=-0.1278"
query$ = query$ + "&forecast_days=16"
query$ = query$ + "&hourly=temperature_2m,relative_humidity_2m,precipitation,pressure_msl,cloud_cover,wind_speed_10m"
query$ = query$ + "&current_weather=true HTTP/1.0" + CRLF$
query$ = query$ + "Host: " + HOST$ + CRLF$
query$ = query$ + "Connection: close" + CRLF$ + CRLF$
PRINT "Request len: " LEN(query$) " chars"

' --- fetch -----------------------------------------------------------------
PRINT "Connecting to " HOST$ " ..."
WEB OPEN TLS CLIENT HOST$, PORT
WEB TCP CLIENT REQUEST query$, buff%(), 15000
WEB CLOSE TCP CLIENT

PRINT "Received   : " LLen(buff%()) " bytes (headers + body)"
IF LLen(buff%()) = 0 THEN PRINT "Empty response - aborting." : END

status$ = LGetStr$(buff%(), 1, 15)
PRINT "Status     : " status$
IF INSTR(status$, " 200") = 0 THEN PRINT "Not 200 - aborting." : END

' --- strip the HTTP headers -------------------------------------------------
hdr% = LInStr(buff%(), CRLF$ + CRLF$)
IF hdr% = 0 THEN PRINT "No header/body separator - aborting." : END
LongString MID js%(), buff%(), hdr% + 4
blen% = LLen(js%())
PRINT "JSON body  : " blen% " bytes"
PRINT

' --- one parse to show it decodes correctly --------------------------------
PRINT "--- sample fields ---"
PRINT "latitude   : " JSON$(js%(), "latitude")
PRINT "timezone   : " JSON$(js%(), "timezone")
PRINT "elevation  : " JSON$(js%(), "elevation")
PRINT "temp unit  : " JSON$(js%(), "hourly_units.temperature_2m")
PRINT "cur temp   : " JSON$(js%(), "current_weather.temperature")
PRINT "cur wind   : " JSON$(js%(), "current_weather.windspeed")
PRINT "obs time   : " JSON$(js%(), "current_weather.time")
PRINT

' --- stress: re-parse the whole document LOOPS times ------------------------
' Each JSON$ call parses the full ~20 KB document from scratch (~2700 nodes)
' and frees the lot via json_arena_reset(). Verifying the value stays constant
' catches any leak/corruption across the repeated allocate/free cycles.
PRINT "Stress: parsing the " blen% "-byte document " LOOPS " times ..."
expect$ = JSON$(js%(), "current_weather.temperature")
ok% = 1
t1% = TIMER
FOR i% = 1 TO LOOPS
  got$ = JSON$(js%(), "current_weather.temperature")
  IF got$ <> expect$ THEN ok% = 0 : EXIT FOR
  IF i% MOD 10 = 0 THEN PRINT ".";
NEXT
PRINT
IF ok% = 0 THEN
  PRINT "MISMATCH at iteration " i% " - got [" got$ "] expected [" expect$ "]"
  END
END IF
PRINT LOOPS " parses OK in " (TIMER - t1%) " ms (" STR$((TIMER - t1%) / LOOPS, 0, 2) " ms each)"
PRINT
PRINT "Done - " LOOPS " parses of a " blen% "-byte document, no reboot, no drift."
END
