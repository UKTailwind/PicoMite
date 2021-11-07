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
extern "C" {
#endif
#define PICO_STDIO_ENABLE_CRLF_SUPPORT 0
#define PICO_STACK_SIZE 0x4000
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "Configuration.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/adc.h"
#include "hardware/exception.h"
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/scb.h"
#include "hardware/vreg.h"
#include "pico/unique_id.h"
#include <pico/bootrom.h>
#include "hardware/irq.h"

#include "../pico-sdk/lib/tinyusb/src/class/cdc/cdc_device.h"
#define MES_SIGNON  "\rPicoMite MMBasic Version " VERSION "\r\n"\
					"Copyright " YEAR " Geoff Graham\r\n"\
					"Copyright " YEAR2 " Peter Mather\r\n\r\n"
#define USBKEEPALIVE 30000
int ListCnt;
int MMCharPos;
int busfault=0;
int ExitMMBasicFlag = false;
volatile int MMAbort = false;
unsigned int _excep_peek;
void CheckAbort(void);
void TryLoadProgram(void);
unsigned char lastchar=0;
unsigned char BreakKey = BREAK_KEY;                                          // defaults to CTRL-C.  Set to zero to disable the break function
volatile char ConsoleRxBuf[CONSOLE_RX_BUF_SIZE]={0};
volatile int ConsoleRxBufHead = 0;
volatile int ConsoleRxBufTail = 0;
volatile char ConsoleTxBuf[CONSOLE_TX_BUF_SIZE]={0};
volatile int ConsoleTxBufHead = 0;
volatile int ConsoleTxBufTail = 0;
volatile unsigned int AHRSTimer = 0;
volatile unsigned int InkeyTimer = 0;
volatile long long int mSecTimer = 0;                               // this is used to count mSec
volatile unsigned int WDTimer = 0;
volatile unsigned int diskchecktimer = 0;
volatile unsigned int clocktimer=60*60*1000;
volatile unsigned int PauseTimer = 0;
volatile unsigned int IntPauseTimer = 0;
volatile unsigned int Timer1=0, Timer2=0;		                       //1000Hz decrement timer
volatile unsigned int USBKeepalive=USBKEEPALIVE;
volatile int ds18b20Timer = -1;
volatile int second = 0;                                            // date/time counters
volatile int minute = 0;
volatile int hour = 0;
volatile int day = 1;
volatile int month = 1;
volatile int year = 2000;
volatile unsigned int GPSTimer = 0;
volatile unsigned int SecondsTimer = 0;
volatile unsigned int I2CTimer = 0;
volatile int day_of_week=1;
unsigned char PulsePin[NBR_PULSE_SLOTS];
unsigned char PulseDirection[NBR_PULSE_SLOTS];
int PulseCnt[NBR_PULSE_SLOTS];
int PulseActive;
const uint8_t *flash_option_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t *SavedVarsFlash = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET +  FLASH_ERASE_SIZE);
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET + FLASH_ERASE_SIZE + SAVEDVARS_FLASH_SIZE);
int ticks_per_second; 
int InterruptUsed;
int calibrate=0;
char id_out[12];
MMFLOAT VCC=3.3;
extern unsigned char __attribute__ ((aligned (32))) Memory[MEMORY_SIZE];

volatile int processtick = 1;
unsigned char WatchdogSet = false;
unsigned char IgnorePIN = false;
bool timer_callback(repeating_timer_t *rt);
uint32_t __uninitialized_ram(_excep_code);
unsigned char lastcmd[STRINGSIZE*4];                                           // used to store the last command in case it is needed by the EDIT command

FATFS fs;                 // Work area (file system object) for logical drive
bool timer_callback(repeating_timer_t *rt);
static uint64_t __not_in_flash_func(uSecFunc)(uint64_t a){
    uint64_t b=time_us_64()+a;
    while(time_us_64()<b){}
    return b;
}
static uint64_t __not_in_flash_func(timer)(void){ return time_us_64();}
static int64_t PinReadFunc(int a){return gpio_get(PinDef[a].GPno);}
extern void CallExecuteProgram(char *p);
extern void CallCFuncmSec(void);
#define CFUNCRAM_SIZE   256
int CFuncRam[CFUNCRAM_SIZE/sizeof(int)];
MMFLOAT IntToFloat(long long int a){ return a; }
MMFLOAT FMul(MMFLOAT a, MMFLOAT b){ return a * b; }
MMFLOAT FAdd(MMFLOAT a, MMFLOAT b){ return a + b; }
MMFLOAT FSub(MMFLOAT a, MMFLOAT b){ return a - b; }
MMFLOAT FDiv(MMFLOAT a, MMFLOAT b){ return a / b; }
int   FCmp(MMFLOAT a,MMFLOAT b){if(a>b) return 1;else if(a<b)return -1; else return 0;}
MMFLOAT LoadFloat(unsigned long long c){union ftype{ unsigned long long a; MMFLOAT b;}f;f.a=c;return f.b; }
const void * const CallTable[] __attribute__((section(".text")))  = {	(void *)uSecFunc,	//0x00
																		(void *)putConsole,	//0x04
																		(void *)getConsole,	//0x08
																		(void *)ExtCfg,	//0x0c
																		(void *)ExtSet,	//0x10
																		(void *)ExtInp,	//0x14
																		(void *)PinSetBit,	//0x18
																		(void *)PinReadFunc,	//0x1c
																		(void *)MMPrintString,	//0x20
																		(void *)IntToStr,	//0x24
																		(void *)CheckAbort,	//0x28
																		(void *)GetMemory,	//0x2c
																		(void *)GetTempMemory,	//0x30
																		(void *)FreeMemory, //0x34
																		(void *)&DrawRectangle,	//0x38
																		(void *)&DrawBitmap,	//0x3c
																		(void *)DrawLine,	//0x40
																		(void *)FontTable,	//0x44
																		(void *)&ExtCurrentConfig,	//0x48
																		(void *)&HRes,	//0x4C
																		(void *)&VRes,	//0x50
																		(void *)SoftReset, //0x54
																		(void *)error,	//0x58
																		(void *)&ProgMemory,	//0x5c
																		(void *)&vartbl, //0x60
																		(void *)&varcnt,  //0x64
																		(void *)&DrawBuffer,	//0x68
																		(void *)&ReadBuffer,	//0x6c
																		(void *)&FloatToStr,	//0x70
                                                                        (void *)CallExecuteProgram, //0x74
                                                                        (void *)CallCFuncmSec,	//0x78
                                                                        (void *)CFuncRam,	//0x7c
                                                                        (void *)&ScrollLCD,	//0x80
																		(void *)IntToFloat, //0x84
																		(void *)FloatToInt64,	//0x88
																		(void *)&Option,	//0x8c
//                                                                        (void *)&CFuncInt1,	//0x90
//                                                                        (void *)&CFuncInt2,	//0x94
																		(void *)sin,	//0x98
																		(void *)DrawCircle,	//0x9c
																		(void *)DrawTriangle,	//0xa0
																		(void *)timer,	//0xa4
                                                                        (void *)FMul,
                                                                        (void *)FAdd,
                                                                        (void *)FSub,
                                                                        (void *)FDiv,
                                                                        (void *)FCmp,
                                                                        (void *)&LoadFloat,
                                                                        (void *)uSecFunc	//0x00
									   	   	   	   	   	   	   	   	   	   };

