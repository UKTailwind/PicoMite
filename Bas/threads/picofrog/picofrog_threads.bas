  '@harm
  'status
  '   b1   game works, keyboard control,framebuffer merge+music sync'd
  '   b2   gamemite keys implemented
  '   b3   music note adsr changed to support 2040 @ 252MHz
  '   b4   @fly and generic LCD support
  '   b5   fixed fly bugs, know bug: high score lost when RUN
  '   b6   removed lady frog, VGA and LCD and Game*Mite
  '   b7   wii, nunchuck and nes controller work
  '   b8   fix wii, add sprites,crocodile + turtle dive
  '   b9   framework levels, lane speed in data statements
  '   t1   threads (PicoMite V8.00.00b4 or later): the music and the sound effects are threads of their own
  '        (MusicThread, SfxThread) in place of the SETTICK routines music, jump and DieSnd
  
  '@harm todo list
  ' - repair high score
  ' - implement croc in home position
  ' - implement lady frog
  '   create combi sprite ladyfrog-log
  '   occasional change log to lady-log when none on screen, new char in string
  '   in underground test frog, detect new char -> change sprite offset to lady
  '   always change new char back to log char in wrap
  '   when home, check sprite offset, if lady then +200, reset sprite offset
  ' - check sounds against real game
  ' - make levels...
  
  'Pico Frog
  ' by Martin Herhaus 2022
  ' This is a Frogger game which resembles the original Arcade version
  ' from Konami  (c) released in 1981, but completely rewritten from the
  ' ground up in MMBasicVGA for the Raspberry Pico
  '
  ' This code has an open source license, you can copy it, change it,
  ' for any purpose for commercial use or not. But these comments
  ' should be in you main file.
  ' The graphics and Music are based upon but not the original
  '------
  ' developed on PicoMite Version 5.07.05RC...
  ' runs on PicoMite Version 5.07.06
  ' would not run on lower Versions due to the use of
  ' Framebuffer, Layer and Sprites. For this, the Pico also has to run
  ' at 252000 KHz or higher
  '
  ' Use OPTION CPUSPEED 252000
  ' Volhout 2025 : PicoMite MMbasic 6 (tested on 6.01.00b15) and RP2350
  '------------------------------------------------------------------------
  
  Option BASE 0
  Option DEFAULT NONE
  Option EXPLICIT ON
  
  'platform
  const lcd%=not(instr(mm.info(lcdpanel),"MODE")) 'every VGA or HDMI display gives "MODEn", an LCD panel its name
  const lc$=choice(lcd%=1,"F","N") '@harm what framebuffer to use
  dim f_merge%=0, l_time%, h_beat%=50'30 '@harm to get suitible game play with LCD
  if instr(mm.device$,"2350") then
    const n_adsr%=10 '10 adsr steps per note
    const beat%=23 'ms
    Dim ADSR%(n_adsr%)=(25,25,25,24,22,20,18,13,7,3,0)
    Dim ADSR2%(n_adsr%)=(2,8,12,17,22,20,15,10,5,2,0)
  else '2040, less cpu power
    const n_adsr%=7
    const beat%=33 'ms
    Dim ADSR%(n_adsr%)=(25,24,22,18,13,7,3,0)
    Dim ADSR2%(n_adsr%)=(3,10,21,20,13,9,3,0)
  end if
  
  'controllers
  const no_nes%=0 '<---- set to 1 to disable NES support
  Const nes_data% = Mm.Info(PinNo GP1) 'define needed, NES is not auto detect
  Const nes_latch% = Mm.Info(PinNo GP2)
  Const nes_clock% = Mm.Info(PinNo GP3)
  Const nes_pulse! = 0.012 ' 12uS
  const gm%=(mm.info(platform)="Game*Mite") '@harm specific gamemite
  dim is_wii%=0,ad_wii%=&h52,iic_wii%,is_nes%=0
  
  'game
  dim is_fly%=0  '@fly
  Dim Col%(15)
  Dim FPOSX%,FPOSY%,FPDIR%,FPOSP%=18,FMOV%,tmp$,f%,RSTRT%,FDEL%,Level%,Homes%,Time%
  Dim flx%,fly%,fox%,foy%,cl1%,cl2%,snr%,mt%,FL%,FR%,JP%,n%,Score%,High%,i%,dxl%
  Dim Lane$(10) Length 32,PLD$ Length 1,lps%(10),Fmax%,LTime%,Lives%,Tm%,FD%=0
  Dim ld%(10)'=(0,2,-1,1,2,-2,-2,4,-2,1,-1) 'this is land speed for level 1
  Dim ly%(10)=(0,24,40,56,72,88,120,136,152,168,184)
  dim dt_seq%(9)=(9,10,11,12,13,63,13,12,11,10),dt2%=0,dtx%,dt5%=0,FU$
  Dim FRB%,SND%,LP%,TX%,TY%,TDX%,TDY%,spnr%,Slot%,Freq1%,Freq2%,Nest%(5),str%(20)
  
  'threads: the songs and sound effects the game asks for (PlaySong, PlaySfx)
  Const S_INTRO=1,S_MAIN=2,S_YANKEE=3,S_MUS2=4,S_MUS3=5,S_HOME1=6,S_COMPLETE=7,S_KILL=8,S_OVER=9
  Const X_JUMP=1,X_DIE=2
  Dim want%=0,sfx%=0
  
  'alternate controllers @harm
  if gm% then
    init_kb 'initialize GM keys
  else
    'check NES
    if nes_possible%() then nes_init : is_nes%=1 'init the port, confirm in startscreen
    'check WII
    if instr(mm.info(system I2C),"I2C") then find_wii
  end if
  
  '--------- prepare the Graphic
  High%=2500
  Restore colors:For f%=1 To 15:Read Col%(f%):Next f%
  JP%=600:FRB%=1:SND%=1:Font 9
  if lcd% then
    FRAMEBUFFER CREATE 'only with LCD, else use N
  else
    mode 2 'VGA
  end if
  FRAMEBUFFER LAYER
  framebuffer write L:cls rgb(blue) 'cover the N with a blue screen
  framebuffer write lc$
  
  'Prepare the playing field
  read_Sprites
  disp
  FRAMEBUFFER WRITE L
  CLS
  Box 0,0,224,218,,col%(1),col%(1)
  Box 224,0,96,216,,COL%(7),COL%(1)
  Box 226,2,92,212,,COL%(7),COL%(1)
  Box 239,6,64,44,,Col%(6),COL%(4)
  bing
  if lcd% then if lcd% then framebuffer merge 0,b
  framebuffer write lc$
  Box 0,0,224,24,,0,0
  For f%=0 To 192 Step 8:Sprite write 29,f%,0: Next f%
  For f%=0 To 192 Step 48:Box f%,0,32,24,,0,0:Sprite write 28,f%,0: Next f%
  
  'threads: the music and the sound effects, each in a loop of its own (RUN ends them)
  Thread Start 1 Locals 12 Nest 4, MusicThread
  Thread Start 2 Locals 8 Nest 4, SfxThread

start:
  FRAMEBUFFER WRITE L
  Box 0,0,224,240,,0,0  'clear L playfield
  startscreen
  Score%=0:Level%=0:Lives%=3
  ShowLives
  RSTRT%=0
  'Start Music
  PlaySong S_INTRO
  LTime%=Timer:FDEL%=50
  
restart: 'this is a new game
  Homes%=0
  'load the new level
   Inc Level% : loadlevel Level% : Create_street
  'clear the whole Playfeeld on the layer
  FRAMEBUFFER WRITE L
  Box 0,0,220,240,,0,0
  Text 240,80, "SCORE:",,,,Col%(7),Col%(1)
  Text 240,104,"HI-SCORE:",,,,Col%(7),Col%(1)
  Text 240,136,"LIVES:",,,,Col%(6),Col%(1)
  Text 240,172,"TIME:",,,,Col%(6),Col%(1)
  write_high
  
