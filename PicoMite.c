/***********************************************************************************************************************
PicoMite MMBasic

Picomite.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "configuration.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/adc.h"
#include "hardware/exception.h"
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "HDMI.h"
#include "VGA.h"
#include "WiFi.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/timer.h"
#include "hardware/vreg.h"
#include "hardware/structs/pads_qspi.h"
#include "pico/unique_id.h"
#include "hardware/pwm.h"
#include "configuration.h"
#include <malloc.h>
#include "hardware/sync.h"

#ifdef rp2350
#include "hardware/structs/qmi.h"
#include "psram.h"
#endif
#ifdef PICOMITEVGA
    extern void start_vga_i2s(void);
#ifndef HDMI
#endif
#endif
#define COPYRIGHT "Copyright " YEAR " Geoff Graham\r\n" \
                  "Copyright " YEAR2 " Peter Mather\r\n"

#ifdef USBKEYBOARD
#include "tusb.h"
#include "host/hcd.h"
#include "usb_host_files/tusb_config.h"
#else
#include "pico/unique_id.h"
#ifndef PICOMITEBT
#include "class/cdc/cdc_device.h"
#endif
#endif
#ifndef rp2350
#include "hardware/structs/ssi.h"
#include "hardware/vreg.h"
#else
#ifdef HDMI
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#endif
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include <string.h>
#include "hardware/regs/sysinfo.h"
#include "hardware/regs/powman.h"
bool rp2350a = true;
uint32_t PSRAMsize = 0;
uint8_t PSRAMpin;
#endif
#include "hardware/structs/bus_ctrl.h"
#include <pico/bootrom.h>
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#ifdef PICOMITEWEB
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#ifdef PICOMITEWEB_TLS
#include "lwip/altcp_tls.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/ssl.h"
#endif
#endif
#ifdef PICOMITEBT
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"
#include "BTConsole.h"
#endif
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
/* PICOMITEBTH and PICOMITEHDMIBTH both use the BLE HID-host stack
   (BTKeyboard.c) for keyboard input and to keep the CYW43 LED
   heartbeat alive — pico_cyw43_arch_none alone has no async pump, so
   cyw43_arch_gpio_put hangs after the first call. btstack's workers
   keep the async_context alive, and bt_keyboard_poll() drives the
   heartbeat from main-thread context. */
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"
#include "BTKeyboard.h"
#endif
#ifdef PICOMITERP2350
#include "VGA222.h"
    const uint8_t kickvga[] = {1, 54, 198, 128, 100, 109, 97, 32, 116, 120, 32, 116, 97, 98, 108,
                               101, 32, 48, 44, 50, 44, 52, 56, 48, 44, 241, 115, 99, 114, 101, 101, 110, 98, 117, 102,
                               102, 41, 130, 243, 65, 41, 135, 53, 133, 243, 66, 41, 133, 52, 44, 49, 50, 56, 0, 1, 20, 198,
                               128, 119, 114, 105, 116, 101, 32, 48, 44, 48, 44, 49, 44, 55, 57, 57, 0, 0};
#endif
#ifdef PICOMITEVGA
    volatile uint8_t transparent = 0;
    volatile uint8_t transparents = 0;
    volatile int RGBtransparent = 0;
    int MODE1SIZE, MODE2SIZE, MODE3SIZE, MODE4SIZE, MODE5SIZE;
#ifdef HDMI
    uint32_t map16quads[16];
    uint32_t map16pairs[16];
    // 126 MHz timings
#else
    uint8_t map16[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    int QVGA_TOTAL; // total clock ticks (= QVGA_HSYNC + QVGA_BP + WIDTH*QVGA_CPP[1600] + QVGA_FP)
    int QVGA_HSYNC; // horizontal sync clock ticks
    int QVGA_BP;    // back porch clock ticks
    int QVGA_FP;    // front porch clock ticks

    // QVGA vertical timings
    int QVGA_VACT;   // V active scanlines (= 2*HEIGHT)
    int QVGA_VFRONT; // V front porch
    int QVGA_VSYNC;  // length of V sync (number of scanlines)
    int QVGA_VBACK;  // V back porch
    int QVGA_VTOT;   // total scanlines (= QVGA_VSYNC + QVGA_VBACK + QVGA_VACT + QVGA_VFRONT)
    int QVGA_HACT;   // V active scanlines (= 2*HEIGHT)
#endif

#ifndef HDMI
/* Hardware headers, PIO programs and the QVGA/VGA scanout configuration that
   used to live in the (non-HDMI-only) Include.h. Include.h's generic type and
   attribute macros (Bool/True/False/INLINE/WEAK/nop/cb/BIT/...) were dead - the
   firmware uses <stdbool.h> bool/true/false. configuration.h is already
   included above, and we are already inside #ifndef HDMI so the PIO headers
   need no further guard. */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/divider.h"
#include "hardware/dma.h"
#include "hardware/exception.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"
#ifndef USBKEYBOARD
#include "class/cdc/cdc_device.h"
#endif
#include "PicoMiteVGA.pio.h"
#include "PicoMiteI2S.pio.h"
/* QVGA scanout config (QVGA_GPIO_*, QVGA_*_F) moved to graphics/Screens.h */
#endif
#ifdef USBKEYBOARD
#ifdef HDMI
#ifdef PICOMITEHDMIBTH
#define MES_SIGNON "\rPicoMiteHDMIBTH MMBasic USB " CHIP " Edition V" VERSION "\r\n"
#elif defined(PICOMITEHDMIWEB)
#define MES_SIGNON "\rPicoMiteHDMIWEB MMBasic USB " CHIP " Edition V" VERSION "\r\n"
#else
#define MES_SIGNON "\rPicoMiteHDMI MMBasic USB " CHIP " Edition V" VERSION "\r\n"
#endif
#else
#define MES_SIGNON "\rPicoMiteVGA MMBasic USB " CHIP " Edition V" VERSION "\r\n"
#endif
    extern void hid_app_task(void);
    /* keytimer now lives in KeyboardMap.c (shared with the BLE-HID-host
       build) -- previously defined here too. */
    extern void USB_bus_reset(void);
    bool USBenabled = false;
#else
#ifdef HDMI
#define MES_SIGNON "\rPicoMiteHDMI MMBasic " CHIP " Edition V" VERSION "\r\n"
#else
#define MES_SIGNON "\rPicoMiteVGA MMBasic " CHIP " Edition V" VERSION "\r\n"
#endif
#endif

#endif
#ifdef PICOMITEWEB
#ifndef PICOMITEHDMIWEB
/* HDMIWEB also defines PICOMITEWEB but is an HDMI build — its MES_SIGNON
   was already set in the HDMI block above ("PicoMiteHDMIWEB"). Only the
   WiFi globals below are shared; don't redefine the WebMite banner. */
#define MES_SIGNON "\rWebMite MMBasic " CHIP " Edition V" VERSION "\r\n"
#endif
    volatile int WIFIconnected = 0;
    volatile int LastWifiErr = 0;
    int startupcomplete = 0;
    void ProcessWeb(int mode);
    char LCDAttrib = 0;
#endif
#ifdef PICOMITE
#ifdef USBKEYBOARD
#include "tusb.h"
#include "host/hcd.h"
#define MES_SIGNON "\rPicoMite MMBasic USB " CHIP " Edition V" VERSION "\r\n"
    extern void hid_app_task(void);
    /* keytimer now lives in KeyboardMap.c (shared with the BLE-HID-host
       build) -- previously defined here too. */
    extern void USB_bus_reset(void);
    bool USBenabled = false;
#include "pico/multicore.h"
    mutex_t frameBufferMutex; // mutex to lock frame buffer
#else
#ifdef PICOMITEBT
#define MES_SIGNON "\rPicoMiteBT MMBasic " CHIP " V" VERSION "\r\n"
#elif defined(PICOMITEBTH)
#define MES_SIGNON "\rPicoMiteBTH MMBasic " CHIP " V" VERSION "\r\n"
#elif defined(PICOMITEMIN)
#define MES_SIGNON "\rPicoMiteMin MMBasic " CHIP " V" VERSION "\r\n"
#else
#define MES_SIGNON "\rPicoMite MMBasic " CHIP " V" VERSION "\r\n"
#endif
#include "pico/multicore.h"
    mutex_t frameBufferMutex; // mutex to lock frame buffer
#endif
    char LCDAttrib = 0;
#endif
#define KEYCHECKTIME 20
#define BUFFER_REFRESH_INTERVAL_MS 20
#define LCDBLCHECKTIME 1000 // once per second - staggered   // *EB*
#define KBDBLCHECKTIME 1000 // once per second - staggered   // *EB*
    int ListCnt;
    int MMCharPos;
    int MMPromptPos;
    int busfault = 0;
    int ExitMMBasicFlag = false;
    volatile int MMAbort = false;
    unsigned int _excep_peek;
    void CheckAbort(void);
    void TryLoadProgram(void);
    unsigned char lastchar = 0;
    int adc_clk_div;
    unsigned char BreakKey = BREAK_KEY; // defaults to CTRL-C.  Set to zero to disable the break function
    volatile char ConsoleRxBuf[CONSOLE_RX_BUF_SIZE] = {0};
    volatile int ConsoleRxBufHead = 0;
    volatile int ConsoleRxBufTail = 0;
    volatile char ConsoleTxBuf[CONSOLE_TX_BUF_SIZE] = {0};
    volatile int ConsoleTxBufHead = 0;
    volatile int ConsoleTxBufTail = 0;
    uint I2SOff;
#ifndef USBKEYBOARD
    extern void initMouse0(int sensitivity);
    volatile unsigned int MouseTimer = 0;
#endif
    volatile unsigned int AHRSTimer = 0;
    volatile unsigned int InkeyTimer = 0;
    volatile long long int mSecTimer = 0; // this is used to count mSec
    volatile unsigned int WDTimer = 0;
    volatile unsigned int diskchecktimer = DISKCHECKRATE;
    volatile unsigned int clocktimer = 60 * 1000;
    volatile unsigned int bufferupdatetimer = 0;
    volatile unsigned int PauseTimer = 0;
    volatile unsigned int ClassicTimer = 0;
    volatile unsigned int NunchuckTimer = 0;
    volatile unsigned int IntPauseTimer = 0;
    volatile unsigned int Timer1 = 0, Timer2 = 0, Timer3 = 0, Timer4 = 0, Timer5 = 0; // 1000Hz decrement timer
    volatile unsigned int KeyCheck = 2000;
    volatile int ds18b20Timer = -1;
    volatile unsigned int ScrewUpTimer = 0;
#if PICOCALC
    volatile unsigned int LcdBlCheck = 4750; // *EB* Lcd Backlight Check (staggered)
    volatile unsigned int KbdBlCheck = 5250; // *EB* Kbd Backlight Check (staggered)
#endif

    // volatile int second = 0;                                            // date/time counters
    // volatile int minute = 0;
    // volatile int hour = 0;
    // volatile int day = 1;
    // volatile int month = 1;
    // volatile int year = 2000;
    volatile unsigned int GPSTimer = 0;
    volatile unsigned int SecondsTimer = 0;
    volatile unsigned int I2CTimer = 0;
    volatile int day_of_week = 1;
    unsigned char PulsePin[NBR_PULSE_SLOTS];
    unsigned char PulseDirection[NBR_PULSE_SLOTS];
    int PulseCnt[NBR_PULSE_SLOTS];
    int PulseActive;
    const uint8_t *flash_option_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    const uint8_t *SavedVarsFlash = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET + FLASH_ERASE_SIZE);
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET + FLASH_ERASE_SIZE + SAVEDVARS_FLASH_SIZE);
    const uint8_t *flash_progmemory = (const uint8_t *)(XIP_BASE + PROGSTART);
    const uint8_t *flash_libmemory = (const uint8_t *)(XIP_BASE + PROGSTART - MAX_PROG_SIZE);
    int ticks_per_second;
    int InterruptUsed;
    int calibrate = 0;
    char id_out[12];
    MMFLOAT VCC = 3.3;
    int PromptFont, PromptFC = 0xFFFFFF, PromptBC = 0; // the font and colours selected at the prompt
    volatile int DISPLAY_TYPE = SCREENMODE1;
    volatile bool processtick = true;
    unsigned char WatchdogSet = false;
    unsigned char IgnorePIN = false;
    unsigned char SPIatRisk = false;
    uint32_t restart_reason = 0;
    /* Live clk_sys speed in kHz after a runtime change (CPUSpeedRuntime),
       0 = still at the boot speed. The error-path option reload
       (ReloadOptionsKeepLive in FileIO.c) re-asserts it into
       Option.CPU_Speed after the flash byte-copy reverts it. */
    volatile uint32_t LiveCPUSpeed = 0;
    uint32_t __uninitialized_ram(_excep_code);
    uint64_t __uninitialized_ram(_persistent);
    extern uint32_t *g_vgalinemap;
    extern uint8_t IRpin;
    unsigned char lastcmd[STRINGSIZE * 2]; // used to store the last command in case it is needed by the EDIT command
    FATFS fs;                              // Work area (file system object) for logical drive
    bool timer_callback(repeating_timer_t *rt);
    static uint64_t __not_in_flash_func(uSecFunc)(uint64_t a)
    {
        uint64_t b = time_us_64() + a;
        while (time_us_64() < b)
        {
        }
        return b;
    }
    extern void MX470Display(int fn);
    // Vector to CFunction routine called every command (ie, from the BASIC interrupt checker)
    extern unsigned int CFuncInt1;
    // Vector to CFunction routine called by the interrupt 2 handler
    extern unsigned int CFuncInt2;
    extern unsigned int CFuncmSec;
    extern void CallCFuncInt1(void);
    extern void CallCFuncInt2(void);
    extern volatile bool CSubComplete;
    static uint64_t __not_in_flash_func(uSecTimer)(void) { return time_us_64(); }
    static int64_t PinReadFunc(int a) { return gpio_get(PinDef[a].GPno); }
    extern void CallCFuncmSec(void);
    extern volatile uint32_t irqs;
#define CFUNCRAM_SIZE 256
    int CFuncRam[CFUNCRAM_SIZE / sizeof(int)];
    repeating_timer_t timer;
    MMFLOAT IntToFloat(long long int a) { return a; }
    MMFLOAT FMul(MMFLOAT a, MMFLOAT b) { return a * b; }
    MMFLOAT FAdd(MMFLOAT a, MMFLOAT b) { return a + b; }
    MMFLOAT FSub(MMFLOAT a, MMFLOAT b) { return a - b; }
    MMFLOAT FDiv(MMFLOAT a, MMFLOAT b) { return a / b; }
    // single-precision (float) helpers for CSUBs - faster software float than
    // double; exposed at the end of the CallTable (0x104+). Soft-float ABI
    // (args in core regs) matches the CSUB, same as the double helpers.
    float SAdd(float a, float b) { return a + b; }
    float SSub(float a, float b) { return a - b; }
    float SMul(float a, float b) { return a * b; }
    float SDiv(float a, float b) { return a / b; }
    int   SCmp(float a, float b) { if (a > b) return 1; else if (a < b) return -1; else return 0; }
    float DtoS(double a) { return (float)a; }                                   // double -> single
    double StoD(float a) { return (double)a; }                                  // single -> double
    long long StoI(float a) { return (long long)(a >= 0 ? a + 0.5f : a - 0.5f); } // single -> int (rounds)
    float ItoS(long long a) { return (float)a; }                                // int -> single
    uint32_t CFunc_delay_us;
#ifndef HDMI
    int QVGA_CLKDIV; // SM divide clock ticks
#endif
    void PIOExecute(int pion, int sm, uint32_t ins)
    {
        PIO pio = (pion ? pio1 : pio0);
        pio_sm_exec(pio, sm, ins);
    }

    int IDiv(int a, int b) { return a / b; }
    int FCmp(MMFLOAT a, MMFLOAT b)
    {
        if (a > b)
            return 1;
        else if (a < b)
            return -1;
        else
            return 0;
    }
    MMFLOAT LoadFloat(unsigned long long c)
    {
        union ftype
        {
            unsigned long long a;
            MMFLOAT b;
        } f;
        f.a = c;
        return f.b;
    }
    const void *const CallTable[] __attribute__((section(".text"))) = {
        (void *)uSecFunc,           // 0x00
        (void *)putConsole,         // 0x04
        (void *)getConsole,         // 0x08
        (void *)ExtCfg,             // 0x0c
        (void *)ExtSet,             // 0x10
        (void *)ExtInp,             // 0x14
        (void *)PinSetBit,          // 0x18
        (void *)PinReadFunc,        // 0x1c
        (void *)MMPrintString,      // 0x20
        (void *)IntToStr,           // 0x24
        (void *)CheckAbort,         // 0x28
        (void *)GetMemory,          // 0x2c
        (void *)GetTempMemory,      // 0x30
        (void *)FreeMemory,         // 0x34
        (void *)&DrawRectangle,     // 0x38
        (void *)&DrawBitmap,        // 0x3c
        (void *)DrawLine,           // 0x40
        (void *)FontTable,          // 0x44
        (void *)&ExtCurrentConfig,  // 0x48
        (void *)&HRes,              // 0x4C
        (void *)&VRes,              // 0x50
        (void *)SoftReset,          // 0x54
        (void *)error,              // 0x58
        (void *)&ProgMemory,        // 0x5c
        (void *)&g_vartbl,          // 0x60
        (void *)&g_varcnt,          // 0x64
        (void *)&DrawBuffer,        // 0x68
        (void *)&ReadBuffer,        // 0x6c
        (void *)&FloatToStr,        // 0x70
        (void *)CallExecuteProgram, // 0x74
        (void *)&CFuncmSec,         // 0x78
        (void *)CFuncRam,           // 0x7c
        (void *)&ScrollLCD,         // 0x80
        (void *)IntToFloat,         // 0x84
        (void *)FloatToInt64,       // 0x88
        (void *)&Option,            // 0x8c
        (void *)sin,                // 0x90
        (void *)DrawCircle,         // 0x94
        (void *)DrawTriangle,       // 0x98
        (void *)uSecTimer,          // 0x9c
        (void *)FMul,               // 0xa0
        (void *)FAdd,               // 0xa4
        (void *)FSub,               // 0xa8
        (void *)FDiv,               // 0xac
        (void *)FCmp,               // 0xb0
        (void *)&LoadFloat,         // 0xb4
        (void *)&CFuncInt1,         // 0xb8
        (void *)&CFuncInt2,         // 0xbc
        (void *)&CSubComplete,      // 0xc0
        (void *)&AudioOutput,       // 0xc4
        (void *)IDiv,               // 0xc8
        (void *)&AUDIO_WRAP,        // 0xcc
        (void *)&CFuncInt3,         // 0xd0
        (void *)&CFuncInt4,         // 0xd4
        (void *)PIOExecute,         // 0xd8
        // --- APPEND-ONLY below: never reorder/insert above this line or every
        //     existing compiled CSUB breaks (offsets are the ABI). ---
        (void *)&WriteBuf,          // 0xdc  current write framebuffer pointer
        (void *)&FrameBuf,          // 0xe0  frame layer pointer
        (void *)&LayerBuf,          // 0xe4  overlay layer pointer
#ifndef PICOMITEVGA
		(void *)&WriteBuf,
#else
        (void *)&DisplayBuf,        // 0xe8  display layer pointer
#endif
        (void *)&DrawPixel,         // 0xec  per-mode pixel writer DrawPixel(x,y,rgb)
        (void *)Display_Refresh,    // 0xf0  push direct writes to buffered panels
        (void *)cos,                // 0xf4  MMFLOAT cos(MMFLOAT)
        (void *)sqrt,               // 0xf8  MMFLOAT sqrt(MMFLOAT)
        (void *)atan2,              // 0xfc  MMFLOAT atan2(MMFLOAT,MMFLOAT)
        (void *)pow,                // 0x100 MMFLOAT pow(MMFLOAT,MMFLOAT)
        // single-precision (float) routines - append-only, do not reorder
        (void *)SAdd,               // 0x104 float SAdd(float,float)
        (void *)SSub,               // 0x108 float SSub(float,float)
        (void *)SMul,               // 0x10c float SMul(float,float)
        (void *)SDiv,               // 0x110 float SDiv(float,float)
        (void *)SCmp,               // 0x114 int   SCmp(float,float)
        (void *)sinf,               // 0x118 float sinf(float)
        (void *)cosf,               // 0x11c float cosf(float)
        (void *)sqrtf,              // 0x120 float sqrtf(float)
        (void *)atan2f,             // 0x124 float atan2f(float,float)
        (void *)powf,               // 0x128 float powf(float,float)
        (void *)DtoS,               // 0x12c float DtoS(double)      double->single
        (void *)StoD,               // 0x130 double StoD(float)      single->double
        (void *)StoI,               // 0x134 long long StoI(float)   single->int (rounds)
        (void *)ItoS,               // 0x138 float ItoS(long long)   int->single
    };
#ifdef rp2350
    // this is a frig to place the calltable at 0x1000023C as in previous releases
    const int AallTableloc[2] __attribute__((section(".text"))) = {0, 1};
