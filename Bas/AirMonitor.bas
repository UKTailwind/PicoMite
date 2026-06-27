' ----------------------------------------------------------------------------
' AirMonitor.bas  -  combined air-quality monitor for the PicoMite:
'   * Bosch BME688/BME680  : temperature, pressure, humidity, gas + IAQ score
'   * Plantower PMS5003     : PM1.0 / PM2.5 / PM10 particulates
'   * 20x4 I2C character LCD: live display (PCF8574 backpack at &H27)
'
' PREREQUISITE (run once at the command prompt, NOT in this program):
'   OPTION SYSTEM I2C sdapin, sclpin
' The BME688 and the LCD both sit on that system I2C bus, so we neither set the
' pins nor open the port here.  The system I2C is on GP14/GP15 = channel 2, so
' the BME688 is accessed with the I2C2 READ/WRITE commands (no OPEN needed) and
' the LCD via I2CLCD, both talking to the system bus directly.
'
' Wiring
'   BME688 : VCC->3V3  GND->GND  SDA/SCL-> the system I2C pins   (3.3V part!)
'   LCD    : VCC->5V   GND->GND  SDA/SCL-> the same system I2C pins  (addr &H27)
'   PMS5003: VCC->5V(VBUS) GND->GND  TXD->GP1(COM1 RX)  RXD->GP0(COM1 TX)
'            SET->GP2 (high=run)     RST->GP3 (active-low reset)
' ----------------------------------------------------------------------------

OPTION EXPLICIT
OPTION DEFAULT NONE

' --- BME688 I2C address: 0x76 (SDO low) or 0x77 (SDO high) ------------------
CONST ADDR = &H76
CONST LCD_ADDR = &H27                  ' PCF8574 LCD backpack address

' --- your site altitude in metres above sea level --------------------------
' Used to report sea-level pressure instead of QFE (station pressure).
' Set to your actual height; 0 leaves the pressure as measured.
CONST ALTITUDE = 120
' Reduction method: 0 = QNH (ISA standard altimeter setting, temperature-
' independent); 1 = QFF (uses the live temperature, as many weather services
' report).  Both equal QFE when ALTITUDE = 0.
CONST USE_QFF = 1

' BME688 calibration coefficients (filled by ReadCalibration)
DIM FLOAT par_t1, par_t2, par_t3
DIM FLOAT par_p1, par_p2, par_p3, par_p4, par_p5, par_p6, par_p7, par_p8, par_p9, par_p10
DIM FLOAT par_h1, par_h2, par_h3, par_h4, par_h5, par_h6, par_h7
DIM FLOAT par_gh1, par_gh2, par_gh3
DIM FLOAT res_heat_range, res_heat_val, range_sw_err
DIM FLOAT t_fine                       ' carries temperature into P and H calc
DIM FLOAT amb_temp = 25                ' ambient for the heater target (updated each read)
DIM INTEGER variant                    ' 0 = BME680, 1 = BME688, 2 = BME690 (chip id 0x61 for all)
DIM INTEGER st                         ' gas status byte (reg 0x2D / 0x2B)

' BME680 (variant 0) gas-range lookup tables
DIM FLOAT k1(15) = (0,0,0,0,0,-1,0,-0.8,0,0,-0.2,-0.5,0,-1,0,0)
DIM FLOAT k2(15) = (0,0,0,0,0.1,0.7,0,-0.8,-0.1,0,0,0,0,0,0,0)

' --- IAQ scoring (open humidity+gas heuristic, NOT Bosch BSEC) --------------
CONST BURN_SECS  = 300                 ' warm-up seconds before showing IAQ
CONST HUM_BASE   = 40.0                ' ideal indoor relative humidity (%)
CONST HUM_WEIGHT = 0.25                ' humidity's share of the score
CONST BASE_UP    = 0.05                ' baseline rise rate toward cleaner air
CONST BASE_DECAY = 0.99999             ' slow baseline downward drift
DIM FLOAT gas_baseline = 0             ' rolling clean-air gas resistance reference

' --- PMS5003 particulate readings (kept between frames) --------------------
DIM INTEGER d(29)                      ' PMS frame bytes 2..31
DIM INTEGER pm1, pm25, pm10            ' last good PM1.0 / PM2.5 / PM10 (ug/m3)
DIM INTEGER c03, c05, c10, c25, c50, c100  ' particle counts per 0.1L (>0.3 .. >10um)

