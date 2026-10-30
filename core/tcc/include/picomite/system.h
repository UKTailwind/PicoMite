/* PicoMite TinyCC: <picomite/system.h> - the firmware, its board and the
   running program.  (API version PM_API_VERSION; see also <picomite.h>.) */
#ifndef _PICOMITE_SYSTEM_H
#define _PICOMITE_SYSTEM_H

/* The version of the <picomite/...> API this header describes.  A program
   built for a later one can check pm_api_version() before using what it
   added. */
#define PM_API_VERSION 1
unsigned pm_api_version(void);

/* pm_get_info(): the firmware and the board.  Set size first, so a program
   built with this header keeps working with a later firmware whose struct
   has grown (only the first 'size' bytes are filled). */
#define PM_CAP_DISPLAY     0x0001 /* a display is configured */
#define PM_CAP_FRAMEBUFFER 0x0002 /* pm_get_framebuffer() gives one now */
#define PM_CAP_PSRAM       0x0004
#define PM_CAP_SDCARD      0x0008 /* B: is configured */
#define PM_CAP_USB_HOST    0x0010 /* USB keyboard, mouse, gamepad */
#define PM_CAP_WIFI        0x0020
#define PM_CAP_BLUETOOTH   0x0040
#define PM_CAP_HDMI        0x0080
#define PM_CAP_VGA         0x0100

typedef struct pm_info
{
    unsigned size;        /* in: sizeof(pm_info) */
    unsigned api_version; /* PM_API_VERSION of the firmware */
    char firmware[16];    /* the version, e.g. "7.00.00RC6" */
    char device[32];      /* MM.DEVICE$, e.g. "PicoMiteHDMI RP2350B" */
    unsigned cpu_khz;     /* clock speed */
    unsigned sram_heap;   /* bytes of the SRAM heap (all of it, used or not) */
    unsigned psram;       /* bytes of PSRAM, 0 if there is none */
    int hres, vres;       /* MM.HRES, MM.VRES */
    unsigned caps;        /* PM_CAP_... */
} pm_info;

int pm_get_info(pm_info *info); /* 0, or -1 if info->size is too small */

/* Keep the firmware running during a long computation: USB, the keyboard,
   Bluetooth, the network and CTRL-C are serviced.  Output, input, pauses and
   the drawing calls do this anyway; call it in loops that do none of them.
   If CTRL-C was pressed the program ends here, as with BASIC. */
void pm_yield(void);

/* Has CTRL-C been pressed?  For a program that wants to tidy up before it
   ends: it should return from main() soon after. */
int pm_abort_requested(void);

/* time (also in <picomite.h>) */
unsigned long long pm_timer_us(void); /* microseconds since power-up */
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms); /* PAUSE */

#endif /* _PICOMITE_SYSTEM_H */
