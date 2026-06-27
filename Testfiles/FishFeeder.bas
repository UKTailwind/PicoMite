' FishFeeder.bas
' Automated fish feeder UI for PicoMite (240x240 display + buttons/joystick)
' Navigation:
' A = next page, B = previous page
' CTRL = toggle edit mode on current page
' UP/DOWN = change selected field/value in edit mode
' LEFT/RIGHT = move field selection in edit mode
' X = manual feed now (from HOME page)
' Y = force RTC SETTIME (from CLOCK page)

Option Explicit

On Error Skip
Stepper Close

Const a = "GP15"
Const b = "GP17"
Const xbtn = "GP19"
Const ybtn = "GP21"
Const up = "GP2"
Const down = "GP18"
Const leftpin = "GP16"
Const rightpin = "GP20"
Const ctrl = "GP3"

Const dir = "GP27"
Const steppin = "GP26"
Const enable = "GP22"

Const FEED_MAX = 6
Const FEED_DEFAULT_ON = 2
Const UNIT_MM = 2
Const STEP_FEEDRATE = 200
Const PRIME_STEP_MM = 5

Const PAGE_HOME = 0
Const PAGE_CLOCK = 1
Const PAGE_FEED0 = 2
Const PAGE_FEED_LAST = PAGE_FEED0 + FEED_MAX - 1
Const PAGE_HELP = PAGE_FEED_LAST + 1
Const PAGE_PRIME = PAGE_HELP + 1
Const PAGE_LAST = PAGE_PRIME

Const EV_NONE = 0
Const EV_A = 1
Const EV_B = 2
Const EV_X = 3
Const EV_Y = 4
Const EV_UP = 5
Const EV_DOWN = 6
Const EV_LEFT = 7
Const EV_RIGHT = 8
Const EV_CTRL = 9

Dim feedHour%(FEED_MAX)
Dim feedMin%(FEED_MAX)
Dim feedUnits%(FEED_MAX)
Dim feedEnabled%(FEED_MAX)

Dim page% = PAGE_HOME
Dim editMode% = 0
Dim fieldSel% = 0
Dim needRedraw% = 1
Dim lastFeedKey$ = ""
Dim startupMinuteKey$ = ""
Dim scheduleDirty% = 0
Dim ev% = EV_NONE
dim notfirst% = 0

Dim setYear% = 2026
Dim setMonth% = 1
Dim setDay% = 1
Dim setHour% = 12
Dim setMin% = 0
Dim setSec% = 0

Dim prevA% = 0, prevB% = 0, prevX% = 0, prevY% = 0
Dim prevUp% = 0, prevDown% = 0, prevLeft% = 0, prevRight% = 0, prevCtrl% = 0
Dim primeEndpos! = 0
Dim backlightTimer% = 1500

InitPins
SeedButtonHistory
InitStepper
LoadSchedule
InitDefaults
LoadClockIntoSetFields
SetStartupMinuteKey
Backlight 100

Do
  ev% = ReadEvent%()

  If ev% <> EV_NONE Then
    HandleEvent ev%
    needRedraw% = 1
    backlightTimer% = 1500
    Backlight 100
  End If

  HandleXActions

  If XPressed%() Then
    backlightTimer% = 1500
    Backlight 100
  End If

  CheckAutoFeed

  If scheduleDirty% Then
    SaveSchedule
    scheduleDirty% = 0
  End If

  If needRedraw% Then
    DrawPage
    needRedraw% = 0
  End If

  If backlightTimer% > 0 Then
    Inc backlightTimer%, -1
    If backlightTimer% = 0 Then Backlight 0
  End If

  Pause 40
Loop

Sub InitPins
  TrySetInput a
  TrySetInput b
  TrySetInput xbtn
  TrySetInput ybtn
  TrySetInput up
  TrySetInput down
  TrySetInput leftpin
  TrySetInput rightpin
  TrySetInput ctrl
  Pause 300
End Sub

Sub TrySetInput(pin$)
  On Error Skip 1
  SetPin pin$, DIN, PULLUP
End Sub

Sub SeedButtonHistory
  prevA% = SafeBtn%(a)
  prevB% = SafeBtn%(b)
  prevX% = XPressed%()
  prevY% = SafeBtn%(ybtn)
  prevUp% = SafeBtn%(up)
  prevDown% = SafeBtn%(down)
  prevLeft% = SafeBtn%(leftpin)
  prevRight% = SafeBtn%(rightpin)
  prevCtrl% = SafeBtn%(ctrl)