' --- pressure trend arrow: based on the change over (up to) the last 3 hours.
'     One arrow = a normal move, a double arrow = changing fast. --------------
CONST NHIST       = 180                ' pressure history slots
CONST P_SAMPLE_MS = 60000              ' one sample per minute -> 180 slots = 3 hours
CONST P_STEADY    = 0.7                ' |3h change| below this (hPa) shows steady (=)
CONST P_FAST      = 1.6                ' |3h change| at/above this (hPa) shows a double arrow
DIM FLOAT phist(NHIST - 1)             ' circular pressure history (hPa)
DIM INTEGER pwi = 0, pn = 0            ' ring write index, count of valid samples

' LCD custom-character codes (0 = degree, auto-created by I2CLCD INIT)
CONST CH_UP = 1, CH_DN = 2, CH_EQ = 3  ' rising / falling / steady arrows

' --- display timing --------------------------------------------------------
CONST MEAS_MS   = 3000                 ' how often to take a full sensor measurement
CONST SCROLL_MS = 250                  ' marquee step interval (line 4)

' ---------------------------------------------------------------------------
' Start up
' ---------------------------------------------------------------------------
I2CLCD INIT LCD_ADDR                    ' uses the system I2C bus
I2CLCD CLEAR
I2CLCD BACKLIGHT 1
I2CLCD CREATECHAR CH_UP, &H04,&H0E,&H15,&H04,&H04,&H04,&H04,&H00   ' rising arrow
I2CLCD CREATECHAR CH_DN, &H04,&H04,&H04,&H04,&H15,&H0E,&H04,&H00   ' falling arrow
I2CLCD CREATECHAR CH_EQ, &H00,&H00,&H1F,&H00,&H1F,&H00,&H00,&H00   ' steady (=)
LcdLine 1, "Air monitor"
LcdLine 2, "starting..."

WB(&HE0, &HB6)                          ' BME688 soft reset (clears heater control)
PAUSE 10

IF RB%(&HD0) <> &H61 THEN
  PRINT "No BME688/BME680 at &H"; HEX$(ADDR); " - chip id &H"; HEX$(RB%(&HD0))
  LcdLine 1, "BME688 not found"
  LcdLine 2, "id &H" + HEX$(RB%(&HD0), 2)
  END
ENDIF

variant = RB%(&HF0)                     ' 0 = BME680, 1 = BME688
ReadCalibration

' PMS5003 on COM1
SETPIN GP1, GP0, COM1                   ' rx=GP1, tx=GP0
OPEN "COM1:9600, 256" AS #1
SETPIN GP2, DOUT                        ' SET   : high = run
SETPIN GP3, DOUT                        ' RESET : active low
PIN(GP2) = 1
PIN(GP3) = 0 : PAUSE 20 : PIN(GP3) = 1  ' pulse reset, then release
PAUSE 100

' ---------------------------------------------------------------------------
' Main loop: lines 1-3 are a fixed at-a-glance summary, line 4 scrolls the PM
' readout.  Sensors are read every MEAS_MS; line 4 scrolls on an absolute
' timebase (nextscroll) so its cadence stays even between reads.
' ---------------------------------------------------------------------------
DIM INTEGER t0 = TIMER
DIM INTEGER burning = 1, measuring = 0
DIM INTEGER nextmeas = 0, nextpsample = 0, nextscroll = 0, scrollpos = 0
DIM INTEGER secsleft, idx
DIM FLOAT tC, pHPa, slp, rh, gasR, iaq, slowref   ' slp = sea-level pressure (QNH or QFF)
DIM STRING l3, marquee, plabel
marquee = "Starting up...      "
IF USE_QFF THEN plabel = "QFF " ELSE plabel = "QNH "