const struct s_PinDef PinDef[NBRPINS + 1]={
	    { 0, 99, "NULL",  UNUSED  ,99, 99},                                                         // pin 0
	    { 1,  0, "GP0",  DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART0TX  | I2C0SDA | PWM0A,99,0},  	// pin 1
		{ 2,  1, "GP1",  DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM0B ,99,128},    		    // pin 2
		{ 3, 99, "GND",  UNUSED  ,99,99},                                                           // pin 3
		{ 4,  2, "GP2",  DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM1A ,99,1},   		    // pin 4
		{ 5,  3, "GP3",  DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM1B ,99,129},    			// pin 5
		{ 6,  4, "GP4",  DIGITAL_IN | DIGITAL_OUT | SPI0RX| UART1TX  | I2C0SDA | PWM2A ,99,2},  	// pin 6
		{ 7,  5, "GP5",  DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM2B	,99,130},    		// pin 7
		{ 8, 99, "GND",  UNUSED  ,99, 99},                                                          // pin 8
		{ 9,  6, "GP6",  DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM3A	,99, 3},  			// pin 9
		{ 10,  7, "GP7",  DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM3B	,99, 131}, 		    // pin 10
	    { 11,  8, "GP8",  DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART1TX  | I2C0SDA | PWM4A ,99, 4}, 	// pin 11
		{ 12,  9, "GP9",  DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL | PWM4B	,99, 132},    		// pin 12
		{ 13, 99, "GND",  UNUSED  ,99, 99},                                                         // pin 13
		{ 14, 10, "GP10",  DIGITAL_IN | DIGITAL_OUT | SPI1SCK | I2C1SDA | PWM5A	,99, 5},  			// pin 14
		{ 15, 11, "GP11",  DIGITAL_IN | DIGITAL_OUT | SPI1TX | I2C1SCL | PWM5B	,99, 133},       	// pin 15
		{ 16, 12, "GP12",  DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART0TX | I2C0SDA | PWM6A ,99, 6},   // pin 16
		{ 17, 13, "GP13",  DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM6B	,99, 134},    		// pin 17
		{ 18, 99, "GND", UNUSED  ,99, 99},                                                          // pin 18
		{ 19, 14, "GP14",  DIGITAL_IN | DIGITAL_OUT | SPI1SCK | I2C1SDA | PWM7A	,99, 7},    		// pin 19
		{ 20, 15, "GP15",  DIGITAL_IN | DIGITAL_OUT | SPI1TX | I2C1SCL | PWM7B	,99, 135},  		// pin 20

		{ 21, 16, "GP16",  DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART0TX | I2C0SDA | PWM0A ,99, 0},   // pin 21
		{ 22, 17, "GP17",  DIGITAL_IN | DIGITAL_OUT | UART0RX | I2C0SCL | PWM0B	,99, 128},    		// pin 22
		{ 23, 99, "GND",  UNUSED  ,99, 99},                                                         // pin 23
	    { 24, 18, "GP18",  DIGITAL_IN | DIGITAL_OUT | SPI0SCK | I2C1SDA | PWM1A	,99, 1}, 			// pin 24
	    { 25, 19, "GP19",  DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL | PWM1B	,99, 129},  		// pin 25
	    { 26, 20, "GP20",  DIGITAL_IN | DIGITAL_OUT | SPI0RX | UART1TX| I2C0SDA | PWM2A	,99, 2},    // pin 26
	    { 27, 21, "GP21",  DIGITAL_IN | DIGITAL_OUT | UART1RX| I2C0SCL | PWM2B	,99, 130},    		// pin 27
		{ 28, 99, "GND",  UNUSED  ,99, 99},                                                         // pin 28
		{ 29, 22, "GP22",  DIGITAL_IN | DIGITAL_OUT | I2C1SDA| PWM3A	,99, 3},    				// pin 29
		{ 30, 99, "RUN",  UNUSED  ,99, 99},                                                         // pin 30
	    { 31, 26, "GP26",  DIGITAL_IN | DIGITAL_OUT	| ANALOG_IN | SPI1SCK| I2C1SDA | PWM5A , 0 , 5},// pin 31
	    { 32, 27, "GP27",  DIGITAL_IN | DIGITAL_OUT	| ANALOG_IN | SPI1TX| I2C1SCL | PWM5B , 1, 133},// pin 32
		{ 33, 99, "AGND", UNUSED  ,99, 99},                                                         // pin 33
		{ 34, 28, "GP28", DIGITAL_IN |DIGITAL_OUT| ANALOG_IN| SPI1RX| UART0TX|I2C0SDA| PWM6A, 2, 6},// pin 34
	    { 35, 99, "VREF", UNUSED  ,99, 99},                                                         // pin 35
		{ 36, 99, "3V3", UNUSED  ,99, 99},                                                          // pin 36
		{ 37, 99, "3V3E", UNUSED  ,99, 99},                                                         // pin 37
		{ 38, 99, "GND", UNUSED  ,99, 99},                                                          // pin 38
		{ 39, 99, "VSYS", UNUSED  ,99, 99},                                                         // pin 39
		{ 40, 99, "VBUS", UNUSED  ,99, 99},                                                         // pin 40
		{ 41, 23, "GP23", DIGITAL_IN | DIGITAL_OUT | SPI0TX | I2C1SCL| PWM3B  ,99 , 131},           // pseudo pin 41
		{ 42, 24, "GP24", DIGITAL_IN | DIGITAL_OUT | SPI1RX | UART1TX | I2C0SDA| PWM4A  ,99 , 4},   // pseudo pin 42
		{ 43, 25, "GP25", DIGITAL_IN | DIGITAL_OUT | UART1RX | I2C0SCL| PWM4B  ,99 , 132},          // pseudo pin 43
		{ 44, 29, "GP29", DIGITAL_IN | DIGITAL_OUT | ANALOG_IN | UART0RX | I2C0SCL | PWM6B, 3, 134},// pseudo pin 44
};

const char DaysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const int enableexFAT = 1;
void __not_in_flash_func(routinechecks)(void){
    char alive[]="\033[?25h";
    int c;
    if(tud_cdc_connected() && Option.SerialConsole==0){
        if((c = getchar_timeout_us(0))!=PICO_ERROR_TIMEOUT){  // store the byte in the ring buffer
            ConsoleRxBuf[ConsoleRxBufHead] = c;
            if(BreakKey && ConsoleRxBuf[ConsoleRxBufHead] == BreakKey) {// if the user wants to stop the progran
                MMAbort = true;                                        // set the flag for the interpreter to see
                ConsoleRxBufHead = ConsoleRxBufTail;                    // empty the buffer
            } else if(ConsoleRxBuf[ConsoleRxBufHead] ==keyselect && KeyInterrupt!=NULL){
                Keycomplete=1;
            } else {
                ConsoleRxBufHead = (ConsoleRxBufHead + 1) % CONSOLE_RX_BUF_SIZE;     // advance the head of the queue
                if(ConsoleRxBufHead == ConsoleRxBufTail) {                           // if the buffer has overflowed
                    ConsoleRxBufTail = (ConsoleRxBufTail + 1) % CONSOLE_RX_BUF_SIZE; // throw away the oldest char
                }
            }
        }
    }
	if(GPSchannel)processgps();
    CheckSDCard();
    if(!calibrate)ProcessTouch();
    if(USBKeepalive==0){
        MMPrintString(alive);
    }
    if(clocktimer==0 && Option.RTC){
        RtcGetTime();
        clocktimer=(1000*60*60);
    }
}

