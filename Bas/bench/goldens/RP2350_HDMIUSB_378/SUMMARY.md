# Goldens on PicoMiteHDMIUSB RP2350B  6.040051 378000000

DIFF 13 ERROR 47 MATCH 41 NO-EXPECTED 10 TIMEOUT 1

| program | class | deterministic | secs | note |
|---|---|---|---|---|
| arrsize | MATCH | True | 0.5 |  |
| arrslice | ERROR | True | 0.9 | [108] Print "  MAP("; Str$(codes(i)); ") = "; HEX$(cout(i), 6); "  MAP() agrees: "; Str$(cout(i) = Map(codes(i))) / Error : Invalid for Mode |
| arrslice1 | MATCH | True | 0.7 |  |
| bench | TIMEOUT | False | 61.5 | > |
| bitbyte | ERROR | True | 0.6 | [38] Print "  flags 2,5      "; Flag(0); Flag(2); Flag(5); "  MM.INFO(FLAGS) = "; MM.Info(Flags) / Error : Invalid syntax |
| blit | DIFF | True | 0.4 |  |
| box | MATCH | True | 0.4 |  |
| byrefelem | MATCH | True | 0.4 |  |
| callname | MATCH | True | 0.4 |  |
| checks | MATCH | True | 0.4 |  |
| circle | NO-EXPECTED | False | 0.4 |  |
| circrnd | NO-EXPECTED | False | 1.8 |  |
| cmath | MATCH | True | 0.4 |  |
| console | MATCH | True | 0.4 |  |
| constonce | MATCH | True | 0.4 |  |
| countpin | ERROR | True | 0.4 | [10] SetPin gp4, FIN / Error : Invalid configuration |
| dataconst | ERROR | True | 0.4 | [32] Const LATE = 7 / Error : LATE Global variable already declared |
| dynarr | ERROR | True | 0.4 | [80] Out % = 0 / Error : Missing Program statement |
| elseif | ERROR | True | 0.4 | [51] Else If n = 2 Then Print "same line body" / Error : Unexpected text |
| fbdemo | NO-EXPECTED | False | 4.8 |  |
| fbwrite | ERROR | True | 0.4 | [11] FRAMEBUFFER Write F / Error : Frame buffer not created |
| flash | DIFF | True | 7.9 |  |
| fnbyref | MATCH | True | 0.4 |  |
| fnretcat | MATCH | True | 0.4 |  |
| fontdef | MATCH | True | 0.4 |  |
| forktest | ERROR | True | 0.4 | [17] SYSTEM "true" / Error : Unknown command |
| framebuf | ERROR | True | 0.4 | [55] FRAMEBUFFER Create / Error : Framebuffer already exists |
| gpio | ERROR | True | 0.4 | [17] SetPin 3, DIN / Error : Invalid pin |
| i2c0 | ERROR | True | 0.4 | [64] SetPin 38, 39, I2C2 / Error : Invalid pin |
| i2c2 | ERROR | True | 0.4 | [17] SetPin 38, 39, I2C2 / Error : Invalid pin |
| imgfmt | ERROR | True | 0.4 | [9] Load IMAGE "g4.bmp" / Error : Could not find the file |
| imgloop | ERROR | True | 1.7 | [21] SYSTEM "sum", "s1.bmp", "s2.bmp" / Error : Unknown command |
| imgm1 | ERROR | True | 4.9 | [13] SYSTEM "sum", "m1.bmp", "m1b.bmp" / Error : Unknown command |
| imgm1b | ERROR | True | 4.9 | [22] SYSTEM "sum", "n1.bmp", "n1b.bmp" / Error : Unknown command |
| imgm1c | ERROR | True | 1.7 | [23] SYSTEM "sum", "q1.bmp", "q1b.bmp" / Error : Unknown command |
| imgm1d | ERROR | True | 2.3 | [26] SYSTEM "sum", "r1.bmp", "r2.bmp", "r3.bmp" / Error : Unknown command |
| imgtrip | ERROR | True | 1.6 | [17] SYSTEM "sum", "a.bmp", "b.bmp" / Error : Unknown command |
| init2d | MATCH | True | 0.4 |  |
| jsonpath | ERROR | True | 0.4 | [6] Print "[" JSON$(j(), "Name") "]" / Error : Dimensions |
| justarg | MATCH | True | 0.4 |  |
| layer | ERROR | True | 0.4 | [36] FRAMEBUFFER MERGE 0 / Error : Frame buffer 2 not created |
| localheap | MATCH | True | 0.4 |  |
| lscomms | ERROR | True | 0.4 | [25] SetPin 2, 3, 4, SPI / Error : Invalid pin |
| matha | MATCH | True | 0.9 |  |
| mathb64 | ERROR | True | 0.4 | [6] n% = Math(BASE64 ENCODE "f", out$) / Error : Variable name |
| mathcrc | ERROR | True | 0.5 | [28] Print HEX$(Math(CRC16 s$, 0, &H1021)) / Error : 0 is invalid (valid is 1 to 65535) |
| mathm | MATCH | True | 0.6 |  |
| mathm1 | MATCH | True | 0.5 |  |
| mathq | MATCH | True | 0.7 |  |
| mathr | DIFF | True | 0.5 |  |
| mathw | MATCH | True | 0.7 |  |
| mminfo | ERROR | True | 0.6 | [26] Print "gp0      "; GP0 / Error : GP0 is not declared |
| negcmp | ERROR | True | 0.5 | [34] ii% = -Pi% / Error : Expression syntax |
| onerror | ERROR | True | 0.6 | [48] r = 10 \ n / Error : Divide by zero |
| onerrwin | MATCH | True | 0.5 |  |
| onewire | ERROR | True | 0.4 | [17] OneWire RESET 26 / Error : Pin 26/GP20 is reserved on startup |
| onkey | MATCH | True | 0.9 |  |
| optangle | MATCH | True | 0.6 |  |
| optbase1 | MATCH | True | 0.7 |  |
| optescape | MATCH | True | 0.7 |  |
| order | MATCH | True | 0.6 |  |
| palette | NO-EXPECTED | True | 0.4 |  |
| pathmap | DIFF | True | 0.4 |  |
| pinint | ERROR | True | 0.4 | [26] SetPin 34, INTH, OnEdge / Error : Pin 34/GP28 is reserved on startup |
| pioout | MATCH | True | 0.4 |  |
| pixart | NO-EXPECTED | False | 10.1 |  |
| pixels | MATCH | True | 0.4 |  |
| play | DIFF | True | 0.5 |  |
| playfile | MATCH | True | 0.4 |  |
| playmp3 | ERROR | True | 0.4 | [18] Play MP3 "/root/mp3/whiter.mp3" / Error : Could not find the file |
| polypoly | NO-EXPECTED | False | 0.4 |  |
| port | ERROR | True | 0.4 | [4] SetPin i, DOut / Error : Invalid pin |
| posflush | MATCH | True | 0.4 |  |
| printat | MATCH | True | 0.4 |  |
| pulse | ERROR | True | 0.4 | [13] SetPin 0, DOut / Error : Invalid pin |
| pulsin | DIFF | True | 0.5 |  |
| pwm | ERROR | True | 0.4 | [9] SetPin 3, PWM / Error : Invalid pin |
| ripple | NO-EXPECTED | False | 2.4 |  |
| rtcreg | DIFF | False | 0.4 |  |
| rtest | NO-EXPECTED | True | 0.4 |  |
| saveimg | ERROR | True | 1.0 | [11] SYSTEM "ls", "-l", "shot2.bmp", "crop2.bmp" / Error : Unknown command |
| settick | MATCH | True | 9.4 |  |
| solar_eclipse | MATCH | True | 15.9 |  |
| sombrero | NO-EXPECTED | False | 8.9 |  |
| spi | ERROR | True | 0.4 | [19] SetPin 2, 3, 4, SPI / Error : Invalid pin |
| sprite | ERROR | True | 0.4 | [99] Sprite LOADARRAY #6, 8, 8, two%() / Error : Argument 4 must be a 1D numerical array |
| strargs | DIFF | True | 0.4 |  |
| strlen | MATCH | True | 0.4 |  |
| structtest | DIFF | True | 1.6 |  |
| t1 | ERROR | True | 0.4 | [99] Function Trim$(s$, ch$) / Error: Invalid identifier |
| t2 | ERROR | True | 0.4 | [8] Fill data() / Error : Invalid syntax |
| t3 | ERROR | True | 0.4 | [27] If a = 4 Then b = 21 : Print "7 then" Else b = 22 : Print "7 else" / Error : Expected closing bracket |
| t4 | ERROR | True | 0.4 | [13] If a = 4 Then Show 11, 12 : Show 13, 14 Else Show 15, 16 / Error : Argument list |
| t5 | ERROR | True | 0.4 | [25] Print Bit(&B1010, 1); Bit(&B1010, 2) / Error : Variable name |
| t6 | MATCH | True | 0.7 |  |
| t7 | ERROR | True | 0.5 | [162] Print "after erase: stat(0) ="; stat(0); " s$ = ["; s$; "]" / Error : STAT is not declared |
| t8 | DIFF | True | 0.5 |  |
| text | DIFF | True | 0.4 |  |
| tiger | ERROR | True | 0.4 | [5] Load IMAGE "tiger.bmp", 0, 20 / Error : Could not find the file |
| tilemap | DIFF | True | 5.1 |  |
| tri | MATCH | True | 0.4 |  |
| type | MATCH | True | 0.4 |  |
| uptime | MATCH | True | 0.4 |  |
| varaddr | DIFF | True | 0.4 |  |
| waitint | MATCH | True | 1.2 |  |
| webnpc | MATCH | True | 0.4 |  |
| webpage | MATCH | True | 0.4 |  |
| webservz | ERROR | True | 0.4 | [4] WEB TCP SERVER PORT 48123 / Error : Unknown command |
| webtcpe | ERROR | True | 0.4 | [5] WEB OPEN TCP CLIENT "127.0.0.1", 9, 100 / Error : Unknown command |
| webtlse | ERROR | True | 0.4 | [3] WEB TLS CA "no_such_bundle.pem" / Error : Unknown command |
| webudp | ERROR | True | 0.4 | [9] WEB UDP SERVER PORT 47999 / Error : Unknown command |
| wtest | NO-EXPECTED | True | 1.0 |  |
