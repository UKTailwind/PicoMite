cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
add_executable(PicoMite
			Picomite.c
            mmbasic.c
            operators.c
            custom.c
            functions.c
            commands.c
            memory.c
            FileIO.c
            ff.c
            ffsystem.c
            ffunicode.c
            mmc_stm32.c
            draw.c
            editor.c
            xmodem.c
            MM_Misc.c
            External.c
            MATHS.c
            Onewire.c
            I2C.c
            SPI.c
            Serial.c
            SPI-LCD.c
            BmpDecoder.c
            Touch.c
            GUI.c
            GPS.c
            Audio.c
            CFunction.c
)
pico_enable_stdio_usb(PicoMite 1)
pico_enable_stdio_uart(PicoMite 0)
pico_add_extra_outputs(PicoMite)
target_link_libraries(PicoMite pico_stdlib
        hardware_flash
        hardware_irq
        hardware_adc
        hardware_pwm
        hardware_i2c
        hardware_spi
		hardware_dma
        hardware_exception
)