restart1: 'this is the next frog
  ' clear Homeslots
  is_fly%=0 : add_fly is_fly% '@fly  
  Inc Score%,10*(Int((Timer-LTime%)/500)) : write_score 'points for remaining time
  If FD%=0 Then Box 0,0,220,240,,0,0
  FD%=0 : FU$=""
  LTime%=Timer
  Do While Inkey$<>"":Loop 'clear input buffer
  FPOSX%=96:FPOSY%=200:FPDIR%=1'@harm
  FMOV%=0:FMAX%=FPOSY%
  
  '----------gameloop-----------------------------------
  LP%=1
  TX%=0:TY%=0:TDX%=2:TDY%=2
  spnr%=10
  Do
    move_lanes
    add_fly is_fly% '@fly
    if lcd% then framebuffer merge 0,b 'threads: the merge is the game's, no longer in the music interrupt
    FRAMEBUFFER WRITE L
    
    if l_time%>timer then pause l_time%-timer 'threads: the frame's wait; the music and sound threads run meanwhile
    l_time% = timer + h_beat%
    
    If FDEL%=1 Then
      Box 0,0,220,240,,0,0
      LTime%=Timer:tm%=1
      Sprite write FPOSP%+1,FPOSX%,FPOSY%
      Do While Inkey$<>"":Loop ' @thwill
    EndIf
    
    If FDEL% Then
      Inc FDEL%,-1
    Else
      Move_Player : Inc tm%,-1
      select case tm%
        case 0 'update timer
          Update_Timer 60-Int((Timer-LTime%)/500)
          tm%=20 '20 x 50ms(h_beat%) = 1 second
        case 4 'diving turtles lane 2
          dxl%=instr(lane$(2),"uuu") : dtx%=16*(dxl%)-lps%(2)-4
          'text 0,225,str$(dxl%,2,0," ")+" "+str$(lps%(2),2,0," ") '@harm debug
          if dt2%<10 then
            if dtx%<240 and dtx%>0 then
              framebuffer write lc$
              sprite write dt_seq%(dt2%),dtx%,ly%(2)
              sprite write dt_seq%(dt2%),dtx%+16,ly%(2)
              sprite write dt_seq%(dt2%),dtx%+32,ly%(2)
              framebuffer write l
            end if
            inc dt2%
          else
            dt2%=0 'end of sequence
          end if
        case 8 'diving turtles lane 5
          dxl%=instr(lane$(5),"uuu") : dtx%=16*(dxl%)-lps%(5)-4
          'text 0,225,str$(dxl%,2,0," ")+" "+str$(lps%(2),2,0," ") '@harm debug
          if dt5%<10 then
            if dtx%<240 and dtx%>0 then
              framebuffer write lc$
              sprite write dt_seq%(dt5%),dtx%,ly%(5)
              sprite write dt_seq%(dt5%),dtx%+16,ly%(5)
              sprite write dt_seq%(dt5%),dtx%+32,ly%(5)
              framebuffer write l
            end if
            inc dt5%
          else
            dt5%=0 'end of sequence
          end if
        case 12 'check if you die
          if FU$="u" and FD%=0 then
            if FPOSY%>ly%(3) then 'turtle line 5
              if dt5%>3 and dt5%<7 then  FD%=1
            else 'turtle line 2
              if dt2%>3 and dt2%<7 then  FD%=1
            end if
          end if
        case 16 'animate croco
          'for croc in line 3
          i%=instr(lane$(3),"m")
          if i% then
            f%=17-(rnd<0.5)
            framebuffer write lc$
            sprite write f%,min(16*(i%-2)+lps%(3)+4,224),ly%(3)
            framebuffer write L
          end if
      end select
    EndIf
    
    If RSTRT%=1 Then RSTRT%=0 : GoTo restart1 'new frog
    If RSTRT%=2 Then RSTRT%=0 : GoTo restart  'next level
    If FMOV%=0 Then automove
    
    Get_frog_Undergrnd
    
    If FD% Then
      'frog death
      PlaySong S_KILL : PlaySfx X_DIE 'threads
      Hide_Frog
      FRAMEBUFFER WRITE L:Sprite write 30,FPOSX%,FPOSY%
      FDEL%=20
      Inc Lives%,-1
      ShowLives
      If Lives%=0 Then
        PlaySong S_OVER : GameOver
      else
        goto restart1
      end if
    EndIf
    
  Loop
  
  
sub add_fly n% '@fly
  'fly sprite = 27, n% = 0(none),1,2,3,4,5 home slot
  if n% then Sprite write 27,8+(n%-1)*48,8
end sub
  
Sub GameOver
  text 112,112,"GAME OVER",CM,,2,rgb(MYRTLE),rgb(white)
  if lcd% then framebuffer merge 0,b
  
  Pause 10000
  run '@harm prevent nesting issue
End Sub
  
Sub ShowLives
  Local F%
  FRAMEBUFFER WRITE L
  Box 236,144,80,16,,col%(1),col%(1)
  For f%=1 To Lives%
    Sprite write FPOSP%+1,222+f%*16,144
  Next f%
End Sub
  
Sub write_score
  Local S$
  s$=Str$(Score%)
  FRAMEBUFFER WRITE L
  Do While Len(s$)<5:s$="0"+s$:Loop
  Text 240,88, S$,,,,Col%(7),Col%(1)
  If Score%>High% Then High%=Score%: write_High
End Sub
  
Sub write_high
  Local S$
  s$=Str$(High%)
  FRAMEBUFFER WRITE L
  Do While Len(s$)<5:s$="0"+s$:Loop
  Text 240,112, S$,,,,Col%(7),Col%(1)
End Sub
  
Sub Update_Timer TimeLeft%
  local mx_try%
  If TimeLeft%>0 Then
    FRAMEBUFFER WRITE L
    Box 240+TimeLeft%,180,60-TimeLeft%,10,,Col%(1),Col%(1)
    Box 240,180,TimeLeft%,10,,,Col%(2+2*(TimeLeft%<20))
    Box 240,180,60,10,,col%(7)
    
    '@fly handling, appear <30 sec, remove >5 sec, do not remove frog in nest
    if timeleft%<30 then
      if is_fly%=0 then
        if timeleft%>5 then
          mx_try%=0
          do '@fly : find free slot, max 8 tries to prevent lockup
            is_fly%=int(1+5*rnd) : inc mx_try%
          loop until nest%(is_fly%)=0 or mx_try%=8
        end if
      else
        if timeleft%<5 then 'remove fly again
          if nest%(is_fly%)=0 then 'no frog landed
            framebuffer write lc$
            Box 8+(is_fly%-1)*48,8,16,16,,col%(1),col%(1) 'clear fly
            framebuffer write L
          end if
          is_fly%=0 'too late to land on frog
        end if
      end if
    end if
  Else '@fly ---end fly section
    FD%=1 'frog = dead
  EndIf
End Sub
  
Sub startscreen
  Local title$,tk%,tc%,x%,y%,f%,bit%
  Do While Inkey$<>"":Loop ' @thwill
  framebuffer write lc$
  CLS
      For x%=200 To 16 Step -8:For y%=16 To 184 Step 16:Sprite write 29,x%,y%
  Next y%: Next x%
  Restore Logo
  For y%=1 To 17:Read str%(y%):Next
  For f%=0 To 218 Step 16
    flowers f%,0,col%(1),Col%(4):flowers f%,200,col%(1),Col%(4)
  Next
  bit%=4
  Box 16,16,200,6,,0,0
  Box 204,16,8,136,,0,0
  Box 16,16,12,136,,0,0
  Box 16,124,200,64,,0,0
  For x%=30 To 1 Step -1
    For y%=1 To 17
      If Not (str%(y%) And bit%) Then
        Box 20+x%*6,16+y%*6,6,6,,0,0
      Else
        FRAMEBUFFER write L
        Box 240+x%*2,10+y%*2,2,2,,col%(6),Col%(6)
        framebuffer write lc$
      EndIf
    Next
    Inc bit%,bit%:Pause 20
  Next
  FRAMEBUFFER write L
  For y%=16 To 200 Step 16
    flowers 0,y%,col%(1),Col%(4):flowers 208,y%,col%(1),Col%(4)
  Next
  framebuffer write lc$
  Colour col%(7)
  text 112,140,"PicoMite Version",C
  text 112,148,"by Martin Herhaus 2022",C
  text 112,156,"PicoGAME VGA Support",C
  text 112,164,"by Thomas Williams",C
  Title$=" Game based on the Arcade Game Frogger - copyright Konami 1981  - "
  Title$=Title$+"The Graphics and Music are based upon but not the original ***"
  tc%=0: tk%=7
  Do
    If TK%=7 Then
      Colour col%(6) : text 14,176,Left$(title$,34)
      title$=Right$(title$,Len(title$)-1)+Left$(title$,1)
      Colour Col%(tc%):Inc tc%:TC%=TC% And 15
      text 112,184,"Press Fire,'A' or Space",C
    Else
      blit 17,176,16,176,197,8
      Box 207,168,1,8,,0,0
    EndIf
    Inc tk%,-1:If tk%=0 Then TK%=7
    if lcd% then
      framebuffer merge 0,b
    else
      pause 15
    endif
  Loop until inkey$=" " or rd_kb$()="A " or nes_read$()="A " 'NES is confirmed here
  if nes_read$()="A " then is_nes%=2 'confirm the NES controller is used here
  
  bing : Pause 500 : start_sound
  'clear the Game layer
  FRAMEBUFFER WRITE L :  Box 0,0,224,240,,0,0
  