End Sub

Sub InitStepper
  Stepper Init
  Stepper Axis X, GP26, GP27, GP22, , 800, 100, 100
  Stepper Position X, 0
  Stepper Run 1
End Sub

Sub InitDefaults
  Local i%
  if notfirst% Then Exit Sub
  For i% = 1 To FEED_MAX
    feedHour%(i%) = 8 + i%
    If feedHour%(i%) > 23 Then feedHour%(i%) = feedHour%(i%) - 24
    feedMin%(i%) = 0
    feedUnits%(i%) = 1
    If i% <= FEED_DEFAULT_ON Then
      feedEnabled%(i%) = 1
    Else
      feedEnabled%(i%) = 0
    End If
  Next i%
End Sub

Sub LoadSchedule
'  On Error Skip 1
  VAR RESTORE
  NormalizeSchedule
End Sub

Sub SaveSchedule
  On Error Skip 1
  notfirst%=1
  VAR SAVE feedHour%(), feedMin%(), feedUnits%(), feedEnabled%(), notfirst%
End Sub

Sub NormalizeSchedule
  Local i%
  For i% = 1 To FEED_MAX
    If feedHour%(i%) < 0 Then feedHour%(i%) = 0
    If feedHour%(i%) > 23 Then feedHour%(i%) = 23
    If feedMin%(i%) < 0 Then feedMin%(i%) = 0
    If feedMin%(i%) > 59 Then feedMin%(i%) = 59
    If feedUnits%(i%) < 1 Then feedUnits%(i%) = 1
    If feedUnits%(i%) > 20 Then feedUnits%(i%) = 20
    If feedEnabled%(i%) <> 0 Then
      feedEnabled%(i%) = 1
    Else
      feedEnabled%(i%) = 0
    End If
  Next i%
End Sub

Sub HandleEvent(ev%)
  If ev% = EV_A Then
    If page% < PAGE_LAST Then
      page% = page% + 1
    Else
      page% = PAGE_HOME
    End If
    editMode% = 0
    fieldSel% = 0
    Exit Sub
  End If

  If ev% = EV_B Then
    If page% > PAGE_HOME Then
      page% = page% - 1
    Else
      page% = PAGE_LAST
    End If
    editMode% = 0
    fieldSel% = 0
    Exit Sub
  End If

  If ev% = EV_X Then
    If page% = PAGE_HOME Or (page% >= PAGE_FEED0 And page% <= PAGE_FEED_LAST) Then
      ManualFeed
    End If
    Exit Sub
  End If

  If page% = PAGE_CLOCK And ev% = EV_Y Then
    ApplyRtc
    Exit Sub
  End If

  If ev% = EV_CTRL Then
    editMode% = 1 - editMode%
    Exit Sub
  End If

  If editMode% = 0 Then Exit Sub

  Select Case page%
    Case PAGE_CLOCK
      EditClock ev%
    Case PAGE_FEED0 To PAGE_FEED_LAST
      EditFeed page% - PAGE_FEED0 + 1, ev%
  End Select
End Sub

Sub HandleXActions
  If page% <> PAGE_PRIME Then Exit Sub

  If XPressed%() Then
    If primeEndpos! = 0 Then
      Stepper Position X, 0
    End If
    If Peek(STEPPER BUFFER) > 10 Then
      Inc primeEndpos!, 0.1
      Stepper GCODE G1, X, primeEndpos!, F, STEP_FEEDRATE
    End If
  Else
    primeEndpos! = 0
  End If
End Sub

Sub EditClock(ev%)
  If ev% = EV_LEFT And fieldSel% > 0 Then fieldSel% = fieldSel% - 1
  If ev% = EV_RIGHT And fieldSel% < 5 Then fieldSel% = fieldSel% + 1

  If ev% = EV_UP Then
    Select Case fieldSel%
      Case 0: Inc setDay%: If setDay% > 31 Then setDay% = 1
      Case 1: Inc setMonth%: If setMonth% > 12 Then setMonth% = 1
      Case 2: Inc setYear%: If setYear% > 2099 Then setYear% = 2000
      Case 3: Inc setHour%: If setHour% > 23 Then setHour% = 0
      Case 4: Inc setMin%: If setMin% > 59 Then setMin% = 0
      Case 5: Inc setSec%: If setSec% > 59 Then setSec% = 0
    End Select
  End If

  If ev% = EV_DOWN Then
    Select Case fieldSel%
      Case 0: Inc setDay%,-1: If setDay% < 1 Then setDay% = 31
      Case 1: Inc setMonth%,-1: If setMonth% < 1 Then setMonth% = 12
      Case 2: Inc setYear%,-1: If setYear% < 2000 Then setYear% = 2099
      Case 3: Inc setHour%,-1: If setHour% < 0 Then setHour% = 23
      Case 4: Inc setMin%,-1: If setMin% < 0 Then setMin% = 59
      Case 5: Inc setSec%,-1: If setSec% < 0 Then setSec% = 59
    End Select
  End If

