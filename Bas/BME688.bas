' ----------------------------------------------------------------------------
' BME688.bas  -  read temperature, pressure, humidity and gas from a Bosch
'                BME688 or BME680 over I2C on the PicoMite, plus a 0-100
'                indoor air-quality score (open humidity+gas heuristic).
'
' These chips are not built-in PicoMite devices, so this talks to one directly
' with I2C WRITE / I2C READ and applies Bosch's floating-point compensation.
' Chip id 0x61 is shared by both parts; the variant_id register (0xF0) tells
' them apart and the gas measurement is configured/decoded accordingly.
'
' Wiring (3.3V part - do NOT use 5V):
'   VCC -> 3V3      GND -> GND
'   SDA -> GP14   (data)        SCL -> GP15   (clock)
' Any pins that list "I2C SDA" / "I2C SCL" in the PicoMite pinout will do;
' GP14/GP15 here are on the second I2C channel (I2C2).
' ----------------------------------------------------------------------------

OPTION EXPLICIT
OPTION DEFAULT NONE

' --- I2C address: 0x76 if SDO is tied low, 0x77 if SDO is tied high ---------
CONST ADDR = &H76

' Calibration coefficients (filled by ReadCalibration, shared with the funcs)
DIM FLOAT par_t1, par_t2, par_t3
DIM FLOAT par_p1, par_p2, par_p3, par_p4, par_p5, par_p6, par_p7, par_p8, par_p9, par_p10
DIM FLOAT par_h1, par_h2, par_h3, par_h4, par_h5, par_h6, par_h7
DIM FLOAT par_gh1, par_gh2, par_gh3
DIM FLOAT res_heat_range, res_heat_val, range_sw_err
DIM FLOAT t_fine                       ' carries temperature into P and H calc
DIM FLOAT amb_temp = 25                ' ambient used to compute the heater target (updated each read)
DIM INTEGER variant                    ' 0 = BME680, 1 = BME688 (chip id is 0x61 for both)
DIM INTEGER st                         ' gas status byte (reg 0x2B)
DIM INTEGER dbg = 0                     ' set 1 to trace the gas config/status once at startup

' Gas-range lookup tables used only by the BME680 (variant 0) gas formula
DIM FLOAT k1(15) = (0,0,0,0,0,-1,0,-0.8,0,0,-0.2,-0.5,0,-1,0,0)
DIM FLOAT k2(15) = (0,0,0,0,0.1,0.7,0,-0.8,-0.1,0,0,0,0,0,0,0)

' --- Indoor Air Quality (IAQ) scoring -------------------------------------
' This is the common open humidity+gas heuristic (Pimoroni / David Bird style),
' NOT Bosch's proprietary BSEC algorithm.  It produces a 0-100 score where a
' HIGHER number is cleaner air.  The gas reading is relative, so we track a
' rolling clean-air baseline: it rises toward cleaner air but only drifts down
' slowly, so a pollution event won't drag the reference down while genuine
' sensor ageing is still followed over hours.
CONST BURN_SECS  = 300                 ' warm-up seconds before showing IAQ (lower for quick tests)
CONST HUM_BASE   = 40.0                ' ideal indoor relative humidity (%)
CONST HUM_WEIGHT = 0.25                ' humidity's share of the score (gas gets the rest)
CONST BASE_UP    = 0.05                ' how fast the baseline rises toward cleaner air (per reading)
CONST BASE_DECAY = 0.99999             ' slow per-reading downward drift to track sensor ageing
DIM FLOAT gas_baseline = 0             ' rolling clean-air gas resistance reference

' ---------------------------------------------------------------------------
' Start up
' ---------------------------------------------------------------------------
SETPIN GP14, GP15, I2C2                ' allocate sda=GP14, scl=GP15 to channel 2
I2C2 OPEN 400, 1000                    ' 400 kHz, 1 s timeout

WB(&HE0, &HB6)                         ' soft reset - clears the device incl. heater control
PAUSE 10                               ' reset takes a few ms; data sheet says 2 ms min

IF RB%(&HD0) <> &H61 THEN
  PRINT "No BME688/BME680 found at &H"; HEX$(ADDR)
  PRINT "chip id read = &H"; HEX$(RB%(&HD0)); "  (expected &H61)"
  I2C2 CLOSE
  END
ENDIF

variant = RB%(&HF0)                    ' variant_id: 0 = BME680, 1 = BME688, 2 = BME690
IF variant = 2 THEN
  PRINT "BME690 found (chip id &H61, variant 2)"