End Sub
  
Sub Get_frog_Undergrnd
  'Guess what is under the Frog
  If FMOV%>=2 Then Exit Sub
  Local LPX%
  LP%=Int(FPOSY% /16)
  If LP%=6 Then Exit Sub
  If LP%>6 Then Inc LP%,-1
  'calculate frog position in string array
  If LP%<11 Then
    If ld%(LP%) > 0 Then
      LPX%=(FPOSX%-Lps%(LP%)-4)/16+2
    Else
      LPX%=(FPOSX%+Lps%(LP%)-8)/16+1
    EndIf
    FU$=Mid$(lane$(LP%),LPx%,1) 'this is at frog location in the lanes/water
    If LP%>5  Then
      If FU$<>"0" Then FD%=1 'street, when not on street=0 you die
    Else
      If instr("0km",FU$) Then FD%=1  'water/croq tail,head, you drown/die
    EndIf
  EndIf
End Sub
  
Sub automove
  LP%=Int(FPOSY%/16)
  If LP%<6 Then
    FRAMEBUFFER WRITE L
    Box FPOSX%,FPOSY%,14,16,,0,0
    Inc FPOSX%,ld%(LP%)
    If FPOSX%>200 Then FPOSX%=200
    If FPOSX%<0 Then FPOSX%=0
    Sprite write FPOSP%+1,FPOSX%,FPOSY%
  EndIf
End Sub
  
Sub Hide_Frog
  FRAMEBUFFER WRITE L
  Box FPOSX%,FPOSY%,16,16,,0,0
  framebuffer write lc$
End Sub
  
Sub Frog
  FRAMEBUFFER WRITE L
  Sprite write FPOSP%+1,FPOSX%,FPOSY%
  framebuffer write lc$
End Sub
  
Sub test_home
  slot% =0
  Select Case FPOSx%
    Case 4 To 20
      slot% =1
    Case 52 To 68
      slot% =2
    Case 102 To 128
      slot% =3
    Case 148 To 164
      slot% =4
    Case 196 To 212
      slot% = 5
  End Select
  If slot%=0 Then Exit Sub
  If Nest%(slot%) Then slot%=0:Exit Sub
  Nest%(slot%)=1 : Inc Score%,50
  
  'fly section
  if slot%=is_fly% then inc Score%,200 'you landed on a @fly
  framebuffer write lc$
  Box 8+(is_fly%-1)*48,8,16,16,,col%(1),col%(1) 'clear fly in nest (in F, frog = L)
  framebuffer write L
  is_fly%=0 '@fly regardless we need a new fly next round
  
  write_score
  Hide_Frog
  FDEL%=70
  Inc Homes%
  'draw HomeFrog in the Slot
  framebuffer write lc$
  Sprite write 26,8+(slot%-1)*48,8 'overwites fly if it is in slot
  'Last_Slot?
  If Homes%=5 Then
    Homes%=0:Inc Level%
    PlaySong S_COMPLETE
    FDEL%=150
    RSTRT%=2
    Inc score%,1000 'points for finishing the level
    Exit Sub
  EndIf
  Select Case homes%
      Case 1:PlaySong S_YANKEE:Case 2:PlaySong S_MUS2
      Case 3:PlaySong S_MUS3:Case 4:PlaySong S_HOME1
  End Select
  RSTRT%=1
End Sub
  
Sub Move_Player
  'allready in Motion?, then give time to show animated Sprite
  If FMOV%>=2 Then Inc FMOV%,-1:Exit Sub
  If FMOV%=1 Then
    Hide_Frog
    FRAMEBUFFER WRITE L
    If FPDIR%=1 Then
      Inc FPOSY%,-8
      If FPOSY%< FMAX% Then Inc Score%,10:FMAX%=FPOSY%:write_score 'points per lane
      GoTo fmout
    EndIf
    If FPDIR%=2 Then
      Inc FPOSY%,8
      GoTo fmout
    EndIf
    If FPDIR%=3 Then
      Inc FPOSX%,-8
      GoTo fmout
    EndIf
    If FPDIR%=4 Then
      Inc FPOSX%,8
      If FPOSX%>200 Then FPOSX%=200
    EndIf
FMout:
    Sprite write FPOSP%+1,FPOSX%,FPOSY%
    FMOV%=0
    Exit Sub
    framebuffer write lc$
  EndIf
  
  ' Read selected controller/keyboard
  Select Case inkey$
      Case chr$(128) : PLD$="U"
      Case chr$(129) : PLD$="D"
      Case chr$(130) : PLD$="L"
      Case chr$(131) : PLD$="R"
      Case Else : PLD$ = left$(rd_kb$(),1)
  End Select
  
  '@harm: added vector the Fjump so FPOSP% could remain unchanged
  'UP
  Select Case PLD$
    Case "U"
      If FPOSY%>16 Then
        If FPOSY%-8 <32 Then
          test_home
          If slot%=0 Then Exit Sub
        EndIf
        FPDIR%=1:FMOV%=4':FPOSP%=10
        Fjump 0,-8,0
      EndIf
      'down
    Case "D"
      If PLD$="D" And FPOSY%<200  Then
        FPDIR%=2:FMOV%=4':FPOSP%=12
        Fjump 0,8,2
      EndIf
      'left
    Case "L"
      If FPOSX%>8 Then
        FPDIR%=3:FMOV%=4':FPOSP%=14
        FJump -8,0,4
      EndIf
      'right
    Case "R"
      If FPOSX%<208 Then
        FPDIR%=4:FMOV%=4':FPOSP%=16
        Fjump 8,0,6
      EndIf
  End Select
End Sub
  
Sub FJump(FDX%,FDY%,o%)
  PlaySfx X_JUMP 'threads
  FRAMEBUFFER WRITE l
  Box FPOSX%,FPOSY%,16,16,,0,0
  Inc FPOSX%,FDX%
  Inc FPOSY%,FDY%
  Sprite write FPOSP%+o%,FPOSX%,FPOSY%
End Sub
  
Sub move_lanes
  framebuffer write lc$
  Local f As integer,co%,tst$
  ' Get a Lane, move it ld%() Pixels to
  ' Right if Positiv
  ' Left if negativ
  ' and fill the gap with Color co%
  For f=1 To 10
    co%=(f<6)
    If ld%(f)>0 Then
      blit 0,ly%(f),ld%(f),ly%(f),224-ld%(f),16
      Box 0,ly%(f),LD%(f),16,,col%(co%),col%(co%)
      Inc Lps%(f),ld%(f)
      ' if Pixelsteps>= 16 Rotate the Lane$() Right
      If Lps%(f)>=16 Then
        Lane$(f)=Right$(Lane$(f),1)+Left$(Lane$(f),Len(Lane$(f))-1)
        Lps%(f)=0
      EndIf
      tst$=Left$(Lane$(f),1)
      'ladyfrog in here
      If tst$<>"0" Then
        'add Sprite to the lane
        snr%=Instr("BrCRTtwWUu   kl m",tst$) '@harm
        Sprite write snr%,-12+Lps%(f),ly%(f)
      EndIf
    Else
      blit -ld%(f),ly%(f),0,ly%(f),222,16 '@harm
      Box 223+ld%(f),ly%(f),-ld%(f),16,,col%(co%),col%(co%)
      Inc Lps%(f),-ld%(F)
      ' if Pixelsteps>= 16 Rotate the Lane$() Left
      If Lps%(f)>=16 Then
        Lane$(f)=Right$(Lane$(f),Len(Lane$(f))-1)+Left$(Lane$(f),1)
        Lps%(f)=0
      EndIf
      tst$=Mid$(Lane$(f),14,1)
      If tst$<>"0" Then
        'add Sprite to the lane
        snr%=Instr("BrCRTtwWUu   kl m",tst$) '@harm
        Box 222-Lps%(f),ly%(f),18,16,,col%(co%),col%(co%)
        Sprite write snr%,220-Lps%(f),ly%(f)
      EndIf
    EndIf
  Next f