End Sub

Sub EditFeed(idx%, ev%)
  If ev% = EV_LEFT And fieldSel% > 0 Then fieldSel% = fieldSel% - 1
  If ev% = EV_RIGHT And fieldSel% < 3 Then fieldSel% = fieldSel% + 1

  If ev% = EV_UP Then
    Select Case fieldSel%
      Case 0: feedEnabled%(idx%) = 1: scheduleDirty% = 1
      Case 1
        Inc feedHour%(idx%)
        If feedHour%(idx%) > 23 Then feedHour%(idx%) = 0
        scheduleDirty% = 1
      Case 2
        Inc feedMin%(idx%)
        If feedMin%(idx%) > 59 Then feedMin%(idx%) = 0
        scheduleDirty% = 1
      Case 3
        Inc feedUnits%(idx%)
        If feedUnits%(idx%) > 20 Then feedUnits%(idx%) = 20
        scheduleDirty% = 1
    End Select
  End If

  If ev% = EV_DOWN Then
    Select Case fieldSel%
      Case 0: feedEnabled%(idx%) = 0: scheduleDirty% = 1
      Case 1
        Inc feedHour%(idx%),-1
        If feedHour%(idx%) < 0 Then feedHour%(idx%) = 23
        scheduleDirty% = 1
      Case 2
        Inc feedMin%(idx%),-1
        If feedMin%(idx%) < 0 Then feedMin%(idx%) = 59
        scheduleDirty% = 1
      Case 3
        Inc feedUnits%(idx%),-1
        If feedUnits%(idx%) < 1 Then feedUnits%(idx%) = 1
        scheduleDirty% = 1
    End Select
  End If

End Sub

Sub ApplyRtc
  Local ys%, ms%, ds%, hs%, ns%, ss%
  ys% = setYear%
  ms% = setMonth%
  ds% = setDay%
  hs% = setHour%
  ns% = setMin%
  ss% = setSec%

  RTC SETTIME ys%, ms%, ds%, hs%, ns%, ss%
  Pause 50
  LoadClockIntoSetFields
End Sub

Sub LoadClockIntoSetFields
  Local d$, t$
  d$ = Date$
  t$ = Time$

  setDay% = Val(Mid$(d$, 1, 2))
  setMonth% = Val(Mid$(d$, 4, 2))
  setYear% = 2000 + Val(Right$(d$, 2))

  setHour% = Val(Mid$(t$, 1, 2))
  setMin% = Val(Mid$(t$, 4, 2))
  setSec% = Val(Mid$(t$, 7, 2))
End Sub

Sub CheckAutoFeed
  Local t$, d$, hh%, mm%, i%
  Local key$, minuteKey$

  t$ = Time$
  d$ = Date$
  hh% = Val(Mid$(t$, 1, 2))
  mm% = Val(Mid$(t$, 4, 2))
  minuteKey$ = Right$(d$, 2) + Mid$(d$, 4, 2) + Left$(d$, 2) + Right$("0" + Str$(hh%), 2) + Right$("0" + Str$(mm%), 2)

  If minuteKey$ = startupMinuteKey$ Then Exit Sub
  startupMinuteKey$ = minuteKey$

  For i% = 1 To FEED_MAX
    If feedEnabled%(i%) And hh% = feedHour%(i%) And mm% = feedMin%(i%) Then
      key$ = Right$(d$, 2) + Mid$(d$, 4, 2) + Left$(d$, 2) + Right$("0" + Str$(hh%), 2) + Right$("0" + Str$(mm%), 2) + Str$(i%)
      If key$ <> lastFeedKey$ Then
        DispenseUnits feedUnits%(i%)
        lastFeedKey$ = key$
      End If
    End If
  Next i%
End Sub