int getConsole(void) {
    int c=-1;
    CheckAbort();
    if(ConsoleRxBufHead != ConsoleRxBufTail) {                            // if the queue has something in it
        c = ConsoleRxBuf[ConsoleRxBufTail];
        ConsoleRxBufTail = (ConsoleRxBufTail + 1) % CONSOLE_RX_BUF_SIZE;   // advance the head of the queue
	}
    return c;
}
void putConsole(int c) {
    MMputchar(c,1);
}
// put a character out to the serial console
char MMputchar(char c, int flush) {
    if(Option.SerialConsole==0){
        if(tud_cdc_connected()){
            putc(c,stdout);
            if(isprint(c)) MMCharPos++;
            if(c == '\r') {
                MMCharPos = 1;
            }
            if(flush){
                USBKeepalive=USBKEEPALIVE;
                fflush(stdout);
            }
        }
    } else {
        int empty=uart_is_writable(Option.SerialConsole==1 ? uart0 : uart1);
		while(ConsoleTxBufTail == ((ConsoleTxBufHead + 1) % CONSOLE_TX_BUF_SIZE)); //wait if buffer full
		ConsoleTxBuf[ConsoleTxBufHead] = c;							// add the char
		ConsoleTxBufHead = (ConsoleTxBufHead + 1) % CONSOLE_TX_BUF_SIZE;		   // advance the head of the queue
		if(empty){
	        uart_set_irq_enables(Option.SerialConsole==1 ? uart0 : uart1, true, true);
			irq_set_pending(Option.SerialConsole==1 ? UART0_IRQ : UART1_IRQ);
		}
    }
    return c;
}
// returns the number of character waiting in the console input queue
int kbhitConsole(void) {
    int i;
    i = ConsoleRxBufHead - ConsoleRxBufTail;
    if(i < 0) i += CONSOLE_RX_BUF_SIZE;
    return i;
}
// check if there is a keystroke waiting in the buffer and, if so, return with the char
// returns -1 if no char waiting
// the main work is to check for vt100 escape code sequences and map to Maximite codes
int MMInkey(void) {
    unsigned int c = -1;                                            // default no character
    unsigned int tc = -1;                                           // default no character
    unsigned int ttc = -1;                                          // default no character
    static unsigned int c1 = -1;
    static unsigned int c2 = -1;
    static unsigned int c3 = -1;
    static unsigned int c4 = -1;

    if(c1 != -1) {                                                  // check if there are discarded chars from a previous sequence
        c = c1; c1 = c2; c2 = c3; c3 = c4; c4 = -1;                 // shuffle the queue down
        return c;                                                   // and return the head of the queue
    }

    c = getConsole();                                               // do discarded chars so get the char
    if(c == 0x1b) {
        InkeyTimer = 0;                                             // start the timer
        while((c = getConsole()) == -1 && InkeyTimer < 30);         // get the second char with a delay of 30mS to allow the next char to arrive
        if(c == 'O'){   //support for many linux terminal emulators
            while((c = getConsole()) == -1 && InkeyTimer < 50);        // delay some more to allow the final chars to arrive, even at 1200 baud
            if(c == 'P') return F1;
            if(c == 'Q') return F2;
            if(c == 'R') return F3;
            if(c == 'S') return F4;
            if(c == '2'){
                while((tc = getConsole()) == -1 && InkeyTimer < 70);        // delay some more to allow the final chars to arrive, even at 1200 baud
                if(tc == 'R') return F3 + 0x20;
                c1 = 'O'; c2 = c; c3 = tc; return 0x1b;                 // not a valid 4 char code
            }
            c1 = 'O'; c2 = c; return 0x1b;                 // not a valid 4 char code
        }
        if(c != '[') { c1 = c; return 0x1b; }                       // must be a square bracket
        while((c = getConsole()) == -1 && InkeyTimer < 50);         // get the third char with delay
        if(c == 'A') return UP;                                     // the arrow keys are three chars
        if(c == 'B') return DOWN;
        if(c == 'C') return RIGHT;
        if(c == 'D') return LEFT;
        if(c < '1' && c > '6') { c1 = '['; c2 = c; return 0x1b; }   // the 3rd char must be in this range
        while((tc = getConsole()) == -1 && InkeyTimer < 70);        // delay some more to allow the final chars to arrive, even at 1200 baud
        if(tc == '~') {                                             // all 4 char codes must be terminated with ~
            if(c == '1') return HOME;
            if(c == '2') return INSERT;
            if(c == '3') return DEL;
            if(c == '4') return END;
            if(c == '5') return PUP;
            if(c == '6') return PDOWN;
            c1 = '['; c2 = c; c3 = tc; return 0x1b;                 // not a valid 4 char code
        }
        while((ttc = getConsole()) == -1 && InkeyTimer < 90);       // get the 5th char with delay
        if(ttc == '~') {                                            // must be a ~
            if(c == '1') {
                if(tc >='1' && tc <= '5') return F1 + (tc - '1');   // F1 to F5
                if(tc >='7' && tc <= '9') return F6 + (tc - '7');   // F6 to F8
            }
            if(c == '2') {
                if(tc =='0' || tc == '1') return F9 + (tc - '0');   // F9 and F10
                if(tc =='3' || tc == '4') return F11 + (tc - '3');  // F11 and F12
                if(tc =='5') return F3 + 0x20;                      // SHIFT-F3
            }
        }
        // nothing worked so bomb out
        c1 = '['; c2 = c; c3 = tc; c4 = ttc;
        return 0x1b;
    }
    return c;
}
// get a line from the keyboard or a serial file handle
// filenbr == 0 means the console input
void MMgetline(int filenbr, char *p) {
	int c, nbrchars = 0;
	char *tp;
    while(1) {
        CheckAbort();												// jump right out if CTRL-C
//        if(FileTable[filenbr].com > MAXCOMPORTS && FileEOF(filenbr)) break;
        c = MMfgetc(filenbr);
        if(c == -1) continue;                                       // keep looping if there are no chars

        // if this is the console, check for a programmed function key and insert the text
        if(filenbr == 0) {
            tp = NULL;
            if(c == F2)  tp = "RUN";
            if(c == F3)  tp = "LIST";
            if(c == F4)  tp = "EDIT";
            if(c == F10) tp = "AUTOSAVE";
            if(c == F11) tp = "XMODEM RECEIVE";
            if(c == F12) tp = "XMODEM SEND";
            if(c == F5) tp = Option.F5key;
            if(c == F6) tp = Option.F6key;
            if(c == F7) tp = Option.F7key;
            if(c == F8) tp = Option.F8key;
            if(c == F9) tp = Option.F9key;
            if(tp) {
                strcpy(p, tp);
                if(EchoOption) { MMPrintString(tp); MMPrintString("\r\n"); }
                return;
            }
        }

		if(c == '\t') {												// expand tabs to spaces
			 do {
				if(++nbrchars > MAXSTRLEN) error("Line is too long");
				*p++ = ' ';
				if(filenbr == 0 && EchoOption) MMputchar(' ',1);
			} while(nbrchars % 4);
			continue;
		}

		if(c == '\b') {												// handle the backspace
			if(nbrchars) {
				if(filenbr == 0 && EchoOption) MMPrintString("\b \b");
				nbrchars--;
				p--;
			}
			continue;
		}

        if(c == '\n') {                                             // what to do with a newline
                break;                                              // a newline terminates a line (for a file)
        }

        if(c == '\r') {
            if(filenbr == 0 && EchoOption) {
                MMPrintString("\r\n");
                break;                                              // on the console this means the end of the line - stop collecting
            } else
                continue ;                                          // for files loop around looking for the following newline
        }
        
		if(isprint(c)) {
			if(filenbr == 0 && EchoOption) MMputchar(c,1);           // The console requires that chars be echoed
		}
		if(++nbrchars > MAXSTRLEN) error("Line is too long");		// stop collecting if maximum length
		*p++ = c;													// save our char
	}
	*p = 0;
}
// insert a string into the start of the lastcmd buffer.
// the buffer is a sequence of strings separated by a zero byte.
// using the up arrow usere can call up the last few commands executed.
void InsertLastcmd(unsigned char *s) {
int i, slen;
    if(strcmp(lastcmd, s) == 0) return;                             // don't duplicate
    slen = strlen(s);
    if(slen < 1 || slen > STRINGSIZE*4 - 1) return;
    slen++;
    for(i = STRINGSIZE*4 - 1; i >=  slen ; i--)
        lastcmd[i] = lastcmd[i - slen];                             // shift the contents of the buffer up
    strcpy(lastcmd, s);                                             // and insert the new string in the beginning
    for(i = STRINGSIZE*4 - 1; lastcmd[i]; i--) lastcmd[i] = 0;             // zero the end of the buffer
}