End Sub
  
  
  'some Decoration
Sub flowers flx%,fly%,cl1%,cl2%
  Box flx%,fly%,16,16,,COL%(13),COL%(13)
  flower flx%+1,fly%+3,cl1%,cl2%:flower flx%+0,fly%+10,cl1%,cl2%
  flower flx%+2,fly%+7,cl1%,cl2%:flower flx%+6,fly%+3,cl1%,cl2%
  flower flx%+6,fly%+8,cl1%,cl2%:flower flx%+6,fly%+8,cl1%,cl2%
  flower flx%+7,fly%+12,cl1%,cl2%:flower flx%+9,fly%+6,cl1%,cl2%
  flower flx%+12,fly%+3,cl1%,cl2%
End Sub
  
Sub Flower fox%,foy%,cl1%,cl2%
  Line fox%,foy%,flx%+fox%+2,fly%+foy%,,cl1%
  Line fox%+1,foy%-1,flx%+fox%+1,fly%+foy%+1,,cl1%
  Pixel fox%+1,foy%,cl2%
End Sub
  
  'Street "BrCRTtwWUu   klm"
Sub Create_street
  Local LNE%,lx%,tst$
  framebuffer write lc$
  disp
  For LNE%=1 To 10
    For lx%=1 To 13
      tst$=Mid$(Lane$(LNE%),lx%,1)
      snr%=Instr("BrCRTtwWUu   klm",tst$)'@harm
      If snr%<>0 Then Sprite write snr%,16*lx%,ly%(LNE%)
    Next lx%
  Next LNE%
End Sub
  
Sub Disp
  CLS 'N/F
  'water
  Box 0,0,320,112,,RGB(blue),RGB(blue)
  'homes
  For f%=0 To 192 Step 8:Sprite write 29,f%,0: Next f%
  For f%=0 To 192 Step 48:Box f%,0,32,24,,0,0:Sprite write 28,f%,0: Next f%
  'grass strips
  For f%=0 To 218 Step 16
    flowers f%,104,col%(1),Col%(4):flowers f%,200,col%(1),Col%(4)
  Next f%
End Sub
  
Sub read_Sprites
  Local nr%,p%,n%,byt$,m$
  '--- Read/Create Sprites
  Restore Bull
  'because the Cars are vertical symmetric, reading half a Sprite is enough
  For nr%=1 To 6
    For p%=1 To 8
      Read Byt$:Byt$=expand$(Byt$)
      For n%=1 To Len(Byt$)
        m$=Mid$(Byt$,n%,1)
        Pixel n%-1,p%+119,COL%(Val("&H"+m$))
        Pixel n%-1,136-p%,COL%(Val("&H"+m$)))
      Next n%
    Next p%
    Sprite read nr%,0,120,16,16
  Next nr%
  '2 wood (7/8)
  For nr%=7 To 8
    For p%=1 To 16
      Read Byt$:Byt$=expand$(Byt$)
      For n%=1 To Len(Byt$)
        Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
      Next n%
    Next p%
    Sprite read nr%,0,120,16,16
  Next nr%
  '@harm following sprites are new, diving turtles and such
  'sprites 9 = turtle, 10 primary diving turtle
  For nr%=9 To 10
    For p%=1 To 16
      Read Byt$:Byt$=expand$(Byt$)
      For n%=1 To Len(Byt$)
        Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
      Next n%
    Next p%
    Sprite read nr%,0,120,16,16
  Next nr%
  'sprites 11,12,13 are diving turtles for animations
  For nr%=11 To 13
    For p%=1 To 16
      Read Byt$:Byt$=expand$(Byt$)
      For n%=1 To Len(Byt$)
        Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
      Next n%
    Next p%
    Sprite read nr%,0,120,16,16
  Next nr%
  'sprites 14,15,16,17 are 4 parts of crocodile
  For nr%=14 To 17
    For p%=1 To 16
      Read Byt$:Byt$=expand$(Byt$)
      For n%=1 To Len(Byt$)
        Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
      Next n%
    Next p%
    Sprite read nr%,0,120,16,16
  Next nr%
  Box 0,120,16,16,,0,0
  
  'Frog (sprite 18 - 25) same template but swapped colors
  'Read the Frog-Sprites and rotate in all Directions ;-)
  Restore FROG11
  For p%=1 To 32
    Read Byt$:Byt$=expand$(Byt$)
    'swap colors Green>Cyan, Yello > Magena, Red > Green
    For n%=1 To Len(Byt$)
      m$=Mid$(Byt$,n%,1)
      'If m$="A" Then m$="3" '@harm, these where the ladyfrog colors
      'If m$="6" Then m$="5"
      'If m$="4" Then m$="2"
      Pixel n%-1,p%+119,COL%(Val("&H"+m$))
      Pixel 16+n%-1,152-p%,COL%(Val("&H"+m$))
      Pixel 32+p%-1,n%+119,COL%(Val("&H"+m$))
      Pixel 64-p%-1,n%+135,COL%(Val("&H"+m$))
    Next n%
  Next p%
  Sprite read 18,0,120,16,16:Sprite read 19,0,136,16,16
  Sprite read 20,16,136,16,16:Sprite read 21,16,120,16,16
  Sprite read 22,32,120,16,16:Sprite read 23,48,120,16,16
  Sprite read 24,48,136,16,16:Sprite read 25,32,136,16,16
  
  'Lady Frog (sprite 32 - 39) same template but swapped colors
  'Read the Frog-Sprites and rotate in all Directions ;-)
  Restore FROG11
  For p%=1 To 32
    Read Byt$:Byt$=expand$(Byt$)
    'swap colors Green>Cyan, Yello > Magena, Red > Green
    For n%=1 To Len(Byt$)
      m$=Mid$(Byt$,n%,1)
      If m$="A" Then m$="3" 'ladyfrog colors
      If m$="6" Then m$="5"
      If m$="4" Then m$="2"
      Pixel n%-1,p%+119,COL%(Val("&H"+m$))
      Pixel 16+n%-1,152-p%,COL%(Val("&H"+m$))
      Pixel 32+p%-1,n%+119,COL%(Val("&H"+m$))
      Pixel 64-p%-1,n%+135,COL%(Val("&H"+m$))
    Next n%
  Next p%
  Sprite read 32,0,120,16,16:Sprite read 33,0,136,16,16
  Sprite read 34,16,136,16,16:Sprite read 35,16,120,16,16
  Sprite read 36,32,120,16,16:Sprite read 37,48,120,16,16
  Sprite read 38,48,136,16,16:Sprite read 39,32,136,16,16
  Box 0,120,96,32,,0,0
  
  ' Frog_Home 26, Fly 27, 31=kroko
  For p%=1 To 48
    Read Byt$:Byt$=expand$(Byt$)
    For n%=1 To Len(Byt$)
      Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
    Next n%
  Next p%
  Sprite read 26,0,120,16,16
  Sprite read 27,0,136,16,16
  sprite read 31,0,152,16,16 '@harm, kroko
  
  restore home_slot '@harm
  'Home slot=28, green = 29, death = 30,
  Box 0,120,96,32,,0,0
  For p%=1 To 24
    Read Byt$:Byt$=expand$(Byt$)
    For n%=1 To Len(Byt$)
      Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
    Next n%
  Next p%
  Sprite read 28,0,120,32,24
  Box 0,120,32,32,,0,0
  For p%=1 To 24
    Read Byt$:Byt$=expand$(Byt$)
    For n%=1 To Len(Byt$)
      Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
    Next n%
  Next p%
  Sprite read 29,0,120,8,24
  Box 0,120,32,32,,0,0
  For p%=1 To 16
    Read Byt$:Byt$=expand$(Byt$)
    For n%=1 To Len(Byt$)
      Pixel n%-1,p%+119,COL%(Val("&H"+Mid$(Byt$,n%,1)))
    Next n%
  Next p%
  Sprite read 30,0,120,16,16
  Box 0,120,32,32,,0,0
  
  'test 64 sprites
  box 0,120,16,16,,rgb(blue),rgb(blue)
  Sprite read 63,0,120,16,16
  
