# Goldens on PicoMite RP2350B  6.040051 378000000

DIFF 9 ERROR 66 LOAD-ERROR 1 MATCH 35 TIMEOUT 1

| program | class | deterministic | secs | note |
|---|---|---|---|---|
| arrsize | MATCH | True | 0.4 |  |
| arrslice | ERROR | True | 0.5 | [108] Print "  MAP("; Str$(codes(i)); ") = "; HEX$(cout(i), 6); "  MAP() agrees: "; Str$(cout(i) = Map(codes(i))) / Error : Invalid for this display |
| arrslice1 | MATCH | True | 0.4 |  |
| bench | TIMEOUT | True | 61.4 | > |
| bitbyte | ERROR | True | 0.5 | [38] Print "  flags 2,5      "; Flag(0); Flag(2); Flag(5); "  MM.INFO(FLAGS) = "; MM.Info(Flags) / Error : Invalid syntax |
| blit | ERROR | True | 0.5 | [6] Mode 2 / Error : Invalid in a program |
| box | ERROR | True | 0.5 | [6] Mode 2 / Error : Invalid in a program |
| byrefelem | MATCH | True | 0.4 |  |
| callname | MATCH | True | 0.4 |  |
| checks | MATCH | True | 0.4 |  |
| circle | ERROR | True | 0.5 | [5] Mode 2 / Error : Invalid in a program |
| circrnd | ERROR | True | 0.5 | [4] Mode 2 / Error : Invalid in a program |
| cmath | MATCH | True | 0.4 |  |
| console | MATCH | True | 0.4 |  |
| constonce | MATCH | True | 0.4 |  |
| countpin | ERROR | True | 0.5 | [10] SetPin gp4, FIN / Error : Invalid configuration |
| dataconst | ERROR | True | 0.5 | [32] Const LATE = 7 / Error : LATE Global variable already declared |
| dynarr | ERROR | True | 0.5 | [80] Out % = 0 / Error : Missing Program statement |
| elseif | ERROR | True | 0.5 | [49] If n = 1 Then / Error : Unexpected text |
| fbdemo | ERROR | True | 0.5 | [12] Mode 2 / Error : Invalid in a program |
| fbwrite | ERROR | True | 0.5 | [11] FRAMEBUFFER Write F / Error : Frame buffer not created |
| flash | ERROR | True | 0.5 | [4] Mode 2 / Error : Invalid in a program |
| fnbyref | MATCH | True | 0.4 |  |
| fnretcat | MATCH | True | 0.4 |  |
| fontdef | ERROR | True | 0.5 | [6] Text 0, 0, "012", "LT", 10 / Error : Display not configured |
| forktest | LOAD-ERROR |  |  | Error : Invalid font number #10 |
| framebuf | ERROR | True | 0.5 | [13] FRAMEBUFFER Create / Error : Not enough memory for 0 bytes |
| gpio | ERROR | True | 0.5 | [17] SetPin 3, DIN / Error : Invalid pin |
| i2c0 | ERROR | True | 0.6 | [64] SetPin 38, 39, I2C2 / Error : Invalid pin |
| i2c2 | ERROR | True | 0.5 | [17] SetPin 38, 39, I2C2 / Error : Invalid pin |
| imgfmt | ERROR | True | 0.5 | [8] Mode 2 / Error : Invalid in a program |
| imgloop | ERROR | True | 0.5 | [7] Mode 1 / Error : Invalid in a program |
| imgm1 | ERROR | True | 0.5 | [5] Mode 1 / Error : Invalid in a program |
| imgm1b | ERROR | True | 0.5 | [9] Mode 1 / Error : Invalid in a program |
| imgm1c | ERROR | True | 0.5 | [10] Mode 1 / Error : Invalid in a program |
| imgm1d | ERROR | True | 0.5 | [9] Mode 1 / Error : Invalid in a program |
| imgtrip | ERROR | True | 0.5 | [7] Mode 2 / Error : Invalid in a program |
| init2d | MATCH | True | 0.4 |  |
| jsonpath | ERROR | True | 0.5 | [6] Print "[" JSON$(j(), "Name") "]" / Error : Dimensions |
| justarg | DIFF | True | 0.4 |  |
| layer | ERROR | True | 0.5 | [19] Mode 1 / Error : Invalid in a program |
| localheap | MATCH | True | 0.4 |  |
| lscomms | ERROR | True | 0.5 | [25] SetPin 2, 3, 4, SPI / Error : Invalid pin |
| matha | MATCH | True | 0.4 |  |
| mathb64 | ERROR | True | 0.5 | [6] n% = Math(BASE64 ENCODE "f", out$) / Error : Variable name |
| mathcrc | ERROR | True | 0.5 | [28] Print HEX$(Math(CRC16 s$, 0, &H1021)) / Error : 0 is invalid (valid is 1 to 65535) |
| mathm | MATCH | True | 0.4 |  |
| mathm1 | ERROR | True | 0.5 | [50] Math M_Inverse s(), si() / Error : Not enough memory for 0 bytes |
| mathq | MATCH | True | 0.4 |  |
| mathr | DIFF | True | 0.4 |  |
| mathw | MATCH | True | 0.4 |  |
| mminfo | ERROR | True | 0.5 | [26] Print "gp0      "; GP0 / Error : GP0 is not declared |
| negcmp | ERROR | True | 0.5 | [34] ii% = -Pi% / Error : Expression syntax |
| onerror | ERROR | True | 0.5 | [48] r = 10 \ n / Error : Divide by zero |
| onerrwin | MATCH | True | 0.4 |  |
| onewire | ERROR | True | 0.5 | [17] OneWire RESET 26 / Error : Pin 26/GP20 is reserved on startup |
| onkey | MATCH | True | 0.8 |  |
| optangle | MATCH | True | 0.4 |  |
| optbase1 | MATCH | True | 0.4 |  |
| optescape | MATCH | True | 0.4 |  |
| order | MATCH | True | 0.4 |  |
| palette | ERROR | True | 0.5 | [7] Mode 2 / Error : Invalid in a program |
| pathmap | DIFF | True | 0.4 |  |
| pinint | ERROR | True | 0.5 | [26] SetPin 34, INTH, OnEdge / Error : Pin 34/GP28 is reserved on startup |
| pioout | MATCH | True | 0.4 |  |
| pixart | ERROR | True | 0.5 | [4] Mode 2 / Error : Invalid in a program |
| pixels | ERROR | True | 0.5 | [31] Pixel xi(), yi() / Error : Display not configured |
| play | DIFF | True | 0.4 |  |
| playfile | MATCH | True | 0.4 |  |
| playmp3 | ERROR | True | 0.5 | [18] Play MP3 "/root/mp3/whiter.mp3" / Error : Could not find the file |
| polypoly | ERROR | True | 0.5 | [6] CLS / Error : Display not configured |
| port | ERROR | True | 0.5 | [4] SetPin i, DOut / Error : Invalid pin |
| posflush | MATCH | True | 0.4 |  |
| printat | MATCH | True | 0.4 |  |
| pulse | ERROR | True | 0.5 | [13] SetPin 0, DOut / Error : Invalid pin |
| pulsin | DIFF | True | 0.4 |  |
| pwm | ERROR | True | 0.5 | [9] SetPin 3, PWM / Error : Invalid pin |
| ripple | ERROR | True | 0.5 | [3] Timer =0 : CLS / Error : Display not configured |
| rtcreg | DIFF | False | 0.4 |  |
| rtest | ERROR | True | 0.5 | [3] Mode 1 / Error : Invalid in a program |
| saveimg | ERROR | True | 0.5 | [4] Mode 2 / Error : Invalid in a program |
| settick | MATCH | True | 8.9 |  |
| solar_eclipse | MATCH | True | 14.7 |  |
| sombrero | ERROR | True | 0.5 | [6] XF=XR/XP: YF=YP/YR: ZF=XR/XP / Error : Divide by zero |
| spi | ERROR | True | 0.5 | [19] SetPin 2, 3, 4, SPI / Error : Invalid pin |
| sprite | ERROR | True | 0.5 | [4] Mode 2 / Error : Invalid in a program |
| strargs | DIFF | True | 0.4 |  |
| strlen | MATCH | True | 0.4 |  |
| structtest | ERROR | True | 1.0 | [1848] Box boxes(i%).x, boxes(i%).y, boxes(i%).w, boxes(i%).h / Error : Display not configured |
| t1 | ERROR | True | 0.4 | [99] Function Trim$(s$, ch$) / Error: Invalid identifier |
| t2 | ERROR | True | 0.5 | [8] Fill data() / Error : Invalid on this display |
| t3 | ERROR | True | 0.5 | [27] If a = 4 Then b = 21 : Print "7 then" Else b = 22 : Print "7 else" / Error : Expected closing bracket |
| t4 | ERROR | True | 0.5 | [13] If a = 4 Then Show 11, 12 : Show 13, 14 Else Show 15, 16 / Error : Argument list |
| t5 | ERROR | True | 0.5 | [25] Print Bit(&B1010, 1); Bit(&B1010, 2) / Error : Variable name |
| t6 | MATCH | True | 0.6 |  |
| t7 | ERROR | True | 0.6 | [162] Print "after erase: stat(0) ="; stat(0); " s$ = ["; s$; "]" / Error : STAT is not declared |
| t8 | DIFF | True | 0.4 |  |
| text | ERROR | True | 0.6 | [18] Text 0, 0, "plain" / Error : Display not configured |
| tiger | ERROR | True | 0.5 | [3] Mode 2 / Error : Invalid in a program |
| tilemap | ERROR | True | 0.6 | [7] Mode 2 / Error : Invalid in a program |
| tri | ERROR | True | 0.5 | [5] Mode 2 / Error : Invalid in a program |
| type | MATCH | True | 0.4 |  |
| uptime | MATCH | True | 0.4 |  |
| varaddr | DIFF | True | 0.4 |  |
| waitint | MATCH | True | 1.1 |  |
| webnpc | MATCH | True | 0.4 |  |
| webpage | MATCH | True | 0.4 |  |
| webservz | ERROR | True | 0.5 | [4] WEB TCP SERVER PORT 48123 / Error : Unknown command |
| webtcpe | ERROR | True | 0.5 | [5] WEB OPEN TCP CLIENT "127.0.0.1", 9, 100 / Error : Unknown command |
| webtlse | ERROR | True | 0.5 | [3] WEB TLS CA "no_such_bundle.pem" / Error : Unknown command |
| webudp | ERROR | True | 0.5 | [9] WEB UDP SERVER PORT 47999 / Error : Unknown command |
| wtest | ERROR | True | 0.5 | [3] Mode 1 / Error : Invalid in a program |