void EditInputLine(void) {
    char *p = NULL;
    char buf[MAXKEYLEN + 3];
    int lastcmd_idx, lastcmd_edit;
    int insert, startline, maxchars;
    int CharIndex, BufEdited;
    int c, i, j;
    maxchars = Option.Width;
    if(strlen(inpbuf) >= maxchars) {
        MMPrintString(inpbuf);
        error("Line is too long to edit");
    }
    startline = MMCharPos - 1;                                                          // save the current cursor position
    MMPrintString(inpbuf);                                                              // display the contents of the input buffer (if any)
    CharIndex = strlen(inpbuf);                                                         // get the current cursor position in the line
    insert = false;
//    Cursor = C_STANDARD;
    lastcmd_edit = lastcmd_idx = 0;
    BufEdited = false; //(CharIndex != 0);
    while(1) {
        c = MMgetchar();
        if(c == TAB) {
            strcpy(buf, "        ");
            switch (Option.Tab) {
              case 2:
                buf[2 - (CharIndex % 2)] = 0; break;
              case 3:
                buf[3 - (CharIndex % 3)] = 0; break;
              case 4:
                buf[4 - (CharIndex % 4)] = 0; break;
              case 8:
                buf[8 - (CharIndex % 8)] = 0; break;
            }
        } else {
            buf[0] = c;
            buf[1] = 0;
        }
        do {
            switch(buf[0]) {
                case '\r':
                case '\n':  //if(autoOn && atoi(inpbuf) > 0) autoNext = atoi(inpbuf) + autoIncr;
                            //if(autoOn && !BufEdited) *inpbuf = 0;
                            goto saveline;
                            break;

                case '\b':  if(CharIndex > 0) {
                                BufEdited = true;
                                i = CharIndex - 1;
                                for(p = inpbuf + i; *p; p++) *p = *(p + 1);                 // remove the char from inpbuf
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; }         // go to the beginning of the line
                                MMPrintString(inpbuf); MMputchar(' ',0); MMputchar('\b',0);     // display the line and erase the last char
                                for(CharIndex = strlen(inpbuf); CharIndex > i; CharIndex--)
                                    MMputchar('\b',0);  
                                fflush(stdout);                                      // return the cursor to the righ position
                            }
                            break;

                case CTRLKEY('S'):
                case LEFT:  if(CharIndex > 0) {
                                if(CharIndex == strlen(inpbuf)) {
                                    insert = true;
      //                              Cursor = C_INSERT;
                                }
                                MMputchar('\b',1);
                                CharIndex--;
                            }
                            break; 

                case CTRLKEY('D'):
                case RIGHT: if(CharIndex < strlen(inpbuf)) {
                                MMputchar(inpbuf[CharIndex],1);
                                CharIndex++;
                            }
                            break;

                case CTRLKEY(']'):
                case DEL:   if(CharIndex < strlen(inpbuf)) {
                                BufEdited = true;
                                i = CharIndex;
                                for(p = inpbuf + i; *p; p++) *p = *(p + 1);                 // remove the char from inpbuf
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; }         // go to the beginning of the line
                                MMPrintString(inpbuf); MMputchar(' ',0); MMputchar('\b',0);     // display the line and erase the last char
                                for(CharIndex = strlen(inpbuf); CharIndex > i; CharIndex--)
                                    MMputchar('\b',0);   
                                fflush(stdout);                                     // return the cursor to the right position
                            }
                            break;

                case CTRLKEY('N'):
                case INSERT:insert = !insert;
//                            Cursor = C_STANDARD + insert;
                            break;

                case CTRLKEY('U'):
                case HOME:  if(CharIndex > 0) {
                                if(CharIndex == strlen(inpbuf)) {
                                    insert = true;
//                                    Cursor = C_INSERT;
                                }
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; }
                                fflush(stdout);
                            }
                            break;

                case CTRLKEY('K'):
                case END:   while(CharIndex < strlen(inpbuf))
                                MMputchar(inpbuf[CharIndex++],0);
                                fflush(stdout);
                            break;

/*            if(c == F2)  tp = "RUN";
            if(c == F3)  tp = "LIST";
            if(c == F4)  tp = "EDIT";
            if(c == F10) tp = "AUTOSAVE";
            if(c == F11) tp = "XMODEM RECEIVE";
            if(c == F12) tp = "XMODEM SEND";
            if(c == F5) tp = Option.F5key;
            if(c == F6) tp = Option.F6key;
            if(c == F7) tp = Option.F7key;
            if(c == F8) tp = Option.F8key;
            if(c == F9) tp = Option.F9key;*/
                case 0x91:
                    break;
                case 0x92:
                    strcpy(&buf[1],"RUN\r\n");
                    break;
                case 0x93:
                    strcpy(&buf[1],"LIST\r\n");
                    break;
                case 0x94:
                    strcpy(&buf[1],"EDIT\r\n");
                    break;
                case 0x95:
                    if(*Option.F5key)strcpy(&buf[1],Option.F5key);
                    break;
                case 0x96:
                    if(*Option.F6key)strcpy(&buf[1],Option.F6key);
                    break;
                case 0x97:
                    if(*Option.F7key)strcpy(&buf[1],Option.F7key);
                    break;
                case 0x98:
                    if(*Option.F8key)strcpy(&buf[1],Option.F8key);
                    break;
                case 0x99:
                    if(*Option.F9key)strcpy(&buf[1],Option.F9key);
                    break;
                case 0x9a:
                    strcpy(&buf[1],"AUTOSAVE\r\n");
                    break;
                case 0x9b:
                    strcpy(&buf[1],"XMODEM RECEIVE\r\n");
                    break;
                 case 0x9c:
                    strcpy(&buf[1],"XMODEM SEND\r\n");
                    break;
                case CTRLKEY('E'):
                case UP:    if(!(BufEdited /*|| autoOn || CurrentLineNbr */)) {
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; } 
                                fflush(stdout);       // go to the beginning of line
                                if(lastcmd_edit) {
                                    i = lastcmd_idx + strlen(&lastcmd[lastcmd_idx]) + 1;    // find the next command
                                    if(lastcmd[i] != 0 && i < STRINGSIZE*4 - 1) lastcmd_idx = i;  // and point to it for the next time around
                                } else
                                    lastcmd_edit = true;
                                strcpy(inpbuf, &lastcmd[lastcmd_idx]);                      // get the command into the buffer for editing
                                goto insert_lastcmd;
                            }
                            break;


                case CTRLKEY('X'):
                case DOWN:  if(!(BufEdited /*|| autoOn || CurrentLineNbr */)) {
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; }   
                                fflush(stdout);      // go to the beginning of line
                                if(lastcmd_idx == 0)
                                    *inpbuf = lastcmd_edit = 0;
                                else {
                                    for(i = lastcmd_idx - 2; i > 0 && lastcmd[i - 1] != 0; i--);// find the start of the previous command
                                    lastcmd_idx = i;                                        // and point to it for the next time around
                                    strcpy(inpbuf, &lastcmd[i]);                            // get the command into the buffer for editing
                                }
                                goto insert_lastcmd;                                        // gotos are bad, I know, I know
                            }
                            break;

                insert_lastcmd:                                                             // goto here if we are just editing a command
                            if(strlen(inpbuf) + startline >= maxchars) {                    // if the line is too long
                                while(CharIndex)  { MMputchar('\b',0); CharIndex--; }         // go to the start of the line
                                MMPrintString(inpbuf);                                      // display the offending line
                                error("Line is too long to edit");
                            }
                            MMPrintString(inpbuf);                                          // display the line
                            CharIndex = strlen(inpbuf);                                     // get the current cursor position in the line
/*                            for(i = 1; i <= maxchars - strlen(inpbuf) - startline; i++) {
                                MMputchar(' ',0);                                             // erase the rest of the line
                                CharIndex++;
                            }
                            while(CharIndex > strlen(inpbuf)) { MMputchar('\b',0); CharIndex--; } // return the cursor to the right position
*/
                            MMPrintString("\033[0K");