End Sub
  
  '----------Audio subs--------
Sub bing
  Local vol%
  For Vol%=25 To 0 Step -1
    Play sound 4,"B","S",2700,Vol%
    Pause 20
  Next vol%
  Play stop
End Sub
  
Sub start_sound
  Local n%,f%
    For n%=1 To 8:For f%=150+n%*50 To 450+n%*50 Step 20
      Play sound 4,"B","Q",f%,25:Pause 6
  Next f%:Next n%
  Play sound 4,"B","Q",1,0
End Sub
  
  'threads: MusicThread plays the songs and SfxThread the sound effects, each in a loop of its own, and the game
  'asks for them with PlaySong and PlaySfx and goes on at once. They replace the SETTICK routines music, jump and
  'DieSnd: the music has its own place in the DATA, so loadlevel no longer stops it and the songs are no longer
  'chosen by RESTOREs in the game, and the two effects no longer share SETTICK 1.
Sub PlaySong s%
  If SND% Then want%=s% 'the music thread takes it at its next note
End Sub

Sub PlaySfx k%
  If SND% Then sfx%=k% : Flag(1)=1 'wakes the sound effects thread; a new effect cuts the one playing short
End Sub

Sub MusicThread
  Local f1%,f2%,fl%,fr%,st%,nw%,on%,nxt!
  Do
    Thread Hold : nw%=want% : want%=0 : Thread Release 'a song asked for, taken whole
    If nw% Then SelectSong nw% : on%=1 : nxt!=Timer
    If on%=0 Then Pause 20 : Continue Do 'silent until a song is asked for
    Read f1%,f2% 'this thread's own place in the DATA
    If f1%=99999 Then Restore Main_song : Read f1%,f2% 'a song's end: on to the main song, as before
    If f1%=88888 Then on%=0 : Play Stop : Continue Do 'the game over song's end: silence
    fl%=f1%-f1%\100 : fr%=f1%+f1%\100 'pseudo chorus
    For st%=1 To n_adsr% 'one note: n_adsr% steps of the envelopes, beat% ms each
      If f1%>1 Then Play sound 1,"L","q",fl%,adsr%(st%) : Play sound 3,"R","q",fr%,adsr%(st%)
      If f2%>1 Then Play sound 2,"B","w",f2%,adsr2%(st%)
      Inc nxt!,beat% : If nxt!>Timer Then Pause nxt!-Timer 'on the beat, however long the statements took
    Next
  Loop
End Sub

Sub SelectSong s%
  Select Case s% 'RESTORE moves this thread's place in the DATA, not the game's
    Case S_INTRO : Restore intro
    Case S_MAIN : Restore Main_song
    Case S_YANKEE : Restore Yankee
    Case S_MUS2 : Restore Mus2
    Case S_MUS3 : Restore Mus3
    Case S_HOME1 : Restore Home1
    Case S_COMPLETE : Restore complete
    Case S_KILL : Restore FKill
    Case S_OVER : Restore Game_OverSng
  End Select
End Sub

Sub SfxThread
  Local f%,k%
  Do
    Thread Wait Flag 1 'asleep until PlaySfx asks for an effect
    Flag(1)=0 : k%=sfx%
    Select Case k%
      Case X_JUMP 'a rising chirp, 600 to 1500 Hz, 20 ms a step (was SETTICK 20,jump)
        For f%=600 To 1500 Step 100
          Play sound 4,"B","Q",f%,25 : Pause 20
          If Flag(1) Then Exit For
        Next
      Case X_DIE 'a falling wail, 220 down to 65 Hz, 15 ms a step (was SETTICK 15,DieSnd)
        For f%=220 To 65 Step -5
          Play sound 4,"B","Q",f%,25 : Pause 15
          If Flag(1) Then Exit For
        Next
    End Select
    If Flag(1)=0 Then Play sound 4,"B","O",f%,0 'silent, unless another effect is already asked for
  Loop
End Sub

Function expand$(pxl$)
  Local n%,nmb%,tmp$,co$
  For n%=1 To Len(pxl$)
    If Asc(Mid$(pxl$,n%,1))< 71 Then
      tmp$=tmp$+Mid$(pxl$,n%,1)
    Else
      co$=Hex$(Asc(Mid$(pxl$,n%,1))-71)
      Inc n%:nmb%=Val("&H"+Mid$(pxl$,n%,1)):tmp$=tmp$+String$(nmb%+1,co$)
    EndIf
  Next n%
  expand$=tmp$
End Function
  
  
sub loadlevel n%
  local i%,dum$,dummy%
  'threads: the music plays on while the level loads: its READs have their own place in the DATA
  restore levels
  if n%>1 then 'skip lower levels
    for i%=1 to 10*(n%-1) : read dum$ : read dummy% : next
  endif
  'load level
  for i%=1 to 10 : read Lane$(i%) : read LD%(i%) : next
  'restart music
  PlaySong S_INTRO
end sub
  
  
  'for GM keyboard support
sub init_kb
  ' Initialise GP8-GP15 as digital inputs with PullUp resistors; if the attached
  ' button is pressed then the corresponding pin will read low (0), otherwise it
  ' will read high (1).
  local i%
  For i% = 8 To 15
    SetPin MM.Info(PinNo "GP" + Str$(i%)), Din, PullUp
  Next
End Sub
  
Function rd_kb$()
  if gm% then
    ' Read digital inputs into the bits of a single integer.
    ' We invert the raw GPIO input so that if a button is pressed the
    ' corresponding bit is set, otherwise it is unset. If multiple
    ' buttons are pressed simultaneously then multiple bits will be set.
    Local x% = (Port(GP8, 8) Xor &h7FFF) And &hFF
    Local s$ = ""
    
    ' Display buttons currently pressed
    If x% And &h01 Then Cat s$, "D "
    If x% And &h02 Then Cat s$, "L "
    If x% And &h04 Then Cat s$, "U "
    If x% And &h08 Then Cat s$, "R "
    'If x% And &h10 Then Cat s$, "S "'SELECT
    'If x% And &h20 Then Cat s$, "G "'START
    'If x% And &h40 Then Cat s$, "B "
    If x% And &h80 Then Cat s$, "A "
    rd_kb$=s$
  else
    rd_kb$="" 'default when no alternative
    if is_nes%=2 then rd_kb$ = nes_read$()
    if is_wii% then rd_kb$=read_wii$(iic_wii%,is_wii%) 'goes to decode both wii
  end if
End Function
  
  'diagonse the platform and find a potential wii controller
sub find_wii
  local d%,wii_dat%(5)
  
  'find I2C port
  select case mm.info(system i2c)
    case "I2C2"
      iic_wii%=2 : I2C2 read ad_wii%,0,1,d%
      if mm.i2c=0 then is_wii%=1  'we have a device at wii address
    case "I2C"
      iic_wii%=1 : I2C read ad_wii%,0,1,d%
      if mm.i2c=0 then is_wii%=1  'we have a device at wii address
  end select
  
  'find what controller it is (is_wii% : 0=none, 1=classic, 2=nunchuck)
  if is_wii%=1 then
    'detect wii controller type
    if iic_wii%=1 then
      'check for classic controller
      I2C write ad_wii%,0,2,&h40,&h00
      is_wii%=check_id%(iic_wii%)
      if is_wii%=0 then 'not found, check nunchuck
        I2C write ad_wii%,0,2,&hF0,&h55
        I2C write ad_wii%,0,2,&hFB,&h00
        is_wii%=check_id%(iic_wii%)
      end if
    else
      'check for classic controller
      I2C2 write ad_wii%,0,2,&h40,&h00
      is_wii%=check_id%(iic_wii%)
      if is_wii%=0 then 'not found, check nunchuck
        I2C2 write ad_wii%,0,2,&hF0,&h55
        I2C2 write ad_wii%,0,2,&hFB,&h00
        is_wii%=check_id%(iic_wii%)
      end if
    end if
  end if
end sub
  
