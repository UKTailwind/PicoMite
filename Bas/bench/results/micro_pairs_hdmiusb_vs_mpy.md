# MMBasic HDMIUSB vs MicroPython, same PC3 hardware, 378 MHz, HDMI on, no WiFi

| statement | MMBasic cycles | MicroPython cycles | MMBasic / MicroPython |
|---|---|---|---|
| empty FOR/for loop | 335 | 372 | 0.9 |
| a=b | 1348 | 228 | 5.9 |
| 30-char names | 4671 | 225 | 20.8 |
| a=a+1 | 1834 | 510 | 3.6 |
| (s+i) AND 65535 | 2908 | 527 | 5.5 |
| float x*1.000001+0.5 | 3127 | 2255 | 1.4 |
| float copy | 1442 | 227 | 6.4 |
| literal | 1670 | 135 | 12.4 |
| constant | 1441 | 417 | 3.5 |
| 1-D array read | 4014 | 586 | 6.8 |
| 1-D array write | 4029 | 587 | 6.9 |
| 2-D array read | 4381 | 703 | 6.2 |
| ABS | 2428 | 2170 | 1.1 |
| ABS nested 5 | 5993 | 7192 | 0.8 |
| SIN | 2461 | 1568 | 1.6 |
| STR$ / str() | 3599 | 12907 | 0.3 |
| LEFT$ / slice | 3391 | 12822 | 0.3 |
| concat | 2695 | 10609 | 0.3 |
| single-line IF | 4695 | 405 | 11.6 |
| multi-line IF | 3624 | 405 | 8.9 |
| call, no args | 1952 | 977 | 2.0 |
| FUNCTION 2 args | 9748 | 1517 | 6.4 |
| call 6 params + 4 locals | 17866 | 1981 | 9.0 |
| call creating 3 local arrays | 14115 | 10363 | 1.4 |
| local copy in a SUB/def | 1506 | 30 | 50.2 |
| global read in a SUB/def | 1484 | 114 | 13.0 |
| float expr in a SUB/def | 3192 | 1702 | 1.9 |
| array read in a SUB/def | 4220 | 374 | 11.3 |
