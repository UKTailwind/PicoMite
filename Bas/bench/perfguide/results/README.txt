Raw results behind docs/MMBasic_Performance_Guide.md, taken 2026-10-07 with
V7.0.00b7. Each R line is: R <test id> <passes> <best of three, ms>.

vga_on.txt          perfguide.bas      PicoMiteVGA RP2040, 378 MHz, symbols on
vga_off.txt         perfguide_off.bas  the same board, OPTION SYMBOLS OFF
pc3_on.txt          perfguide.bas      PicoMiteHDMIWEB RP2350B (PicoComputer 3),
                                       252 MHz, OPTION COMPILE OFF
pc3_off.txt         perfguide_off.bas  the same board, OPTION SYMBOLS OFF
pc3_comp.txt        perfguide.bas      the same board, OPTION COMPILE ON
vgam_on.txt         perfmath.bas       RP2040 as above
pc3m_on.txt         perfmath.bas       RP2350 as above, OPTION COMPILE OFF
pc3m_comp.txt       perfmath.bas       RP2350 as above, OPTION COMPILE ON
vga_verify.txt      verify.bas         RP2040 as above
pc3_verify_comp.txt verify.bas         RP2350 as above, OPTION COMPILE ON