function check_id%(a%)
  local d%,wii_dat%(5)
  Pause 10
  if a%=1 then
    I2C write ad_wii%,0,1,&hFA
    Pause 10
    I2C read ad_wii%,0,6,wii_dat%()
  else
    I2C2 write ad_wii%,0,1,&hFA
    Pause 10
    I2C2 read ad_wii%,0,6,wii_dat%()
  end if
  d%=((wii_dat%(2)*256+wii_dat%(3))*256+wii_dat%(4))*256+wii_dat%(5)
  select case d%
    case &hA4200000
      check_id%=2' : Print "WII nunchuck plugged in"
    case &hA4200101
      check_id%=1' : PRINT "WII classic controller"
    case else
      check_id%=0' : print "unknown device, ID = ";hex$(dummy%)
  end select
  
end function
  
  'read all 6 register from the WII controller and decode
function read_wii$(p%,c%)
  local s$="",wii_dat%(5)
  
  'read controllers
  if p%=1 then 'I2C
    I2C write ad_wii%,0,1,&h00
    Pause 1 'needed for nunchuck response....
    I2C read ad_wii%,0,6,wii_dat%()
  else 'p%=2 = I2C2
    I2C2 write ad_wii%,0,1,&h00
    Pause 1 'needed for nunchuck response....
    I2C2 read ad_wii%,0,6,wii_dat%()
  end if
  
  'decode controllers
  if c%=1 then 'classic
    if not (wii_dat%(5) and 2)  then cat s$,"L "
    if not (wii_dat%(4) and 128) then cat s$,"R "
    if not (wii_dat%(5) and 1)  then cat s$,"U "
    if not (wii_dat%(4) and 64) then cat s$,"D "
    if not (wii_dat%(5) and 16) then cat s$,"A "
  else 'nunchuck
    if wii_dat%(0)<100 then cat s$,"L "
    if wii_dat%(0)>160 then cat s$,"R "
    if wii_dat%(1)<100 then cat s$,"D "
    if wii_dat%(1)>160 then cat s$,"U "
    if not (wii_dat%(5) and 1) then cat s$,"A " 'z-button
  end if
  read_wii$=s$
End function
  
  
  'check if default pins for PicoGameVGA are not assigned, so the port can be opened
Function nes_possible%()
  Local cnt%,n%
  inc n%:Inc cnt%,(MM.Info(pin nes_data%)="OFF")
  inc n%:Inc cnt%,(MM.Info(pin nes_latch%)="OFF")
  inc n%:Inc cnt%,(MM.Info(pin nes_clock%)="OFF")
  nes_possible% = (cnt% = n%) and (no_nes%=0)
End Function
  
  'sets GPIO pins correct for reading the NES controller
sub nes_init
  SetPin nes_data%, DIn, pullup  'stable reading when no controller
  SetPin nes_latch%, DOut
  SetPin nes_clock%, DOut
end sub
  
  'reads NES controller
function nes_read$()
  Local x%, i%, s$
  
  if is_nes% then
    Pulse nes_latch%, nes_pulse!
    For i% = 0 To 7
      If Not Pin(nes_data%) Then x%=x% Or 2^i%
      Pulse nes_clock%, nes_pulse!
    Next
    
    If x% And &h01 Then Cat s$, "A "
    'If x% And &h02 Then Cat s$, "B "
    'If x% And &h04 Then Cat s$, "S "'SELECT
    'If x% And &h08 Then Cat s$, "G "'START
    If x% And &h10 Then Cat s$, "U "
    If x% And &h20 Then Cat s$, "D "
    If x% And &h40 Then Cat s$, "L "
    If x% And &h80 Then Cat s$, "R "
  end if
  nes_read$ = s$
end function
  
  
  '
  '
  '----------Data Section----------
  ' I try to store everything in this File to make it possible to run
  'without an SD Card
  '
colors:
  '--Colorscheme accordung to matherp
  Data RGB(BLUE),RGB(GREEN),RGB(CYAN),RGB(RED)
  Data RGB(MAGENTA),RGB(YELLOW),RGB(WHITE),RGB(MYRTLE)
  Data RGB(COBALT) ,RGB(MIDGREEN),RGB(CERULEAN),RGB(RUST)
  Data RGB(FUCHSIA),RGB(BROWN),RGB(LILAC)
  
  'Sprites
  'repainted the sprites mostly from this source
  'https://www.spriters-resource.com/resources/sheets/11/11067.png
  '
  'Compression of repeating Pixels:
  'if a Pixel repeats more than two times, Colors replaced
  'to Color +16  from '0123456789ABCDEF to GHIJKLMNOPQRSTUV
  'followed by the number (single Hex value) of repeats-1
Bull:
  '1=Bulldozer 1/2
  Data "GF","GF","04747474G3N20","04747474G37400","002G2200I2N20","00N7007400"
  Data "0N4227200N20","077I27277007400"
  '2=Racer1  1/2
  Data "GF","0K4G9","0K4G2K400","0K4G2K400","G2DG6DG3","00MB00"
  Data "T566T2M30","064646466T2M3"
  '3=car  1/2
  Data "GF","GF","GF","G266G566G2","03T500T300","3T3J2D3T330"
  Data "36DD33T3J2D00","3T233T3J2D00"
  '4=Racer4  1/2
  Data "GF","G9K40","00K4G2K40","00K4G2K40","G32G62G2","00NC0"
  Data "0N3I277I5","N3I2774047400"
  '5,6=Truck  1/2
  Data "GF","GF","GF","G4I200I300","G24N30N6","004N40N6","004N44N6"
  Data "004N44N6","GF","GF","GF","G8I2G3","NCG2","NCG2","NCG2","NCG2"
  '7=Wood1
  Data "HF","HF","HF","U41U4N211","U67EEN41","U97U271","87U77U277"
  Data "U87U47","U87EE7EE7","U4EEN2E7E77","EES77S271","S97CC771"
  Data "S61S287711","HF","HF","HF"
  '8=Wood2
  Data "HF","HF","HF","11U61U31E","U57U47EEE","EE77UB","UEE","U977U3"
  Data "E77U37U57E","U67U7","U61C7S5","11C1SB","1EEECCC1S61","HF","HF","HF"
  ''9=Turtle 1
  'Data "HF","HF","H22H62H3","11I2H4I2H2","H32K42H4","H3K7H4","172K81121"
  'Data "I2K9211","172K674H3","H347K274H4","H324N243H4","11I2H4I2H2"
  'Data "H22H62H3","HF","HF","HF"
  ''new
  ''turtle0
  DATA "HF","HF","HF","H47H47H3","H3I17H22N1H2","H32K42H4","H3K6H4","172K8H3","I2K92H1"
  DATA "172K674H121","H347K274H4","H324N242H4","H3I17H22N1H2","H47H47H3","HF","HF"
  'turtle1
  DATA "HF","HF","HF","H22H62H3","H1I2H4I2H2","H32K42H4","H3K6H4","172K8H121","I2K92H1"
  DATA "172K674H3","H347K274H4","H324N242H4","H1I2H4I2H2","H22H62H3","HF","HF"
  'turtle2
  DATA "HF","HF","HF","HF","H32H42H4","H2I1K4I1H3","H3K6H4","H17K8H3","1I1K92H1"
  DATA "H17K674H3","H347K274H4","H2I14N24I1H3","H32H42H4","HF","HF","HF"
  'turtle3
  DATA "HF","HF","HF","H37HA","H27H67H3","H5I3H17H2","H4I5H4","H3I7H3","H3I572H3"
  DATA "H4I1N22H4","H5I3H5","H27H77H2","H37H57H3","HF","HF","HF"
  'turtle4
  DATA "HF","HF","H6N1H6","HF","HF","H47H27H5","H37H47H4","17H3I2H471","17H2I4H371"
  DATA "H5I17H6","H37H47H4","H47H27H5","HF","HF","H6N1H6","HF"
  'croqu1
  DATA "HF","HF","HF","HF","HF","HF","1CHD","1CH7CH2C1","1S1H2CH1S6","H1S17S47S4"
  DATA "H3S27S47S1","N1H5S7","H1N3H9","H9N3H1","H2N4H7","HBN3"
  'croqu2
  DATA "HF","HF","HF","HF","HF","H7CH3CH1","H1CH3S2H1C7C1","1SE","S67S7","S17SC"
  DATA "S97S4","7SB1S1","H3C7S1H7","N21S317H1N11C","H2S3H27H2S1","H3N5H3N1"
  'croqu3
  DATA "H8S1H4","H7C7S1H3","H7S3H3","H6S37H3","H5S2717H3","H1N1S317H5","H1N1S27H7","S517H7"
  DATA "CN1S1HA","S17S1H1717H5","S67S11717H1","SE1","S3HB","S317H27H5","S2H27H27H4"
  DATA "1N4H1N1H5"
  'croqu4
  DATA "HF","HF","HF","HF","HF","HF","H1N1S1H9","S1N1S3H4S11","SC7C1","S5N1S61"
  DATA "S7N61","SE1","S3HB","S317H27H5","S2H27H27H4","1N4H1N1H5"
  
