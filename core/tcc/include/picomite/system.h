/* TinyCC: <picomite/system.h>. See docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_SYSTEM_H
#define _PICOMITE_SYSTEM_H

#define PM_API_VERSION 9
unsigned pm_api_version(void);

#define PM_CAP_DISPLAY     0x0001
#define PM_CAP_FRAMEBUFFER 0x0002
#define PM_CAP_PSRAM       0x0004
#define PM_CAP_SDCARD      0x0008
#define PM_CAP_USB_HOST    0x0010
#define PM_CAP_WIFI        0x0020
#define PM_CAP_BLUETOOTH   0x0040
#define PM_CAP_HDMI        0x0080
#define PM_CAP_VGA         0x0100

typedef struct pm_info
{
    unsigned size;
    unsigned api_version;
    char firmware[16];
    char device[32];
    unsigned cpu_khz;
    unsigned sram_heap;
    unsigned psram;
    int hres, vres;
    unsigned caps;
} pm_info;

int pm_get_info(pm_info *info);

void pm_yield(void);

int pm_abort_requested(void);

unsigned long long pm_timer_us(void);
void pm_delay_us(unsigned int us);
void pm_pause(unsigned int ms);

#endif