DO
  ' --- BME688: trigger a measurement when due, collect it when the chip is
  '     ready.  Splitting it this way keeps the ~150ms conversion from blocking
  '     the scroll.  measuring/new_data are tested in NESTED IFs (never ANDed)
  '     because MMBasic AND is bitwise - "measuring AND (st AND &H80)" would be
  '     1 AND 128 = 0 and the read would never happen. ------------------------
  IF measuring THEN
    IF (RB%(&H1D) AND &H80) THEN         ' new_data set -> read the result
      tC   = ReadTemperature()
      pHPa = ReadPressure() / 100.0                 ' QFE (station pressure)
      IF USE_QFF THEN                               ' reduce QFE to sea level
        slp = pHPa * EXP(ALTITUDE * 9.80665 / (287.05 * (tC + 273.15)))   ' QFF (live temp)
      ELSE
        slp = pHPa / (1 - ALTITUDE / 44330.0) ^ 5.255                     ' QNH (ISA standard)
      ENDIF
      rh   = ReadHumidity()
      IF variant >= 1 THEN st = RB%(&H2D) ELSE st = RB%(&H2B)  ' 688/690 gas_lsb 0x2D, 680 0x2B
      IF (st AND &H20) THEN gasR = ReadGas() ELSE gasR = -1

      ' rolling gas baseline - adapt only on valid, heater-stable readings
      IF gasR >= 0 THEN
        IF (st AND &H10) THEN
          IF gas_baseline = 0 THEN
            gas_baseline = gasR
          ELSEIF gasR > gas_baseline THEN
            gas_baseline = gas_baseline + BASE_UP * (gasR - gas_baseline)
          ELSE
            gas_baseline = gas_baseline * BASE_DECAY
          ENDIF
        ENDIF
      ENDIF
      secsleft = INT((BURN_SECS * 1000 - (TIMER - t0)) / 1000)
      IF secsleft <= 0 THEN burning = 0

      PollPMS                            ' parse whatever PMS frames have arrived

      ' air-quality text for line 3
      IF gasR < 0 THEN
        l3 = "IAQ --"
      ELSEIF burning THEN
        l3 = "Warmup " + STR$(secsleft) + "s"
      ELSE
        iaq = AirQuality(gasR, rh)
        l3  = "IAQ" + STR$(iaq, 0, 0) + " " + AirLabel$(iaq)
      ENDIF

      ' sea-level pressure change over (up to) the last 3 hours
      IF pn > 0 THEN
        idx = (pwi - pn + NHIST) MOD NHIST : slowref = phist(idx)   ' oldest (<=3h) sample
      ELSE
        slowref = slp
      ENDIF

      ' fixed summary lines (one arrow = moving, two arrows = moving fast)
      LcdLine 1, "Temp " + STR$(tC, 0, 1) + CHR$(0) + "C   RH " + STR$(rh, 0, 0) + "%"
      LcdLine 2, plabel + STR$(slp, 0, 1) + "hPa " + Trend$(slp - slowref)
      LcdLine 3, l3

      ' PM values + per-size particle counts for the scrolling line
      marquee = "PM1.0 " + STR$(pm1) + " ug/m3    "
      marquee = marquee + "PM2.5 " + STR$(pm25) + " ug/m3    "
      marquee = marquee + "PM10 " + STR$(pm10) + " ug/m3    "
      marquee = marquee + "Counts per 0.1L  "
      marquee = marquee + ">0.3um " + STR$(c03) + "  >0.5um " + STR$(c05) + "  >1.0um " + STR$(c10) + "  "
      marquee = marquee + ">2.5um " + STR$(c25) + "  >5.0um " + STR$(c50) + "  >10um " + STR$(c100) + "        "

      measuring = 0
      nextmeas = TIMER + MEAS_MS
    ENDIF
  ELSEIF TIMER >= nextmeas THEN
    TriggerMeasure                       ' start a forced conversion (returns immediately)
    measuring = 1
  ENDIF

  ' --- sample the sea-level pressure history once a minute ---
  IF TIMER >= nextpsample AND pHPa > 0 THEN
    phist(pwi) = slp
    pwi = (pwi + 1) MOD NHIST
    IF pn < NHIST THEN pn = pn + 1
    nextpsample = TIMER + P_SAMPLE_MS
  ENDIF

  ' --- scroll line 4 on an absolute, self-correcting cadence ---
  IF TIMER >= nextscroll THEN
    LcdLine 4, Marq$(marquee, scrollpos, 20)
    scrollpos = scrollpos + 1
    IF scrollpos >= LEN(marquee) THEN scrollpos = 0
    nextscroll = nextscroll + SCROLL_MS
    IF nextscroll < TIMER THEN nextscroll = TIMER + SCROLL_MS   ' resync after the measurement stall
  ENDIF

  PAUSE 2                                ' small pause so nextscroll is checked promptly
LOOP

' ===========================================================================
' LCD helper: write text left-justified on a line, padded/truncated to 20 cols
' ===========================================================================
SUB LcdLine(ln%, t$)
  I2CLCD ln%, 1, LEFT$(t$ + SPACE$(20), 20)
END SUB