FROG11:
  'Frog  Hop
  Data "GF","GF","G2AG7AG2","00AA006A6600AA00","G2A04A66A40AG2","G2A0AA66AA0AG2"
  Data "G3AM5AG3","G46AM3G4","G46AM3G4","G4A6A66AG4","G3Q266Q2G3","G2AAG5AAG2"
  Data "00AAG7AA00","00AAG7AA00","G2AG7AG2","GF"
  'Frog Sit
  Data "GF","GF","GF","G2A006A6600AG2","00AA04A66A40AA00","G2A0AA66AA0AG2"
  Data "G2AAM5AAG2","G46AM3G4","G2AA6AM3AAG2","G2A0A6A66A0AG2"
  Data "00AA00A66A00AA00","G2AG7AG2","GF","GF","GF","GF"
Fhome:
  'Frog Home Sprite
  Data "H2AAH5AAH2","11A11AH3A11A11","11ACCQ5CCA11","H2Q9H2","H4Q5H4"
  Data "H3AACAACAAH3","1A11AAACCAAA11A1","AAA13Q531AAA","Q33Q53Q3","Q333Q333Q3"
  Data "Q4J5Q4","1Q33CCC33Q31","1Q33CCC33Q31","H2AAAJ3AAAH2","11Q3H3Q311"
  Data "1AAA1AH3A1AAA1"
  'Fly
  Data "HF","HF","HF","H6J4H3","H5J4H4","H2CC1J3CCH3"""
  Data "H2CAAJ2S3H2","11AAAS7311","H2CAAJ2S3H2","H2CC1J3CCH3","H5J4H4"
  Data "H6J4H3","HF","HF","HF","HF"
  'new
  'krok
  DATA "HF","HF","H7S3H3","H6S32H3","H5S2212H3","H1I1S312H5","H1I1S22H7","S512H7","CI1S1HA"
  DATA "S12S1H1212H5","3S52S11212H1","SE1","S3HB","S312H22H5","S2H22H22H4","1I4H1I1H5"
  'new
Home_slot:
  Data "0I50I20I60I60I20","I20EI70EEI40EEI4022","20220EI20EE220220EE220220EE2202"
  Data "I90I60I60I4","020EEIEI70EE2","I2022U220U220E20U220EEI2022"
  Data "0I3EE1E1EE1E1EE1EE1E1E1EEI30","I4EHEH4EI4","I40EHF11EEI4"
  Data "0EEI2HEH4220E22","20I2EEHF11E0220E0","I4EHEH4EI4","20EE2EHEH4EI4"
  Data "220220EHF11EE22U2","I5HEH4I304","0I3EEHF11E0I4","I4EHEH4EI4"
  Data "20EE2EHEH3EE20EE2","220220EHEH3E22022","0I4HEH4I40","I4EEHF11E0I4"
  Data "U221EHEH4EE21EE","U22HEH7E1E11","HFHF"
  'green
  Data "0I50","I4022","I7","20EEI3","220I30","I7","202200EE2","I4022","0I50"
  Data "I4022","I7","20EEI3","220I30","I7","202200EE2","I4022","I20I3"
  Data "I40EE","20EE2202","220I4","I7","E20U220","E1EE1E1E","H7"
  'Death
  Data "G4M4G5","G3M6G4","G2660M2066G3","G260M406G3","660M80660"
  Data "66G2M4G2660","G26G66G3","006G2M2G26G2","G36G46G4","G46G26G5"
  Data "G5606G6","G66G60","G5606G6","G46G26G5","G266G466G3","G266G466G3"
  ' more to come
  '
  
Logo:
  
  Data 1060110336,830472192,833872864,1060517424,808846896,808859184,808707040
  Data 0,4261412864,3221225472,3235508720,4242742040,3234011928,3234011928
  Data 3233935864, 24, 496
  
  
  ' ----- Music -----
  'Music data. Values are the Frequencies alternating Voice 1 and Voice 2
  '1 is muted,99999 marks the End of the Song
  
Yankee:
  Data 741,185,741,233,829,139,932,233,741,185,932,233,829,139
  Data 554,233,741,185,741,233,829,139,932,233,741,185,1,233,699,208
  Data 554,139,741,185,741,233,829,139,932,233,988,185,932,247,829,156
  Data 741,247,699,174,554,277,621,139,699,277,741,185,1,233,741,139,1,233
  Data 621,185,699,247,621,156,554,247,621,185,699,247,741,156,1,247
  Data 554,185,621,233,554,139,493,233,466,185,1,233,554,139,1,233
  Data 621,185,699,247,621,156,554,247,621,185,699,247,741,156,621,247
  Data 554,174,741,247,699,139,829,247,741,185,1,233,741,139,99999,99999
Mus2:
  Data 1,233,1,233,1,1,1,233,1,1,1,233,1,1,1,233,350,1,466,233,1,1,466,233
  Data 554,1,699,233,1,1,1,233,350,1,440,174,350,1,440,174,523,1
  Data 621,174,1,1,1,174,350,1,440,174,1,1,440,174,523,1,621,174
  Data  1,1,1,174,350,1,466,233,350,1,466,233,554,1,699,233,1,1
  Data 1,233,350,1,466,233,1,1,466,233,554,1,699,233,1,1,1,233,350,1
  Data 440,174,350,1,440,174,523,1,621,174,1,1,1,174,621,1,621,156,621,1
  Data 699,1,621,1,554,156,554,1,621,1,554,1,523,174,523,1,554,1,621,1
  Data 699,174,1,1,1,1,1,1,621,156,554,1,523,1,350,1,1,174,440,1
  Data 466,1,1,1,1,233,1,1,99999,99999
Mus3:
  Data 1,1,1,1,1,1,523,147,1,1,589,1,658,1
  Data 699,131,1,1,880,1,782,1,699,165,1,1,523,147,699,165,658,174
  Data 523,220,1,131,1,220,350,174,392,220,440,131,392,220,466,174,440,233
  Data 392,147,350,233,466,174,523,233,589,147,523,233,621,174,589,220
  Data  523,131,466,220,440,174,523,220,350,131,440,220,261,165,350,196
  Data 220,131,261,196,330,147,261,196,392,165,330,196,523,174,392,220
  Data 658,131,523,220,699,174,523,220,350,131,261,220,174,174,131,233
  Data 87,147,65,233,58,174,87,233,117,147,174,233,233,174,350,233
  Data 466,147,699,233,932,174,1177,233,1398,147,1865,233,1398,196,1177,261
  Data 932,131,699,261,658,196,589,261,523,131,589,261,658,174,699,220
  Data 782,131,932,220,880,174,782,220,699,131,589,220,523,174,440,233
  Data 392,147,350,233,466,174,523,233,589,147,658,233,699,165,782,233
  Data 699,131,589,233,658,165,589,233,523,131,466,233,440,174,392,220
  Data 294,131,330,220,350,185,1,1,1,1,1,1,99999,99999