#endif
    const struct s_PinDef PinDef[] = {
        {0, 99, "NULL", UNUSED, 99, 99},
        {1, 0, "GP0", DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART0TX | I2C0SDA | PWM0A, 99, 0},  // pin 1
        {2, 1, "GP1", DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM0B, 99, 128},         // pin 2
        {3, 99, "GND", UNUSED, 99, 99},                                                       // pin 3
        {4, 2, "GP2", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM1A, 99, 1},           // pin 4
        {5, 3, "GP3", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM1B, 99, 129},          // pin 5
        {6, 4, "GP4", DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART1TX | I2C0SDA | PWM2A, 99, 2},  // pin 6
        {7, 5, "GP5", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM2B, 99, 130},         // pin 7
        {8, 99, "GND", UNUSED, 99, 99},                                                       // pin 8
        {9, 6, "GP6", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM3A, 99, 3},           // pin 9
        {10, 7, "GP7", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM3B, 99, 131},         // pin 10
        {11, 8, "GP8", DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART1TX | I2C0SDA | PWM4A, 99, 4}, // pin 11
        {12, 9, "GP9", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM4B, 99, 132},        // pin 12
        {13, 99, "GND", UNUSED, 99, 99},                                                      // pin 13
        {14, 10, "GP10", DIGITAL_IN | DIGITAL_OUT | SPI1SCK | I2C1SDA | PWM5A, 99, 5},        // pin 14
        {15, 11, "GP11", DIGITAL_IN | DIGITAL_OUT | SPI1TX | I2C1SCL | PWM5B, 99, 133},       // pin 15
#ifdef HDMI
        {16, 12, "HDMI", UNUSED, 99, 99}, // pin 16
        {17, 13, "HDMI", UNUSED, 99, 99}, // pin 17
        {18, 99, "GND", UNUSED, 99, 99},  // pin 18
        {19, 14, "HDMI", UNUSED, 99, 99}, // pin 19
        {20, 15, "HDMI", UNUSED, 99, 99}, // pin 20
        {21, 16, "HDMI", UNUSED, 99, 99}, // pin 21
        {22, 17, "HDMI", UNUSED, 99, 99}, // pin 22
        {23, 99, "GND", UNUSED, 99, 99},  // pin 23
        {24, 18, "HDMI", UNUSED, 99, 99}, // pin 24
        {25, 19, "HDMI", UNUSED, 99, 99}, // pin 25
#else
    {16, 12, "GP12", DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART0TX | I2C0SDA | PWM6A, 99, 6}, // pin 16
    {17, 13, "GP13", DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM6B, 99, 134},        // pin 17
    {18, 99, "GND", UNUSED, 99, 99},                                                        // pin 18
    {19, 14, "GP14", DIGITAL_IN | DIGITAL_OUT | SPI1SCK | I2C1SDA | PWM7A, 99, 7},          // pin 19
    {20, 15, "GP15", DIGITAL_IN | DIGITAL_OUT | SPI1TX | I2C1SCL | PWM7B, 99, 135},         // pin 20

    {21, 16, "GP16", DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART0TX | I2C0SDA | PWM0A, 99, 0}, // pin 21
    {22, 17, "GP17", DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM0B, 99, 128},        // pin 22
    {23, 99, "GND", UNUSED, 99, 99},                                                        // pin 23
    {24, 18, "GP18", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM1A, 99, 1},          // pin 24
    {25, 19, "GP19", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM1B, 99, 129},         // pin 25
#endif
        {26, 20, "GP20", DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART1TX | I2C0SDA | PWM2A, 99, 2},            // pin 26
        {27, 21, "GP21", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM2B, 99, 130},                   // pin 27
        {28, 99, "GND", UNUSED, 99, 99},                                                                   // pin 28
        {29, 22, "GP22", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM3A, 99, 3},                     // pin 29
        {30, 99, "RUN", UNUSED, 99, 99},                                                                   // pin 30
        {31, 26, "GP26", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1SCK | I2C1SDA | PWM5A, 0, 5},          // pin 31
        {32, 27, "GP27", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1TX | I2C1SCL | PWM5B, 1, 133},         // pin 32
        {33, 99, "AGND", UNUSED, 99, 99},                                                                  // pin 33
        {34, 28, "GP28", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1RX | UART0TX | I2C0SDA | PWM6A, 2, 6}, // pin 34
        {35, 99, "VREF", UNUSED, 99, 99},                                                                  // pin 35
        {36, 99, "3V3", UNUSED, 99, 99},                                                                   // pin 36
        {37, 99, "3V3E", UNUSED, 99, 99},                                                                  // pin 37
        {38, 99, "GND", UNUSED, 99, 99},                                                                   // pin 38
        {39, 99, "VSYS", UNUSED, 99, 99},                                                                  // pin 39
        {40, 99, "VBUS", UNUSED, 99, 99},                                                                  // pin 40
#if (defined(PICOMITEWEB) && defined(rp2350)) || defined(PICOMITEBT) || defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
        /* GP23/24/25/29 wire to the CYW43439 wireless chip on Pico W /
           Pico 2 W (and Pimoroni Pico Plus 2W). Mark them UNUSED so
           CheckPin() refuses to reset them in ClearExternalIO —
           otherwise the SPI link breaks and cyw43_arch_init() fails
           silently. Applies to RP2040 and RP2350 CYW43 builds alike;
           the previous &&defined(rp2350) qualifier left WebMite RP2040
           without the pseudo-pins, exposing GP23-29 to whatever code
           paths reach them. */
        {41, 23, "GP23", UNUSED, 99, 99}, // pseudo pin 41 reserved for WEB/BT/BTH/HDMIBTH interface
        {42, 24, "GP24", UNUSED, 99, 99}, // pseudo pin 42 reserved for WEB/BT/BTH/HDMIBTH interface
        {43, 25, "GP25", UNUSED, 99, 99}, // pseudo pin 43 reserved for WEB/BT/BTH/HDMIBTH interface
        {44, 29, "GP29", UNUSED, 99, 99}, // pseudo pin 44 reserved for WEB/BT/BTH/HDMIBTH interface
#else
#ifndef PICOMITEWEB
    {41, 23, "GP23", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM3B, 99, 131},             // pseudo pin 41
    {42, 24, "GP24", DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART1TX | I2C0SDA | PWM4A, 99, 4},     // pseudo pin 42
    {43, 25, "GP25", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM4B, 99, 132},            // pseudo pin 43
    {44, 29, "GP29", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | UART0RX | I2C0SCL | PWM6B, 3, 134}, // pseudo pin 44
#endif
#endif
#ifdef rp2350
        {45, 30, "GP30", DIGITAL_IN | DIGITAL_OUT | SPI1SCK | I2C1SDA | PWM7A, 99, 7},                       // pseudo pin 45
        {46, 31, "GP31", DIGITAL_IN | DIGITAL_OUT | SPI1TX | I2C1SCL | PWM7B, 99, 135},                      // pseudo pin 46
        {47, 32, "GP32", DIGITAL_IN | DIGITAL_OUT | UART0TX | SPI0RX | I2C0SDA | PWM8A, 99, 8},              // pseudo pin 47
        {48, 33, "GP33", DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM8B, 99, 136},                     // pseudo pin 48
        {49, 34, "GP34", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM9A, 99, 9},                       // pseudo pin 49
        {50, 35, "GP35", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM9B, 99, 137},                      // pseudo pin 50
        {51, 36, "GP36", DIGITAL_IN | DIGITAL_OUT | UART1TX | SPI0RX | I2C0SDA | PWM10A, 99, 10},            // pseudo pin 51
        {52, 37, "GP37", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM10B, 99, 138},                    // pseudo pin 52
        {53, 38, "GP38", DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM11A, 99, 11},                     // pseudo pin 53
        {54, 39, "GP39", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM11B, 99, 139},                     // pseudo pin 54
        {55, 40, "GP40", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | UART1TX | SPI1RX | I2C0SDA | PWM8A, 0, 8},   // pseudo pin 55
        {56, 41, "GP41", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | UART1RX | I2C0SCL | PWM8B, 1, 136},          // pseudo pin 56
        {57, 42, "GP42", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1SCK | I2C1SDA | PWM9A, 2, 9},            // pseudo pin 57
        {58, 43, "GP43", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1TX | I2C1SCL | PWM9B, 3, 137},           // pseudo pin 58
        {59, 44, "GP44", DIGITAL_IN | DIGITAL_OUT | UART0TX | ANALOG_IN | SPI1RX | I2C0SDA | PWM10A, 4, 10}, // pseudo pin 59
        {60, 45, "GP45", DIGITAL_IN | DIGITAL_OUT | UART0RX | ANALOG_IN | I2C0SCL | PWM10B, 5, 138},         // pseudo pin 60
        {61, 46, "GP46", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1SCK | I2C1SDA | PWM11A, 6, 11},          // pseudo pin 61
        {62, 47, "GP47", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | SPI1TX | I2C1SCL | PWM11B, 7, 139},          // pseudo pin 62
#endif
    };
    char alive[] = "\033[?25h";
    const char DaysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    char banner[64];
// Helper macro for mutex-protected operations
#if defined(PICOMITE) || defined(PICOMITEMIN)
#define WITH_FRAMEBUFFER_LOCK(condition, code)       \
    do                                               \
    {                                                \
        if (condition)                               \
            mutex_enter_blocking(&frameBufferMutex); \
        code;                                        \
        if (condition)                               \
            mutex_exit(&frameBufferMutex);           \
    } while (0)
#else
#define WITH_FRAMEBUFFER_LOCK(condition, code) code
#endif

    // Check if the system I2C bus is being held by user code
    // Returns true if the bus is held and background tasks should be skipped
    static inline int SystemI2CBusHeld(void)
    {
        if (I2C0locked && (I2C_Status & I2C_Status_BusHold))
            return 1;
        if (I2C1locked && (I2C2_Status & I2C_Status_BusHold))
            return 1;
        return 0;
    }

    // Wii controller state machine processing
    typedef enum
    {
        WII_STATE_IDLE = 0,
        WII_STATE_SEND = 1,
        WII_STATE_RECEIVE = 2
    } WiiState;

    static inline void process_wii_controller(volatile uint8_t *device_flag,
                                              volatile unsigned int *timer,
                                              unsigned char *readstate,
                                              void (*proc_func)(void),
                                              int threshold)
    {
        if (!(*device_flag) || *timer < threshold)
            return;

        int status;
        switch (*readstate)
        {
        case WII_STATE_IDLE:
            status = WiiSend(sizeof(readcontroller), (char *)readcontroller);
            if (!status)
                *readstate = WII_STATE_SEND;
            break;

        case WII_STATE_SEND:
            status = WiiReceive(6, (char *)nunbuff);
            *readstate = status ? WII_STATE_IDLE : WII_STATE_RECEIVE;
            break;

        case WII_STATE_RECEIVE:
            proc_func();
            *readstate = WII_STATE_IDLE;
            *device_flag = 2;
            break;
        }
        *timer = 0;
    }
    static inline int console_rx_buf_space(void)
    {
        if (ConsoleRxBufHead >= ConsoleRxBufTail)
            return CONSOLE_RX_BUF_SIZE - (ConsoleRxBufHead - ConsoleRxBufTail);
        else
            return ConsoleRxBufTail - ConsoleRxBufHead;
    }
#ifdef GUICONTROLS
    /* Wired-panel pen-down, polled in the MAIN THREAD by routinechecks()
       and read by the 1ms timer ISR's touch edge detector. The GT911's
       TOUCH_DOWN is an I2C transaction, which must never run from interrupt
       context — so the ISR consumes this cached flag instead of evaluating
       TOUCH_DOWN itself. Resistive / FT6336 (cheap pin reads) ride along
       for uniformity. */
    volatile bool TouchPanelDown = false;
#endif

#if defined(TOUCH_GESTURES) && !defined(PICOMITEVGA)
    /* ================================================================
     *  Wired-panel software-gesture driver (XPT2046 / FT6336 / GT911).
     *
     *  Driven from routinechecks() so gestures work in every context the
     *  interpreter polls from — program execution, PAUSE, the prompt —
     *  and crucially WITHOUT requiring GUI controls (ProcessTouch only
     *  runs when Ctrl != NULL) or even a GUICONTROLS build. The shared
     *  gesture state machine lives in Draw.c; this just feeds it the
     *  panel's down / move / up stream. Mouse and USB-touch gestures are
     *  fed from ProcessTouch / process_touch_report instead.
     *
     *  @param down  pre-polled pen-down state (routinechecks polls the
     *               panel once, in the main thread, and shares the result).
     *  Overhead: down/up edges are acted on immediately; the heavier
     *  coordinate reads while held are throttled to PANEL_GESTURE_SAMPLE_US.
     *  Two-finger pinch/rotate is sampled on capacitive panels only
     *  (resistive has no contact 2).
     * ================================================================ */
#define PANEL_GESTURE_SAMPLE_US 15000ULL
    static void PanelGestureService(bool down)
    {
#ifdef USBKEYBOARD
        if (usb_touch_present)
            return; /* a USB touch screen owns the gesture machine */
#endif
        static uint64_t svc_us = 0;
        static bool svc_down_prev = false;
        static int16_t sx = 0, sy = 0, sx2 = 0, sy2 = 0;
        static bool s_two = false;

        if (down && !svc_down_prev)
        {
            /* Down edge — capture the swipe start position. */
            int x = GetTouch(GET_X_AXIS), y = GetTouch(GET_Y_AXIS);
            if (x != TOUCH_ERROR && y != TOUCH_ERROR)
            {
                sx = (int16_t)x;
                sy = (int16_t)y;
            }
            touch_gesture_on_down(sx, sy);
            s_two = false;
            svc_us = time_us_64();
        }
        else if (down)
        {
            /* Held — track the live position + long-press + pinch, but
               throttle the (comparatively expensive) coordinate reads. */
            uint64_t now = time_us_64();
            if (now - svc_us >= PANEL_GESTURE_SAMPLE_US)
            {
                svc_us = now;
                int x = GetTouch(GET_X_AXIS), y = GetTouch(GET_Y_AXIS);
                if (x != TOUCH_ERROR && y != TOUCH_ERROR)
                {
                    sx = (int16_t)x;
                    sy = (int16_t)y;
                }
                if (Option.TOUCH_CAP) /* two-finger: capacitive panels only */
                {
                    int x2 = GetTouch(GET_X_AXIS2), y2 = GetTouch(GET_Y_AXIS2);
                    if (x2 != TOUCH_ERROR && y2 != TOUCH_ERROR)
                    {
                        sx2 = (int16_t)x2;
                        sy2 = (int16_t)y2;
                        if (!s_two)
                        {
                            touch_gesture_pinch_start(sx, sy, sx2, sy2);
                            s_two = true;
                        }
                    }
                    else if (s_two)
                    {
                        touch_gesture_pinch_end(sx, sy, sx2, sy2);
                        s_two = false;
                    }
                }
                touch_gesture_tick(sx, sy, !s_two);
            }
        }
        else if (svc_down_prev)
        {
            /* Up edge — last sampled position is the swipe end point. */
            if (s_two)
            {
                touch_gesture_pinch_end(sx, sy, sx2, sy2);
                s_two = false;
            }
            touch_gesture_on_up(sx, sy);
        }
        svc_down_prev = down;
    }