Sub SetStartupMinuteKey
  Local t$, d$, hh%, mm%
  t$ = Time$
  d$ = Date$
  hh% = Val(Mid$(t$, 1, 2))
  mm% = Val(Mid$(t$, 4, 2))
  startupMinuteKey$ = Right$(d$, 2) + Mid$(d$, 4, 2) + Left$(d$, 2) + Right$("0" + Str$(hh%), 2) + Right$("0" + Str$(mm%), 2)
End Sub

Sub ManualFeed
  Local idx%, i%
  idx% = 1
  If page% >= PAGE_FEED0 And page% <= PAGE_FEED_LAST Then
    idx% = page% - PAGE_FEED0 + 1
    DispenseUnits feedUnits%(idx%)
    Exit Sub
  End If

  For i% = 1 To FEED_MAX
    If feedEnabled%(i%) Then
      idx% = i%
      Exit For
    End If
  Next i%

  If idx% <= FEED_MAX And feedEnabled%(idx%) = 0 Then Exit Sub
  DispenseUnits feedUnits%(idx%)
End Sub

Sub DispenseUnits(units%)
  Local dist!
  If units% < 1 Then units% = 1
  dist! = units% * UNIT_MM

  Stepper Position X, 0
  Stepper GCODE G1, X, dist!, F, STEP_FEEDRATE

  Do While Peek(STEPPER ACTIVE)
  Loop
End Sub

Sub DrawPage
  CLS
  Select Case page%
    Case PAGE_HOME
      DrawHome
    Case PAGE_CLOCK
      DrawClock
    Case PAGE_FEED0 To PAGE_FEED_LAST
      DrawFeed page% - PAGE_FEED0 + 1
    Case PAGE_HELP
      DrawHelp
    Case PAGE_PRIME
      DrawPrime
  End Select
End Sub

Sub DrawHelp
  Text 4, 4, "HELP"
  Text 4, 28, "A NEXT PAGE"
  Text 4, 52, "B PREV PAGE"
  Text 4, 76, "CTRL EDIT"
  Text 4, 100, "UPDN CHANGE"
  Text 4, 124, "LR MOVE"
  Text 4, 148, "X FEED NOW"
  Text 4, 172, "Y RTC SET"
End Sub

Sub DrawPrime
  Text 4, 4, "PRIME"
  Text 4, 28, "HOLD X RUN"
  Text 4, 52, "REL X STOP"
  Text 4, 76, "CONT MOVE"
  Text 4, 100, "A/B PAGE"
End Sub

Sub DrawHome
  Local nexti%, i%, nowMins%, fm%, m%, t$
  Local dMins%, cH%, cM%
  t$ = Time$
  nowMins% = Val(Mid$(t$, 1, 2)) * 60 + Val(Mid$(t$, 4, 2))

  nexti% = 1
  fm% = 24 * 60 + 1

  For i% = 1 To FEED_MAX
    If feedEnabled%(i%) Then
      m% = feedHour%(i%) * 60 + feedMin%(i%)
      If m% >= nowMins% And m% < fm% Then
        fm% = m%
        nexti% = i%
      End If
    End If
  Next i%

  If fm% = 24 * 60 + 1 Then
    For i% = 1 To FEED_MAX
      If feedEnabled%(i%) Then
        fm% = feedHour%(i%) * 60 + feedMin%(i%)
        nexti% = i%
        Exit For
      End If
    Next i%
  End If

  Text 4, 4, "FISH FEEDER"
  Text 4, 28, Left$(Date$, 8)
  Text 4, 52, Left$(Time$, 8)
  Text 4, 76, "NEXT F" + Trim$(Str$(nexti%))
  If fm% = 24 * 60 + 1 Then
    Text 4, 100, "--:--"
    Text 4, 172, "IN --:--"
  Else
    Text 4, 100, TwoDigit$(feedHour%(nexti%)) + ":" + TwoDigit$(feedMin%(nexti%))
    dMins% = fm% - nowMins%
    If dMins% < 0 Then dMins% = dMins% + (24 * 60)
    cH% = dMins% \ 60
    cM% = dMins% Mod 60
    Text 4, 172, "IN " + TwoDigit$(cH%) + ":" + TwoDigit$(cM%)
  End If
  Text 4, 124, "EN " + Trim$(Str$(EnabledCount%()))
  Text 4, 148, "X FEED NOW"
End Sub

