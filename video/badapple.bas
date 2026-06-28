'============================================================
' Bad Apple Demo para PicoMite HDMI/USB
' Autor: Marcos LM (2025)
'
'   - Version HQ: decodificador CSUB en C (320x240 @ 22 FPS)
'   - Version LQ: decodificador MMBasic   (160x120 @ 6 FPS)
'
' Requisitos:
'   - Firmware PicoMite HDMI/USB >= 6.00.03
'   - Archivos: ba.vid, balq.vid, ba.aud, ba.jpg en la SD
'
' Controles:
'   - Cursores arriba/abajo: seleccionar opcion
'   - ENTER                : aceptar
'   - ESC                  : parar reproduccion / salir
'============================================================
OPTION EXPLICIT

'------------------------
' Configuracion de video
'------------------------
DIM INTEGER vidWidth, vidHeight, vDelay
DIM FLOAT fps, totalFrames
DIM STRING vidFile, audFile

'------------------------
' Configuracion extra
'------------------------
CONST HQ_DECODE = 0, LQ_DECODE = 1

CONST T_ESC=27, T_ENTER=13, T_UP=128, T_DOWN=129

CONST F_HANDLER    = 1   ' canal para la apertura del archivo

CONST C_WHITE      = RGB(255,255,255)
CONST C_GRAY       = RGB(128,128,128)
CONST C_BLACK      = RGB(0,0,0)

CONST SHOW_FPS     = 0   ' Muestra FPS actuales
CONST SHOW_FPS_MIN = 0   ' Muestra el FPS mas bajo
CONST LIMIT_FPS    = 1   ' Activa el limite a FPS objetivo

'------------------------
' Variables generales
'------------------------
DIM FLOAT frameIndex
DIM INTEGER x, y
DIM INTEGER runLen
DIM INTEGER colorIsWhite

DIM STRING chunk        ' buffer de datos comprimidos
CONST CHUNK_SIZE = 255  ' 255 como maximo (limitacion de bytes para string)
DIM INTEGER bytesInChunk
DIM INTEGER chunkPos

DIM FLOAT targetTime
DIM FLOAT frameDuration
DIM FLOAT fpsCurrent
DIM FLOAT frameStartTime
DIM FLOAT fpsMin = 1000

DIM INTEGER selection   ' Menu



'------------------------
' Comprobación inicial
'------------------------
' Comprobar velocidad de CPU (debe ser 315000 KHz para sincronizar A/V)
DIM FLOAT cpuhz
cpuhz = VAL(MM.INFO$(CPUSPEED$))

IF cpuhz <> 315000000 THEN
  PRINT "Bad Apple demo expects CPUSPEED = 315000 (315 MHz)"
  PRINT "Current: "; MM.INFO$(CPUSPEED$); " Hz"
  PRINT
  PRINT "Adjust with: OPTION RESOLUTION 640x480,315000"
  END
ENDIF

DO
	'------------------------
	' Menu de Seleccion
	' E inicializacion
	'------------------------
	selection = MenuSel()
	' Ajustamos configuracion
	SELECT CASE selection
	
	  CASE HQ_DECODE
      ' Demo con decoder CSUB
      vidWidth    = 320
      vidHeight   = 240
      fps         = 22
      vidFile     = "ba.vid"
      audFile     = "ba.aud"
      totalFrames = 4754
      targetTime  = 44.7       ' ms por frame (22 FPS aprox.)
      vDelay      = 400
		
	  CASE LQ_DECODE
      ' Demo con decoder BASIC
      vidWidth    = 160
      vidHeight   = 120
      fps         = 6
      vidFile     = "balq.vid"
      audFile     = "ba.aud"
      totalFrames = 1296
      targetTime  = 1000/fps   ' ms por frame objetivo
      vDelay      = 0
		
	  CASE ELSE
		' Salir
		FONT 1 : CLS : END
		
	END SELECT

	' Configuracion de pantalla y framebuffers
	FONT 8
	COLOR C_WHITE, C_BLACK
	CLS

	' Creamos un framebuffer donde dibujaremos y despues copiaremos al visible
	' para evitar parpadeo (double buffering)
	FRAMEBUFFER CREATE
	FRAMEBUFFER WRITE N : CLS
	FRAMEBUFFER WRITE F : CLS

	' Abrir archivo de video
	OPEN vidFile FOR INPUT AS F_HANDLER

	' Lanzar audio
	PLAY WAV audFile
	PAUSE vDelay  ' ajuste de tiempo para cuadrar audio con video


	'------------------------
	' Bucle de reproduccion
	'------------------------
	TIMER        = 0
	fpsCurrent   = 0
	frameIndex   = 0
	chunk        = ""
	bytesInChunk = 0
	chunkPos     = 0

	' Dibujamos en F
	FRAMEBUFFER WRITE F

	DO WHILE frameIndex < totalFrames
	  frameStartTime = TIMER
	  CLS
	  
	  ' Carga de 'chunk' y decodificar frame completo
	  FOR y = 0 TO vidHeight - 1
      ' Inicializar linea para nuevo frame
      x = 0
      colorIsWhite = 0  ' RLE personalizado: cada linea empieza en negro
      
      DO WHILE x < vidWidth
        ' Cargar nuevo chunk si es necesario
        IF chunkPos >= bytesInChunk THEN LoadChunk
        
        SELECT CASE selection
          CASE HQ_DECODE
            ' Decodificador compilado CSUB (decode.c)
            Decode chunk, bytesInChunk, chunkPos, x, y, colorIsWhite
          CASE LQ_DECODE
            ' Decodificador lento en BASIC (version LQ)
            DecodeLQ
        END SELECT
      LOOP
	  NEXT y
	  
	  ' Medir duracion del frame
	  frameDuration = TIMER - frameStartTime
	  
	  ' Limitar/Sincronizar FPS
	  IF LIMIT_FPS THEN
		DO WHILE frameDuration < targetTime
		  frameDuration = TIMER - frameStartTime
		LOOP
	  ENDIF
	  
	  ' Mostrar FPS (opcional)
	  IF SHOW_FPS THEN
      fpsCurrent = 1000.0 / frameDuration
      TEXT 0, 0, "FPS:" + STR$(INT(fpsCurrent + 0.5))
      IF SHOW_FPS_MIN THEN
        ' Actualizamos framerate minimo
        IF fpsCurrent < fpsMin THEN fpsMin = fpsCurrent
        TEXT 0, 6,  "Min:" + STR$(INT(fpsMin + 0.5))
      ENDIF
	  ENDIF
	  
	  ' Volcar framebuffer F a N
	  FRAMEBUFFER COPY F, N
	  frameIndex = frameIndex + 1
	  
	  IF ASC(INKEY$) = T_ESC THEN PLAY STOP : EXIT DO
	LOOP

	CLOSE F_HANDLER

	DO WHILE MM.INFO(SOUND)="WAV"
	  PAUSE 20
	LOOP
	PLAY STOP

	FRAMEBUFFER CLOSE