#endif /* TOUCH_GESTURES && !PICOMITEVGA */

    // should be run not more than once a millisecond
    void MIPS32 __not_in_flash_func(routinechecks)(void)
    {
        //        static int when = 0;
        // Note: classicread and nunchuckread are global variables declared in I2C.c
        static uint64_t lastrun = 0;
        uint64_t timenow = time_us_64();
        if (timenow - lastrun < 100)
            return;
        lastrun = timenow;
#if defined(PICOMITEVGA) || defined(GUICONTROLS)
        /* Mouse / virtual-cursor refresh. routinechecks() runs from the
           prompt's getchar loop as well as during program execution, so
           the cursor follows the mouse in both contexts. Throttled to
           ~10 ms — refreshing on every routinechecks tick (~100 µs)
           burns cycles redrawing the same pixels and slows the prompt
           noticeably. CursorRefresh's own enabled/position checks short-
           circuit when nothing has changed. Available on every build
           where the cursor module compiles (VGA/HDMI, and touch-LCD
           GUICONTROLS builds like PICORP2350). */
        static uint64_t cursor_lastrun = 0;
        if (timenow - cursor_lastrun >= 10000)
        {
            cursor_lastrun = timenow;
            CursorRefresh();
        }
#endif
#if defined(TOUCH_GESTURES) && !defined(PICOMITEVGA)
        /* Poll the wired panel's pen-down here, in the main thread, never
           from the 1ms timer ISR (the GT911's TOUCH_DOWN is an I2C
           transaction). Throttled — the GT911 only reports at ~10-20ms, so
           faster polling gains nothing. The cached flag feeds both the ISR
           touch edge detector (TouchPanelDown) and the gesture service. */
        {
            static uint64_t pd_us = 0;
            static bool pd_state = false;
            if (timenow - pd_us >= 5000)
            {
                pd_us = timenow;
                pd_state = (Option.TOUCH_CS || Option.TOUCH_IRQ) && TOUCH_DOWN;
#ifdef GUICONTROLS
                TouchPanelDown = pd_state;
#endif
            }
            PanelGestureService(pd_state);
        }
#endif
#ifndef USBKEYBOARD
        static int keyread = 0;
#ifndef PICOMITEBT
        /* Used by the USB CDC RX poll below; unused on BT (BT uses
           bt_console_rx_available()). */
        int count;
#endif
#ifndef PICOMITEBTH
        /* PS/2 polling. Gated off for PICOMITEBTH: the BLE-HID-host
           build doesn't support PS/2 keyboards (it would compete with
           the BT keymap path and confuse OPTION KEYBOARD semantics). */
        if (Option.KeyboardConfig)
            CheckKeyboard();
#endif
#endif

        if (CurrentlyPlaying != P_NOTHING)
        {
            // === Audio playback processing ===
            if (CurrentlyPlaying == P_WAV || CurrentlyPlaying == P_FLAC ||
                CurrentlyPlaying == P_MP3 || CurrentlyPlaying == P_MIDI)
            {
                WITH_FRAMEBUFFER_LOCK(SPIatRisk, checkWAVinput());
            }
            else if (CurrentlyPlaying == P_MOD || CurrentlyPlaying == P_ARRAY ||
                     CurrentlyPlaying == P_SOUND || CurrentlyPlaying == P_TONE
#ifdef rp2350
                     || CurrentlyPlaying == P_SAMPLE
#endif
            )
            {
                checkWAVinput();
            }
        }
        // === Early exit for frequent calls ===
        //        if ((++when & 0xF) && CurrentLinePtr)
        //            return;
        // === Timer synchronization ===
        if (abs((time_us_64() - mSecTimer * 1000)) > 5000)
        {
            cancel_repeating_timer(&timer);
            add_repeating_timer_us(-1000, timer_callback, NULL, &timer);
            mSecTimer = time_us_64() / 1000;
        }

        // === USB task processing ===
#ifdef USBKEYBOARD
        if (USBenabled && mSecTimer > 2000)
        {
            tuh_task();
            hid_app_task();
        }
#endif
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
        /* Pump btstack/cyw43 from the keyboard-tick site too. HID
           reports route into the console RX ring from inside the
           packet handler (process_kbd_report path), so no extra
           drain logic is needed here. Also drives the CYW43 LED
           heartbeat — see bt_keyboard_poll() in BTKeyboard.c. */
        bt_keyboard_poll();
#endif
#ifdef PICOMITEBT
        /* Pump btstack/cyw43 and drain inbound RFCOMM bytes into the
           standard console RX ring. Mirrors the USB CDC path. */
        bt_console_poll();
        if (bt_console_connected() &&
            (Option.SerialConsole == 0 || Option.SerialConsole > 4))
        {
            uint32_t space = console_rx_buf_space();
            while (space > 1 && bt_console_rx_available() > 0)
            {
                int c = bt_console_getc();
                if (c < 0)
                    break;
                if (BreakKey && c == BreakKey)
                {
                    MMAbort = true;
                    ConsoleRxBufHead = ConsoleRxBufTail;
                    break;
                }
                else if (c == keyselect && KeyInterrupt != NULL)
                {
                    Keycomplete = true;
                }
                else
                {
                    ConsoleRxBuf[ConsoleRxBufHead] = c;
                    ConsoleRxBufHead = (ConsoleRxBufHead + 1) % CONSOLE_RX_BUF_SIZE;
                    space--;
                }
            }
        }
#elif !defined(USBKEYBOARD)
    if (tud_cdc_connected() &&
        (Option.SerialConsole == 0 || Option.SerialConsole > 4) &&
        Option.Telnet != -1 &&
        (count = tud_cdc_available()))
    {
        uint32_t space = console_rx_buf_space();

        // Ensure we leave at least 1 byte free to distinguish full from empty
        if (space > 1)
        {
            uint8_t tempBuf[CFG_TUD_CDC_RX_BUFSIZE]; // USB CDC buffer size
            uint32_t to_read = space - 1;            // Maximum we can safely buffer

            // Limit to USB available data
            if (to_read > count)
                to_read = count;

            // Limit to temp buffer size
            if (to_read > sizeof(tempBuf))
                to_read = sizeof(tempBuf);

            // Read from USB with interrupts disabled to prevent race conditions
            irq_set_enabled(USBCTRL_IRQ, false);
            uint32_t actual = tud_cdc_read(tempBuf, to_read);
            irq_set_enabled(USBCTRL_IRQ, true);

            // Process received bytes
            for (uint32_t i = 0; i < actual; i++)
            {
                int c = tempBuf[i];

                // Check for break key
                if (BreakKey && c == BreakKey)
                {
                    MMAbort = true;
                    ConsoleRxBufHead = ConsoleRxBufTail; // Flush buffer
                    break;                               // Exit loop after abort
                }
                // Check for key interrupt (signal only, don't buffer)
                else if (c == keyselect && KeyInterrupt != NULL)
                {
                    Keycomplete = true;
                }
                // Normal character - store in buffer
                else
                {
                    ConsoleRxBuf[ConsoleRxBufHead] = c;
                    ConsoleRxBufHead = (ConsoleRxBufHead + 1) % CONSOLE_RX_BUF_SIZE;
                }
            }
        }
    }
#endif

        // === Display buffer refresh (RP2350) ===
#if PICOMITERP2350
        if (bufferupdatetimer == 0)
        {

            bufferupdatetimer = BUFFER_REFRESH_INTERVAL_MS;

            if (Option.DISPLAY_TYPE >= NEXTGEN &&
                !(low_x == silly_low && high_x == silly_high &&
                  low_y == silly_low && high_y == silly_high))
            {

                if (Option.Refresh)
                {
                    int dirty_x1 = low_x;
                    int dirty_x2 = high_x;
                    int dirty_y1 = low_y;
                    int dirty_y2 = high_y;

                    uint32_t xbounds = (uint32_t)dirty_x1 | (dirty_x2 << 16);
                    uint32_t ybounds = (uint32_t)dirty_y1 | (dirty_y2 << 16);
                    uint32_t scroll = (uint32_t)ScrollStart;

                    // Fully blocking enqueue for benchmarking refresh cost.
                    multicore_fifo_push_blocking(6);
                    multicore_fifo_push_blocking(xbounds);
                    multicore_fifo_push_blocking(ybounds);
                    multicore_fifo_push_blocking(scroll);
                    low_x = low_y = silly_low;
                    high_x = high_y = silly_high;
                }
            }
        }
#endif

        // === GPS processing ===
        if (GPSchannel || PinDef[Option.GPSTX].mode & UART0TX || PinDef[Option.GPSTX].mode & UART1TX)
            processgps();

        // === SD card check ===
        if (diskchecktimer == 0)
            CheckSDCard();

        // === Touch processing ===
        /*
        #ifdef GUICONTROLS
                if (Ctrl && TOUCH_GETIRQTRIS && !calibrate)
                    ProcessTouch();
        #endif
        */

        // === RTC update ===
        // Skip if user code is holding the I2C bus
        if (clocktimer == 0 && Option.RTC && !classicread && !nunchuckread && !SystemI2CBusHeld())
        {
            RtcGetTime(0);
        }

        // === I2C keyboard check ===
        // Skip if user code is holding the I2C bus
#ifndef USBKEYBOARD
        if (Option.KeyboardConfig == CONFIG_I2C && KeyCheck == 0 && !SystemI2CBusHeld())
        {
            CheckI2CKeyboard(0, keyread);
            keyread = !keyread;
            KeyCheck = KEYCHECKTIME;
        }
#endif
#if PICOCALC // *EB*
        //@@ Improved keyboard handling                          // *EB*
        if (Option.KeyboardConfig == CONFIG_PICOCALC && KeyCheck == 0 && !SystemI2CBusHeld()) // *EB*
        {                                                                                     // *EB*
            CheckPicoCalcKeyboard(0, keyread);                                                //            CheckPicoCalcKeyboard(1, 0);                                                      // *EB*
            KeyCheck = KEYCHECKTIME;                                                          // *EB*
        } // *EB*
        //@@ LCD backlight sync                                  // *EB*
        if (Option.KeyboardConfig == CONFIG_PICOCALC && LcdBlCheck == 0 && !SystemI2CBusHeld()) // *EB*
        {                                                                                       // *EB*
            CheckLcdBacklight();                                                                // *EB*
            LcdBlCheck = LCDBLCHECKTIME;                                                        // *EB*
        } // *EB*
        //@@ KBD backlight sync                                  // *EB*
        if (Option.KeyboardConfig == CONFIG_PICOCALC && KbdBlCheck == 0 && !SystemI2CBusHeld()) // *EB*
        {                                                                                       // *EB*
            CheckKbdBacklight();                                                                // *EB*
            KbdBlCheck = KBDBLCHECKTIME;                                                        // *EB*
        } // *EB*
#endif

        // === Wii Classic Controller ===
        // Skip if user code is holding the I2C bus
        if (!SystemI2CBusHeld())
            process_wii_controller(&classic1, &ClassicTimer, &classicread,
                                   classicproc, 10);

        // === Wii Nunchuck ===
        // Skip if user code is holding the I2C bus
        if (!SystemI2CBusHeld())
            process_wii_controller(&nunchuck1, &NunchuckTimer, &nunchuckread,
                                   nunproc, 10);
    }
    int __not_in_flash_func(getConsole)(void)
    {
        int c = -1;
#ifdef PICOMITEWEB
        ProcessWeb(1);
#endif
#ifdef rp2350
        stepper_poll_events();
#endif
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
        /* Pump btstack/cyw43 from the main loop. No bytes-to-drain
           wrapper like PICOMITEBT — HID reports route directly into
           the console RX ring via process_kbd_report() from inside
           the packet handler. */
        bt_keyboard_poll();
#endif
#ifdef PICOMITEBT
        /* Service cyw43/btstack work from the main thread at full
           console-read rate. Without this, pending work scheduled by
           the cyw43 SPI IRQ has to wait for the alarm callback or
           the next routinechecks tick (which is throttled). XMODEM
           ACKs and rapid bidirectional traffic need polling much
           more frequently than every 1 ms. Also drains any newly-
           received BLE bytes into ConsoleRxBuf so the read below
           sees them immediately. */
        bt_console_poll();
        if (bt_console_connected())
        {
            uint32_t space = console_rx_buf_space();
            while (space > 1 && bt_console_rx_available() > 0)
            {
                int b = bt_console_getc();
                if (b < 0)
                    break;
                if (BreakKey && b == BreakKey)
                {
                    MMAbort = true;
                    ConsoleRxBufHead = ConsoleRxBufTail;
                    break;
                }
                else if (b == keyselect && KeyInterrupt != NULL)
                {
                    Keycomplete = true;
                }
                else
                {
                    ConsoleRxBuf[ConsoleRxBufHead] = b;
                    ConsoleRxBufHead = (ConsoleRxBufHead + 1) % CONSOLE_RX_BUF_SIZE;
                    space--;
                }
            }
        }
#endif
        CheckAbort();
        if (ConsoleRxBufHead != ConsoleRxBufTail)
        { // if the queue has something in it
            c = ConsoleRxBuf[ConsoleRxBufTail];
            ConsoleRxBufTail = (ConsoleRxBufTail + 1) % CONSOLE_RX_BUF_SIZE; // advance the head of the queue
        }
        return c;
    }

    void __not_in_flash_func(putConsole)(int c, int flush)
    {
        if (OptionConsole & 2)
            DisplayPutC(c);
        if (OptionConsole & 1)
            SerialConsolePutC(c, flush);
    }
    // put a character out to the serial console
    char __not_in_flash_func(SerialConsolePutC)(char c, int flush)
    {
        if (c == '\b')
        {
            if (MMCharPos != 1)
            {
                MMCharPos -= 1;
            }
        }
#ifdef PICOMITEWEB
        if (Option.Telnet != -1)
        {
#endif
#ifdef PICOMITEBT
            if (Option.SerialConsole == 0 || Option.SerialConsole > 4)
            {
                bt_console_putc((uint8_t)c);
                if (flush)
                    bt_console_flush();
            }
#elif !defined(USBKEYBOARD)
    if (Option.SerialConsole == 0 || Option.SerialConsole > 4)
    {
        if (tud_cdc_connected() && (tud_cdc_get_line_state() & 0x01))
        {
            while (!tud_cdc_write_available())
            {
                busy_wait_us(250);
                if (!(tud_cdc_connected() && (tud_cdc_get_line_state() & 0x01)))
                    break;
            }
            tud_cdc_write_char(c);
            if (flush)
            {
                // fflush(stdout);
                tud_cdc_write_flush();
            }
        }
    }
#endif
            if (Option.SerialConsole)
            {
                int empty = uart_is_writable((Option.SerialConsole & 3) == 1 ? uart0 : uart1);
                while (ConsoleTxBufTail == ((ConsoleTxBufHead + 1) % CONSOLE_TX_BUF_SIZE))
                    ;                                                            // wait if buffer full
                ConsoleTxBuf[ConsoleTxBufHead] = c;                              // add the char
                ConsoleTxBufHead = (ConsoleTxBufHead + 1) % CONSOLE_TX_BUF_SIZE; // advance the head of the queue
                if (empty)
                {
                    while (irqs)
                    {
                    }
                    uart_set_irq_enables((Option.SerialConsole & 3) == 1 ? uart0 : uart1, true, true);
                    irq_set_pending((Option.SerialConsole & 3) == 1 ? UART0_IRQ : UART1_IRQ);
                }
            }
#ifdef PICOMITEWEB
        }
        TelnetPutC(c, flush);
        ProcessWeb(1);
#endif
        return c;
    }
    char MMputchar(char c, int flush)
    {
        putConsole(c, flush);
        if (isprint(c))
            MMCharPos++;
        if (c == '\r')
        {
            MMCharPos = 1;
        }
        return c;
    }
    // returns the number of character waiting in the console input queue
    int kbhitConsole(void)
    {
        int i;
        i = ConsoleRxBufHead - ConsoleRxBufTail;
        if (i < 0)
            i += CONSOLE_RX_BUF_SIZE;
        return i;
    }