' Return a w-char window of s$ starting at pos, wrapping around (for the marquee).
' Builds at most w chars, so it never exceeds the 255-char string limit.
' (Named Marq$ not Scroll$ - SCROLL is a reserved MMBasic command keyword.)
FUNCTION Marq$(s$, p0%, nch%)
  LOCAL INTEGER n, p
  n = LEN(s$)
  p = p0% MOD n
  IF p + nch% <= n THEN
    Marq$ = MID$(s$, p + 1, nch%)
  ELSE
    Marq$ = MID$(s$, p + 1, n - p) + MID$(s$, 1, nch% - (n - p))
  ENDIF
END FUNCTION

' Arrow(s) for a pressure change 'delta' (hPa over ~3h):
'   below P_STEADY     -> "="   steady
'   P_STEADY..P_FAST   -> one arrow   (rising / falling)
'   >= P_FAST          -> two arrows  (rising / falling fast)
FUNCTION Trend$(delta AS FLOAT)
  IF delta >= P_FAST THEN
    Trend$ = CHR$(CH_UP) + CHR$(CH_UP)
  ELSEIF delta >= P_STEADY THEN
    Trend$ = CHR$(CH_UP)
  ELSEIF delta <= -P_FAST THEN
    Trend$ = CHR$(CH_DN) + CHR$(CH_DN)
  ELSEIF delta <= -P_STEADY THEN
    Trend$ = CHR$(CH_DN)
  ELSE
    Trend$ = CHR$(CH_EQ)
  ENDIF
END FUNCTION

