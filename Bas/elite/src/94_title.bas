' =====================================================================
'  The title screen, and the page that says what the keys do
'
'  The picture is a file rather than a drawing: elite_tools/titlescreen.py
'  renders the Cobra Mk III from the same blueprint the game flies, as a
'  hidden-line wireframe, and saves it as a JPEG.  Drawing it here would
'  cost a mesh and a pose for one screen that never moves.
'
'  If the file is missing the screen still works - it just draws the words
'  instead - so a copy of the program on its own is not broken by it.
'
'  Waiting here starts the demo, which is what an arcade cabinet does with
'  an idle machine; any key during the demo hands the controls back.
' =====================================================================

' Returns the key that was pressed, or 0 if nobody came.
FUNCTION TitleScreen() AS INTEGER
  LOCAL INTEGER k
  DO
    DrawTitle
    k = WaitKey(TITLEWAIT)
    IF k <> 72 AND k <> 104 THEN             ' anything but H
      TitleScreen = k
      EXIT FUNCTION
    ENDIF
    ControlsScreen
  LOOP
END FUNCTION

SUB DrawTitle
  CLS
  IF DIR$(TITLEPIC, FILE) <> "" THEN
    LOAD JPG TITLEPIC
  ELSE
    ' No picture to hand, so say it in words.
    TEXT VCX, 40, "E L I T E", "CT", 1, 4, cWhite
    LINE 24, 100, SCRW - 25, 100, 1, cCyan
    TEXT VCX, 130, "after Bell and Braben, 1984", "CT", 7, 1, cWhite
    LINE 24, 190, SCRW - 25, 190, 1, cCyan
    TEXT VCX, 202, "PRESS ANY KEY TO PLAY", "CT", 7, 1, cYellow
    TEXT VCX, 218, "H FOR THE CONTROLS", "CT", 7, 1, cCyan
  ENDIF
  FRAMEBUFFER COPY F, N
END SUB

' Everything the ship answers to, on one screen.
SUB ControlsScreen
  LOCAL INTEGER y, k
  CLS
  TEXT VCX, 1, "FLIGHT", "CT", 7, 1, cWhite
  LINE 0, 11, SCRW - 1, 11, 1, cCyan
  y = 14
  KeyLine y, "Roll", "< >  or left/right" : y = y + 9
  KeyLine y, "Pitch", "S X  or up/down" : y = y + 9
  KeyLine y, "Speed", "SPACE faster, / slower" : y = y + 9
  KeyLine y, "Fire", "A" : y = y + 9
  KeyLine y, "Missile", "T locks on, M fires" : y = y + 9
  KeyLine y, "E.C.M.", "E" : y = y + 9
  KeyLine y, "Docking computer", "C" : y = y + 9
  KeyLine y, "Hyperspace", "H, outside the zone" : y = y + 9
  KeyLine y, "In-system jump", "J, with nothing about" : y = y + 9
  KeyLine y, "Galactic jump", "G, if one is fitted" : y = y + 9
  KeyLine y, "Energy bomb", "TAB" : y = y + 9
  KeyLine y, "Escape pod", "ESC, if one is fitted" : y = y + 9
  KeyLine y, "Views", "F1 fore, F2 aft" : y = y + 9
  KeyLine y, "", "F3 left, F4 right" : y = y + 12

  TEXT VCX, y, "SCREENS, FLYING OR DOCKED", "CT", 7, 1, cWhite
  y = y + 11
  KeyLine y, "F5 Galactic chart", "F8 Market prices" : y = y + 9
  KeyLine y, "F6 Short range", "F9 Status" : y = y + 9
  KeyLine y, "F7 System data", "F10 Inventory" : y = y + 12

  TEXT VCX, y, "DOCKED", "CT", 7, 1, cWhite
  y = y + 11
  KeyLine y, "F1 Launch", "F2 buy, F3 sell" : y = y + 9
  KeyLine y, "F4 Equip ship", "SPACE buys one" : y = y + 9
  KeyLine y, "F fills the tank", "S save, L load" : y = y + 9

  TEXT VCX, SCRH - 9, "any key goes back", "CT", 7, 1, cDim
  FRAMEBUFFER COPY F, N
  k = WaitKey(0)
END SUB

SUB KeyLine(y AS INTEGER, lb$, v$)
  TEXT 14, y, lb$, "LT", 7, 1, cYellow
  TEXT 150, y, v$, "LT", 7, 1, cWhite
END SUB

' One key.  ms is how long to wait before giving up; 0 waits for ever.
FUNCTION WaitKey(ms AS INTEGER) AS INTEGER
  LOCAL FLOAT t
  LOCAL k$ LENGTH 2
  t = TIMER + ms
  DO
    SoundService
    k$ = INKEY$
    IF k$ <> "" THEN WaitKey = ASC(k$) : EXIT FUNCTION
  LOOP UNTIL ms > 0 AND TIMER > t
  WaitKey = 0
END FUNCTION
