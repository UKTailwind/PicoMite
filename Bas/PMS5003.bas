' ----------------------------------------------------------------------------
' PMS5003.bas  -  read PM1.0 / PM2.5 / PM10 particulate levels from a Plantower
'                 PMS5003 (also PMS7003 / PMSA003) laser sensor on the PicoMite.
'
' The sensor free-runs in "active" mode, sending a fixed 32-byte frame roughly
' once a second over a 9600-baud UART.  We sync to the 0x42 0x4D header, read
' the frame, verify the checksum, and decode the big-endian values.
'
' Wiring (sensor needs 5V power, but its data lines are 3.3V logic - Pico-safe):
'   VCC  -> VBUS (5V, pin 40 when USB-powered)      GND -> GND
'   TXD  -> Pico COM1 RX (GP1)                       <- this is the data we read
'   RXD  -> Pico COM1 TX (GP0)   (optional, only needed to send mode commands)
'   SET  -> GP2   (high = run, low = standby: fan + laser off to save power)
'   RST  -> GP3   (active-low reset)
' SET/RESET are optional (the sensor runs with them open), but wiring them lets
' the program reset the module and put it to sleep on demand.
' Give the fan ~30 s after power-up before trusting the numbers.
' ----------------------------------------------------------------------------

OPTION EXPLICIT
OPTION DEFAULT NONE

SETPIN GP1, GP0, COM1                  ' rx=GP1, tx=GP0 (tx unused in active mode)
OPEN "COM1:9600, 256" AS #1

SETPIN GP2, DOUT                       ' PMS SET   pin: high = run, low = standby
SETPIN GP3, DOUT                       ' PMS RESET pin: active low
PIN(GP2) = 1                           ' SET high -> normal run/measure mode
PIN(GP3) = 0 : PAUSE 20 : PIN(GP3) = 1 ' pulse reset low, then release
PAUSE 100                              ' let the module reboot

DIM INTEGER d(29)                      ' frame bytes 2..31 (header stripped)

PRINT "Reading PMS5003 (allow ~30 s for the fan to stabilise)..."

DO
  IF GetFrame%(d()) THEN
    PRINT "PM1.0="; STR$(W%(d(), 8), 3); "  ";
    PRINT "PM2.5="; STR$(W%(d(), 10), 3); "  ";
    PRINT "PM10="; STR$(W%(d(), 12), 3); " ug/m3";
    PRINT "    counts/0.1L  >0.5um="; STR$(W%(d(), 16));
    PRINT "  >2.5um="; STR$(W%(d(), 20))
  ELSE
    PRINT "no valid frame (check wiring, 5V power, and the RX pin)"
  ENDIF
  PAUSE 1000
LOOP

' ===========================================================================
' Read one validated 32-byte frame into d(0..29).  Returns 1 on success, 0 on
' timeout or checksum failure.
' ===========================================================================
FUNCTION GetFrame%(d%())
  LOCAL INTEGER c, i, sum, chk
  ' sync to the 0x42 0x4D start-of-frame sequence
  DO
    c = GetByte%(2000)
    IF c < 0 THEN GetFrame% = 0 : EXIT FUNCTION          ' timed out waiting for data
    IF c = &H42 THEN
      c = GetByte%(2000)
      IF c = &H4D THEN EXIT DO
    ENDIF
  LOOP
  sum = &H42 + &H4D
  FOR i = 0 TO 29                       ' bytes 2..31 of the frame
    c = GetByte%(2000)
    IF c < 0 THEN GetFrame% = 0 : EXIT FUNCTION
    d%(i) = c
    IF i < 28 THEN sum = sum + c         ' checksum covers everything but the 2 checksum bytes
  NEXT i
  chk = (d%(28) << 8) OR d%(29)
  IF chk = sum THEN GetFrame% = 1 ELSE GetFrame% = 0
END FUNCTION

' Read one byte from COM1, or return -1 if nothing arrives within timeout ms.
FUNCTION GetByte%(timeout%)
  LOCAL INTEGER t
  LOCAL STRING s
  t = TIMER
  DO
    s = INPUT$(1, #1)                    ' returns "" immediately if buffer empty
    IF s <> "" THEN GetByte% = ASC(s) : EXIT FUNCTION
    IF TIMER - t > timeout% THEN GetByte% = -1 : EXIT FUNCTION
  LOOP
END FUNCTION

' Combine two big-endian frame bytes d(hi),d(hi+1) into a value.
FUNCTION W%(d%(), hi%)
  W% = (d%(hi%) << 8) OR d%(hi% + 1)
END FUNCTION