//                            fflush(stdout);
                            break;

                default:    if(buf[0] >= ' ' && buf[0] < 0x7f) {
                                BufEdited = true;                                           // this means that something was typed
                                i = CharIndex;
                                j = strlen(inpbuf);
                                if(insert) {
                                    if(strlen(inpbuf) >= maxchars - 1) break;               // sorry, line full
                                    for(p = inpbuf + strlen(inpbuf); j >= CharIndex; p--, j--) *(p + 1) = *p;
                                    inpbuf[CharIndex] = buf[0];                             // insert the char
                                    MMPrintString(&inpbuf[CharIndex]);                      // display new part of the line
                                    CharIndex++;
                                    for(j = strlen(inpbuf); j > CharIndex; j--)
                                        MMputchar('\b',0); 
                                        fflush(stdout);                                   // return the cursor to the right position
                                } else {
                                    inpbuf[strlen(inpbuf) + 1] = 0;                         // incase we are adding to the end of the string
                                    inpbuf[CharIndex++] = buf[0];                           // overwrite the char
                                    MMputchar(buf[0],1);                                      // display it
                                    if(CharIndex + startline >= maxchars) {                 // has the input gone beyond the end of the line?
                                        MMgetline(0, inpbuf);                               // use the old fashioned way of getting the line
                                        //if(autoOn && atoi(inpbuf) > 0) autoNext = atoi(inpbuf) + autoIncr;
                                        goto saveline;
                                    }
                                }
                            }
                            break;
            }
            for(i = 0; i < MAXKEYLEN + 1; i++) buf[i] = buf[i + 1];                             // shuffle down the buffer to get the next char
        } while(*buf);
    if(CharIndex == strlen(inpbuf)) {
        insert = false;
//        Cursor = C_STANDARD;
        }
    }

    saveline:
//    Cursor = C_STANDARD;
    MMPrintString("\r\n");
}
// get a keystroke.  Will wait forever for input
// if the unsigned char is a cr then replace it with a newline (lf)
int MMgetchar(void) {
	int c;
	do {
		c=MMInkey();
	} while(c == -1);
	return c;
}
// print a string to the console interfaces
void MMPrintString(char* s) {
    while(*s) {
        MMputchar(*s,0);
        s++;
    }
    fflush(stdout);
    USBKeepalive=USBKEEPALIVE;
}

/*void myprintf(char *s){
   fputs(s,stdout);
     fflush(stdout);
}*/

void mT4IntEnable(int status){
	if(status){
		processtick=1;
	} else{
		processtick=0;
	}
}