' ===========================================================================
' PMS5003 serial helpers (non-blocking)
' ===========================================================================
' Parse any complete frames already in the COM1 buffer, keeping the latest.
' Never waits, so the scrolling display stays smooth.
SUB PollPMS
  LOCAL INTEGER c, i, sum, chk
  DO WHILE LOC(#1) >= 32                  ' only start when a full frame is buffered
    c = GetByteNB%()
    IF c = &H42 THEN
      c = GetByteNB%()
      IF c = &H4D THEN
        sum = &H42 + &H4D
        FOR i = 0 TO 29
          c = GetByteNB%()
          IF c < 0 THEN EXIT SUB           ' ran dry mid-frame (rare); bail out
          d(i) = c
          IF i < 28 THEN sum = sum + c
        NEXT i
        chk = (d(28) << 8) OR d(29)
        IF chk = sum THEN
          pm1 = BE16%(d(), 8) : pm25 = BE16%(d(), 10) : pm10 = BE16%(d(), 12)
          c03  = BE16%(d(), 14) : c05  = BE16%(d(), 16) : c10  = BE16%(d(), 18)
          c25  = BE16%(d(), 20) : c50  = BE16%(d(), 22) : c100 = BE16%(d(), 24)
        ENDIF
      ENDIF
    ENDIF
  LOOP
END SUB

' Read one byte from COM1, or -1 if the buffer is empty (no waiting)
FUNCTION GetByteNB%()
  LOCAL STRING s
  s = INPUT$(1, #1)
  IF s = "" THEN GetByteNB% = -1 ELSE GetByteNB% = ASC(s)
END FUNCTION

' Combine two big-endian PMS frame bytes into a 16-bit value
FUNCTION BE16%(d%(), hi%)
  BE16% = (d%(hi%) << 8) OR d%(hi% + 1)
END FUNCTION

' ===========================================================================
' BME688 low-level I2C helpers (system I2C bus - no OPEN needed)
' ===========================================================================
FUNCTION RB%(reg%)
  LOCAL INTEGER b
  I2C2 WRITE ADDR, 1, 1, reg%            ' option 1 = keep the bus (repeated start)
  I2C2 READ ADDR, 0, 1, b
  RB% = b
END FUNCTION

SUB WB(reg%, val%)
  I2C2 WRITE ADDR, 0, 2, reg%, val%
END SUB

FUNCTION S8(v%) AS FLOAT
  IF v% > 127 THEN S8 = v% - 256 ELSE S8 = v%
END FUNCTION

FUNCTION S16(v%) AS FLOAT
  IF v% > 32767 THEN S16 = v% - 65536 ELSE S16 = v%
END FUNCTION

' ===========================================================================
' BME688 calibration, measurement and compensation
' ===========================================================================
SUB ReadCalibration
  par_t1  = (RB%(&HEA) << 8) OR RB%(&HE9)
  par_t2  = S16((RB%(&H8B) << 8) OR RB%(&H8A))
  par_t3  = S8(RB%(&H8C))

  par_p1  = (RB%(&H8F) << 8) OR RB%(&H8E)
  par_p2  = S16((RB%(&H91) << 8) OR RB%(&H90))
  par_p3  = S8(RB%(&H92))
  par_p4  = S16((RB%(&H95) << 8) OR RB%(&H94))
  par_p5  = S16((RB%(&H97) << 8) OR RB%(&H96))
  par_p6  = S8(RB%(&H99))
  par_p7  = S8(RB%(&H98))
  par_p8  = S16((RB%(&H9D) << 8) OR RB%(&H9C))
  par_p9  = S16((RB%(&H9F) << 8) OR RB%(&H9E))
  par_p10 = RB%(&HA0)

  par_h1  = (RB%(&HE3) << 4) OR (RB%(&HE2) AND &H0F)
  par_h2  = (RB%(&HE1) << 4) OR (RB%(&HE2) >> 4)
  par_h3  = S8(RB%(&HE4))
  par_h4  = S8(RB%(&HE5))
  par_h5  = S8(RB%(&HE6))
  par_h6  = RB%(&HE7)
  par_h7  = S8(RB%(&HE8))

  par_gh1 = S8(RB%(&HED))
  par_gh2 = S16((RB%(&HEC) << 8) OR RB%(&HEB))
  par_gh3 = S8(RB%(&HEE))

  res_heat_range = (RB%(&H02) AND &H30) >> 4
  res_heat_val   = S8(RB%(&H00))
  range_sw_err   = S8(RB%(&H04) AND &HF0) / 16.0
END SUB

' Start one forced-mode measurement and return immediately (non-blocking).
' The main loop polls status reg 0x1D bit7 (new_data) and reads the result
' when it is set, so the ~150ms conversion does not stall the scroll.
SUB TriggerMeasure
  WB(&H72, &H01)                         ' ctrl_hum: osrs_h = 1
  WB(&H5A, ResHeat(300, amb_temp))       ' res_heat_0 (target 300C)
  WB(&H64, &H65)                         ' gas_wait_0: ~148 ms
  IF variant >= 1 THEN
    WB(&H71, &H20)                       ' run_gas bit 5 (BME688/BME690)
  ELSE
    WB(&H71, &H10)                       ' run_gas bit 4 (BME680)
  ENDIF
  WB(&H70, &H00)                         ' heater on
  WB(&H74, (2 << 5) OR (5 << 2) OR 1)    ' osrs_t x2, osrs_p x16, forced mode -> start
END SUB

FUNCTION ReadTemperature() AS FLOAT
  LOCAL INTEGER raw
  LOCAL FLOAT v1, v2
  raw = (RB%(&H22) << 12) OR (RB%(&H23) << 4) OR (RB%(&H24) >> 4)
  v1 = (raw / 16384.0 - par_t1 / 1024.0) * par_t2
  v2 = ((raw / 131072.0 - par_t1 / 8192.0) * (raw / 131072.0 - par_t1 / 8192.0)) * par_t3 * 16.0
  t_fine = v1 + v2
  amb_temp = t_fine / 5120.0
  ReadTemperature = amb_temp
END FUNCTION

FUNCTION ReadPressure() AS FLOAT
  LOCAL INTEGER raw
  LOCAL FLOAT v1, v2, v3, p
  raw = (RB%(&H1F) << 12) OR (RB%(&H20) << 4) OR (RB%(&H21) >> 4)
  v1 = (t_fine / 2.0) - 64000.0
  v2 = v1 * v1 * (par_p6 / 131072.0)
  v2 = v2 + (v1 * par_p5 * 2.0)
  v2 = (v2 / 4.0) + (par_p4 * 65536.0)
  v1 = (((par_p3 * v1 * v1) / 16384.0) + (par_p2 * v1)) / 524288.0
  v1 = (1.0 + (v1 / 32768.0)) * par_p1
  IF v1 = 0 THEN ReadPressure = 0 : EXIT FUNCTION
  p = 1048576.0 - raw
  p = ((p - (v2 / 4096.0)) * 6250.0) / v1
  v1 = (par_p9 * p * p) / 2147483648.0
  v2 = p * (par_p8 / 32768.0)
  v3 = (p / 256.0) * (p / 256.0) * (p / 256.0) * (par_p10 / 131072.0)
  ReadPressure = p + (v1 + v2 + v3 + (par_p7 * 128.0)) / 16.0
END FUNCTION

FUNCTION ReadHumidity() AS FLOAT
  LOCAL INTEGER raw
  LOCAL FLOAT temp, v1, v2, v3, v4, h
  raw = (RB%(&H25) << 8) OR RB%(&H26)
  temp = t_fine / 5120.0
  v1 = raw - ((par_h1 * 16.0) + ((par_h3 / 2.0) * temp))
  v2 = v1 * ((par_h2 / 262144.0) * (1.0 + ((par_h4 / 16384.0) * temp) + ((par_h5 / 1048576.0) * temp * temp)))
  v3 = par_h6 / 16384.0
  v4 = par_h7 / 2097152.0
  h = v2 + ((v3 + (v4 * temp)) * v2 * v2)
  IF h > 100.0 THEN h = 100.0
  IF h < 0.0 THEN h = 0.0
  ReadHumidity = h
END FUNCTION

' Gas resistance in Ohms.  BME688/BME690 result is at 0x2C/0x2D, BME680 at 0x2A/0x2B.
FUNCTION ReadGas() AS FLOAT
  LOCAL INTEGER raw, rng, msb, lsb
  IF variant >= 1 THEN
    msb = RB%(&H2C) : lsb = RB%(&H2D)
    raw = (msb << 2) OR (lsb >> 6) : rng = lsb AND &H0F
    ReadGas = GasHigh(raw, rng)
  ELSE
    msb = RB%(&H2A) : lsb = RB%(&H2B)
    raw = (msb << 2) OR (lsb >> 6) : rng = lsb AND &H0F
    ReadGas = GasLow(raw, rng)
  ENDIF
END FUNCTION

FUNCTION GasHigh(raw%, rng%) AS FLOAT
  LOCAL INTEGER v1, v2
  v1 = &H40000 >> rng%
  v2 = 4096 + ((raw% - 512) * 3)
  GasHigh = 1000000.0 * v1 / v2
END FUNCTION

FUNCTION GasLow(raw%, rng%) AS FLOAT
  LOCAL FLOAT var1, var2, var3, grange
  grange = 1 << rng%
  var1 = 1340.0 + (5.0 * range_sw_err)
  var2 = var1 * (1.0 + k1(rng%) / 100.0)
  var3 = 1.0 + (k2(rng%) / 100.0)
  GasLow = 1.0 / (var3 * 0.000000125 * grange * (((raw% - 512.0) / var2) + 1.0))
END FUNCTION

' IAQ score 0-100 (higher = cleaner) from gas resistance and humidity
FUNCTION AirQuality(g AS FLOAT, h AS FLOAT) AS FLOAT
  LOCAL FLOAT hum_offset, hum_score, gas_score
  hum_offset = h - HUM_BASE
  IF hum_offset > 0 THEN
    hum_score = (100.0 - HUM_BASE - hum_offset) / (100.0 - HUM_BASE) * (HUM_WEIGHT * 100.0)
  ELSE
    hum_score = (HUM_BASE + hum_offset) / HUM_BASE * (HUM_WEIGHT * 100.0)
  ENDIF
  IF g < gas_baseline THEN
    gas_score = (g / gas_baseline) * (100.0 - HUM_WEIGHT * 100.0)
  ELSE
    gas_score = 100.0 - HUM_WEIGHT * 100.0
  ENDIF
  AirQuality = hum_score + gas_score
END FUNCTION

FUNCTION AirLabel$(s AS FLOAT)
  IF s >= 90 THEN
    AirLabel$ = "excellent"
  ELSEIF s >= 70 THEN
    AirLabel$ = "good"
  ELSEIF s >= 50 THEN
    AirLabel$ = "fair"
  ELSEIF s >= 30 THEN
    AirLabel$ = "poor"
  ELSE
    AirLabel$ = "bad"
  ENDIF
END FUNCTION

FUNCTION ResHeat(target%, amb%) AS FLOAT
  LOCAL FLOAT t, v1, v2, v3, v4, v5
  t = target%
  IF t > 400 THEN t = 400
  v1 = (par_gh1 / 16.0) + 49.0
  v2 = ((par_gh2 / 32768.0) * 0.0005) + 0.00235
  v3 = par_gh3 / 1024.0
  v4 = v1 * (1.0 + (v2 * t))
  v5 = v4 + (v3 * amb%)
  ResHeat = INT(3.4 * ((v5 * (4.0 / (4.0 + res_heat_range)) * (1.0 / (1.0 + (res_heat_val * 0.002)))) - 25))
END FUNCTION
