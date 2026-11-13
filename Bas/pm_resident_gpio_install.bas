' Change GP26 to a free input on the target board.
LIBRARY LOAD "pm_resident_gpio_probe.bas", O
ResidentGpioProbe 0, 26
PAUSE 300
ResidentGpioProbe 1, 26