ELSEIF variant = 1 THEN
  PRINT "BME688 found (chip id &H61, variant 1)"
ELSE
  PRINT "BME680 found (chip id &H61, variant 0)"
ENDIF

ReadCalibration

' ---------------------------------------------------------------------------
' Main loop: forced-mode reading once per second, then an IAQ score
' ---------------------------------------------------------------------------
PRINT "Burning in for "; STR$(BURN_SECS); " s in clean air to learn the gas baseline..."

DIM INTEGER t0 = TIMER                  ' TIMER is free-running milliseconds
DIM INTEGER burning = 1
DIM FLOAT tC, pHPa, rh, gasR, iaq

DO
  IF dbg THEN TraceMeas : dbg = 0
  Measure
  tC   = ReadTemperature()
  pHPa = ReadPressure() / 100.0
  rh   = ReadHumidity()
  IF variant >= 1 THEN st = RB%(&H2D) ELSE st = RB%(&H2B)  ' 688/690 gas_lsb 0x2D, 680 0x2B
  IF (st AND &H20) THEN gasR = ReadGas() ELSE gasR = -1     ' -1 means gas not valid this cycle

  PRINT "T="; STR$(tC, 3, 2); " C   ";
  PRINT "P="; STR$(pHPa, 4, 2); " hPa   ";
  PRINT "H="; STR$(rh, 3, 2); " %   ";

  IF gasR < 0 THEN
    PRINT "Gas=---- (not valid)"
  ELSE
    ' rolling baseline: rise toward cleaner air, drift down only slowly
    IF (st AND &H10) THEN                ' only update when the heater is stable
      IF gas_baseline = 0 THEN
        gas_baseline = gasR
      ELSEIF gasR > gas_baseline THEN
        gas_baseline = gas_baseline + BASE_UP * (gasR - gas_baseline)
      ELSE
        gas_baseline = gas_baseline * BASE_DECAY
      ENDIF
    ENDIF

    PRINT "Gas="; STR$(gasR / 1000.0, 4, 1); " kOhm   ";
    IF burning THEN
      PRINT "warming up, "; STR$(INT((BURN_SECS * 1000 - (TIMER - t0)) / 1000)); "s  (base ";
      PRINT STR$(gas_baseline / 1000.0, 4, 1); "k)"
      IF (TIMER - t0) >= BURN_SECS * 1000 THEN burning = 0
    ELSEIF gas_baseline > 0 THEN
      iaq = AirQuality(gasR, rh)
      PRINT "IAQ="; STR$(iaq, 3, 0); "/100 ("; AirLabel$(iaq); ")   base=";
      PRINT STR$(gas_baseline / 1000.0, 4, 1); "k"
    ELSE
      PRINT "(no baseline yet)"
    ENDIF
  ENDIF
  PAUSE 1000
LOOP

' ===========================================================================
' Low-level I2C helpers
' ===========================================================================
' Read one byte from register reg%
FUNCTION RB%(reg%)
  LOCAL INTEGER b
  I2C2 WRITE ADDR, 1, 1, reg%          ' option 1 = keep the bus (repeated start)
  I2C2 READ ADDR, 0, 1, b
  RB% = b
END FUNCTION

' Write value val% to register reg%
SUB WB(reg%, val%)
  I2C2 WRITE ADDR, 0, 2, reg%, val%
END SUB

' Sign-extend an 8-bit value
FUNCTION S8(v%) AS FLOAT
  IF v% > 127 THEN S8 = v% - 256 ELSE S8 = v%
END FUNCTION

' Sign-extend a 16-bit value
FUNCTION S16(v%) AS FLOAT
  IF v% > 32767 THEN S16 = v% - 65536 ELSE S16 = v%
END FUNCTION

' ===========================================================================
' Read and assemble the factory calibration coefficients
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
  range_sw_err   = S8(RB%(&H04) AND &HF0) / 16.0   ' signed high nibble (BME680 gas calc)
END SUB