LOOP



'------------------------
' SUBs y Funciones
'------------------------

' Menu de seleccion
FUNCTION MenuSel() AS INTEGER
  CONST NOPC = 3
  LOCAL STRING opciones(NOPC-1) = (" Bad Apple Demo    ", " Bad Apple Demo LQ ", " Exit              ")
  LOCAL STRING ayudas(NOPC-1) = ("Lanzar version optimizada        ","Lanzar version puramente en BASIC","Salir de la demo                 ")
  LOCAL INTEGER estado=0, tecla, i
  LOCAL INTEGER x=20, y=110
  
  CLS
  LOAD JPG "ba.jpg"
  
  DO
    ' Pintar opciones
    FONT 7
    FOR i = 0 TO NOPC-1
      IF i = estado THEN
        COLOR C_WHITE, C_BLACK
      ELSE
        COLOR C_BLACK, C_WHITE
      ENDIF
      TEXT x, y + i * 10, opciones(i)
    NEXT i
    
    ' Texto de ayuda
    FONT 8
    COLOR C_GRAY, C_WHITE
    TEXT 20, 230, ayudas(estado)
    
    ' Esperar tecla
    DO
      tecla = ASC(INKEY$)
    LOOP WHILE tecla = 0
    
    IF tecla = T_ENTER THEN EXIT DO
    PLAY TONE 500, 550, 20
    
    SELECT CASE tecla
    
      CASE T_ESC
        estado = -1
        EXIT DO
        
      CASE T_UP
        IF estado > 0 THEN estado = estado-1
        
      CASE T_DOWN
        IF estado < NOPC-1 THEN estado = estado+1
        
    END SELECT
    
  LOOP
  
  PLAY TONE 1000,1000,20  ' Beep de confirmacion
  DO WHILE MM.INFO(SOUND)="TONE"
    PAUSE 5
  LOOP
  
  MenuSel=estado
END FUNCTION


' Lee un nuevo bloque del video comprimido (RLE)
SUB LoadChunk
  chunk        = INPUT$(CHUNK_SIZE, F_HANDLER)
  bytesInChunk = LEN(chunk)
  chunkPos     = 0
  
  IF bytesInChunk = 0 THEN
    FRAMEBUFFER CLOSE : PLAY STOP : CLOSE F_HANDLER
    PRINT "EOF inesperado en frame"; frameIndex
    END ' Abortamos
  ENDIF
END SUB
  
  
' Decodificador LQ (MMBasic puro, Run-Length por lineas)
SUB DecodeLQ
  DO WHILE x < vidWidth
    runLen = BYTE(chunk, chunkPos + 1)
    INC chunkPos
    
    ' Si es tramo blanco y longitud > 0, dibujar la linea
    IF colorIsWhite AND runLen > 0 THEN
      LINE x*2, y*2, x*2 + (runLen-1)*2, y*2, 2
    ENDIF
    
    x = x + runLen
    colorIsWhite = 1 - colorIsWhite  ' alternar negro/blanco
    
    IF chunkPos >= bytesInChunk THEN EXIT DO
  LOOP
END SUB
  
  
'============================================================
' CSUB: Decodificador de video RLE en C
'   Archivo original: csub/decode.c
'============================================================
' Parametros: chunk, bytesInChunk, chunkPos, x, y, colorIsWhite
CSUB Decode STRING, INTEGER, INTEGER, INTEGER, INTEGER, INTEGER
  00000000
  4645B5F0 46DE4657 001C464E B5E023A0 68220017 4688B085 005B9D0F DA30429A
  90034B1B 2301469B E00B469A 68234652 6023199B 1AD3682B 23A0602B 005B6822
  DA1E429A 683B4642 42936812 9A03DA19 330118D2 603B7856 2B01682B 2E00D1E6
  9A0ED0E4 68116820 93001E72 4694465B 44844A08 681B9201 46994662 47C8000B
  B005E7D4 46BBBCF0 46A946B2 BDF046A0 1000027C 00FFFFFF 00000000 00000000
  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
  00000000 00000000 00000000 00000000 00000000 00000000
END CSUB