/*void init_systick()
{ 
	systick_hw->csr = 0; 	    //Disable 
	systick_hw->rvr = 249999; //Standard System clock (125Mhz)/ (rvr value + 1) = 1ms 
    systick_hw->csr = 0x7;      //Enable Systic, Enable Exceptions	
}*/
bool __not_in_flash_func(timer_callback)(repeating_timer_t *rt)
{
    mSecTimer++;                                                      // used by the TIMER function
    if(processtick){
        static int IrTimeout, IrTick, NextIrTick;
        int ElapsedMicroSec, IrDevTmp, IrCmdTmp;
        AHRSTimer++;
        InkeyTimer++;                                                     // used to delay on an escape character
        mSecTimer++;                                                      // used by the TIMER function
        PauseTimer++;													// used by the PAUSE command
        IntPauseTimer++;												// used by the PAUSE command inside an interrupt
        ds18b20Timer++;
		GPSTimer++;
        I2CTimer++;
        if(clocktimer)clocktimer--;
        if(Timer2)Timer2--;
        if(Timer1)Timer1--;
        if(USBKeepalive)USBKeepalive--;
        if(diskchecktimer)diskchecktimer--;
        if(InterruptUsed) {
            int i;
            for(i = 0; i < NBRSETTICKS; i++) TickTimer[i]++;			// used in the interrupt tick
        }
        if(WDTimer) {
            if(--WDTimer == 0) {
                _excep_code = WATCHDOG_TIMEOUT;
                SoftReset();                                            // crude way of implementing a watchdog timer.
            }
        }
        if(PulseActive) {
            int i;
            for(PulseActive = i = 0; i < NBR_PULSE_SLOTS; i++) {
                if(PulseCnt[i] > 0) {                                   // if the pulse timer is running
                    PulseCnt[i]--;                                      // and decrement our count
                    if(PulseCnt[i] == 0)                                // if this is the last count reset the pulse
                        PinSetBit(PulsePin[i], LATINV);
                    else
                        PulseActive = true;                             // there is at least one pulse still active
                }
            }
        }
        ElapsedMicroSec = readIRclock();
        if(IrState > IR_WAIT_START && ElapsedMicroSec > 15000) IrReset();
        IrCmdTmp = -1;
        
        // check for any Sony IR receive activity
        if(IrState == SONY_WAIT_BIT_START && ElapsedMicroSec > 2800 && (IrCount == 12 || IrCount == 15 || IrCount == 20)) {
            IrDevTmp = ((IrBits >> 7) & 0b11111);
            IrCmdTmp = (IrBits & 0b1111111) | ((IrBits >> 5) & ~0b1111111);
        }
        
        // check for any NEC IR receive activity
        if(IrState == NEC_WAIT_BIT_END && IrCount == 32) {
            // check if it is a NON extended address and adjust if it is
            if((IrBits >> 24) == ~((IrBits >> 16) & 0xff)) IrBits = (IrBits & 0x0000ffff) | ((IrBits >> 8) & 0x00ff0000);
            IrDevTmp = ((IrBits >> 16) & 0xffff);
            IrCmdTmp = ((IrBits >> 8) & 0xff);
        }
    // check on the touch panel, is the pen down?

    TouchTimer++;
    if(CheckGuiFlag) CheckGuiTimeouts();                            // are blinking LEDs in use?  If so count down their timers

    if(TouchIrqPortAddr && TOUCH_GETIRQTRIS){                       // is touch enabled and the PEN IRQ pin an input?
        if(TOUCH_DOWN) {                                            // is the pen down
            if(!TouchState) {                                       // yes, it is.  If we have not reported this before
                TouchState = TouchDown = true;                      // set the flags
//                TouchUp = false;
            }
        } else {
            if(TouchState) {                                        // the pen is not down.  If we have not reported this before
                TouchState/* = TouchDown*/ = false;                     // set the flags
                TouchUp = true;
            }
        }
    }

    if(ClickTimer) {
        ClickTimer--;
        if(Option.TOUCH_Click) PinSetBit(Option.TOUCH_Click, ClickTimer ? LATSET : LATCLR);
    }

    // now process the IR message, this includes handling auto repeat while the key is held down
    // IrTick counts how many mS since the key was first pressed
    // NextIrTick is used to time the auto repeat
    // IrTimeout is used to detect when the key is released
    // IrGotMsg is a signal to the interrupt handler that an interrupt is required
    if(IrCmdTmp != -1) {
        if(IrTick > IrTimeout) {
            // this is a new keypress
            IrTick = 0;
            NextIrTick = 650;
        }
        if(IrTick == 0 || IrTick > NextIrTick) {
            if(IrVarType & 0b01)
                *(MMFLOAT *)IrDev = IrDevTmp;
            else
                *(long long int *)IrDev = IrDevTmp;
            if(IrVarType & 0b10)
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
	if(ExtCurrentConfig[Option.INT1pin] == EXT_PER_IN) INT1Count++;
	if(ExtCurrentConfig[Option.INT2pin] == EXT_PER_IN) INT2Count++;
	if(ExtCurrentConfig[Option.INT3pin] == EXT_PER_IN) INT3Count++;
	if(ExtCurrentConfig[Option.INT4pin] == EXT_PER_IN) INT4Count++;
    if(ExtCurrentConfig[Option.INT1pin] == EXT_FREQ_IN && --INT1Timer <= 0) { INT1Value = INT1Count; INT1Count = 0; INT1Timer = INT1InitTimer; }
    if(ExtCurrentConfig[Option.INT2pin] == EXT_FREQ_IN && --INT2Timer <= 0) { INT2Value = INT2Count; INT2Count = 0; INT2Timer = INT2InitTimer; }
    if(ExtCurrentConfig[Option.INT3pin] == EXT_FREQ_IN && --INT3Timer <= 0) { INT3Value = INT3Count; INT3Count = 0; INT3Timer = INT3InitTimer; }
    if(ExtCurrentConfig[Option.INT4pin] == EXT_FREQ_IN && --INT4Timer <= 0) { INT4Value = INT4Count; INT4Count = 0; INT4Timer = INT4InitTimer; }

    ////////////////////////////////// this code runs once a second /////////////////////////////////
    if(++SecondsTimer >= 1000) {
        SecondsTimer -= 1000; 
        if(ExtCurrentConfig[PinDef[HEARTBEATpin].pin]==EXT_HEARTBEAT)gpio_xor_mask(1<<PICO_DEFAULT_LED_PIN);
            // keep track of the time and date
        if(++second >= 60) {
            second = 0 ;
            if(++minute >= 60) {
                minute = 0;
                if(++hour >= 24) {
                    hour = 0;
                    if(++day > DaysInMonth[month] + ((month == 2 && (year % 4) == 0)?1:0)) {
                        day = 1;
                        if(++month > 12) {
                            month = 1;
                            year++;
                        }
                    }
                }
            }
        }
    }
    }
  return 1;
}
void __not_in_flash_func(CheckAbort)(void) {
    routinechecks();
    if(MMAbort) {
        WDTimer = 0;                                                // turn off the watchdog timer
        calibrate=0;
        memset(inpbuf,0,STRINGSIZE);
        longjmp(mark, 1);                                           // jump back to the input prompt
    }
}
void PRet(void){
    MMPrintString("\r\n");
}

void PInt(int64_t n) {
    char s[20];
    IntToStr(s, (int64_t)n, 10);
    MMPrintString(s);
}

void PIntComma(int64_t n) {
    MMPrintString(", "); PInt(n);
}

void PIntH(unsigned long long int n) {
    char s[20];
    IntToStr(s, (int64_t)n, 16);
    MMPrintString(s);
}
void PIntHC(unsigned long long int n) {
    MMPrintString(", "); PIntH(n);
}

void PFlt(MMFLOAT flt){
	   char s[20];
	   FloatToStr(s, flt, 4,4, ' ');
	    MMPrintString(s);
}
void PFltComma(MMFLOAT n) {
    MMPrintString(", "); PFlt(n);
}
void sigbus(void){
    MMPrintString("Error: Invalid address - resetting\r\n");
    SoftReset();
}

int main() {
    static int ErrorInPrompt;
    repeating_timer_t timer;
    int i;
    LoadOptions();
    if(Option.Baudrate == 0 ||
        !(Option.Tab==2 || Option.Tab==3 || Option.Tab==4 ||Option.Tab==8) ||
        !(Option.Autorun>=0 && Option.Autorun<=11) ||
        !(Option.CPU_Speed >=48000 && Option.CPU_Speed<=MAX_CPU) ||
        !((Option.PROG_FLASH_SIZE + Option.HEAP_SIZE + MRoundUpK2(Option.MaxCtrls*sizeof(struct s_ctrl)))==(160*1024))
        ){
        ResetAllFlash();              // init the options if this is the very first startup
        _excep_code=0;
        SoftReset();
    }
    if(Option.CPU_Speed>200000)vreg_set_voltage(VREG_VOLTAGE_1_20);
    uint64_t cpu=((Option.CPU_Speed >=48000 && Option.CPU_Speed<=MAX_CPU) ? Option.CPU_Speed : 125000);
    set_sys_clock_khz(cpu, true);
    m_alloc(M_VAR);
    m_alloc(M_PROG);                                           // init the variables for program memory
    pico_get_unique_board_id_string (id_out,12);
    // The previous line automatically detached clk_peri from clk_sys, and
    // attached it to pll_usb, so that clk_peri won't be disturbed by future
    // changes to system clock or system PLL. If we need higher clk_peri
    // frequencies, we can attach clk_peri directly back to system PLL (no
    // divider available) and then use the clk_sys divider to scale clk_sys
    // independently of clk_peri.
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        cpu * 1000,                               // Input frequency
        cpu * 1000                                // Output (must be same as no divider)
    );
    systick_hw->csr = 0x5;
    systick_hw->rvr = 0x00FFFFFF;
    ticks_per_second = Option.CPU_Speed*1000;
    // The serial clock won't vary from this point onward, so we can configure
    // the UART etc.
    stdio_set_translate_crlf(&stdio_usb, false);
    LoadOptions();
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    add_repeating_timer_us(-1000, timer_callback, NULL, &timer);
    if(Option.SerialConsole==0) while (!tud_cdc_connected() && mSecTimer<5000) {}
    InitReservedIO();
    ClearExternalIO();
    ConsoleRxBufHead = 0;
    ConsoleRxBufTail = 0;
    ConsoleTxBufHead = 0;
    ConsoleTxBufTail = 0;
    InitHeap();              										// initilise memory allocation
    uSecFunc(10);
//    LoadOptions();
    // negative timeout means exact delay (rather than delay between callbacks)
	OptionErrorSkip = false;
	InitBasic();
    InitDisplaySPI(0);
    InitDisplayI2C(0);
    InitTouch();
    ErrorInPrompt = false;
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION,sigbus);
    while((i=getConsole())!=-1){}

    RestoreProg();

    if(!(_excep_code == RESTART_NOAUTORUN || _excep_code == WATCHDOG_TIMEOUT)){
        if(Option.Autorun==0 ){
            if(!(_excep_code == RESET_COMMAND))MMPrintString(MES_SIGNON); //MMPrintString(b);                                 // print sign on message
        } else {
            int j=Option.PROG_FLASH_SIZE>>2,i=Option.Autorun;
            int *pp=(int *)(flash_target_contents+(i-1)*MAX_PROG_SIZE);
            int *qq=(int *)ProgMemory;
            while(j--)*qq++ = *pp++;
            FlashLoad=i;
            if(*ProgMemory != 0x01 ) MMPrintString(MES_SIGNON); 
        }
    }
//    PIntH((uint32_t)ProgMemory);PIntHC((uint32_t)MMHeap);PIntHC((uint32_t)Ctrl);PIntHC((uint32_t)&Memory[MEMORY_SIZE]);PRet();
//    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
//    uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
//    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
//    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
//    PInt(f_rosc);PIntComma(f_pll_sys);PIntComma(f_clk_sys);PIntComma(f_clk_peri);PRet();
//    PInt(sizeof(struct  option_s));PRet();
//    PInt(PinDef[Option.SD_CS].GPno);PIntComma(PinDef[Option.SYSTEM_MOSI].GPno);PIntComma(PinDef[Option.SYSTEM_MISO].GPno);PIntComma(PinDef[Option.SYSTEM_CLK].GPno);PR5et();
    if(_excep_code == WATCHDOG_TIMEOUT) {
        WatchdogSet = true;                                 // remember if it was a watchdog timeout
        MMPrintString("\r\n\nWatchdog timeout\r\n");
    }
 	*tknbuf = 0;
     ContinuePoint = nextstmt;                               // in case the user wants to use the continue command
	if(setjmp(mark) != 0) {
     // we got here via a long jump which means an error or CTRL-C or the program wants to exit to the command prompt
     ContinuePoint = nextstmt;                               // in case the user wants to use the continue command
		*tknbuf = 0;											// we do not want to run whatever is in the token buffer
    } else {
        if(*ProgMemory == 0x01 ) ClearVars(0);
        else {
//            PInt(*ProgMemory);PRet();
            ClearProgram();
        }
        PrepareProgram(true);
        if(FindSubFun("MM.STARTUP", 0) >= 0) ExecuteProgram("MM.STARTUP\0");
        if(Option.Autorun && _excep_code != RESTART_DOAUTORUN) {
            ClearRuntime();
            PrepareProgram(true);
            if(*ProgMemory == 0x01 ){
                ExecuteProgram(ProgMemory);  
            }  else {
                Option.Autorun=0;
                SaveOptions();
            }       
        }
    }
    while(1) {
#if defined(MX470)
    if(Option.DISPLAY_CONSOLE) {
        SetFont(PromptFont);
        gui_fcolour = PromptFC;
        gui_bcolour = PromptBC;
        if(CurrentX != 0) MMPrintString("\r\n");                    // prompt should be on a new line
    }
    if(CurrentLinePtr) {
        CloseAudio();
        vol_left = 100; vol_right = 100;
    }
#endif
        MMAbort = false;
        BreakKey = BREAK_KEY;
        EchoOption = true;
        LocalIndex = 0;                                             // this should not be needed but it ensures that all space will be cleared
        ClearTempMemory();                                          // clear temp string space (might have been used by the prompt)
        CurrentLinePtr = NULL;                                      // do not use the line number in error reporting
        if(MMCharPos > 1) MMPrintString("\r\n");                    // prompt should be on a new line
        while(Option.PIN && !IgnorePIN) {
            _excep_code = PIN_RESTART;
            if(Option.PIN == 99999999)                              // 99999999 is permanent lockdown
                MMPrintString("Console locked, press enter to restart: ");
            else
                MMPrintString("Enter PIN or 0 to restart: ");
            MMgetline(0, inpbuf);
            if(Option.PIN == 99999999) SoftReset();
            if(*inpbuf != 0) {
                uSec(3000000);
                i = atoi(inpbuf);
                if(i == 0) SoftReset();
                if(i == Option.PIN) {
                    IgnorePIN = true;
                    break;
                }
            }
        }
        _excep_code = 0;
        PrepareProgram(false);
        if(!ErrorInPrompt && FindSubFun("MM.PROMPT", 0) >= 0) {
            ErrorInPrompt = true;
            ExecuteProgram("MM.PROMPT\0");
        } else
            MMPrintString("> ");                                    // print the prompt
        ErrorInPrompt = false;
        EditInputLine();
        InsertLastcmd(inpbuf);                                  // save in case we want to edit it later
//        MMgetline(0, inpbuf);                                       // get the input
        if(!*inpbuf) continue;                                      // ignore an empty line
	  char *p=inpbuf;
	  skipspace(p);
	  if(*p=='*'){ //shortform RUN command so convert to a normal version
		  memmove(&p[4],&p[0],strlen(p)+1);
		  p[0]='R';p[1]='U';p[2]='N';p[3]='$';p[4]=34;
		  char  *q;
		  if((q=strchr(p,' ')) != 0){ //command line after the filename
			  *q=','; //chop the command at the first space character
			  memmove(&q[1],&q[0],strlen(q)+1);
			  q[0]=34;
		  } else strcat(p,"\"");
		  p[3]=' ';
//		  PRet();MMPrintString(inpbuf);PRet();
	  }
        tokenise(true);                                             // turn into executable code
        ExecuteProgram(tknbuf);                                     // execute the line straight away
        memset(inpbuf,0,STRINGSIZE);
    }
}
volatile union u_flash {
  uint64_t i64[4];
  uint8_t  i8[32];
  uint32_t  i32[8];
} MemWord;
volatile int mi8p=0;
// globals used when writing bytes to flash
volatile uint32_t realmempointer;
void MemWriteBlock(void){
    int i;
    uint32_t address=realmempointer-32;
//    if(address % 32)error("Memory write address");
    memcpy((char *)address,(char*)&MemWord.i64[0],32);
	for(i=0;i<8;i++)MemWord.i32[i]=0xFFFFFFFF;
}
void MemWriteByte(unsigned char b) {
	realmempointer++;
	MemWord.i8[mi8p]=b;
	mi8p++;
	mi8p %= 32;
	if(mi8p==0){
		MemWriteBlock();
	}
}
void MemWriteWord(unsigned int i) {
	MemWriteByte(i & 0xFF);
	MemWriteByte((i>>8) & 0xFF);
	MemWriteByte((i>>16) & 0xFF);
	MemWriteByte((i>>24) & 0xFF);
}