' ===========================================================================
' Trigger one forced-mode measurement (incl. gas) and wait for the data
' ===========================================================================
SUB Measure
  ' humidity oversampling x1
  WB(&H72, &H01)                       ' ctrl_hum: osrs_h = 1

  ' set up the gas heater: target 300C, ambient from the last reading, wait ~150ms
  WB(&H5A, ResHeat(300, amb_temp))     ' res_heat_0
  WB(&H64, &H65)                       ' gas_wait_0: 4 x 37 = 148 ms
  IF variant >= 1 THEN                 ' ctrl_gas_1: run_gas (differs by chip) + heater profile 0
    WB(&H71, &H20)                     '   BME688/BME690: run_gas is bit 5
  ELSE
    WB(&H71, &H10)                     '   BME680: run_gas is bit 4
  ENDIF
  WB(&H70, &H00)                       ' ctrl_gas_0: heater on

  ' ctrl_meas: osrs_t = 2 (x2), osrs_p = 5 (x16), mode = 1 (forced) -> starts it
  WB(&H74, (2 << 5) OR (5 << 2) OR 1)

  ' wait until the new-data bit (0x1D bit7) is set and the gas conversion done
  DO
    PAUSE 5
  LOOP UNTIL (RB%(&H1D) AND &H80)
END SUB

' ===========================================================================
' Diagnostic: read the gas config and status registers straight back
' ===========================================================================
' Trigger one forced measurement and watch the status register evolve,
' so we can see whether the gas phase (bit6) ever runs and when new_data (bit7)
' and gas_valid (0x2B bit5) appear.
SUB TraceMeas
  LOCAL INTEGER i, s, prev, glsb
  IF variant >= 1 THEN glsb = &H2D ELSE glsb = &H2B   ' 688/690 gas_lsb 0x2D, 680 0x2B
  PRINT "--- trace: status 0x1D and gas_lsb &H"; HEX$(glsb, 2); " during one measurement ---"
  WB(&H72, &H01)
  WB(&H5A, ResHeat(300, amb_temp))
  WB(&H64, &H65)
  WB(&H71, &H20)
  WB(&H70, &H00)
  WB(&H74, (2 << 5) OR (5 << 2) OR 1)   ' trigger forced mode
  prev = -1
  FOR i = 0 TO 150                       ' watch for up to ~300 ms
    s = RB%(&H1D)
    IF s <> prev THEN
      PRINT STR$(i * 2); " ms:  0x1D=&H"; HEX$(s, 2);
      PRINT "  new="; STR$((s AND &H80) <> 0);
      PRINT " gasmeas="; STR$((s AND &H40) <> 0);
      PRINT " meas="; STR$((s AND &H20) <> 0);
      PRINT "   gas_lsb=&H"; HEX$(RB%(glsb), 2)
      prev = s
    ENDIF
    PAUSE 2
  NEXT i
  s = RB%(glsb)
  PRINT "final gas_lsb=&H"; HEX$(s, 2); "   valid="; STR$((s AND &H20) <> 0); " stable="; STR$((s AND &H10) <> 0)
  PRINT "-------------------------------------------------------------------"
END SUB

SUB DumpRegs
  PRINT "--- register dump (read back after one forced measurement) ---"
  PRINT "variant_id 0xF0 = &H"; HEX$(RB%(&HF0), 2)
  PRINT "ctrl_gas_0 0x70 = &H"; HEX$(RB%(&H70), 2); "   (heat_off is bit3, want 0)"
  PRINT "ctrl_gas_1 0x71 = &H"; HEX$(RB%(&H71), 2); "   (run_gas bit5, want &H20 set)"
  PRINT "ctrl_meas  0x74 = &H"; HEX$(RB%(&H74), 2); "   (mode returns to 00=sleep when done)"
  PRINT "res_heat_0 0x5A = &H"; HEX$(RB%(&H5A), 2); "   (heater code we wrote)"
  PRINT "gas_wait_0 0x64 = &H"; HEX$(RB%(&H64), 2); "   (want &H59)"
  PRINT "status     0x1D = &H"; HEX$(RB%(&H1D), 2); "   (bit7 new_data, bit6 gas_measuring, bit5 measuring)"
  IF variant >= 1 THEN                  ' BME688/BME690 gas data is at 0x2C/0x2D, BME680 at 0x2A/0x2B
    PRINT "gas_r_msb  0x2C = &H"; HEX$(RB%(&H2C), 2)
    PRINT "gas_r_lsb  0x2D = &H"; HEX$(RB%(&H2D), 2); "   (bit5 valid, bit4 stable, nibble=range)"
  ELSE
    PRINT "gas_r_msb  0x2A = &H"; HEX$(RB%(&H2A), 2)
    PRINT "gas_r_lsb  0x2B = &H"; HEX$(RB%(&H2B), 2); "   (bit5 valid, bit4 stable, nibble=range)"
  ENDIF
  PRINT "ResHeat(300,25) = "; ResHeat(300, 25); "   par_gh1="; par_gh1; " gh2="; par_gh2; " gh3="; par_gh3
  PRINT "res_heat_range  = "; res_heat_range; "   res_heat_val="; res_heat_val
  PRINT "--------------------------------------------------------------"