// check if there is a keystroke waiting in the buffer and, if so, return with the char
// returns -1 if no char waiting
// the main work is to check for vt100 escape code sequences and map to Maximite codes
#if LOWRAM
    int MMInkey(void)
    {
#else
int __not_in_flash_func(MMInkey)(void)
{
#endif
        unsigned int c;
        unsigned int tc, ttc;
        static unsigned int c1 = -1;
        static unsigned int c2 = -1;
        static unsigned int c3 = -1;
        static unsigned int c4 = -1;

        // Fast path: return queued characters
        if (c1 != -1)
        {
            c = c1;
            c1 = c2;
            c2 = c3;
            c3 = c4;
            c4 = -1;
            return c;
        }

        c = getConsole();
#if !defined(USBKEYBOARD) && !defined(PICOMITEBTH)
        if (c == -1)
            CheckKeyboard();
#endif
#if defined(USBKEYBOARD) && defined(GUICONTROLS) && defined(PICOMITEVGA)
        /* OSK taps should always reach the console RX queue while the
           keyboard is visible — including during PRESS-ANY-KEY prompts and
           any other input poll that calls MMInkey directly (cmd_list,
           INPUT, etc). Only run when nothing arrived this poll so we don't
           add work to the hot path. */
        if (c == (unsigned int)-1 && OSK_IsActive())
        {
            ProcessTouch();
            c = getConsole();
        }
#endif

        // Fast path: return normal characters immediately
        if (c != 0x1b)
            return c;

        // --- Escape sequence handling (rare in normal use) ---

        InkeyTimer = 0;
        while ((c = getConsole()) == -1 && InkeyTimer < 30)
            ;

        // Handle ESC-O sequences (Linux terminal emulators)
        if (c == 'O')
        {
            while ((c = getConsole()) == -1 && InkeyTimer < 50)
                ;

            // Use lookup table for single character mapping
            if (c >= 'P' && c <= 'T')
            {
                return F1 + (c - 'P'); // F1-F5
            }

            if (c == '2')
            {
                while ((tc = getConsole()) == -1 && InkeyTimer < 70)
                    ;
                if (tc == 'R')
                    return F3 + 0x20;
                c1 = 'O';
                c2 = c;
                c3 = tc;
                return 0x1b;
            }

            c1 = 'O';
            c2 = c;
            return 0x1b;
        }

        // Must be square bracket
        if (c != '[')
        {
            c1 = c;
            return 0x1b;
        }

        while ((c = getConsole()) == -1 && InkeyTimer < 50)
            ;

        // Arrow keys (3 char sequences) - use lookup
        if (c >= 'A' && c <= 'D')
        {
            static const unsigned int arrow_keys[] = {UP, DOWN, RIGHT, LEFT};
            return arrow_keys[c - 'A'];
        }

        if (c < '1' || c > '6')
        {
            c1 = '[';
            c2 = c;
            return 0x1b;
        }

        while ((tc = getConsole()) == -1 && InkeyTimer < 70)
            ;

        // 4 character codes ending with ~
        if (tc == '~')
        {
            if (c >= '1' && c <= '6')
            {
                static const unsigned int four_char_keys[] = {
                    HOME, INSERT, DEL, END, PUP, PDOWN};
                return four_char_keys[c - '1'];
            }
            c1 = '[';
            c2 = c;
            c3 = tc;
            return 0x1b;
        }

        // 5 character codes
        while ((ttc = getConsole()) == -1 && InkeyTimer < 90)
            ;

        if (ttc == '~')
        {
            if (c == '1')
            {
                if (tc >= '1' && tc <= '5')
                    return F1 + (tc - '1');
                if (tc >= '7' && tc <= '9')
                    return F6 + (tc - '7');
            }
            else if (c == '2')
            {
                if (tc >= '0' && tc <= '1')
                    return F9 + (tc - '0');
                if (tc >= '3' && tc <= '4')
                    return F11 + (tc - '3');
                if (tc >= '5' && tc <= '6')
                    return F3 + 0x20 + tc - '5';
                if (tc >= '8' && tc <= '9')
                    return F5 + 0x20 + tc - '8';
            }
            else if (c == '3')
            {
                if (tc >= '1' && tc <= '4')
                    return F7 + 0x20 + (tc - '1');
            }
        }

        // Nothing worked
        c1 = '[';
        c2 = c;
        c3 = tc;
        c4 = ttc;
        return 0x1b;
    }
    // get a line from the keyboard or a serial file handle
    // filenbr == 0 means the console input
    void MMgetline(int filenbr, char *p)
    {
        int c, nbrchars = 0;
        char *tp;
        static int skip_console_lf = 0;

#ifdef MMBASIC_FM
        if (filenbr == 0 && fm_sanitize_next_console_input)
        {
            while (MMInkey() != -1)
                ;
            fm_sanitize_next_console_input = 0;
            skip_console_lf = 0;
        }
#endif

        while (1)
        {
            CheckAbort();
            if (FileTable[filenbr].com > MAXCOMPORTS && FileEOF(filenbr))
                break;
            c = MMfgetc(filenbr);
            if (c <= 0)
                continue; // keep looping if there are no chars

            // If the previous console line ended on CR, ignore a following LF once.
            if (filenbr == 0 && skip_console_lf)
            {
                if (c == '\n')
                {
                    skip_console_lf = 0;
                    continue;
                }
                skip_console_lf = 0;
            }

            // if this is the console, check for a programmed function key and insert the text
            if (filenbr == 0)
            {
                tp = NULL;
                if (c == F2)
                    tp = "RUN";
                if (c == F3)
                    tp = "LIST";
                if (c == F4)
                    tp = "EDIT";
                if (c == F10)
                    tp = "AUTOSAVE";
                if (c == F11)
                    tp = "XMODEM RECEIVE";
                if (c == F12)
                    tp = "XMODEM SEND";
                if (c == F1)
                    tp = (char *)Option.F1key;
                if (c == F5)
                    tp = (char *)Option.F5key;
                if (c == F6)
                    tp = (char *)Option.F6key;
                if (c == F7)
                    tp = (char *)Option.F7key;
                if (c == F8)
                    tp = (char *)Option.F8key;
                if (c == F9)
                    tp = (char *)Option.F9key;
                if (tp)
                {
                    strcpy(p, tp);
                    if (EchoOption)
                    {
                        MMPrintString(tp);
                        MMPrintString("\r\n");
                    }
                    return;
                }
            }

            if (c == '\t')
            { // expand tabs to spaces
                do
                {
                    if (++nbrchars > MAXSTRLEN)
                        error("Line is too long");
                    *p++ = ' ';
                    if (filenbr == 0 && EchoOption)
                        MMputchar(' ', 1);
                } while (nbrchars % 4);
                continue;
            }

            if (c == '\b')
            { // handle the backspace
                if (nbrchars)
                {
                    if (filenbr == 0 && EchoOption)
                        MMPrintString("\b \b");
                    nbrchars--;
                    p--;
                }
                continue;
            }

            if (c == '\n')
            { // what to do with a newline
                if (filenbr == 0)
                {
                    if (EchoOption)
                        MMPrintString("\r\n");
                    break; // LF-only terminals terminate console input here
                }
                break; // a newline terminates a line for a file
            }

            if (c == '\r')
            {
                if (filenbr == 0)
                {
                    if (EchoOption)
                        MMPrintString("\r\n");
                    skip_console_lf = 1;
                    break; // on the console this means the end of the line - stop collecting
                }
                else
                    continue; // for files loop around looking for the following newline
            }

            if (filenbr == 0 && !isprint(c))
                continue; // ignore stray console control codes

            if (isprint(c))
            {
                if (filenbr == 0 && EchoOption)
                    MMputchar(c, 1); // The console requires that chars be echoed
            }
            if (++nbrchars > MAXSTRLEN)
                error("Line is too long"); // stop collecting if maximum length
            *p++ = c;                      // save our char
        }
        *p = 0;
    }
    // Insert a string into the lastcmd buffer
    void MIPS16 InsertLastcmd(unsigned char *s)
    {
        int i, slen;

        if (strcmp((const char *)lastcmd, (const char *)s) == 0)
            return;

        slen = strlen((const char *)s);
        if (slen < 1 || slen > sizeof(lastcmd) - 1)
            return;

        slen++;
        for (i = sizeof(lastcmd) - 1; i >= slen; i--)
            lastcmd[i] = lastcmd[i - slen];

        strcpy((char *)lastcmd, (char *)s);

        for (i = sizeof(lastcmd) - 1; lastcmd[i]; i--)
            lastcmd[i] = 0;
    }

    void MIPS16 EditInputLine(void)
    {
        char *p;
        char buf[MAXKEYLEN + 3];
        char goend[10];
        int lastcmd_idx = 0, lastcmd_edit = 0;
        int CharIndex, BufEdited = false;
        int insert = false;
        int c, i, j, len;
        int l2, l3, l4;

        // Calculate line wrap positions
        if (Option.DISPLAY_CONSOLE && Option.Width <= SCREENWIDTH)
        {
            l2 = SCREENWIDTH - MMPromptPos;
            l3 = 2 * SCREENWIDTH - MMPromptPos;
            l4 = 3 * SCREENWIDTH - MMPromptPos;
        }
        else
        {
            l2 = Option.Width - MMPromptPos;
            l3 = 2 * Option.Width - MMPromptPos;
            l4 = 3 * Option.Width - MMPromptPos;
        }

        strcpy(goend, "\e[");
        IntToStr(&goend[strlen(goend)], l2 + MMPromptPos, 10);
        strcat(goend, "C");

        /* The wrap-trick below (write a char to force wrap, then \b) only works
         * if the serial terminal has DECAWM (autowrap) enabled. Make sure of it. */
        SSPrintString("\033[?7h");

        MMPrintString((char *)inpbuf);
        CharIndex = strlen((const char *)inpbuf);

        while (1)
        {
            c = MMgetchar();

            if (c == TAB)
            {
                strcpy(buf, "        ");
                switch (Option.Tab)
                {
                case 2:
                    buf[2 - (CharIndex % 2)] = 0;
                    break;
                case 3:
                    buf[3 - (CharIndex % 3)] = 0;
                    break;
                case 4:
                    buf[4 - (CharIndex % 4)] = 0;
                    break;
                case 8:
                    buf[8 - (CharIndex % 8)] = 0;
                    break;
                }
            }
            else
            {
                buf[0] = c;
                buf[1] = 0;
            }

            do
            {
                switch (buf[0])
                {
                case '\r':
                case '\n':
                    goto saveline;

                case '\b':
                    if (CharIndex > 0)
                    {
                        BufEdited = true;
                        i = CharIndex - 1;
                        j = CharIndex;

                        for (p = (char *)inpbuf + i; *p; p++)
                            *p = *(p + 1);

                        // Backspace to beginning
                        while (j)
                        {
                            if (j == l4 || j == l3 || j == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                            j--;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                        MX470Display(CLEAR_TO_EOS);
                        SSPrintString("\033[0J");

                        // Redisplay buffer
                        j = 0;
                        len = strlen((const char *)inpbuf);
                        while (j < len)
                        {
                            MMputchar(inpbuf[j], 0);
                            if ((j == l4 - 1 || j == l3 - 1 || j == l2 - 1) && j == len - 1)
                            {
                                SSPrintString(" \b");
                            }
                            if ((j == l4 - 1 || j == l3 - 1 || j == l2 - 1) && j < len - 1)
                            {
                                SerialConsolePutC(inpbuf[j + 1], 0);
                                SSPrintString("\b");
                            }
                            j++;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif

                        // Return cursor to position
                        for (j = len; j > i; j--)
                        {
                            if (j == l4 || j == l3 || j == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                        }
                        CharIndex--;
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                        if (strlen((const char *)inpbuf) == 0)
                            BufEdited = false;
                    }
                    break;

                case CTRLKEY('S'):
                case LEFT:
                    BufEdited = true;
                    insert = false;
                    if (CharIndex > 0)
                    {
                        if (CharIndex == l4 || CharIndex == l3 || CharIndex == l2)
                        {
                            DisplayPutC('\b');
                            SSPrintString("\e[1A");
                            SSPrintString(goend);
                        }
                        else
                        {
                            MMputchar('\b', 1);
                        }
                        insert = true;
                        CharIndex--;
                    }
                    break;

                case CTRLKEY('D'):
                case RIGHT:
                    len = strlen((const char *)inpbuf);
                    if (CharIndex < len)
                    {
                        BufEdited = true;
                        MMputchar(inpbuf[CharIndex], 1);
                        if ((CharIndex == l4 - 1 || CharIndex == l3 - 1 || CharIndex == l2 - 1) && CharIndex == len - 1)
                        {
                            SSPrintString(" \b");
                        }
                        if ((CharIndex == l4 - 1 || CharIndex == l3 - 1 || CharIndex == l2 - 1) && CharIndex < len - 1)
                        {
                            SerialConsolePutC(inpbuf[CharIndex + 1], 0);
                            SSPrintString("\b");
                        }
                        CharIndex++;
                    }
                    break;

                case CTRLKEY(']'):
                case DEL:
                    len = strlen((const char *)inpbuf);
                    if (CharIndex < len)
                    {
                        BufEdited = true;
                        i = CharIndex;

                        for (p = (char *)inpbuf + i; *p; p++)
                            *p = *(p + 1);

                        j = CharIndex;
                        while (j)
                        {
                            if (j == l4 || j == l3 || j == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                            j--;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                        MX470Display(CLEAR_TO_EOS);
                        SSPrintString("\033[0J");

                        j = 0;
                        len = strlen((const char *)inpbuf);
                        while (j < len)
                        {
                            MMputchar(inpbuf[j], 0);
                            if ((j == l4 - 1 || j == l3 - 1 || j == l2 - 1) && j == len - 1)
                            {
                                SSPrintString(" \b");
                            }
                            if ((j == l4 - 1 || j == l3 - 1 || j == l2 - 1) && j < len - 1)
                            {
                                SerialConsolePutC(inpbuf[j + 1], 0);
                                SSPrintString("\b");
                            }
                            j++;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif

                        for (j = len; j > i; j--)
                        {
                            if (j == l4 || j == l3 || j == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                    }
                    break;

                case CTRLKEY('N'):
                case INSERT:
                    insert = !insert;
                    break;

                case CTRLKEY('U'):
                case HOME:
                    BufEdited = true;
                    if (CharIndex > 0)
                    {
                        if (CharIndex == strlen((const char *)inpbuf))
                            insert = true;

                        while (CharIndex)
                        {
                            if (CharIndex == l4 || CharIndex == l3 || CharIndex == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                            CharIndex--;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                    }
                    else
                    {
                        BufEdited = insert = false;
                    }
                    break;

                case CTRLKEY('K'):
                case END:
                    BufEdited = true;
                    len = strlen((const char *)inpbuf);
                    while (CharIndex < len)
                    {
                        MMputchar(inpbuf[CharIndex++], 0);
                    }
#ifndef USBKEYBOARD
                    // fflush(stdout);
                    tud_cdc_write_flush();
#endif
                    break;

                // Function keys - consolidated
                case 0x91:
                    if (*Option.F1key)
                        strcpy(&buf[1], (char *)Option.F1key);
                    break;
                case 0x92:
                    strcpy(&buf[1], "RUN\r\n");
                    break;
                case 0x93:
                    strcpy(&buf[1], "LIST\r\n");
                    break;
                case 0x94:
                    strcpy(&buf[1], "EDIT\r\n");
                    break;
                case 0x96:
                    if (*Option.F6key)
                        strcpy(&buf[1], (char *)Option.F6key);
                    break;
                case 0x97:
                    if (*Option.F7key)
                        strcpy(&buf[1], (char *)Option.F7key);
                    break;
                case 0x98:
                    if (*Option.F8key)
                        strcpy(&buf[1], (char *)Option.F8key);
                    break;
                case 0x99:
                    if (*Option.F9key)
                        strcpy(&buf[1], (char *)Option.F9key);
                    break;
                case 0x9a:
                    strcpy(&buf[1], "AUTOSAVE\r\n");
                    break;
                case 0x9b:
                    strcpy(&buf[1], "XMODEM RECEIVE\r\n");
                    break;
                case 0x9c:
                    strcpy(&buf[1], "XMODEM SEND\r\n");
                    break;

                case 0x95:
                    if (*Option.F5key)
                    {
                        strcpy(&buf[1], (char *)Option.F5key);
                    }
                    else
                    {
                        SSPrintString("\e[2J\e[H");
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                        if (Option.DISPLAY_CONSOLE)
                        {
                            ClearScreen(gui_bcolour);
                            CurrentX = CurrentY = 0;
                        }
                        if (FindSubFun((unsigned char *)"MM.PROMPT", 0) >= 0)
                        {
                            ExecuteProgram((unsigned char *)"MM.PROMPT\0");
                        }
                        else
                        {
                            MMPrintString("> ");
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                    }
                    break;

                case CTRLKEY('E'):
                case UP:
                    if (!BufEdited)
                    {
                        if (lastcmd_edit)
                        {
                            i = lastcmd_idx + strlen((const char *)&lastcmd[lastcmd_idx]) + 1;
                            if (lastcmd[i] != 0 && i < sizeof(lastcmd) - 1)
                                lastcmd_idx = i;
                        }
                        else
                        {
                            lastcmd_edit = true;
                        }
                        strcpy((char *)inpbuf, (const char *)&lastcmd[lastcmd_idx]);
                        goto insert_lastcmd;
                    }
                    break;

                case CTRLKEY('X'):
                case DOWN:
                    if (!BufEdited)
                    {
                        if (lastcmd_idx == 0)
                        {
                            *inpbuf = lastcmd_edit = 0;
                        }
                        else
                        {
                            for (i = lastcmd_idx - 2; i > 0 && lastcmd[i - 1] != 0; i--)
                                ;
                            lastcmd_idx = i;
                            strcpy((char *)inpbuf, (const char *)&lastcmd[i]);
                        }
                        goto insert_lastcmd;
                    }
                    break;

                insert_lastcmd:
                    len = strlen((const char *)inpbuf);

                    if (Option.NoScroll && Option.DISPLAY_CONSOLE &&
                        (CurrentY + (2 + len / Option.Width) * gui_font_height >= VRes))
                    {
                        ClearScreen(gui_bcolour);
                        CurrentX = CurrentY = 0;
                        if (FindSubFun((unsigned char *)"MM.PROMPT", 0) >= 0)
                        {
                            SSPrintString("\r");
                            ExecuteProgram((unsigned char *)"MM.PROMPT\0");
                        }
                        else
                        {
                            SSPrintString("\r");
                            MMPrintString("> ");
                        }
                    }
                    else
                    {
                        j = CharIndex;
                        while (j)
                        {
                            if (j == l4 || j == l3 || j == l2)
                            {
                                DisplayPutC('\b');
                                SSPrintString("\e[1A");
                                SSPrintString(goend);
                            }
                            else
                            {
                                MMputchar('\b', 0);
                            }
                            j--;
                        }
#ifndef USBKEYBOARD
                        // fflush(stdout);
                        tud_cdc_write_flush();
#endif
                        MX470Display(CLEAR_TO_EOS);
                        SSPrintString("\033[0J");
                    }

                    CharIndex = len;
                    MMPrintString((char *)inpbuf);
                    if (CharIndex == l4 || CharIndex == l3 || CharIndex == l2)
                    {
                        SSPrintString(" \b");
                    }
#ifndef USBKEYBOARD
                    // fflush(stdout);
                    tud_cdc_write_flush();
#endif
                    break;

                default:
                    if (buf[0] >= ' ' && buf[0] < 0x7f)
                    {
                        i = CharIndex;
                        j = strlen((const char *)inpbuf);

                        if (insert)
                        {
                            if (j >= 254)
                                break;
                            for (p = (char *)inpbuf + j; j >= CharIndex; p--, j--)
                                *(p + 1) = *p;
                            inpbuf[CharIndex] = buf[0];
                            MMPrintString((char *)&inpbuf[CharIndex]);
                            CharIndex++;

                            len = strlen((const char *)inpbuf);
                            for (j = len; j > CharIndex; j--)
                            {
                                if (j == l4 || j == l3 || j == l2)
                                {
                                    DisplayPutC('\b');
                                    SSPrintString("\e[1A");
                                    SSPrintString(goend);
                                }
                                else
                                {
                                    MMputchar('\b', 0);
                                }
                            }
#ifndef USBKEYBOARD
                            // fflush(stdout);
                            tud_cdc_write_flush();
#endif
                        }
                        else
                        {
                            if (j >= 254)
                                break;
                            inpbuf[j + (CharIndex == j ? 1 : 0)] = 0;
                            inpbuf[CharIndex++] = buf[0];
                            MMputchar(buf[0], 0);
                            if (j == l4 - 1 || j == l3 - 1 || j == l2 - 1)
                            {
                                SSPrintString(" \b");
                            }
#ifndef USBKEYBOARD
                            // fflush(stdout);
                            tud_cdc_write_flush();
#endif
                        }

#ifndef PICOMITEVGA
                        if (Option.NoScroll && Option.DISPLAY_CONSOLE &&
                            CurrentY + 2 * gui_font_height >= VRes)
                        {
                            ClearScreen(gui_bcolour);
                            CurrentY = 0;
                            CurrentX = (MMPromptPos - 2) * gui_font_width;
                            DisplayPutC('>');
                            DisplayPutC(' ');
                            DisplayPutS((char *)inpbuf);
                        }
#endif
                    }
                    break;
                }

                for (i = 0; i < MAXKEYLEN + 1; i++)
                    buf[i] = buf[i + 1];

            } while (*buf);

            if (CharIndex == strlen((const char *)inpbuf))
            {
                insert = false;
            }
        }

    saveline:
        if (strlen((const char *)inpbuf) < 255)
            InsertLastcmd(inpbuf);
        MMPrintString("\r\n");
    }

    // get a keystroke.  Will wait forever for input
    // if the unsigned char is a cr then replace it with a newline (lf)
    int MMgetchar(void)
    {
        int c;
        do
        {
            ShowCursor(1);
            c = MMInkey();
        } while (c == -1);
        ShowCursor(0);
        return c;
    }
    // print a string to the console interfaces
    void MMPrintString(char *s)
    {
        while (*s)
        {
            if (s[1])
                MMputchar(*s, 0);
            else
                MMputchar(*s, 1);
            s++;
        }
#ifndef USBKEYBOARD
        // fflush(stdout);
        tud_cdc_write_flush();
#endif
    }
    void SSPrintString(char *s)
    {
        while (*s)
        {
            SerialConsolePutC(*s, 0);
            s++;
        }
#ifndef USBKEYBOARD
        // fflush(stdout);
        tud_cdc_write_flush();
#endif
    }

    void __not_in_flash_func(mT4IntEnable)(int status)
    {
        if (status)
        {
            processtick = true;
        }
        else
        {
            processtick = false;
        }
    }

    volatile int onoff = 0;
// Helper macros to reduce code duplication
#define DECREMENT_IF_ACTIVE(timer) \
    if (timer)                     \
    timer--

#define UPDATE_FREQ_INPUT(pin, timer, init_timer, count, value) \
    if (ExtCurrentConfig[pin] == EXT_FREQ_IN && --timer <= 0)   \
    {                                                           \
        value = count;                                          \
        count = 0;                                              \
        timer = init_timer;                                     \
    }

#define UPDATE_PER_INPUT(pin, count)         \
    if (ExtCurrentConfig[pin] == EXT_PER_IN) \
    count++

    // Optimized atomic 64-bit counter read
    static inline uint64_t read_counter_atomic(void)
    {
        uint32_t hi, lo;
        do
        {
            hi = INT5Count;
            lo = pwm_get_counter(0);
        } while (hi != INT5Count);
        return ((uint64_t)hi * 50000) + lo;
    }

    bool MIPS32 __not_in_flash_func(timer_callback)(repeating_timer_t *rt)
    {
        static int IrTimeout, IrTick, NextIrTick;
        int ElapsedMicroSec, IrDevTmp, IrCmdTmp;
        mSecTimer++;

        if (!processtick)
            return 1;

        // === RP2350 Fast Timer Processing ===
#ifdef rp2350
        if (ExtCurrentConfig[FAST_TIMER_PIN] == EXT_FAST_TIMER && --INT5Timer <= 0)
        {
            static uint64_t last = 0;
            uint64_t now = read_counter_atomic();
            INT5Value = now - last;
            last = now;
            INT5Timer = INT5InitTimer;
        }

#if PICOMITERP2350
        DECREMENT_IF_ACTIVE(bufferupdatetimer);
        if (Option.LOCAL_KEYBOARD && mSecTimer % LOCALKEYSCANRATE == 0)
            cmd_keyscan();
#endif
#endif

        // === Increment-only timers ===
        AHRSTimer++;
        InkeyTimer++;
        PauseTimer++;
        IntPauseTimer++;
        ds18b20Timer++;
        GPSTimer++;
        I2CTimer++;
        ClassicTimer++;
        NunchuckTimer++;

#ifdef USBKEYBOARD
        keytimer++;
        for (int i = 0; i < 4; i++)
        {
            if (HID[i].Device_type && HID[i].report_timer < 10000)
            {
                HID[i].report_timer++;
            }
        }
#else
    nunstruct[2].type++;
    MouseTimer++;
#endif

        // === Decrement-only timers ===
        DECREMENT_IF_ACTIVE(clocktimer);
        DECREMENT_IF_ACTIVE(Timer5);
        DECREMENT_IF_ACTIVE(Timer4);
        DECREMENT_IF_ACTIVE(Timer3);
        DECREMENT_IF_ACTIVE(Timer2);
        DECREMENT_IF_ACTIVE(Timer1);
        DECREMENT_IF_ACTIVE(KeyCheck);
#if PICOCALC                             // *EB*
        DECREMENT_IF_ACTIVE(LcdBlCheck); // *EB*
        DECREMENT_IF_ACTIVE(KbdBlCheck); // *EB*
#endif                                   // *EB*

        if (diskchecktimer && (Option.SD_CS || Option.CombinedCS))
            diskchecktimer--;

        // === Cursor blink control ===
        if (++CursorTimer > CURSOR_OFF + CURSOR_ON)
            CursorTimer = 0;

        // === CFunction callback ===
        if (CFuncmSec)
            CallCFuncmSec();

        // === Interrupt tick timers ===
        if (InterruptUsed)
        {
            for (int i = 0; i < NBRSETTICKS; i++)
            {
                if (TickActive[i])
                    TickTimer[i]++;
            }
        }

        // === Watchdog timer ===
        if (WDTimer && --WDTimer == 0)
        {
            SoftReset(WATCHDOG_TIMEOUT);
        }

        // === Screw-up timer ===
        if (ScrewUpTimer && --ScrewUpTimer == 0)
        {
            SoftReset(SCREWUP_TIMEOUT);
        }

        // === Pulse management ===
        if (PulseActive)
        {
            PulseActive = false;
            for (int i = 0; i < NBR_PULSE_SLOTS; i++)
            {
                if (PulseCnt[i] > 0)
                {
                    if (--PulseCnt[i] == 0)
                    {
                        PinSetBit(PulsePin[i], LATINV);
                    }
                    else
                    {
                        PulseActive = true;
                    }
                }
            }
        }
#ifdef GUICONTROLS
        // === Touch panel processing ===
        TouchTimer++;

        if (CheckGuiFlag)
            CheckGuiTimeouts();

        /* Touch panel edge detector. Fires for either a wired
           resistive/capacitive panel (TOUCH_GETIRQTRIS && TouchPanelDown)
           or a USB touch screen (usb_touch_active). Records ownership
           so the mouse/click detector below doesn't see a touch press
           as "mouse should release". GetTouch() (or its stub in GUI.c)
           returns whichever source is currently active when
           ProcessTouch reads coords.
           NB: TouchPanelDown is a flag polled in the main thread by
           routinechecks() — we must NOT evaluate TOUCH_DOWN here, as the
           GT911's pen-down read is an I2C transaction and this runs in the
           1ms timer ISR. */
        {
            bool panel_armed = TOUCH_GETIRQTRIS;
#ifdef USBKEYBOARD
            bool usb_armed = usb_touch_present;
            /* USB-touch no-report watchdog. Some controllers omit the
               release report (count=0 / tip=0) under specific timing —
               the lift goes undetected, usb_touch_active stays stuck
               true, and ProcessTouch's static `repeat` (for spinners)
               never clears. Force-release if no report has arrived for
               100 ms. Typical reporting rates are 100-200 Hz while a
               finger is on the surface, so 100 ms is ~10-20 missed
               reports — well past any normal jitter. */
            if (usb_armed && usb_touch_active && (time_us_64() - usb_touch_last_us) > 100000ULL)
            {
                usb_touch_active = false;
            }
#else
            bool usb_armed = false;
#endif
            if (panel_armed || usb_armed)
            {
                bool pen_down = (panel_armed && TouchPanelDown)
#ifdef USBKEYBOARD
                                || usb_touch_active
#endif
                    ;
                if (pen_down && !TouchState)
                {
                    TouchState = TouchDown = true;
                    gui_click_from_mouse = false;
                    gui_click_emulated = false; /* real pointing device */
                }
                else if (!pen_down && TouchState && !gui_click_from_mouse)
                {
                    TouchState = false;
                    TouchUp = true;
                }
            }
        }
        /* Mouse / synthetic-click / click-pin edge detector. Runs on
           every GUICONTROLS build (touch-LCD too, so a USB/PS-2 mouse
           can drive controls alongside the touch panel). Latches
           TouchX/Y at the down-edge: from nunstruct[2].ax/.ay for
           mouse and BASIC-driven GUI CLICK, or from cursor_x/cursor_y
           for a GUI CLICK PIN (the user has been steering the soft
           cursor with arrow keys / joystick — that's where the click
           lands). ProcessTouch reads gui_click_from_mouse to know it
           should trust the latched values rather than calling
           GetTouch(). */
        if (Ctrl != NULL)
        {
            bool pin_held = click_pin_pressed();
            bool btn = (nunstruct[2].L || gui_click_synthetic_down || pin_held)
                           ? true
                           : false;
            if (btn && !TouchState)
            {
                if (pin_held && !nunstruct[2].L && !gui_click_synthetic_down)
                {
                    TouchX = cursor_x;
                    TouchY = cursor_y;
                }
                else
                {
                    TouchX = nunstruct[2].ax;
                    TouchY = nunstruct[2].ay;
                }
                TouchState = TouchDown = true;
                gui_click_from_mouse = true;
                /* MsgBox needs a real pointing device to be usable.
                   nunstruct[2].L is true only when a real mouse is
                   driving the click; the other two contributors
                   (synthetic GUI CLICK from BASIC, GUI CLICK PIN)
                   can't move the cursor while MsgBox blocks. */
                gui_click_emulated = !nunstruct[2].L;
            }
            else if (!btn && TouchState && gui_click_from_mouse)
            {
                TouchState = false;
                TouchUp = true;
            }
        }

        if (ClickTimer)
        {
            ClickTimer--;
            if (Option.TOUCH_Click)
            {
                PinSetBit(Option.TOUCH_Click, ClickTimer ? LATSET : LATCLR);
            }
        }
#endif
        // now process the IR message, this includes handling auto repeat while the key is held down
        // IrTick counts how many mS since the key was first pressed
        // NextIrTick is used to time the auto repeat
        // IrTimeout is used to detect when the key is released
        // IrGotMsg is a signal to the interrupt handler that an interrupt is required
        if (IRpin != 99)
        {
            ElapsedMicroSec = readIRclock();
            if (IrState > IR_WAIT_START && ElapsedMicroSec > 15000)
                IrReset();
            IrCmdTmp = -1;

            // check for any Sony IR receive activity
            if (IrState == SONY_WAIT_BIT_START && ElapsedMicroSec > 2800 && (IrCount == 12 || IrCount == 15 || IrCount == 20))
            {
                IrDevTmp = ((IrBits >> 7) & 0b11111);
                IrCmdTmp = (IrBits & 0b1111111) | ((IrBits >> 5) & ~0b1111111);
            }

            // check for any NEC IR receive activity
            if (IrState == NEC_WAIT_BIT_END && IrCount == 32)
            {
                // check if it is a NON extended address and adjust if it is
                if ((IrBits >> 24) == ~((IrBits >> 16) & 0xff))
                    IrBits = (IrBits & 0x0000ffff) | ((IrBits >> 8) & 0x00ff0000);
                IrDevTmp = ((IrBits >> 16) & 0xffff);
                IrCmdTmp = ((IrBits >> 8) & 0xff);
            }
            if (IrCmdTmp != -1)
            {
                if (IrTick > IrTimeout)
                {
                    // this is a new keypress
                    IrTick = 0;
                    NextIrTick = 650;
                }
                if (IrTick == 0 || IrTick > NextIrTick)
                {
                    if (IrVarType & 0b01)
                        *(MMFLOAT *)IrDev = IrDevTmp;
                    else
                        *(long long int *)IrDev = IrDevTmp;
                    if (IrVarType & 0b10)
                        *(MMFLOAT *)IrCmd = IrCmdTmp;
                    else
                        *(long long int *)IrCmd = IrCmdTmp;
                    IrGotMsg = true;
                    NextIrTick += 250;
                }
                IrTimeout = IrTick + 150;
                IrReset();
            }
            IrTick++;
        }
        // === Period counter updates ===
        UPDATE_PER_INPUT(Option.INT1pin, INT1Count);
        UPDATE_PER_INPUT(Option.INT2pin, INT2Count);
        UPDATE_PER_INPUT(Option.INT3pin, INT3Count);
        UPDATE_PER_INPUT(Option.INT4pin, INT4Count);

        // === Frequency counter updates ===
        UPDATE_FREQ_INPUT(Option.INT1pin, INT1Timer, INT1InitTimer, INT1Count, INT1Value);
        UPDATE_FREQ_INPUT(Option.INT2pin, INT2Timer, INT2InitTimer, INT2Count, INT2Value);
        UPDATE_FREQ_INPUT(Option.INT3pin, INT3Timer, INT3InitTimer, INT3Count, INT3Value);
        UPDATE_FREQ_INPUT(Option.INT4pin, INT4Timer, INT4InitTimer, INT4Count, INT4Value);

        // === Second timer (heartbeat) ===
        if (++SecondsTimer >= 1000)
        {
            SecondsTimer -= 1000;
            // MMBasic doesn't use the heap but USB uses a bit and web functions use a lot
            // keep an occasional eye on heap usage so that we can check for the stack hitting the heap
            // used by TestStackOverflow()

#if !defined(PICOMITEWEB) && !defined(PICOMITEHDMIBTH)
            /* HDMIBTH: default Option.heartbeatpin=43 (GP25) is the
               CYW43 chip select line. Even though ExtCurrentConfig[43]
               should stay EXT_NOT_CONFIG (the gpio_init in External.c
               is gated off for HDMIBTH), keep the IRQ-context toggle
               out entirely — the heartbeat is driven from main thread
               via cyw43_arch_gpio_put in routinechecks(). */
            if (ExtCurrentConfig[PinDef[HEARTBEATpin].pin] == EXT_HEARTBEAT)
                gpio_xor_mask64((uint64_t)1 << PinDef[HEARTBEATpin].GPno);
#endif
        }

        return 1;
    }
    void __not_in_flash_func(uSec)(int us)
    {
#ifdef PICOMITEWEB
        if (us < 500)
        {
            busy_wait_us(us);
        }
        else
        {
            uint64_t end = time_us_64() + us;
            while (time_us_64() < end)
            {
                if (time_us_64() % 500 == 0)
                    ProcessWeb(1);
            }
        }
#else
    busy_wait_us(us);
#endif
    }
/* ProcessWeb() moved to net/WiFi.c */
    void __not_in_flash_func(CheckAbort)(void)
    {
#ifdef PICOMITEWEB
        ProcessWeb(1);
#endif
        routinechecks();
        if (MMAbort)
        {
            MMAbort = false;
            WDTimer = 0; // turn off the watchdog timer
            calibrate = 0;
            ShowCursor(false);
#ifdef PICOMITE
            if (mergerunning)
            {
                multicore_fifo_push_blocking(0xFF);
                busy_wait_ms(mergetimer + 200);
                if (mergerunning)
                {
                    SoftReset(SOFT_RESET);
                }
            }
#endif
            do_end(false);
#ifdef MMBASIC_FM
            if (fm_program_launched_from_fm)
            {
                CurrentLinePtr = NULL;
                cmdline = (unsigned char *)"";
            }
#endif
            longjmp(mark, 1); // jump back to the input prompt
        }
    }
    void PRet(void)
    {
        MMPrintString("\r\n");
    }
    void SRet(void)
    {
        SSPrintString("\r\n");
    }

    void PInt(int64_t n)
    {
        char s[20];
        IntToStr(s, (int64_t)n, 10);
        MMPrintString(s);
    }
    void SInt(int64_t n)
    {
        char s[20];
        IntToStr(s, (int64_t)n, 10);
        SSPrintString(s);
    }

    void SIntComma(int64_t n)
    {
        SSPrintString(", ");
        SInt(n);
    }

    void PIntComma(int64_t n)
    {
        MMPrintString(", ");
        PInt(n);
    }

    void PIntH(unsigned long long int n)
    {
        char s[20];
        IntToStr(s, (int64_t)n, 16);
        MMPrintString(s);
    }
    void PIntB(unsigned long long int n)
    {
        char s[65];
        IntToStr(s, (int64_t)n, 2);
        MMPrintString(s);
    }
    void PIntHC(unsigned long long int n)
    {
        MMPrintString(", ");
        PIntH(n);
    }
    void PIntBC(unsigned long long int n)
    {
        MMPrintString(", ");
        PIntB(n);
    }

    void PFlt(MMFLOAT flt)
    {
        char s[20];
        FloatToStr(s, flt, 4, 4, ' ');
        MMPrintString(s);
    }
    void PFltComma(MMFLOAT n)
    {
        MMPrintString(", ");
        PFlt(n);
    }
    void __attribute__((naked)) sigbus(void)
    {
        __asm volatile(
            "mrs r0, msp\n"
            "ldr r1, =sigbus_c\n"
            "bx r1\n");
    }
    void __no_inline_not_in_flash_func(sigbus_c)(uint32_t *frame)
    {
        char hex[] = "0123456789ABCDEF";
        /* Stacked exception frame: [r0 r1 r2 r3 r12 lr pc xpsr]. The pre-fault
           SP is just above the 8-word basic frame. */
        uint32_t pc = frame[6];
        uint32_t lr = frame[5];
        uint32_t sp = (uint32_t)frame + 0x20;
        /* Cortex-M33 fault status: CFSR bit 20 (UFSR STKOF) = stack overflow,
           MMFSR bits flag MemManage; HFSR bit 30 (FORCED) = escalated fault. */
        uint32_t cfsr = *(volatile uint32_t *)0xE000ED28;
        uint32_t hfsr = *(volatile uint32_t *)0xE000ED2C;

        if (Option.SerialConsole >= 1 && Option.SerialConsole <= 4)
        {
            /* Emit the fault dump by polling the console UART's TX FIFO
               directly. The normal console path (USB CDC / buffered IRQ output)
               is unreliable inside a hard-fault and previously produced garbled
               output that collided with the reboot banner. Register polling
               needs no interrupts, buffering or USB stack, so it survives. */
            uart_hw_t *hw = uart_get_hw(((Option.SerialConsole & 3) == 1) ? uart0 : uart1);
#define FAULT_PUTC(c)                                          \
    do                                                         \
    {                                                          \
        while (hw->fr & UART_UARTFR_TXFF_BITS)                 \
            tight_loop_contents();                             \
        hw->dr = (uint8_t)(c);                                 \
    } while (0)
#define FAULT_PUTS(str)                                        \
    do                                                         \
    {                                                          \
        const char *_p = (str);                               \
        while (*_p)                                            \
            FAULT_PUTC(*_p++);                                 \
    } while (0)
#define FAULT_PUTHEX(v)                                        \
    do                                                         \
    {                                                          \
        for (int _i = 28; _i >= 0; _i -= 4)                    \
            FAULT_PUTC(hex[((v) >> _i) & 0xF]);                \
    } while (0)
            FAULT_PUTS("\r\n*** FAULT PC=");
            FAULT_PUTHEX(pc);
            FAULT_PUTS(" LR=");
            FAULT_PUTHEX(lr);
            FAULT_PUTS(" SP=");
            FAULT_PUTHEX(sp);
            FAULT_PUTS(" CFSR=");
            FAULT_PUTHEX(cfsr);
            FAULT_PUTS(" HFSR=");
            FAULT_PUTHEX(hfsr);
            FAULT_PUTS("\r\n");
            /* wait for the line to fully shift out before we reset */
            while (hw->fr & UART_UARTFR_BUSY_BITS)
                tight_loop_contents();
#undef FAULT_PUTC
#undef FAULT_PUTS
#undef FAULT_PUTHEX
        }
        else
        {
            MMPrintString("PC=");
            for (int i = 28; i >= 0; i -= 4)
                putConsole(hex[(pc >> i) & 0xF], 0);
            MMPrintString(" LR=");
            for (int i = 28; i >= 0; i -= 4)
                putConsole(hex[(lr >> i) & 0xF], 0);
            MMPrintString("\r\n");
        }
        uSec(250000);
        disable_interrupts_pico();
        LoadOptions();
        if (Option.NoReset == 0)
        {
            Option.Autorun = 0;
            SaveOptions();
        }
        enable_interrupts_pico();
        memset(inpbuf, 0, STRINGSIZE);
        SoftReset(SOFT_RESET);
    }

#ifdef PICOMITEVGA
    int vgaloop1, vgaloop2, vgaloop4, vgaloop8, vgaloop16, vgaloop32;

#ifndef HDMI
/* PIO VGA (QVGA) core1 scanout (QVgaCore + QVgaLine1/PioInit/BufInit/
   DmaInit/Init) moved to graphics/VGA.c; launched from the core0 code below. */
    uint32_t core1stack[128];
/* HDMI/DVI core1 scanout (HDMICore + loops + DMA + resolution dispatch)
   moved to graphics/HDMI.c; launched from the core0 code below. */
#endif
#else
#ifdef PICOMITE
#include "pico/multicore.h"
void __not_in_flash_func(UpdateCore)()
{
    while (true)
    {
        // data memory barrier
        __dmb();
        if (multicore_fifo_rvalid())
        {
            int command = multicore_fifo_pop_blocking();
            if (command == 3)
            {
                uint8_t colour = (uint8_t)multicore_fifo_pop_blocking();
                uint32_t timer = (uint32_t)multicore_fifo_pop_blocking();
                uint64_t delaytime = 0;
                if (timer)
                    delaytime = time_us_64() + timer;
                mergerunning = true;
                while (1)
                {
                    if (multicore_fifo_rvalid())
                    {
                        int a;
                        if (((a = multicore_fifo_pop_blocking()) == 0xff))
                        {
                            mergerunning = false;
                            break;
                        }
                    }
                    if (timer)
                    {
                        busy_wait_until(delaytime);
                        delaytime = time_us_64() + timer;
                    }
                    merge(colour);
                }
            }
            else if (command == 2)
            {
                uint8_t colour = (uint8_t)multicore_fifo_pop_blocking();
                merge(colour);
            }
            else if (command == 4)
            {
                int x1 = multicore_fifo_pop_blocking();
                int y1 = multicore_fifo_pop_blocking();
                int w = multicore_fifo_pop_blocking();
                int h = multicore_fifo_pop_blocking();
                uint8_t colour = (uint8_t)multicore_fifo_pop_blocking();
                blitmerge(x1, y1, w, h, colour);
            }
            else if (command == 5)
            {
                int x1 = multicore_fifo_pop_blocking();
                int y1 = multicore_fifo_pop_blocking();
                int w = multicore_fifo_pop_blocking();
                int h = multicore_fifo_pop_blocking();
                uint8_t colour = (uint8_t)multicore_fifo_pop_blocking();
                uint32_t timer = (uint32_t)multicore_fifo_pop_blocking();
                uint64_t delaytime = 0;
                if (timer)
                    delaytime = time_us_64() + timer;
                mergerunning = true;
                while (1)
                {
                    if (multicore_fifo_rvalid())
                    {
                        int a;
                        if (((a = multicore_fifo_pop_blocking()) == 0xff))
                        {
                            mergerunning = false;
                            break;
                        }
                    }
                    if (timer)
                    {
                        busy_wait_until(delaytime);
                        delaytime = time_us_64() + timer;
                    }
                    blitmerge(x1, y1, w, h, colour);
                }
#if PICOMITERP2350
            }
            else if (command == 6)
            {
                int x_low = (int)multicore_fifo_pop_blocking();
                int y_low = (int)multicore_fifo_pop_blocking();
                int scrollStart = (int)multicore_fifo_pop_blocking();
                int x_high = x_low >> 16;
                x_low &= 0xFFFF;
                int y_high = y_low >> 16;
                y_low &= 0xFFFF;
                mutex_enter_blocking(&frameBufferMutex); // lock the frame buffer
                copybuffertoscreen(x_low, y_low, x_high, y_high, scrollStart);
                mutex_exit(&frameBufferMutex);
            }
            else if (command == 7)
            {
                int t = (int)multicore_fifo_pop_blocking();
                if (Option.DISPLAY_TYPE >= SSD1963_5_12BUFF)
                {
                    // SSD1963 buffered displays use 2x upscaling:
                    // VRes is half the display resolution, so scale scroll value
                    WriteComand(CMD_SET_SCROLL_START);
                    WriteData((t * 2) >> 8);
                    WriteData((t * 2) & 0xFF);
                }
                else
                {
                    spi_write_command(CMD_SET_SCROLL_START);
                    spi_write_data(t >> 8);
                    spi_write_data(t);
                }
#endif
            }
            else if (command == 1)
            {
                uint8_t *s = (uint8_t *)multicore_fifo_pop_blocking();
                mutex_enter_blocking(&frameBufferMutex); // lock the frame buffer
                copyframetoscreen(s, 0, HRes - 1, 0, VRes - 1, 0);
                mergedone = true;
                mutex_exit(&frameBufferMutex);
            }
        }
    }
}
uint32_t core1stack[512];
#endif
#endif
#ifndef rp2350
    void __no_inline_not_in_flash_func(modclock)(uint16_t speed)
    {
        ssi_hw->ssienr = 0;
        ssi_hw->baudr = 0;
        ssi_hw->baudr = speed;
        ssi_hw->ssienr = 1;
    }
#else
#ifndef PICOMITEWEB
uint32_t testPSRAM(void)
{
    uint32_t *p = (uint32_t *)PSRAMbase;
    uint32_t *q = (uint32_t *)PSRAMbase;
    for (int i = 0; i < 65536; i++)
        *p++ = i;
    __dmb();
    for (int i = 0; i < 65536; i++)
        if (*q++ != i)
            return 0;
    p = (uint32_t *)PSRAMbase;
    q = (uint32_t *)PSRAMbase;
    p[8 * 1024 * 1024 / 4 - 1] = 0x12345678;
    __dmb();
    if (q[8 * 1024 * 1024 / 4 - 1] == 0x12345678)
        return 6 * 1024 * 1024;
    else
        return 0;
}
#endif
#endif
    lfs_t lfs;
    lfs_dir_t lfs_dir;
    struct lfs_info lfs_info;
    void MIPS16 updatebootcount(bool format)
    {
        lfs_file_t lfs_file;
        pico_lfs_cfg.block_count = (Option.FlashSize - RoundUpK4(TOP_OF_SYSTEM_FLASH) - (Option.modbuff ? 1024 * Option.modbuffsize : 0)) / 4096;
        int err, boot_count = 0;
        if (format)
            err = true;
        else
            err = lfs_mount(&lfs, &pico_lfs_cfg);
        // reformat if we can't mount the filesystem
        // this should only happen on the first boot
        if (err)
        {
            MMPrintString("Formatting the A: drive\r\n");
            err = lfs_format(&lfs, &pico_lfs_cfg);
            err = lfs_mount(&lfs, &pico_lfs_cfg);
            ResetFlashStorage(1);
        }

        err = lfs_file_open(&lfs, &lfs_file, "bootcount", LFS_O_RDWR | LFS_O_CREAT);
        ;
        int dt = get_fattime();
        err = lfs_setattr(&lfs, "bootcount", 'A', &dt, 4);
        err = lfs_file_read(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
        ;
        boot_count += 1;
        err = lfs_file_rewind(&lfs, &lfs_file);
        err = lfs_file_write(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
        err = lfs_file_close(&lfs, &lfs_file);
    }
    /**
     * @brief Transforms input beginning with * into a corresponding RUN command.
     *
     * e.g.
     *   *foo              =>  RUN "foo"
     *   *"foo bar"        =>  RUN "foo bar"
     *   *foo --wombat     =>  RUN "foo", "--wombat"
     *   *foo "wom"        =>  RUN "foo", Chr$(34) + "wom" + Chr$(34)
     *   *foo "wom" "bat"  =>  RUN "foo", Chr$(34) + "wom" + Chr$(34) + " " + Chr$(34) + "bat" + Chr$(34)
     *   *foo --wom="bat"  =>  RUN "foo", "--wom=" + Chr$(34) + "bat" + Chr$(34)
     */
    static void MIPS16 transform_star_command(char *input)
    {
        char *src = input;
        while (isspace((uint8_t)*src))
            src++; // Skip leading whitespace.
        if (*src != '*')
            error("Internal fault");
        src++;

        // Trim any trailing whitespace from the input.
        char *end = input + strlen(input) - 1;
        while (isspace((uint8_t)*end))
            *end-- = '\0';

        // Allocate extra space to avoid string overrun.
        char *tmp = (char *)GetTempMemory(STRINGSIZE + 32);
        strcpy(tmp, "RUN");
        char *dst = tmp + 3;

        if (*src == '"')
        {
            // Everything before the second quote is the name of the file to RUN.
            *dst++ = ' ';
            *dst++ = *src++; // Leading quote.
            while (*src && *src != '"')
                *dst++ = *src++;
            if (*src == '"')
                *dst++ = *src++; // Trailing quote.
        }
        else
        {
            // Everything before the first space is the name of the file to RUN.
            int count = 0;
            while (*src && !isspace((uint8_t)*src))
            {
                if (++count == 1)
                {
                    *dst++ = ' ';
                    *dst++ = '\"';
                }
                *dst++ = *src++;
            }
            if (count)
                *dst++ = '\"';
        }

        while (isspace((uint8_t)*src))
            src++; // Skip whitespace.

        // Anything else is arguments.
        if (*src)
        {
            *dst++ = ',';
            *dst++ = ' ';

            // If 'src' starts with double-quote then replace with: Chr$(34) +
            if (*src == '"')
            {
                memcpy(dst, "Chr$(34) + ", 11);
                dst += 11;
                src++;
            }

            *dst++ = '\"';

            // Copy from 'src' to 'dst'.
            while (*src)
            {
                if (*src == '"')
                {
                    // Close current set of quotes to insert a Chr$(34)
                    memcpy(dst, "\" + Chr$(34)", 12);
                    dst += 12;

                    // Open another set of quotes unless this was the last character.
                    if (*(src + 1))
                    {
                        memcpy(dst, " + \"", 4);
                        dst += 4;
                    }
                    src++;
                }
                else
                {
                    *dst++ = *src++;
                }
                if (dst - tmp >= STRINGSIZE)
                    error("String too long");
            }

            // End with a double quote unless 'src' ended with one.
            if (*(src - 1) != '"')
                *dst++ = '\"';

            *dst = '\0';
        }

        if (dst - tmp >= STRINGSIZE)
            error("String too long");

        // Copy transformed string back into the input buffer.
        memcpy(input, tmp, STRINGSIZE);
        input[STRINGSIZE - 1] = '\0';

        ClearSpecificTempMemory(tmp);
    }
/* WiFi/web runtime (web_async_*, wifi_country_from_string, WebConnect,
   TLS/NTP) moved to net/WiFi.c */

#ifdef HDMI
    /* Runtime resolution switch for the HDMI builds (no reboot). Only the
       HSTX source clock, timing and scanout loop change — all redone by
       HDMICore. On the full HDMI/HDMIUSB builds the resolutions run at
       different CPU speeds: the caller (cmd_resolution) must have already
       moved clk_sys to the new resolution's speed via CPUSpeedRuntime()
       BEFORE calling this, because HDMICore derives clk_hstx from the
       clk_sys it observes when it rebuilds. (The cut-down HDMIBTH/HDMIWEB
       resolutions both run at 252 MHz, so no clock change there.)
       Rather than reset core1 and fight the chained DMA from core0, we ask
       the scanout loop (running on core1) to return via hdmi_switch_pending;
       core1 then tears down its own DMA/HSTX and rebuilds for the new
       resolution. Call from core0; the caller does the subsequent
       setmode()/redraw. */
    void restartHDMI(int newres)
    {
        HDMIres = newres;           // canonical live value, survives LoadOptions
        Option.Resolution = newres; // read by HDMICore once it re-enters setup
        __dmb();
        hdmi_switch_pending = true;
        __dmb();
        /* Wait (bounded) for core1 to acknowledge — it clears the flag once
           the new mode is fully live. The timeout must exceed core1's mid-
           switch blank hold (HDMI_BLANK_US) plus rebuild time. */
        uint64_t deadline = time_us_64() + HDMI_BLANK_US + 300000;
        while (hdmi_switch_pending && time_us_64() < deadline)
            tight_loop_contents();
        uSec(3000); // small settle before setmode() repaints
    }
#endif
#if defined(PICOMITEWEB) && defined(rp2350)
    /* ---- core0 stack high-water diagnostics (WEBRP2350 / HDMIWEB) ----
       Validates the PICO_CORE0_STACK_SIZE headroom against the real
       WiFi/TLS peak. The core0 stack grows DOWN from __StackTop (top of
       SCRATCH_Y) through SCRATCH_Y + the free part of SCRATCH_X, then would
       spill into main RAM only past ~8 KB. StackPaintCore0() (called first
       thing in main) fills the unused region below the live frame with a
       sentinel; StackPeakBytes() later scans up from the lowest free address
       to the first word the stack actually touched. Painting starts at
       __scratch_x_end__ so it never disturbs any .scratch_x content.
       Surfaced via MM.INFO(STACKPEAK) = peak bytes used. */
#define STACK_PAINT_WORD 0x5A5A5A5Au
    extern uint32_t __scratch_x_end__; /* lowest free stack address */
    extern uint32_t __StackTop;        /* stack base (highest address) */
    static void StackPaintCore0(void)
    {
        register uint32_t sp asm("sp");
        uint32_t *lo = &__scratch_x_end__;
        uint32_t *hi = (uint32_t *)((sp - 512u) & ~3u); /* clear of the live frame */
        while (lo < hi)
            *lo++ = STACK_PAINT_WORD;
    }
    uint32_t StackPeakBytes(void)
    {
        uint32_t *p = &__scratch_x_end__;
        while (p < &__StackTop && *p == STACK_PAINT_WORD)
            p++;
        return (uint32_t)((char *)&__StackTop - (char *)p);
    }
#endif

#if !defined(PICOMITEVGA) || defined(HDMI)
    /* ------------------------------------------------------------------
       Change the system clock (PLL_SYS) at runtime, without rebooting.

       This mirrors the boot-time ordering in main(): when raising the
       clock the core voltage and the flash (QMI) timing must be made
       safe for the *target* speed BEFORE clk_sys moves; when lowering,
       the clock is dropped first and voltage relaxed afterwards.
       Everything that is derived from clk_sys / clk_peri (the busy-wait
       tick base, ADC clock, hardware-UART baud divisors, the cyw43 SPI
       PIO divider and the backlight PWM wrap) is re-derived afterwards.

       Gated out of the VGA builds: there clk_sys is the pixel-clock
       source and the display is hard-locked to a fixed CPU frequency.
       The HDMI builds DO compile this — not for a user CPU SPEED command
       (still blocked: clk_sys is the HSTX source) but for the live
       RESOLUTION switch, which moves clk_sys to the new resolution's
       speed while core1 has the scanout torn down/glitching and then
       lets HDMICore re-derive clk_hstx from the new clk_sys. The
       cut-down HDMIBTH/HDMIWEB builds compile it too: their 640x480x8
       supports the same 252/315/378 MHz speed options as the full
       build's 640x480.

       Returns 0 on success, 1 if `speed` (kHz) is not a realisable PLL
       configuration. Does NOT persist the change (no SaveOptions) - a
       reboot returns to the configured OPTION CPUSPEED. */
    /* Move the core regulator setting to `target` one enum step at a time,
       letting each step settle. The vreg_voltage enum values are contiguous
       integers, so stepping by 1 walks the discrete voltage levels in order
       (each <=100 mV apart). A single large write (e.g. 1.60 V -> 1.15 V) can
       make the on-chip regulator under/overshoot and brown the core out; a
       stepped ramp avoids that. Harmless on boards with an external fixed
       DVDD - the register tracks what we set but the rail ignores it. */
    static void ramp_core_voltage(enum vreg_voltage target)
    {
        vreg_disable_voltage_limit();
        int cur = (int)vreg_get_voltage();
        int t = (int)target;
        while (cur != t)
        {
            cur += (t > cur) ? 1 : -1;
            vreg_set_voltage((enum vreg_voltage)cur);
            sleep_ms(2);
        }
    }

    int CPUSpeedRuntime(uint32_t speed)
    {
        uint vco, postdiv1, postdiv2;
        if (!check_sys_clock_khz(speed, &vco, &postdiv1, &postdiv2))
            return 1;
        uint32_t old = Option.CPU_Speed;
        if (speed == old)
            return 0;
        int raising = (speed > old);

        /* Target core voltage for the new speed (same thresholds as boot). */
        enum vreg_voltage v;
        if (speed <= 200000)
            v = VREG_VOLTAGE_1_15;
        else if (speed <= 320000)
            v = VREG_VOLTAGE_1_30;
#ifdef rp2350
        else
            v = VREG_VOLTAGE_1_60;
#else
        else
            v = VREG_VOLTAGE_1_30;
#endif

        /* When raising: bring the voltage up (stepped, with settle) while
           interrupts are still live - the ramp must not stall USB/timer
           servicing, and the core must be at the higher voltage before the
           clock is raised. */
        if (raising)
            ramp_core_voltage(v);

        /* The actual PLL switch is the only part that must be atomic w.r.t.
           XIP flash timing, and it lasts only microseconds. set_sys_clock_pll
           parks clk_sys on clk_ref while it reprograms PLL_SYS, so the brief
           masked window cannot starve anything important. */
        uint32_t irqs = save_and_disable_interrupts();
#ifdef rp2350
        /* The pre-switch QMI timing is written while clk_sys is still at the
           OLD speed and must remain valid through the switch, so it has to be
           conservative for whichever clock is faster - max(old, new). Using
           the target speed here would, when lowering from a high clock,
           relax CLKDIV (e.g. to /2) while still running fast, pushing the
           flash clock past spec and faulting the next XIP fetch. The relaxed
           timing for the new (lower) speed is applied only AFTER the switch,
           in the second block below. */
        uint32_t preswitch = (old > speed) ? old : speed;
        pads_qspi_hw->io[0] = 0x67;
        pads_qspi_hw->io[1] = 0x67;
        pads_qspi_hw->io[2] = 0x67;
        pads_qspi_hw->io[3] = 0x6B;
        pads_qspi_hw->io[4] = 0x6B;
        pads_qspi_hw->io[5] = 0x6B;
        if (preswitch <= 288000)
            qmi_hw->m[0].timing = 0x40006202; // CLKDIV=2
        else
            qmi_hw->m[0].timing = 0x40006204; // CLKDIV=4
        busy_wait_us(2);
#endif
        set_sys_clock_khz(speed, false);
#ifdef rp2350
        /* set_sys_clock can rewrite the QSPI pads, so restore them and the
           (now possibly relaxed) timing once more - matches boot. */
        pads_qspi_hw->io[0] = 0x67;
        pads_qspi_hw->io[1] = 0x67;
        pads_qspi_hw->io[2] = 0x67;
        pads_qspi_hw->io[3] = 0x6B;
        pads_qspi_hw->io[4] = 0x6B;
        pads_qspi_hw->io[5] = 0x6B;
        if (speed <= 288000)
            qmi_hw->m[0].timing = 0x40006202;
        else
            qmi_hw->m[0].timing = 0x40006204;
        busy_wait_us(2);
#endif

        /* Re-tune the cyw43 gSPI link to the NEW clk_sys *before* interrupts
           (and therefore any gSPI transaction) resume. The SDK only applies
           the divider in cyw43_spi_init(), so cyw43_set_pio_clkdiv_int_frac8()
           alone updates the stored value but leaves the LIVE PIO state machine
           on the old divider - at the new clk_sys that shifts SCK out of range
           and permanently desyncs the link (hdr mismatch / ioctl timeout).
           cyw43 owns the highest PIO exclusively and hidden from MMBasic (pio2
           on RP2350, pio1 on the RP2040 WiFi build - see Custom.c), so
           retuning every SM on it is safe, and doing it inside the masked
           window guarantees the SM is idle (no transfer mid-flight). */
#if defined(PICOMITEBT)
        uint32_t cyw43_div = (speed + 79999) / 80000;
#elif defined(PICOMITEWEB) || defined(PICOMITEBTH)
        uint32_t cyw43_div = (speed + 99999) / 100000;
#endif
#if defined(PICOMITEBT) || defined(PICOMITEWEB) || defined(PICOMITEBTH)
        if (cyw43_div < 2)
            cyw43_div = 2;
        cyw43_set_pio_clkdiv_int_frac8(cyw43_div, 0); // keep the stored value coherent
#ifdef rp2350
        PIO cyw43_pio = pio2;
#else
        PIO cyw43_pio = pio1;
#endif
        for (uint sm = 0; sm < 4; sm++)
            pio_sm_set_clkdiv_int_frac8(cyw43_pio, sm, cyw43_div, 0);
        pio_clkdiv_restart_sm_mask(cyw43_pio, 0xf);
#endif
        restore_interrupts(irqs);

        /* When lowering: drop the voltage (stepped) only after the clock is
           down, so the core is never under-volted for the speed it is
           running at. */
        if (!raising)
            ramp_core_voltage(v);

        /* Publish the new speed before re-deriving anything that reads it
           (ADC_CLK_SPEED and the cyw43/PWM/baud helpers all key off
           Option.CPU_Speed). */
        Option.CPU_Speed = speed;

        /* Busy-wait tick base used by uSec()/bit-banged protocols/soft-UART. */
        ticks_per_second = speed * 1000;
        systick_hw->csr = 0x5;
        systick_hw->rvr = 0x00FFFFFF;

        /* ADC clock is sourced from PLL_SYS; keep it proportional to the new
           CPU speed exactly as boot does. The ADC's own sample divider
           (adc_hw->div / adc_clk_div) is left untouched, so any user ADC
           rate set with the ADC commands survives. */
        clock_configure(
            clk_adc,
            0,
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
            speed * 1000,
            ADC_CLK_SPEED);

        /* clk_peri followed clk_sys, so every open hardware UART needs its
           divisor recomputed for the new peripheral clock. */
        if (Option.SerialConsole)
            uart_set_baudrate((Option.SerialConsole & 3) == 1 ? uart0 : uart1, Option.Baudrate);
        if (Option.GPSTX)
            uart_set_baudrate((PinDef[Option.GPSTX].mode & UART0TX) ? uart0 : uart1, Option.GPSBaud);
        if (com1)
            uart_set_baudrate(uart0, com1_baud);
        if (com2)
            uart_set_baudrate(uart1, com2_baud);

#ifndef PICOMITEVGA
        /* Backlight PWM wrap is derived from CPU_Speed. (The VGA/HDMI
           builds have no LCD backlight — setBacklight is not compiled.) */
        if (Option.BackLightLevel)
            setBacklight(Option.BackLightLevel, 0);
#endif

        /* Re-derive the audio PWM wrap / I2S divider for the active rate so a
           sound playing across the speed change keeps its pitch. */
        ResetAudioRate();

        /* Shadow of the live clk_sys speed (kHz). The error routine reloads
           the Option struct from flash, which would silently revert
           Option.CPU_Speed to the stored value while the PLL is still at
           the runtime speed — every later derivation (UART baud, SPI/I2C
           dividers, PWM wraps) would then be computed from the wrong
           frequency. ReloadOptionsKeepLive() re-asserts this value when
           non-zero. */
        LiveCPUSpeed = speed;

        return 0;
    }
#endif /* !PICOMITEVGA || full-HDMI */

    int MIPS16 main()
    {
#if defined(PICOMITEWEB) && defined(rp2350)
        StackPaintCore0(); /* must run before any deep call chain */
#endif
        static int ErrorInPrompt;
        int i = 0;
        char savewatchdog = false;
        /* Publish the CFunction CallTable address in reserved Cortex-M vector
           slot 7 (VTOR+0x1C). Lets CSUBs locate the CallTable via the
           architectural VTOR register (0xE000ED08) with no chip- or
           build-specific flash address. See PicoCFunctions.h / armcfgen. */
        ((volatile uint32_t *)(*(volatile uint32_t *)0xE000ED08))[7] = (uint32_t)CallTable;
        i = watchdog_caused_reboot();
#ifdef rp2350
        restart_reason = powman_hw->chip_reset | i;
        rp2350a = (*((io_ro_32 *)(SYSINFO_BASE + SYSINFO_PACKAGE_SEL_OFFSET)) & 1);
#else
    restart_reason = vreg_and_chip_reset_hw->chip_reset | i;
#endif
        if (_excep_code == SOFT_RESET || _excep_code == SCREWUP_TIMEOUT)
            restart_reason = 0xFFFFFFFF;
        if ((_excep_code == WATCHDOG_TIMEOUT) & i)
            restart_reason = 0xFFFFFFFE;
        if ((_excep_code == POSSIBLE_WATCHDOG) & i)
            restart_reason = 0xFFFFFFFD;
        LoadOptions();
#ifdef rp2350
        if (rom_get_last_boot_type() == BOOT_TYPE_FLASH_UPDATE)
            restart_reason = 0xFFFFFFFC;
#else
    if (restart_reason == 0x10001 || restart_reason == 0x101)
        restart_reason = 0xFFFFFFFC;
#endif
        uint32_t excep = _excep_code;
        if (Option.Baudrate == 0 ||
            !(Option.Tab == 2 || Option.Tab == 3 || Option.Tab == 4 || Option.Tab == 8) ||
            !(Option.Autorun >= 0 && Option.Autorun <= MAXFLASHSLOTS + 1) ||
            Option.CPU_Speed < MIN_CPU || Option.CPU_Speed > MAX_CPU ||
            Option.PROG_FLASH_SIZE != MAX_PROG_SIZE ||
#if !(defined(PICOMITEWEB) || defined(PICOMITEBT) || defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH))
            /* CYW43 builds legitimately run with heartbeatpin==0 and
               NoHeartbeat==0 (LED on the wireless chip), so this pair
               can't be used as a corruption check there. */
            (Option.heartbeatpin == 0 && Option.NoHeartbeat == 0) ||
#endif
            !(Option.Magic == MagicKey))
        {
            ResetAllFlash(); // init the options if this is the very first startup
            _excep_code = 0;
            watchdog_enable(1, 1);
            while (1)
                ;
        }
#ifndef HDMI
        if (Option.VGA_HSYNC == 0)
        {
            Option.VGA_HSYNC = 21;
            Option.VGA_BLUE = 24;
            SaveOptions();
        }
#else
#ifdef HDMICUTDOWN
    /* HDMIBTH/HDMIWEB store CPU_Speed independently of the resolution
       (R640x480x8 doesn't encode a speed the way the full build's
       f252/f315/f378 enums do), so validate the pair rather than
       unconditionally overwriting the speed: doing the latter here
       silently undid OPTION RESOLUTION 640,378000 on the reboot. */
    if (!((Option.Resolution == R640x480x8 && (Option.CPU_Speed == Freq252P || Option.CPU_Speed == Freq480P || Option.CPU_Speed == Freq378P)) ||
          (Option.Resolution == R720x400x8 && Option.CPU_Speed == Freq400) ||
          (Option.Resolution == R1024x600 && Option.CPU_Speed == FreqX)))
    {
        Option.Resolution = R640x480x8; // factory display defaults
        Option.CPU_Speed = Freq480P;
        SaveOptions();
    }
#else
    if (!(Option.Resolution == R1280x720 || Option.Resolution == R640x480f378 || Option.Resolution == R640x480f252 || Option.Resolution == R848x480 || Option.Resolution == R720x400 || Option.Resolution == R800x600 || Option.Resolution == R640x480f315 || Option.Resolution == R1024x768 || Option.Resolution == R1024x600 || Option.Resolution == R800x480))
    {
        Option.CPU_Speed = Freq480P;
        SaveOptions();
    }
#endif
#endif
        m_alloc(M_PROG); // init the variables for program memory
        LibMemory = (uint8_t *)flash_libmemory;
        uSec(100);
        if (_excep_code == RESET_CLOCKSPEED)
        {
#ifdef PICOMITEVGA
#ifdef HDMI
#ifdef HDMICUTDOWN
            Option.Resolution = R640x480x8; // recover to the factory display defaults as a pair —
                                            // Freq480P alone is not valid for R1024x600
#endif
            Option.CPU_Speed = Freq480P; // init the options if this is the very first startup
#else
            Option.CPU_Speed = Freq252P; // init the options if this is the very first startup
#endif
#else
        Option.CPU_Speed = 200000; // init the options if this is the very first startup
#endif
            SaveOptions();
            SoftReset(INVALID_CLOCKSPEED);
            while (1)
                ;
        }
        else
        {
            _excep_code = RESET_CLOCKSPEED;
            watchdog_enable(1000, 1);
        }
#ifdef rp2350
        if (!rp2350a)
        {
            if (!Option.AllPins)
            {
                Option.AllPins = true;
                SaveOptions();
            }
        }
#endif
        vreg_disable_voltage_limit();
        if (Option.CPU_Speed <= 200000)
            vreg_set_voltage(VREG_VOLTAGE_1_15);
        else if (Option.CPU_Speed > 200000 && Option.CPU_Speed <= 320000)
            vreg_set_voltage(VREG_VOLTAGE_1_30); // Std default @ boot is 1_10
#ifdef rp2350
        else if (Option.CPU_Speed > 320000)
            vreg_set_voltage(VREG_VOLTAGE_1_60); // Std default @ boot is 1_10
#else
    else
        vreg_set_voltage(VREG_VOLTAGE_1_30);
#endif
        sleep_ms(10);
#ifdef rp2350
        pads_qspi_hw->io[0] = 0x67;
        pads_qspi_hw->io[1] = 0x67;
        pads_qspi_hw->io[2] = 0x67;
        pads_qspi_hw->io[3] = 0x6B;
        pads_qspi_hw->io[4] = 0x6B;
        pads_qspi_hw->io[5] = 0x6B;
        if (Option.CPU_Speed <= 288000)
            qmi_hw->m[0].timing = 0x40006202; // COOLDOWN=1, RXDELAY=2, MIN_DESELECT=6, CLKDIV=2
        else
            qmi_hw->m[0].timing = 0x40006204; // COOLDOWN=1, RXDELAY=2, MIN_DESELECT=6, CLKDIV=4
        sleep_ms(2);
#endif
        set_sys_clock_khz(Option.CPU_Speed, false);
// NB: set_sys_clock can change the pad configuration so we need to redo it
#ifdef rp2350
        pads_qspi_hw->io[0] = 0x67;
        pads_qspi_hw->io[1] = 0x67;
        pads_qspi_hw->io[2] = 0x67;
        pads_qspi_hw->io[3] = 0x6B;
        pads_qspi_hw->io[4] = 0x6B;
        pads_qspi_hw->io[5] = 0x6B;
        if (Option.CPU_Speed <= 288000)
            qmi_hw->m[0].timing = 0x40006202; // COOLDOWN=1, RXDELAY=2, MIN_DESELECT=6, CLKDIV=2
        else
            qmi_hw->m[0].timing = 0x40006204; // COOLDOWN=1, RXDELAY=2, MIN_DESELECT=6, CLKDIV=4
        sleep_ms(2);
#endif
        PWM_FREQ = 44100;
        pico_get_unique_board_id_string(id_out, 12);
#ifdef rp2350
        if (Option.PSRAM_CS_PIN)
        {
            PSRAMpin = PinDef[Option.PSRAM_CS_PIN].GPno;
            psram_setup();
            if (!(PSRAMsize = psram_size()))
            {
                Option.PSRAM_CS_PIN = 0;
                SaveOptions();
            }
            else
                PSRAMsize -= 2 * 1024 * 1024;
        }
#endif
        if (clock_get_hz(clk_usb) != 48000000)
        {
            ResetAllFlash(); // init the options if this is the very first startup
            SoftReset(INVALID_CLOCKSPEED);
            while (1)
                ;
        }
        clock_configure(
            clk_adc,
            0,                                                // No glitchless mux
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
            Option.CPU_Speed * 1000,                          // Input frequency
            ADC_CLK_SPEED                                     // Output (must be same as no divider)
        );
        SetADCFreq(500000);
        adc_clk_div = adc_hw->div;
        systick_hw->csr = 0x5;
        systick_hw->rvr = 0x00FFFFFF;
#ifdef PICOMITE
        mutex_init(&frameBufferMutex); // create a mutex to lock frame buffer
#endif

#ifndef rp2350
        if (Option.CPU_Speed <= 200000)
            modclock(2);
#else
#if PICOMITERP2350
    if (Option.DISPLAY_TYPE >= VGA222 && Option.DISPLAY_TYPE < NEXTGEN)
    {
        int mapsize = display_details[Option.DISPLAY_TYPE].vertical * display_details[Option.DISPLAY_TYPE].bits * sizeof(uint32_t);
        int framesize = (display_details[Option.DISPLAY_TYPE].horizontal / 5) * sizeof(uint32_t) * display_details[Option.DISPLAY_TYPE].vertical;
        framebuffersize = mapsize + framesize;
        heap_memory_size -= MRoundUp(framebuffersize); // keep heap top 256-aligned (top-down GetMemory relies on it)
        FRAMEBUFFER = (uint8_t *)(AllMemory + heap_memory_size + 256);
        g_vgalinemap = (uint32_t *)(AllMemory + heap_memory_size + 256 + framesize);
    }
    if (Option.DISPLAY_TYPE >= NEXTGEN)
    { // adjust the size of the heap
        framebuffersize = display_details[Option.DISPLAY_TYPE].horizontal * display_details[Option.DISPLAY_TYPE].vertical;
        heap_memory_size -= MRoundUp(framebuffersize); // keep heap top 256-aligned
        FRAMEBUFFER = AllMemory + heap_memory_size + 256;
    }
#endif
#ifdef HDMI
#ifndef HDMICUTDOWN
    /* clk_hstx is no longer configured here: HDMICore sets it explicitly on
       every (re)entry of its setup — required for the live RESOLUTION
       switch and identical at cold boot.
       The per-resolution heap/framebuffer resize blocks below stay gated
       out of HDMIBTH/HDMIWEB: those builds are locked to 1024x600/640x480
       and the +320*240*2 literal in the resize formula would overrun the
       96000-byte pool that FRAMEBUFFER_POOL_SIZE allocates. These three
       resolutions need a framebuffer bigger than the default 153600-byte
       pool, which is why a live RESOLUTION switch INTO them is refused
       (cmd_resolution checks against framebuffersize) — only OPTION
       RESOLUTION plus this boot-time resize can enable them. */
    if (Option.Resolution == R800x600)
    { // adjust the size of the heap
        framebuffersize = 400 * 300 * 2;
        heap_memory_size = HEAP_MEMORY_SIZE - MRoundUp(framebuffersize) + 320 * 240 * 2;
        FRAMEBUFFER = AllMemory + heap_memory_size + 256;
    }
    if (Option.Resolution == R848x480)
    { // adjust the size of the heap
        framebuffersize = 424 * 240 * 2;
        heap_memory_size = HEAP_MEMORY_SIZE - MRoundUp(framebuffersize) + 320 * 240 * 2;
        FRAMEBUFFER = AllMemory + heap_memory_size + 256;
    }
    if (Option.Resolution == R800x480)
    { // adjust the size of the heap
        framebuffersize = 400 * 240 * 2;
        heap_memory_size = HEAP_MEMORY_SIZE - MRoundUp(framebuffersize) + 320 * 240 * 2;
        FRAMEBUFFER = AllMemory + heap_memory_size + 256;
    }
#endif /* !HDMICUTDOWN */
#endif

#endif
#ifdef PICOMITE
        if (IS_VIRTUAL_DISPLAY(Option.DISPLAY_TYPE))
        {
            int framebuffersize = 320 * 240 / 2;
            heap_memory_size -= MRoundUp(framebuffersize); // keep heap top 256-aligned
            FRAMEBUFFER = (uint8_t *)(AllMemory + heap_memory_size + 256);
        }
#endif
#ifdef GUICONTROLS
        // Allocate GUI controls from top of heap if enabled
        if (Option.MaxCtrls > 0)
        {
            int ctrlsSize = Option.MaxCtrls * sizeof(struct s_ctrl);
            // Round the carve-out up to a page so the heap top stays 256-aligned.
            // sizeof(s_ctrl) (52) is not a 256-multiple, so an unrounded subtraction
            // here offsets every top-down GetMemory() block by ctrlsSize%256 and
            // breaks VARADDR 8-alignment (MEMORY PACK/POKE INTEGER "not divisible by 8").
            heap_memory_size -= MRoundUp(ctrlsSize);
            Ctrl = (struct s_ctrl *)(AllMemory + heap_memory_size + 256);
            memset(Ctrl, 0, ctrlsSize);
        }
#endif
        uSec(100);
        hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
        _excep_code = excep;
#ifdef PICOMITEVGA
#ifndef HDMI
        if (Option.Resolution == R640x480f252 || Option.Resolution == R640x480f315 || Option.Resolution == R848x480 || Option.Resolution == R720x400 || Option.Resolution == R800x600)
            QVGA_CLKDIV = 2;
        else if (Option.Resolution == R640x480f378)
            QVGA_CLKDIV = 3;
        else
            QVGA_CLKDIV = 1;
#ifdef rp2350
        if (Option.Resolution == R848x480)
        { // adjust the size of the heap
            framebuffersize = 424 * 240 * 2;
            heap_memory_size = HEAP_MEMORY_SIZE - MRoundUp(framebuffersize) + 320 * 240 * 2;
            FRAMEBUFFER = AllMemory + heap_memory_size + 256;
            MODE1SIZE = MODE1SIZE_8;
            MODE2SIZE = MODE2SIZE_8;
            MODE2SIZE = MODE2SIZE_8;
            MODE2SIZE = MODE2SIZE_8;
            MODE2SIZE = MODE2SIZE_8;
            HRes = 848;
        }
        if (Option.Resolution == R800x600)
        { // adjust the size of the heap
            framebuffersize = 400 * 300 * 2;
            heap_memory_size = HEAP_MEMORY_SIZE - MRoundUp(framebuffersize) + 320 * 240 * 2;
            FRAMEBUFFER = AllMemory + heap_memory_size + 256;
            MODE1SIZE = MODE1SIZE_V;
            MODE2SIZE = MODE2SIZE_V;
            MODE3SIZE = MODE3SIZE_V;
            MODE5SIZE = MODE5SIZE_V;
            HRes = 800;
            VRes = 600;
        }

#endif
#endif
#endif
        systick_hw->csr = 0x5;
        systick_hw->rvr = 0x00FFFFFF;
        ticks_per_second = Option.CPU_Speed * 1000;
        // The serial clock won't vary from this point onward, so we can configure
        // the UART etc.
#if !defined(USBKEYBOARD) && !defined(PICOMITEBT)
        stdio_set_translate_crlf(&stdio_usb, false);
#endif
        LoadOptions();
        stdio_init_all();
        adc_init();
        adc_set_temp_sensor_enabled(true);
        mSecTimer = time_us_64() / 1000;
        add_repeating_timer_us(-1000, timer_callback, NULL, &timer);
#if PICOCALC
        TestPicoCalc(); // Test if firmware is running on PicoCalc
#endif
        InitReservedIO();
        ClearExternalIO();
        ConsoleRxBufHead = 0;
        ConsoleRxBufTail = 0;
        ConsoleTxBufHead = 0;
        ConsoleTxBufTail = 0;
        PromptFC = gui_fcolour = Option.DefaultFC;
        PromptBC = gui_bcolour = Option.DefaultBC;
        InitHeap(true); // initilise memory allocation
        uSecFunc(1000);
        disable_interrupts_pico();
        enable_interrupts_pico();
        mSecTimer = time_us_64() / 1000;
        DISPLAY_TYPE = Option.DISPLAY_TYPE;
        // negative timeout means exact delay (rather than delay between callbacks)
        OptionErrorSkip = false;
#ifdef PICOMITEBT
        /* No CDC peer to wait for on BT. The actual cyw43/btstack init is
           done later (same place WEB calls cyw43_arch_init), once the
           rest of the hardware is up. Boot output is buffered in the BT
           TX ring until then. */
        uSec(1000000);
        initKeyboard();
#elif defined(PICOMITEBTH)
    /* BTH still waits for the USB CDC console peer like the non-USB-
       host PICOMITE branch — but does NOT call initKeyboard() because
       PS/2 keyboards aren't supported on this variant. Keyboard input
       comes via the BLE-HID-host path (BTKeyboard.c). */
    if (!(Option.SerialConsole == 1 || Option.SerialConsole == 2) || Option.Telnet == -1)
    {
        uint64_t t = time_us_64();
        while (1)
        {
            if (tud_cdc_connected())
                break;
            if (time_us_64() - t > 500000)
                break;
        }
    }
    uSec(1000000);
#elif !defined(USBKEYBOARD)
    if (!(Option.SerialConsole == 1 || Option.SerialConsole == 2) || Option.Telnet == -1)
    {
        uint64_t t = time_us_64();
        while (1)
        {
            if (tud_cdc_connected())
                break;
            if (time_us_64() - t > 500000)
                break;
        }
    }
    uSec(1000000);
    initKeyboard();
#endif
#ifdef PICOMITEBT
        /* Same PIO clock divider scaling as WEB — the cyw43 SPI link
           is shared between WiFi and BT, so the divider matters
           regardless of which radio we're using. */
        {
            uint32_t cyw43_div = (Option.CPU_Speed + 79999) / 80000;
            if (cyw43_div < 2)
                cyw43_div = 2;
            cyw43_set_pio_clkdiv_int_frac8(cyw43_div, 0);
        }
        if (cyw43_arch_init() == 0)
        {
            bt_console_init();
        }
#endif
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
#ifdef PICOMITEHDMIBTH
        /* HDMICore hardcodes DMACH_PING=0 / DMACH_PONG=1 (see the HDMI
           scanout block above). CYW43's bus_pio_spi grabs two unused
           DMA channels at cyw43_arch_init() time via
           dma_claim_unused_channel — which would pick 0 and 1 first.
           Reserve them here so CYW43 falls through to channels 2/3 and
           HDMI's later DMA setup doesn't tear down the cyw43 SPI link.
           PICOMITEBTH has no HDMI so this isn't needed there. */
        dma_channel_claim(0);
        dma_channel_claim(1);
#endif
        /* Same PIO clock divider scaling as PICOMITEBT / WEB. */
        {
            uint32_t cyw43_div = (Option.CPU_Speed + 99999) / 100000;
            if (cyw43_div < 2)
                cyw43_div = 2;
            cyw43_set_pio_clkdiv_int_frac8(cyw43_div, 0);
        }
        if (cyw43_arch_init() == 0)
        {
            bt_keyboard_init();
        }
#endif
        InitBasic();
#ifndef PICOMITEVGA
#ifndef PICOMITEMIN
        InitDisplaySSD();
#endif
        InitDisplaySPI(0);
#if PICOMITERP2350
        InitDisplay222();
#endif
        InitDisplayI2C(0);
        InitDisplayVirtual();
        InitTouch();
        if (Option.BackLightLevel)
            setBacklight(Option.BackLightLevel, 0);
#endif
#if PICOCALC // *EB*
        if (Option.BACKLIGHT_LCD)
            set_lcd_backlight(Option.BACKLIGHT_LCD); // *EB*
        if (Option.BACKLIGHT_KBD)
            set_kbd_backlight(Option.BACKLIGHT_KBD); // *EB*
#endif                                               // *EB*
        ErrorInPrompt = false;
        exception_set_exclusive_handler(HARDFAULT_EXCEPTION, sigbus);
        exception_set_exclusive_handler(SVCALL_EXCEPTION, sigbus);
        exception_set_exclusive_handler(PENDSV_EXCEPTION, sigbus);
        exception_set_exclusive_handler(NMI_EXCEPTION, sigbus);
        exception_set_exclusive_handler(SYSTICK_EXCEPTION, sigbus);
        while ((i = getConsole()) != -1)
        {
        }

#ifdef PICOMITEVGA
        //        bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
#ifdef HDMI
        //    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_PROC1_BITS;
        multicore_launch_core1_with_stack(HDMICore, core1stack, 512);
        core1stack[0] = 0x12345678;
        uSec(1000);
#else
#ifdef rp2350
        piomap[QVGA_PIO_NUM] = (uint64_t)((uint64_t)1 << (uint64_t)PinDef[Option.VGA_BLUE].GPno);
        piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)(PinDef[Option.VGA_BLUE].GPno + 1));
        piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)(PinDef[Option.VGA_BLUE].GPno + 2));
        piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)(PinDef[Option.VGA_BLUE].GPno + 3));
        piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)PinDef[Option.VGA_HSYNC].GPno);
        piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)(PinDef[Option.VGA_HSYNC].GPno + 1));
        if (Option.audio_i2s_bclk)
        {
            piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)PinDef[Option.audio_i2s_data].GPno);
            piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)PinDef[Option.audio_i2s_bclk].GPno);
            piomap[QVGA_PIO_NUM] |= (uint64_t)((uint64_t)1 << (uint64_t)(PinDef[Option.audio_i2s_bclk].GPno + 1));
        }
#endif
        X_TILE = Option.X_TILE;
        Y_TILE = Option.Y_TILE;
        ytileheight = (X_TILE == 80 || X_TILE == 106) ? 12 : 16;
        bus_ctrl_hw->priority = 0x100;
        multicore_launch_core1_with_stack(QVgaCore, core1stack, 512);
        core1stack[0] = 0x12345678;
        memset((void *)WriteBuf, 0, 38400);
#endif
        ResetDisplay();
        ClearScreen(Option.DefaultBC);
#else
#ifdef PICOMITE
#ifdef rp2350
    if (!(Option.DISPLAY_TYPE >= VGA222 && Option.DISPLAY_TYPE < NEXTGEN))
#endif
    {
        bus_ctrl_hw->priority = 0x100;
        multicore_launch_core1_with_stack(UpdateCore, core1stack, 2048);
        core1stack[0] = 0x12345678;
    }
#ifdef rp2350
    else
    {
        multicore_launch_core1_with_stack(init_vga222, core1stack, 2048);
        core1stack[0] = 0x12345678;
    }
#endif
#endif
#endif
        strcpy((char *)banner, MES_SIGNON);
#ifdef rp2350
#ifdef PICOMITEVGA
#ifdef HDMI
#ifdef USBKEYBOARD
#ifdef HDMICUTDOWN
        /* "PicoMiteHDMIBTH" / "PicoMiteHDMIWEB" are both 3 chars longer
           than "PicoMiteHDMI", so the A/B insertion point shifts by 3:
           banner[35] is the trailing space inside the CHIP macro
           "RP2350 ". */
        banner[35] = (rp2350a ? 'A' : 'B');
#else
        banner[32] = (rp2350a ? 'A' : 'B');
#endif
#else
        banner[28] = (rp2350a ? 'A' : 'B');
#endif
#else
#ifdef USBKEYBOARD
        banner[31] = (rp2350a ? 'A' : 'B');
#else
        banner[27] = (rp2350a ? 'A' : 'B');
#endif
#endif
#else
#ifdef USBKEYBOARD
        banner[28] = (rp2350a ? 'A' : 'B');
#else
#ifdef PICOMITEWEB
        banner[23] = (rp2350a ? 'A' : 'B');
#elif defined(PICOMITEBT)
        /* "PicoMiteBT" is 2 chars longer than "PicoMite", so the
           A/B insertion point shifts by 2: banner[26] is the space
           between "RP2350" and "V" in our signon. */
        banner[26] = (rp2350a ? 'A' : 'B');
#elif defined(PICOMITEBTH)
        /* "PicoMiteBTH" is 3 chars longer than "PicoMite". */
        banner[27] = (rp2350a ? 'A' : 'B');
#else
        banner[24] = (rp2350a ? 'A' : 'B');
#endif
#endif
#endif
#endif
        if (!(_excep_code == RESET_FLASHSTORAGE || _excep_code == INVALID_CLOCKSPEED || _excep_code == SCREWUP_TIMEOUT || _excep_code == WATCHDOG_TIMEOUT || (_excep_code == POSSIBLE_WATCHDOG && watchdog_caused_reboot())))
        {
            if (Option.Autorun == 0)
            {
                if (!(_excep_code == SOFT_RESET))
                {
                    MMPrintString((char *)banner);    // print sign on message
                    MMPrintString((char *)COPYRIGHT); // print sign on message
                    PRet();
                }
            }
            else
            {
                if (Option.Autorun != MAXFLASHSLOTS + 1)
                {
                    ProgMemory = (unsigned char *)(flash_target_contents + (Option.Autorun - 1) * MAX_PROG_SIZE);
                }
                if (*ProgMemory != 0x01)
                {
                    MMPrintString((char *)banner);
                    MMPrintString((char *)COPYRIGHT); // print sign on message
                    PRet();
                }
            }
        }
        memset(inpbuf, 0, STRINGSIZE);
        WatchdogSet = false;
        if (_excep_code == INVALID_CLOCKSPEED)
        {
            MMPrintString("\r\nInvalid clock speed - reset to default\r\n");
            restart_reason = 0xFFFFFFFF;
        }
        if (_excep_code == SCREWUP_TIMEOUT)
        {
            MMPrintString("\r\nCommand timeout\r\n");
            restart_reason = 0xFFFFFFFF;
        }
        if (restart_reason == 0xFFFFFFFE)
        {
            WatchdogSet = true; // remember if it was a watchdog timeout
            MMPrintString("\r\nMMBasic Watchdog timeout\r\n");
        }
        if (restart_reason == 0xFFFFFFFD)
        {
            MMPrintString("\r\nHW Watchdog timeout\r\n");
            WatchdogSet = true; // remember if it was a watchdog timeout
            _excep_code = 0;
        }
        if (restart_reason == 0xFFFFFFFC)
        {
            WatchdogSet = true; // remember if it was a watchdog timeout
            MMPrintString("\rFirmware updated\r\n");
        }
        savewatchdog = WatchdogSet;
        if (noRTC)
        {
            noRTC = 0;
            Option.RTC = 0;
            SaveOptions();
            MMPrintString("RTC not found, OPTION RTC AUTO disabled\r\n");
        }
        if (noI2C)
        {
            noI2C = 0;
            Option.KeyboardConfig = NO_KEYBOARD;
            SaveOptions();
            MMPrintString("I2C Keyboard not found, OPTION KEYBOARD disabled\r\n");
        }
        updatebootcount(_excep_code == RESET_FLASHSTORAGE || _excep_code == RESET_PICOCALCINIT);
        *tknbuf = 0;
        ContinuePoint = nextstmt; // in case the user wants to use the continue command
        clearrepeat();
#ifdef USBKEYBOARD
        for (int i = 0; i < 4; i++)
        {
            memset((void *)&HID[i], 0, sizeof(struct s_HID));
            HID[i].report_requested = true;
        }
        // Bring the host controller up first so the PHY is powered, then drive
        // a real SE0 bus reset. hcd_port_reset() is a no-op on this silicon, so
        // without this an externally-powered hub (which keeps VBUS across an
        // MMBasic reboot) holds its old USB address and ignores re-enumeration.
        // Enumeration is deferred to tuh_task() in the main loop, which runs
        // after the reset, so re-enumeration starts from a clean bus.
        tuh_init(BOARD_TUH_RHPORT);
        USB_bus_reset(); // force any attached hub back to Default state
        uSec(50000);     // recovery: let the hub re-detect its downstream ports
        USBenabled = true;
#else
    initMouse0(0);
#endif
#ifdef rp2350
        if (PSRAMsize)
        {
            MMPrintString("Total of ");
            PInt(PSRAMsize / (1024 * 1024));
            MMPrintString(" Mbytes PSRAM available\r\n");
        }
#if defined(PICOMITEVGA) && !defined(HDMI)
        start_i2s(QVGA_PIO_NUM, 1);
#elif defined(PICOMITEWEB)
        // WEBRP2350: keep PIO2 free for the cyw43 SPI driver (the SDK
        // picks the highest-numbered PIO with a free SM). I2S on PIO1.
        start_i2s(1, 1);
#else
        start_i2s(2, 1);
#endif
#else
    start_i2s(QVGA_PIO_NUM, 1);
#endif
        if (setjmp(mark) != 0)
        {
            // we got here via a long jump which means an error or CTRL-C or the program wants to exit to the command prompt
            FlashLoad = 0;
            clearrepeat();
            ScrewUpTimer = 0;
            ProgMemory = (uint8_t *)flash_progmemory;
            ContinuePoint = nextstmt; // in case the user wants to use the continue command
            *tknbuf = 0;              // we do not want to run whatever is in the token buffer
            // Do NOT reset optionangle/useoptionangle here - this landing pad fires
            // after every prompt command, so resetting would cancel an OPTION ANGLE
            // DEGREES set at the prompt. ClearRuntime() resets it at program start
            // (RUN/NEW), which is the correct place for the reset.
            savewatchdog = WatchdogSet = false;
            char *ptr = findvar((unsigned char *)"MM.ENDLINE$", V_NOFIND_NULL);
            if (ptr && *ptr)
            {
                CurrentLinePtr = 0;
                memcpy(inpbuf, ptr, *ptr + 1);
                *ptr = 0;
                MtoC(inpbuf);
                *ptr = 0;
                tokenise(true);
                goto autorun;
            }
        }
        else
        {
            if (*ProgMemory == 0x01)
                ClearVars(0, true);
            else
            {
                ClearProgram(true);
            }
#ifdef PICOMITEWEB
#ifdef PICOMITEHDMIWEB
            /* HDMIWEB launched HDMICore on core1 above (multicore_launch),
               which drives the HSTX scanout via hardcoded DMA channels 0/1
               WITHOUT claiming them in the SDK. cyw43_arch_init() below grabs
               two unused DMA channels via dma_claim_unused_channel and would
               pick 0/1 first — tearing down the live scanout. Claim them here
               so cyw43 falls through to 2/3. (HDMIBTH does the same claim, but
               earlier — before its cyw43 init in the IS_BTH path.) */
            dma_channel_claim(DMACH_PING);
            dma_channel_claim(DMACH_PONG);
#endif
            if (cyw43_arch_init_with_country(wifi_country_to_cyw43(Option.wifi_country_code)) == 0)
            {
                uint32_t cyw43_div = (Option.CPU_Speed + 99999) / 100000;
                if (cyw43_div < 2)
                    cyw43_div = 2;
                cyw43_set_pio_clkdiv_int_frac8(cyw43_div, 0);
                /*                char dbg[80];
                                sprintf(dbg, "[cyw43] CPU_Speed=%u kHz, clk_sys=%u Hz, div=%u, SPI=%u kHz\r\n",
                                        (unsigned)Option.CPU_Speed, (unsigned)clock_get_hz(clk_sys),
                                        (unsigned)cyw43_div,
                                        (unsigned)(clock_get_hz(clk_sys) / (2 * cyw43_div) / 1000));
                                MMPrintString(dbg);*/
                startupcomplete = 1;
                WebConnect();
            }
#endif
#ifdef PICOMITE
            SPIatRisk = ((Option.DISPLAY_TYPE > I2C_PANEL && Option.DISPLAY_TYPE < BufferedPanel) && Option.SD_CLK_PIN == 0);
            low_x = 0;
            high_x = HRes - 1;
            low_y = 0;
            high_y = VRes - 1;
            if (Option.Refresh)
                Display_Refresh();
#endif
            if (PrepareProgram(true))
            {
                // Error in program - print message but continue to prompt
                PrintPreprogramError();
            }
            if (ProgramValid && FindSubFun((unsigned char *)"MM.STARTUP", 0) >= 0)
            {
                ExecuteProgram((unsigned char *)"MM.STARTUP\0");
                memset(inpbuf, 0, STRINGSIZE);
            }
            /*#ifdef PICOMITERP2350
                        if (Option.DISPLAY_TYPE >= VGA222 && Option.DISPLAY_TYPE < NEXTGEN)
                        {
                            CallExecuteProgram((char *)kickvga);
                        }
            #endif*/
            if (Option.Autorun && ProgramValid)
            {
                ClearRuntime(true);
                if (PrepareProgram(true))
                {
                    // Error in program - print message and disable autorun
                    PrintPreprogramError();
                    Option.Autorun = 0;
                    SaveOptions();
                }
                else if (*ProgMemory == 0x01)
                {
                    memset(tknbuf, 0, STRINGSIZE);
                    unsigned short tkn = GetCommandValue((unsigned char *)"RUN");
                    tknbuf[0] = (tkn & 0x7f) + C_BASETOKEN;
                    tknbuf[1] = (tkn >> 7) + C_BASETOKEN; // tokens can be 14-bit
                    goto autorun;
                }
                else
                {
                    Option.Autorun = 0;
                    SaveOptions();
                }
            }
        }
        while (1)
        {
#ifdef MMBASIC_FM
            if (fm_program_launched_from_fm || fm_relaunch_status_valid)
            {
                fm_program_launched_from_fm = 0;
                CurrentLinePtr = NULL;
                cmdline = (unsigned char *)"";
                memset(tknbuf, 0, STRINGSIZE);
                {
                    unsigned short fmtkn = GetCommandValue((unsigned char *)"FM");
                    tknbuf[0] = (fmtkn & 0x7f) + C_BASETOKEN;
                    tknbuf[1] = (fmtkn >> 7) + C_BASETOKEN;
                }
                goto autorun;
                continue;
            }
#endif
            if (Option.DISPLAY_CONSOLE)
            {
                SetFont(PromptFont);
                gui_fcolour = PromptFC;
                gui_bcolour = PromptBC;
                // The program may have left the cursor on the bottom line while
                // using a shorter font.  Switching back to the (possibly taller)
                // prompt font here can push the current line past the bottom of
                // the screen, clipping the prompt characters.  Make room for the
                // new font before the prompt is printed.
                int reservedBottom = VRes - (VRes * OptionVResreserved / 100);
                if (CurrentY + gui_font_height > reservedBottom)
                {
#ifdef PICOMITEVGA
                    int canscroll = !Option.NoScroll; // framebuffer display always scrolls unless NoScroll
#elif PICOMITERP2350
                    int canscroll = !((SPIREAD && ScrollLCD != ScrollLCDSPISCR && ScrollLCD != ScrollLCDMEM332) || Option.NoScroll);
#else
                    int canscroll = !((SPIREAD && ScrollLCD != ScrollLCDSPISCR) || Option.NoScroll);
#endif
                    if (canscroll)
                    {
                        int diff = CurrentY + gui_font_height - reservedBottom;
                        ScrollLCD(diff); // scroll up so the prompt font fits on screen
                        CurrentY -= diff;
                    }
                    else
                    {
                        ClearScreen(gui_bcolour); // can't scroll - clear and restart at the top
                        CurrentX = 0;
                        CurrentY = 0;
                    }
                }
                if (CurrentX != 0)
                    MMPrintString("\r\n"); // prompt should be on a new line
            }
#if defined(USBKEYBOARD) && defined(GUICONTROLS) && defined(PICOMITEVGA)
            OSK_OnPromptIdle();
#endif
            MMAbort = false;
            BreakKey = BREAK_KEY;
            EchoOption = true;
            g_LocalIndex = 0;      // this should not be needed but it ensures that all space will be cleared
            ClearTempMemory();     // clear temp string space (might have been used by the prompt)
            CurrentLinePtr = NULL; // do not use the line number in error reporting
            if (MMCharPos > 1)
                MMPrintString("\r\n"); // prompt should be on a new line
            while (Option.PIN && !IgnorePIN)
            {
                if (Option.PIN == 99999999) // 99999999 is permanent lockdown
                    MMPrintString("Console locked, press enter to restart: ");
                else
                    MMPrintString("Enter PIN or 0 to restart: ");
                MMgetline(0, (char *)inpbuf);
                if (Option.PIN == 99999999)
                    SoftReset(SOFT_RESET);
                if (*inpbuf != 0)
                {
                    uSec(3000000);
                    i = atoi((char *)inpbuf);
                    if (i == 0)
                        SoftReset(SOFT_RESET);
                    if (i == Option.PIN)
                    {
                        IgnorePIN = true;
                        break;
                    }
                }
            }
            if (_excep_code != POSSIBLE_WATCHDOG)
                _excep_code = 0;
            PrepareProgram(false); // Don't abort on error - just silently skip bad definitions
            if (ProgramValid && !ErrorInPrompt && FindSubFun((unsigned char *)"MM.PROMPT", 0) >= 0)
            {
                ErrorInPrompt = true;
                ExecuteProgram((unsigned char *)"MM.PROMPT\0");
                MMPromptPos = MMCharPos - 1; // Save length of prompt
            }
            else
            {
                MMPrintString("> "); // print the prompt
                MMPromptPos = 2;     // Save length of prompt
            }
            ErrorInPrompt = false;
            EditInputLine();
            // InsertLastcmd(inpbuf);                                  // save in case we want to edit it later
            if (!*inpbuf)
                continue; // ignore an empty line
            char *p = (char *)inpbuf;
            skipspace(p);
            //        executelocal(p);
            if (strlen(p) == 2 && p[1] == ':')
            {
                if (mytoupper(*p) == 'A')
                    strcpy(p, "drive \"a:\"");
                if (mytoupper(*p) == 'B')
                    strcpy(p, "drive \"b:\"");
#if defined(USBKEYBOARD) && defined(rp2350)
                if (mytoupper(*p) == 'C')
                    strcpy(p, "drive \"c:\"");
#endif
            }
            if (*p == '*' && p[1] != '(')
            { // shortform RUN command so convert to a normal version
#ifdef CALCPROMPT
              // CALCPROMPT: extract the filename from *<name>... and check whether it exists
                // (also try with .bas appended, matching how RUN locates files).  If no matching
                // file is found, skip the *RUN transform so the tokeniser's operator-led CALC
                // injection treats the input as MM.ANSWER * <expr>.  This lets *32 multiply by
                // 32 when no file "32" or "32.bas" exists, while still preserving *<filename>
                // for any name (numeric or otherwise) that does exist on disk.
                char fname[FF_MAX_LFN] = {0};
                char *src = p + 1;
                while (*src == ' ')
                    src++;
                char *fp = fname;
                if (*src == '"')
                {
                    src++;
                    while (*src && *src != '"' && (fp - fname) < FF_MAX_LFN - 1)
                        *fp++ = *src++;
                }
                else
                {
                    while (*src && *src != ' ' && (fp - fname) < FF_MAX_LFN - 1)
                        *fp++ = *src++;
                }
                *fp = 0;
                int file_found = 0;
                if (fname[0])
                {
                    file_found = (ExistsFile(fname) == 1);
                    if (!file_found && !HasExtension(fname) && (int)strlen(fname) < FF_MAX_LFN - 5)
                    {
                        strcat(fname, ".bas");
                        file_found = (ExistsFile(fname) == 1);
                    }
                }
                int looks_like_calc = 0;
                if (!file_found)
                {
                    if (*p == '*' && p[1] == 0)
                    {
                        transform_star_command((char *)inpbuf);
                        p = (char *)inpbuf;
                    }
                    else
                    {
                        // Decide whether the input resembles a calculator expression
                        // (digit/dot start, or contains parens/operators/dot anywhere).
                        // Bare names like *foo or *sqrt2 don't, so we report a clear
                        // error instead of letting CALC fail with "is not declared".
                        char *q = p + 1;
                        while (*q == ' ')
                            q++;
                        if (IsDigitinline(*q) || *q == '.')
                        {
                            looks_like_calc = 1;
                        }
                        else
                        {
                            for (char *r = q; *r; r++)
                            {
                                if (*r == '(' || *r == '.' || *r == '+' || *r == '-' ||
                                    *r == '*' || *r == '/' || *r == '^' || *r == '<' ||
                                    *r == '>' || *r == '\\')
                                {
                                    looks_like_calc = 1;
                                    break;
                                }
                            }
                        }
                        if (!looks_like_calc)
                            error("Neither file nor function found");
                    }
                }
                if (file_found)
#endif
                {
                    transform_star_command((char *)inpbuf);
                    p = (char *)inpbuf;
                }
            }
            multi = false;
            tokenise(true); // turn into executable code
        autorun:
            i = 0;
            WatchdogSet = savewatchdog;
            CommandToken tkn = commandtbl_decode(tknbuf);
            if (tkn == GetCommandValue((unsigned char *)"RUN") || tkn == GetCommandValue((unsigned char *)"EDIT") || tkn == GetCommandValue((unsigned char *)"AUTOSAVE"))
                i = 1;
            if (setjmp(jmprun) != 0)
            {
                PrepareProgram(false); // Don't abort on error
                CurrentLinePtr = 0;
            }
            ExecuteProgram(tknbuf); // execute the line straight away
#ifdef MMBASIC_FM
            if (fm_program_launched_from_fm || fm_relaunch_status_valid)
            {
                fm_program_launched_from_fm = 0;
                CurrentLinePtr = NULL;
                cmdline = (unsigned char *)"";
                memset(tknbuf, 0, STRINGSIZE);
                {
                    unsigned short fmtkn = GetCommandValue((unsigned char *)"FM");
                    tknbuf[0] = (fmtkn & 0x7f) + C_BASETOKEN;
                    tknbuf[1] = (fmtkn >> 7) + C_BASETOKEN;
                }
                goto autorun;
            }
#endif
            if (i)
            {
                cmdline = NULL;
                do_end(false);
                longjmp(mark, 1); // jump back to the input prompt
            }
            else
            {
                memset(inpbuf, 0, STRINGSIZE);
                longjmp(mark, 1); // jump back to the input prompt
            }
        }
    }
    void stripcomment(char *p)
    {
        char *q = p;
        int toggle = 0;
        while (*q)
        {
            if (*q == '\'' && toggle == 0)
            {
                *q = 0;
                break;
            }
            if (*q == '"')
                toggle ^= 1;
            q++;
        }
    }

    // takes a pointer to RAM containing a program (in clear text) and writes it to memory in tokenised format
    void MIPS16 SaveProgramToFlash(unsigned char *pm, int msg)
    {
        unsigned char *p, fontnbr, prevchar = 0, buf[STRINGSIZE];
        unsigned short endtoken, tkn;
        int nbr, i, j, n, SaveSizeAddr;
        bool continuation = false;
        multi = false;
        uint32_t storedupdates[MAXCFUNCTION], updatecount = 0, realflashsave;
        initFonts();
#ifdef rp2350
        __dsb();
#endif
        clearrepeat();
        memcpy(buf, tknbuf, STRINGSIZE); // save the token buffer because we are going to use it
        FlashWriteInit(PROGRAM_FLASH);
        safe_flash_range_erase(realflashpointer, MAX_PROG_SIZE);
        j = MAX_PROG_SIZE / 4;
        int *pp = (int *)(flash_progmemory);
        while (j--)
            if (*pp++ != 0xFFFFFFFF)
            {
                enable_interrupts_pico();
                error("Flash erase problem");
            }
        nbr = 0;
        // this is used to count the number of bytes written to flash
        while (*pm)
        {
        contloop:
            if (continuation)
            {
                p = &inpbuf[strlen((char *)inpbuf)];
                continuation = false;
            }
            else
                p = inpbuf;
            while (!(*pm == 0 || *pm == '\r' || (*pm == '\n' && prevchar != '\r')))
            {
                if (*pm == TAB)
                {
                    do
                    {
                        *p++ = ' ';
                        if ((p - inpbuf) >= MAXSTRLEN)
                            goto exiterror;
                    } while ((p - inpbuf) % 2);
                }
                else
                {
                    if (isprint((uint8_t)*pm))
                    {
                        *p++ = *pm;
                        if ((p - inpbuf) >= MAXSTRLEN)
                            goto exiterror;
                    }
                }
                prevchar = *pm++;
            }
            if (*pm)
                prevchar = *pm++; // step over the end of line char but not the terminating zero
            *p = 0;               // terminate the string in inpbuf

            if (*inpbuf == 0 && (*pm == 0 || (!isprint((uint8_t)*pm) && pm[1] == 0)))
                break; // don't save a trailing newline
            if (inpbuf[strlen((char *)inpbuf) - 1] == Option.continuation && inpbuf[strlen((char *)inpbuf) - 2] == ' ' && Option.continuation)
            {
                continuation = true;
                inpbuf[strlen((char *)inpbuf) - 2] = 0; // strip the continuation character
                goto contloop;
            }
            tokenise(false); // turn into executable code
            p = tknbuf;
            while (!(p[0] == 0 && p[1] == 0))
            {
                FlashWriteByte(*p++);
                nbr++;

                if ((int)((char *)realflashpointer - (uint32_t)PROGSTART) >= MAX_PROG_SIZE - 5)
                    goto exiterror;
            }
            FlashWriteByte(0);
            nbr++; // terminate that line in flash
        }
        FlashWriteByte(0);
        FlashWriteAlign(); // this will flush the buffer and step the flash write pointer to the next word boundary
        // now we must scan the program looking for CFUNCTION/CSUB/DEFINEFONT statements, extract their data and program it into the flash used by  CFUNCTIONs
        // programs are terminated with two zero bytes and one or more bytes of 0xff.  The CFunction area starts immediately after that.
        // the format of a CFunction/CSub/Font in flash is:
        //   Unsigned Int - Address of the CFunction/CSub in program memory (points to the token representing the "CFunction" keyword) or NULL if it is a font
        //   Unsigned Int - The length of the CFunction/CSub/Font in bytes including the Offset (see below)
        //   Unsigned Int - The Offset (in words) to the main() function (ie, the entry point to the CFunction/CSub).  Omitted in a font.
        //   word1..wordN - The CFunction/CSub/Font code
        // The next CFunction/CSub/Font starts immediately following the last word of the previous CFunction/CSub/Font
        int firsthex = 1;
        realflashsave = realflashpointer;
        p = (unsigned char *)flash_progmemory; // start scanning program memory
        while (*p != 0xff)
        {
            nbr++;
            if (*p == 0)
                p++; // if it is at the end of an element skip the zero marker
            if (*p == 0)
                break; // end of the program
            if (*p == T_NEWLINE)
            {
                CurrentLinePtr = p;
                p += T_NEWLINE_HDR; // skip newline + skip byte
            }
            if (*p == T_LINENBR)
                p += 3; // step over the line number

            skipspace(p);
            if (*p == T_LABEL)
            {
                p += p[1] + 2; // skip over the label
                skipspace(p);  // and any following spaces
            }
            tkn = p[0] & 0x7f;
            tkn |= ((unsigned short)(p[1] & 0x7f) << 7);
            if (tkn == cmdCSUB || tkn == GetCommandValue((unsigned char *)"DefineFont"))
            { // found a CFUNCTION, CSUB or DEFINEFONT token
                if (tkn == GetCommandValue((unsigned char *)"DefineFont"))
                {
                    endtoken = GetCommandValue((unsigned char *)"End DefineFont");
                    p += 2; // step over the token
                    skipspace(p);
                    if (*p == '#')
                        p++;
                    fontnbr = getint(p, 1, FONT_TABLE_SIZE);
                    // font 6 has some special characters, some of which depend on font 1
                    if (fontnbr == 1 || fontnbr == 6 || fontnbr == 7)
                    {
                        enable_interrupts_pico();
                        error("Cannot redefine fonts 1, 6 or 7");
                    }
                    realflashpointer += 4;
                    skipelement(p); // go to the end of the command
                    p--;
                }
                else
                {
                    endtoken = GetCommandValue((unsigned char *)"End CSub");
                    realflashpointer += 4;
                    fontnbr = 0;
                    firsthex = 0;
                    p++;
                }
                SaveSizeAddr = realflashpointer; // save where we are so that we can write the CFun size in here
                realflashpointer += 4;
                p++;
                skipspace(p);
                if (!fontnbr)
                { // process CSub
                    if (!isnamestart((uint8_t)*p))
                    {
                        enable_interrupts_pico();
                        error("Function name");
                    }
                    do
                    {
                        p++;
                    } while (isnamechar((uint8_t)*p));
                    skipspace(p);
                    if (!(isxdigit((uint8_t)p[0]) && isxdigit((uint8_t)p[1]) && isxdigit((uint8_t)p[2])))
                    {
                        skipelement(p);
                        p++;
                        if (*p == T_NEWLINE)
                        {
                            CurrentLinePtr = p;
                            p += T_NEWLINE_HDR; // skip newline + skip byte
                        }
                        if (*p == T_LINENBR)
                            p += 3; // skip over a line number
                    }
                }
                do
                {
                    while (*p && *p != '\'')
                    {
                        skipspace(p);
                        n = 0;
                        for (i = 0; i < 8; i++)
                        {
                            if (!isxdigit((uint8_t)*p))
                            {
                                enable_interrupts_pico();
                                error("Invalid hex word");
                            }
                            if ((int)((char *)realflashpointer - (uint32_t)PROGSTART) >= MAX_PROG_SIZE - 5)
                                goto exiterror;
                            n = n << 4;
                            if (*p <= '9')
                                n |= (*p - '0');
                            else
                                n |= (mytoupper(*p) - 'A' + 10);
                            p++;
                        }
                        realflashpointer += 4;
                        skipspace(p);
                        if (firsthex)
                        {
                            firsthex = 0;
                            if (((n >> 16) & 0xff) < 0x20)
                            {
                                enable_interrupts_pico();
                                error("Can't define non-printing characters");
                            }
                        }
                    }
                    // we are at the end of a embedded code line
                    while (*p)
                        p++; // make sure that we move to the end of the line
                    p++;     // step to the start of the next line
                    if (*p == 0)
                    {
                        enable_interrupts_pico();
                        error("Missing END declaration");
                    }
                    if (*p == T_NEWLINE)
                    {
                        CurrentLinePtr = p;
                        p += T_NEWLINE_HDR; // skip newline + skip byte
                    }
                    if (*p == T_LINENBR)
                        p += 3; // skip over the line number
                    skipspace(p);
                    tkn = p[0] & 0x7f;
                    tkn |= ((unsigned short)(p[1] & 0x7f) << 7);
                } while (tkn != endtoken);
                storedupdates[updatecount++] = realflashpointer - SaveSizeAddr - 4;
            }
            while (*p)
                p++; // look for the zero marking the start of the next element
        }
        realflashpointer = realflashsave;
        updatecount = 0;
        p = (unsigned char *)flash_progmemory; // start scanning program memory
        while (*p != 0xff)
        {
            nbr++;
            if (*p == 0)
                p++; // if it is at the end of an element skip the zero marker
            if (*p == 0)
                break; // end of the program
            if (*p == T_NEWLINE)
            {
                CurrentLinePtr = p;
                p += T_NEWLINE_HDR; // skip newline + skip byte
            }
            if (*p == T_LINENBR)
                p += 3; // step over the line number

            skipspace(p);
            if (*p == T_LABEL)
            {
                p += p[1] + 2; // skip over the label
                skipspace(p);  // and any following spaces
            }
            tkn = p[0] & 0x7f;
            tkn |= ((unsigned short)(p[1] & 0x7f) << 7);
            if (tkn == cmdCSUB || tkn == GetCommandValue((unsigned char *)"DefineFont"))
            { // found a CFUNCTION, CSUB or DEFINEFONT token
                if (tkn == GetCommandValue((unsigned char *)"DefineFont"))
                { // found a CFUNCTION, CSUB or DEFINEFONT token
                    endtoken = GetCommandValue((unsigned char *)"End DefineFont");
                    p += 2; // step over the token
                    skipspace(p);
                    if (*p == '#')
                        p++;
                    fontnbr = getint(p, 1, FONT_TABLE_SIZE);
                    // font 6 has some special characters, some of which depend on font 1
                    if (fontnbr == 1 || fontnbr == 6 || fontnbr == 7)
                    {
                        enable_interrupts_pico();
                        error("Cannot redefine fonts 1, 6, or 7");
                    }

                    // FlashWriteWord(fontnbr - 1);                        // a low number (< FONT_TABLE_SIZE) marks the entry as a font
                    //  B31 = 1 now marks entry as font.
                    FlashWriteByte(fontnbr - 1);
                    FlashWriteByte(0x00);
                    FlashWriteByte(0x00);
                    FlashWriteByte(0x80);

                    skipelement(p); // go to the end of the command
                    p--;
                }
                else
                {
                    endtoken = GetCommandValue((unsigned char *)"End CSub");
                    FlashWriteWord((unsigned int)(p - flash_progmemory)); // if a CFunction/CSub save a relative pointer to the declaration
                    fontnbr = 0;
                    p++;
                }
                SaveSizeAddr = realflashpointer;              // save where we are so that we can write the CFun size in here
                FlashWriteWord(storedupdates[updatecount++]); // leave this blank so that we can later do the write
                p++;
                skipspace(p);
                if (!fontnbr)
                {
                    if (!isnamestart((uint8_t)*p))
                    {
                        enable_interrupts_pico();
                        error("Function name");
                    }
                    do
                    {
                        p++;
                    } while (isnamechar(*p));
                    skipspace(p);
                    if (!(isxdigit(p[0]) && isxdigit(p[1]) && isxdigit(p[2])))
                    {
                        skipelement(p);
                        p++;
                        if (*p == T_NEWLINE)
                        {
                            CurrentLinePtr = p;
                            p += T_NEWLINE_HDR; // skip newline + skip byte
                        }
                        if (*p == T_LINENBR)
                            p += 3; // skip over a line number
                    }
                }
                do
                {
                    while (*p && *p != '\'')
                    {
                        skipspace(p);
                        n = 0;
                        for (i = 0; i < 8; i++)
                        {
                            if (!isxdigit(*p))
                            {
                                enable_interrupts_pico();
                                error("Invalid hex word");
                            }
                            if ((int)((char *)realflashpointer - (uint32_t)PROGSTART) >= MAX_PROG_SIZE - 5)
                                goto exiterror;
                            n = n << 4;
                            if (*p <= '9')
                                n |= (*p - '0');
                            else
                                n |= (mytoupper(*p) - 'A' + 10);
                            p++;
                        }

                        FlashWriteWord(n);
                        skipspace(p);
                    }
                    // we are at the end of a embedded code line
                    while (*p)
                        p++; // make sure that we move to the end of the line
                    p++;     // step to the start of the next line
                    if (*p == 0)
                    {
                        enable_interrupts_pico();
                        error("Missing END declaration");
                    }
                    if (*p == T_NEWLINE)
                    {
                        CurrentLinePtr = p;
                        p += T_NEWLINE_HDR; // skip newline + skip byte
                    }
                    if (*p == T_LINENBR)
                        p += 3; // skip over a line number
                    skipspace(p);
                    tkn = p[0] & 0x7f;
                    tkn |= ((unsigned short)(p[1] & 0x7f) << 7);
                } while (tkn != endtoken);
            }
            while (*p)
                p++; // look for the zero marking the start of the next element
        }
        FlashWriteWord(0xffffffff); // make sure that the end of the CFunctions is terminated with an erased word
        FlashWriteClose();          // this will flush the buffer and step the flash write pointer to the next word boundary
        if (msg)
        { // if requested by the caller, print an informative message
            if (MMCharPos > 1)
                MMPrintString("\r\n"); // message should be on a new line
            MMPrintString("Saved ");
            IntToStr((char *)tknbuf, nbr + 3, 10);
            MMPrintString((char *)tknbuf);
            MMPrintString(" bytes\r\n");
        }
        memcpy(tknbuf, buf, STRINGSIZE); // restore the token buffer in case there are other commands in it
                                         //    initConsole();
        clearrepeat();
        enable_interrupts_pico();
        return;

    // we only get here in an error situation while writing the program to flash
    exiterror:
        FlashWriteByte(0);
        FlashWriteByte(0);
        FlashWriteByte(0); // terminate the program in flash
        FlashWriteClose();
        StandardError(29);
    }

#ifdef __cplusplus
}
#endif

/// \end:uart_advanced[]