void MemWriteAlign(void) {
	  while(mi8p != 0) {
		  MemWriteByte(0x0);
	  }
	  MemWriteWord(0xFFFFFFFF);
}



void MemWriteClose(void){
	  while(mi8p != 0) {
		  MemWriteByte(0xff);
	  }

}

// takes a pointer to RAM containing a program (in clear text) and writes it to memory in tokenised format
void SaveProgramToMemory(unsigned char *pm, int msg) {
    unsigned char *p, endtoken, fontnbr, prevchar = 0, buf[STRINGSIZE];
    int nbr, i, n, SaveSizeAddr;
    uint32_t storedupdates[MAXCFUNCTION], updatecount=0, realmemsave, retvalue;
    memcpy(buf, tknbuf, STRINGSIZE);                                // save the token buffer because we are going to use it
    realmempointer=(uint32_t)ProgMemory;
    memset(ProgMemory,0xFF,Option.PROG_FLASH_SIZE);
    nbr = 0;
    // this is used to count the number of bytes written to flash
    while(*pm) {
        p = inpbuf;
        while(!(*pm == 0 || *pm == '\r' || (*pm == '\n' && prevchar != '\r'))) {
            if(*pm == TAB) {
                do {*p++ = ' ';
                    if((p - inpbuf) >= MAXSTRLEN) goto exiterror1;
                } while((p - inpbuf) % 2);
            } else {
                if(isprint((uint8_t)*pm)) {
                    *p++ = *pm;
                    if((p - inpbuf) >= MAXSTRLEN) goto exiterror1;
                }
            }
            prevchar = *pm++;
        }
        if(*pm) prevchar = *pm++;                                   // step over the end of line char but not the terminating zero
        *p = 0;                                                     // terminate the string in inpbuf

        if(*inpbuf == 0 && (*pm == 0 || (!isprint((uint8_t)*pm) && pm[1] == 0))) break; // don't save a trailing newline

        tokenise(false);                                            // turn into executable code
        p = tknbuf;
        while(!(p[0] == 0 && p[1] == 0)) {
            MemWriteByte(*p++); nbr++;

            if((int)((char *)realmempointer - (uint32_t)ProgMemory) >= Option.PROG_FLASH_SIZE - 5) error("Not enough memory");
//                goto exiterror1;
        }
        MemWriteByte(0); nbr++;                              // terminate that line in flash
    }
    MemWriteByte(0);
    MemWriteAlign();                                            // this will flush the buffer and step the flash write pointer to the next word boundary
    // now we must scan the program looking for CFUNCTION/CSUB/DEFINEFONT statements, extract their data and program it into the flash used by  CFUNCTIONs
     // programs are terminated with two zero bytes and one or more bytes of 0xff.  The CFunction area starts immediately after that.
     // the format of a CFunction/CSub/Font in flash is:
     //   Unsigned Int - Address of the CFunction/CSub in program memory (points to the token representing the "CFunction" keyword) or NULL if it is a font
     //   Unsigned Int - The length of the CFunction/CSub/Font in bytes including the Offset (see below)
     //   Unsigned Int - The Offset (in words) to the main() function (ie, the entry point to the CFunction/CSub).  Omitted in a font.
     //   word1..wordN - The CFunction/CSub/Font code
     // The next CFunction/CSub/Font starts immediately following the last word of the previous CFunction/CSub/Font
    int firsthex=1;
    realmemsave= realmempointer;
    p = (char *)ProgMemory;                                              // start scanning program memory
    while(*p != 0xff) {
    	nbr++;
        if(*p == 0) p++;                                            // if it is at the end of an element skip the zero marker
        if(*p == 0) break;                                          // end of the program
        if(*p == T_NEWLINE) {
            CurrentLinePtr = p;
            p++;                                                    // skip the newline token
        }
        if(*p == T_LINENBR) p += 3;                                 // step over the line number

        skipspace(p);
        if(*p == T_LABEL) {
            p += p[1] + 2;                                          // skip over the label
            skipspace(p);                                           // and any following spaces
        }
        if(*p == cmdCSUB || *p == GetCommandValue("DefineFont")) {      // found a CFUNCTION, CSUB or DEFINEFONT token
            if(*p == GetCommandValue("DefineFont")) {
             endtoken = GetCommandValue("End DefineFont");
             p++;                                                // step over the token
             skipspace(p);
             if(*p == '#') p++;
             fontnbr = getint(p, 1, FONT_TABLE_SIZE);
                                                 // font 6 has some special characters, some of which depend on font 1
             if(fontnbr == 1 || fontnbr == 6 || fontnbr == 7) error("Cannot redefine fonts 1, 6 or 7");
             realmempointer+=4;
             skipelement(p);                                     // go to the end of the command
             p--;
            } else {
                endtoken = GetCommandValue("End CSub");
                realmempointer+=4;
                fontnbr = 0;
                firsthex=0;
            }
             SaveSizeAddr = realmempointer;                                // save where we are so that we can write the CFun size in here
             realmempointer+=4;
             p++;
             skipspace(p);
             if(!fontnbr) {
                 if(!isnamestart((uint8_t)*p))  error("Function name");
                 do { p++; } while(isnamechar((uint8_t)*p));
                 skipspace(p);
                 if(!(isxdigit((uint8_t)p[0]) && isxdigit((uint8_t)p[1]) && isxdigit((uint8_t)p[2]))) {
                     skipelement(p);
                     p++;
                    if(*p == T_NEWLINE) {
                        CurrentLinePtr = p;
                        p++;                                        // skip the newline token
                    }
                    if(*p == T_LINENBR) p += 3;                     // skip over a line number
                 }
             }
             do {
                 while(*p && *p != '\'') {
                     skipspace(p);
                     n = 0;
                     for(i = 0; i < 8; i++) {
                         if(!isxdigit((uint8_t)*p)) error("Invalid hex word");
                         if((int)((char *)realmempointer - (uint32_t)ProgMemory) >= Option.PROG_FLASH_SIZE - 5) error("Not enough memory");
                         n = n << 4;
                         if(*p <= '9')
                             n |= (*p - '0');
                         else
                             n |= (toupper(*p) - 'A' + 10);
                         p++;
                     }
                     realmempointer+=4;
                     skipspace(p);
                     if(firsthex){
                    	 firsthex=0;
                    	 if(((n>>16) & 0xff) < 0x20)error("Can't define non-printing characters");
                     }
                 }
                 // we are at the end of a embedded code line
                 while(*p) p++;                                      // make sure that we move to the end of the line
                 p++;                                                // step to the start of the next line
                 if(*p == 0) error("Missing END declaration");
                 if(*p == T_NEWLINE) {
                     CurrentLinePtr = p;
                     p++;                                            // skip the newline token
                 }
                 if(*p == T_LINENBR) p += 3;                         // skip over the line number
                 skipspace(p);
             } while(*p != endtoken);
             storedupdates[updatecount++]=realmempointer - SaveSizeAddr - 4;
         }
         while(*p) p++;                                              // look for the zero marking the start of the next element
     }
    realmempointer = realmemsave ;
    updatecount=0;
    p = (char *)ProgMemory;                                              // start scanning program memory
     while(*p != 0xff) {
     	nbr++;
         if(*p == 0) p++;                                            // if it is at the end of an element skip the zero marker
         if(*p == 0) break;                                          // end of the program
         if(*p == T_NEWLINE) {
             CurrentLinePtr = p;
             p++;                                                    // skip the newline token
         }
         if(*p == T_LINENBR) p += 3;                                 // step over the line number

         skipspace(p);
         if(*p == T_LABEL) {
             p += p[1] + 2;                                          // skip over the label
             skipspace(p);                                           // and any following spaces
         }
         if(*p == cmdCSUB || *p == GetCommandValue("DefineFont")) {      // found a CFUNCTION, CSUB or DEFINEFONT token
         if(*p == GetCommandValue("DefineFont")) {      // found a CFUNCTION, CSUB or DEFINEFONT token
             endtoken = GetCommandValue("End DefineFont");
             p++;                                                // step over the token
             skipspace(p);
             if(*p == '#') p++;
             fontnbr = getint(p, 1, FONT_TABLE_SIZE);
                                                 // font 6 has some special characters, some of which depend on font 1
             if(fontnbr == 1 || fontnbr == 6 || fontnbr == 7) error("Cannot redefine fonts 1, 6, or 7");

             MemWriteWord(fontnbr - 1);             // a low number (< FONT_TABLE_SIZE) marks the entry as a font
             skipelement(p);                                     // go to the end of the command
             p--;
         } else {
             endtoken = GetCommandValue("End CSub");
             MemWriteWord((unsigned int)p);               // if a CFunction/CSub save a pointer to the declaration
             fontnbr = 0;
         }
            SaveSizeAddr = realmempointer;                                // save where we are so that we can write the CFun size in here
             MemWriteWord(storedupdates[updatecount++]);                        // leave this blank so that we can later do the write
             p++;
             skipspace(p);
             if(!fontnbr) {
                 if(!isnamestart((uint8_t)*p))  error("Function name");
                 do { p++; } while(isnamechar(*p));
                 skipspace(p);
                 if(!(isxdigit(p[0]) && isxdigit(p[1]) && isxdigit(p[2]))) {
                     skipelement(p);
                     p++;
                    if(*p == T_NEWLINE) {
                        CurrentLinePtr = p;
                        p++;                                        // skip the newline token
                    }
                    if(*p == T_LINENBR) p += 3;                     // skip over a line number
                 }
             }
             do {
                 while(*p && *p != '\'') {
                     skipspace(p);
                     n = 0;
                     for(i = 0; i < 8; i++) {
                         if(!isxdigit(*p)) error("Invalid hex word");
                         if((int)((char *)realmempointer - (uint32_t)ProgMemory) >= Option.PROG_FLASH_SIZE - 5) error("Not enough memory");
                         n = n << 4;
                         if(*p <= '9')
                             n |= (*p - '0');
                         else
                             n |= (toupper(*p) - 'A' + 10);
                         p++;
                     }

                     MemWriteWord(n);
                     skipspace(p);
                 }
                 // we are at the end of a embedded code line
                 while(*p) p++;                                      // make sure that we move to the end of the line
                 p++;                                                // step to the start of the next line
                 if(*p == 0) error("Missing END declaration");
                 if(*p == T_NEWLINE) {
                    CurrentLinePtr = p;
                    p++;                                        // skip the newline token
                 }
                 if(*p == T_LINENBR) p += 3;                     // skip over a line number
                 skipspace(p);
             } while(*p != endtoken);
         }
         while(*p) p++;                                              // look for the zero marking the start of the next element
     }
     MemWriteWord(0xffffffff);                                // make sure that the end of the CFunctions is terminated with an erased word
     MemWriteClose();                                              // this will flush the buffer and step the flash write pointer to the next word boundary
    SaveProg();
    if(msg) {                                                       // if requested by the caller, print an informative message
        if(MMCharPos > 1) MMPrintString("\r\n");                    // message should be on a new line
        MMPrintString("Saved ");
        IntToStr(tknbuf, nbr + 3, 10);
        MMPrintString(tknbuf);
        MMPrintString(" bytes\r\n");
    }
    memcpy(tknbuf, buf, STRINGSIZE);                                // restore the token buffer in case there are other commands in it
//    initConsole();
    return;

    // we only get here in an error situation while writing the program to flash
    exiterror1:
        MemWriteByte(0); MemWriteByte(0); MemWriteByte(0);    // terminate the program in flash
        MemWriteClose();
        error("Not enough memory");
}

#ifdef __cplusplus
}
#endif

/// \end:uart_advanced[]