END SUB

' ===========================================================================
' Compensation maths (Bosch BME68x datasheet, floating-point versions)
' ===========================================================================
FUNCTION ReadTemperature() AS FLOAT
  LOCAL INTEGER raw
  LOCAL FLOAT v1, v2
  raw = (RB%(&H22) << 12) OR (RB%(&H23) << 4) OR (RB%(&H24) >> 4)
  v1 = (raw / 16384.0 - par_t1 / 1024.0) * par_t2
  v2 = ((raw / 131072.0 - par_t1 / 8192.0) * (raw / 131072.0 - par_t1 / 8192.0)) * par_t3 * 16.0
  t_fine = v1 + v2
  amb_temp = t_fine / 5120.0           ' keep the heater calc using the real ambient
  ReadTemperature = amb_temp
END FUNCTION

' Returns pressure in Pascals.  Call ReadTemperature() first (sets t_fine).
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

' Returns relative humidity in %.  Call ReadTemperature() first (sets t_fine).
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

' Returns gas resistance in Ohms, using the formula for the detected chip.
' The gas result lives in different registers per variant:
'   BME688/BME690 (variant 1/2): gas_r_msb 0x2C, gas_r_lsb 0x2D
'   BME680 (variant 0):          gas_r_msb 0x2A, gas_r_lsb 0x2B
FUNCTION ReadGas() AS FLOAT
  LOCAL INTEGER raw, rng, msb, lsb
  IF variant >= 1 THEN
    msb = RB%(&H2C) : lsb = RB%(&H2D)
    raw = (msb << 2) OR (lsb >> 6)
    rng = lsb AND &H0F
    ReadGas = GasHigh(raw, rng)        ' BME688/BME690 high-gas formula
  ELSE
    msb = RB%(&H2A) : lsb = RB%(&H2B)
    raw = (msb << 2) OR (lsb >> 6)
    rng = lsb AND &H0F
    ReadGas = GasLow(raw, rng)         ' BME680
  ENDIF
END FUNCTION

' Indoor air-quality score 0-100 (higher = cleaner) from gas resistance and RH.
' Humidity is scored against the ideal HUM_BASE; gas against the learned baseline.
FUNCTION AirQuality(g AS FLOAT, h AS FLOAT) AS FLOAT
  LOCAL FLOAT hum_offset, hum_score, gas_score
  hum_offset = h - HUM_BASE
  IF hum_offset > 0 THEN                ' too humid: penalise toward 100% RH
    hum_score = (100.0 - HUM_BASE - hum_offset) / (100.0 - HUM_BASE) * (HUM_WEIGHT * 100.0)
  ELSE                                  ' too dry: penalise toward 0% RH
    hum_score = (HUM_BASE + hum_offset) / HUM_BASE * (HUM_WEIGHT * 100.0)
  ENDIF
  IF g < gas_baseline THEN              ' dirtier than baseline: scale gas points down
    gas_score = (g / gas_baseline) * (100.0 - HUM_WEIGHT * 100.0)
  ELSE                                  ' as clean or cleaner: full gas points
    gas_score = 100.0 - HUM_WEIGHT * 100.0
  ENDIF
  AirQuality = hum_score + gas_score
END FUNCTION

' Word description for an IAQ score
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

' BME688 high-range gas formula (no per-chip gas calibration needed)
FUNCTION GasHigh(raw%, rng%) AS FLOAT
  LOCAL INTEGER v1, v2
  v1 = &H40000 >> rng%                 ' 262144 >> gas_range
  v2 = 4096 + ((raw% - 512) * 3)
  GasHigh = 1000000.0 * v1 / v2
END FUNCTION

' BME680 gas formula (uses range_sw_err and the k1/k2 lookup tables)
FUNCTION GasLow(raw%, rng%) AS FLOAT
  LOCAL FLOAT var1, var2, var3, grange
  grange = 1 << rng%
  var1 = 1340.0 + (5.0 * range_sw_err)
  var2 = var1 * (1.0 + k1(rng%) / 100.0)
  var3 = 1.0 + (k2(rng%) / 100.0)
  GasLow = 1.0 / (var3 * 0.000000125 * grange * (((raw% - 512.0) / var2) + 1.0))
END FUNCTION

' Heater resistance register value for a target temperature (Bosch formula)
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