Sub DrawClock
  Local m$, markX%
  markX% = 240 - (3 * MM.FONTWIDTH)
  If editMode% Then
    m$ = "EDIT"
  Else
    m$ = "VIEW"
  End If

  Text 4, 4, "CLOCK " + m$
  Text 4, 28, "D " + TwoDigit$(setDay%) + "/" + TwoDigit$(setMonth%)
  Text 4, 52, "Y " + Trim$(Str$(setYear%))
  Text 4, 76, "T " + TwoDigit$(setHour%) + ":" + TwoDigit$(setMin%)
  Text 4, 100, "S " + TwoDigit$(setSec%)
  Text 4, 124, "Y RTC SET"
  Text 4, 148, "C EDIT"

  If editMode% Then
    Select Case fieldSel%
      Case 0: Text markX%, 28, "<D"
      Case 1: Text markX%, 28, "<M"
      Case 2: Text markX%, 52, "<Y"
      Case 3: Text markX%, 76, "<H"
      Case 4: Text markX%, 76, "<N"
      Case 5: Text markX%, 100, "<S"
    End Select
  End If
End Sub

Sub DrawFeed(idx%)
  Local m$, en$, markX%
  markX% = 240 - (3 * MM.FONTWIDTH)
  If feedEnabled%(idx%) Then
    en$ = "ON"
  Else
    en$ = "OFF"
  End If

  If editMode% Then
    m$ = "EDIT"
  Else
    m$ = "VIEW"
  End If

  Text 4, 4, "FEED " + Trim$(Str$(idx%))
  Text 4, 28, "STATE " + en$
  Text 4, 52, "TIME " + TwoDigit$(feedHour%(idx%)) + ":" + TwoDigit$(feedMin%(idx%))
  Text 4, 76, "UNITS " + Trim$(Str$(feedUnits%(idx%)))
  Text 4, 100, "X TEST"
  Text 4, 124, m$

  If editMode% Then
    Select Case fieldSel%
      Case 0: Text markX%, 28, "<E"
      Case 1: Text markX%, 52, "<H"
      Case 2: Text markX%, 52, "<M"
      Case 3: Text markX%, 76, "<U"
    End Select
  End If
End Sub

Function TwoDigit$(v%)
  TwoDigit$ = Right$("0" + Trim$(Str$(v%)), 2)
End Function

Function EnabledCount%()
  Local i%, c%
  c% = 0
  For i% = 1 To FEED_MAX
    If feedEnabled%(i%) Then Inc c%
  Next i%
  EnabledCount% = c%
End Function

Function ReadEvent%()
  Local aNow%, bNow%, xNow%, yNow%
  Local uNow%, dNow%, lNow%, rNow%, cNow%

  aNow% = SafeBtn%(a)
  bNow% = SafeBtn%(b)
  xNow% = XPressed%()
  yNow% = SafeBtn%(ybtn)
  uNow% = SafeBtn%(up)
  dNow% = SafeBtn%(down)
  lNow% = SafeBtn%(leftpin)
  rNow% = SafeBtn%(rightpin)
  cNow% = SafeBtn%(ctrl)

  ReadEvent% = EV_NONE

  If aNow% And Not prevA% Then
    ReadEvent% = EV_A
  ElseIf bNow% And Not prevB% Then
    ReadEvent% = EV_B
  ElseIf xNow% And Not prevX% Then
    ReadEvent% = EV_X
  ElseIf yNow% And Not prevY% Then
    ReadEvent% = EV_Y
  ElseIf uNow% And Not prevUp% Then
    ReadEvent% = EV_UP
  ElseIf dNow% And Not prevDown% Then
    ReadEvent% = EV_DOWN
  ElseIf lNow% And Not prevLeft% Then
    ReadEvent% = EV_LEFT
  ElseIf rNow% And Not prevRight% Then
    ReadEvent% = EV_RIGHT
  ElseIf cNow% And Not prevCtrl% Then
    ReadEvent% = EV_CTRL
  End If

  prevA% = aNow%
  prevB% = bNow%
  prevX% = xNow%
  prevY% = yNow%
  prevUp% = uNow%
  prevDown% = dNow%
  prevLeft% = lNow%
  prevRight% = rNow%
  prevCtrl% = cNow%
End Function

Function SafeBtn%(pin$)
  Local v%
  v% = 0
  v% = (Pin(pin$) = 0)
  SafeBtn% = v%
End Function

Function SafePin%(pin$)
  Local v%
  v% = 1
  On Error Skip 1
  v% = Pin(pin$)
  SafePin% = v%
End Function

Function XPressed%()
  XPressed% = SafeBtn%(xbtn)
End Function