Main_song:
  Data 1,1,589,392,1,1,493,1
  Data 392,1,782,330,1,1,741,1,658,1,589,392,1,1,782,1,493,1
  Data 440,370,589,1,1,1,1,1,1,196,589,247,589,147,589,247,589
  Data 196,493,247,440,147,392,247,1,196,782,261,782,165,782
  Data 261,881,196,782,261,741,165,658,261,589,196,493,247,1
  Data 147,493,247,782,196,1,247,493,147,1,247,589,220,440,294
  Data 1,147,1,294,1,165,1,294,1,185,1,294,1,196,493,247,493
  Data 147,523,247,589,196,658,247,741,147,658,247,1,196,1,261
  Data 1,165,1,261,1,196,1,261,1,165,1,261,523,196,523,261,589
  Data 165,658,261,1,196,741,261,782,165,741,261,1,220,1,294,1
  Data 147,1,294,1,220,589,294,1,147,782,294,1,196,1,247,1,147
  Data 782,247,741,196,658,247,589,147,741,247,1,196,1,247,1
  Data 156,658,247,1,165,658,261,1,220,589,261,1,220,881,294
  Data 1,147,782,294,1,220,741,294,1,147,782,294,1,196,1,247,1
  Data 147,1,247,1,196,1,1,1,1,782,1,782,261,782,1,782,1,782,1
  Data 782,1,741,1,658,1,782,1,1,247,1,1,1,1,589,1,1,1,493,1
  Data 493,1,440,1,1,1,440,1,440,1,782,1,782,1,741,1,658,1,658
  Data 1,1,1,1,1,1,1,589,1,1,1,1,1,1,1,589,1,493,1,493,1,493,1
  Data 493,1,1,1,440,1,392,1,523,1,493,1,523,1,589,1,658,1,1,1
  Data 1,1,1,1,589,1,523,1,440,1,440,1,440,1,1,1,392,1,370,1
  Data 392,1,370,1,392,1,440,1,493,1,1,1,1,1,1,1,589,1,493,1
  Data 493,1,493,1,493,1,1,1,440,1,392,1,523,1,493,1,523,1,589
  Data 1,658,1,1,1,741,1,658,1,589,1,1,1,589,1,658,1,589,1,523
  Data 1,493,1,440,1,330,1,1,1,1,1,1,1,370,1,1,1,440,1,1,1,392
  Data 1,1,1,1,1,1,1,99999,99999
intro:
  'Main song intro
  Data 493,196,392,247,392,147,392,247,493,196,392,247,392
  Data 147,392,247,523,220,523,261,493,147,493,261,440,220,1
  Data 261,1,147,1,261,523,220,523,261,493,147,493,261,440
  Data 220,440,261,658,147,658,261,589,220,523,261,493,147
  Data 440,261,392,247,1,1,99999,99999
complete:
  'Level complete
  Data 1,1,1,1,1,1,392,247,440,294,493,247
  Data 523,294,589,247,1,294,493,247,1,294,392,247,440,294
  Data 493,247,440,294,392,247,1,294,392,247,1,294,392,247
  Data 440,294,493,247,523,294,589,247,1,294,493,247,1,294
  Data 589,261,523,294,493,261,440,294,392,247,1,1,1,1
  Data 1,1,99999,99999
Home1:
  'Frog-Home1
  Data 1,1,1,1,881,741,990,699,881,741,1,1,741,589,1,1,881,741
  Data 990,699,881,741,1,1,741,589,1,1,881,741,881,658,990,621
  Data 1,1,1,1,881,621,782,658,554,699,741,589,1,1,1,1,1,1,440
  Data 440,440,440,440,440,554,554,658,658,1,1,440,440,440,392
  Data 440,370,589,440,741,589,1,1,881,741,881,658,990,589,1,1
  Data 1,1,881,554,782,440,554,392,589,370,1,1,1,1,99999,99999
FKill:
  Data 1,1,1,1,1,1,1,1,1,1,1,1,1,11,99999,99999
Game_OverSng:
  Data 311, 156,311, 156,311, 156, 311, 156,440, 185,440, 185,1, 220,370, 220
  Data 589, 185, 589, 185, 589, 220, 589, 220, 741, 147, 741, 147, 658, 220
  Data 658, 220, 658, 185, 658, 185, 589, 220, 589, 220, 589, 165, 589, 165
  Data 554, 220, 554, 220, 554, 196, 554, 196, 493, 220, 493, 220, 493, 147
  Data  493,147,440,220,440,220,440,185,440,185,392,185,392,185,440,0,88888,88888
  
  'levels ----------------------------------------------------------------------
Levels:
  'level 1, lane string, lane speed, original from Martin.H
  'legend W=log,w=log-end,U=turtle, T+t=truck,r,R=racer,B=bulldozer, C=car
  DATA "0WWWWw000WWWw00000WWWWw000WWw000",2 
  DATA "0UU000UUU000UUU000UU00UU000UU000",-1
  DATA "00WWWWWw000WWWWWw00WWWWw0000WWw0",1
  DATA "00Ww0Ww000WWw000WWw0000WWWw00WWw",2
  DATA "00UUU00UUU00UUU00UUU00000UUU00UU",-2
  DATA "000000Tt00000000Tt00000Tt000Tt00",-2
  DATA "000r000000r0000000000r000000r000",4
  DATA "00C00000C00000C000000C0000C00000",-2
  DATA "00000000B00000B00000B0000B0000B0",1
  DATA "0000R000000R00000R0000000R000R00",-1
    
  'level 2 : u=diving turtle, klm=crocodile
  'this level needs design, only shows what is possible now
  'Movement: Ww:l+r, Uu:l, klm:r, RCTt:l, Br:r, l=neg/r=pos
  data "0WWWWw000WWWw00000WWWWw000WWw000",1
  data "0UU000uuu000UUU000UU00U000uuu000",-1
  data "00WWWWWw000WWWWWw0000klm0000klm0",1
  data "00Ww0Ww000WWw000WWw0000WWWw00WWw",1
  data "00uuu00UUU00uuu00UUU000000UUU0U0",-1
  data "000000Tt00000000Tt00000Tt000Tt00",-1
  data "000r000000r0000000000r000000r000",1
  data "00C00000C00000C000000C0000C00000",-1
  data "00000000B00000B00000B0000B0000B0",1
  data "0000R000000R00000R0000000R000R00",-1
  
  
  '-----------------------------------------------------------------------------
  ' Konami Style Font (Martin H.)
  ' Font type    : Full (95 ChArACtErs)
  ' Font size    : 8x8 pixels
  ' Memory usage : 764 Bytes
DefineFont 9
  5F200808
  00000000 00000000 18181818 00180018 006C6C6C 00000000 367F3636 0036367F
  3E683F0C 00187E0B 180C6660 00066630 386C6C38 003B666D 0030180C 00000000
  3030180C 000C1830 0C0C1830 0030180C 3C7E1800 0000187E 7E181800 00001818
  00000000 30181800 7E000000 00000000 00000000 00181800 180C0600 00006030
  7E6E663C 003C6676 18183818 007E1818 0C06663C 007E3018 1C06663C 003C6606
  6C3C1C0C 000C0C7E 067C607E 003C6606 7C60301C 003C6666 180C067E 00303030
  3C66663C 003C6666 3E66663C 00380C06 18180000 00181800 18180000 30181800
  6030180C 000C1830 007E0000 0000007E 060C1830 0030180C 180C663C 00180018
  6A6E663C 003C606E 7E66663C 00666666 7C66667C 007C6666 6060663C 003C6660
  66666C78 00786C66 7C60607E 007E6060 7C60607E 00606060 6E60663C 003C6666
  7E666666 00666666 1818187E 007E1818 0C0C0C3E 00386C0C 70786C66 00666C78
  60606060 007E6060 6B7F7763 0063636B 7E766666 0066666E 6666663C 003C6666
  7C66667C 00606060 6666663C 00366C6A 7C66667C 0066666C 3C60663C 003C6606
  1818187E 00181818 66666666 003C6666 66666666 00183C66 6B6B6363 0063777F
  183C6666 0066663C 3C666666 00181818 180C067E 007E6030 6060607C 007C6060
  18306000 0000060C 0606063E 003E0606 42663C18 00000000 00000000 FF000000
  7C30361C 007E3030 063C0000 003E663E 667C6060 007C6666 663C0000 003C6660
  663E0606 003E6666 663C0000 003C607E 7C30301C 00303030 663E0000 3C063E66
  667C6060 00666666 18380018 003C1818 18380018 70181818 6C666060 00666C78
  18181838 003C1818 7F360000 00636B6B 667C0000 00666666 663C0000 003C6666
  667C0000 60607C66 663E0000 07063E66 766C0000 00606060 603E0000 007C063C
  307C3030 001C3030 66660000 003E6666 66660000 00183C66 6B630000 00367F6B
  3C660000 00663C18 66660000 3C063E66 0C7E0000 007E3018 7018180C 000C1818
  00181818 00181818 0E181830 00301818 00466B31 00000000 FFFFFFFF FFFFFFFF
End DefineFont
