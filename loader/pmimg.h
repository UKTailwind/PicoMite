/*
 * pmimg.h: loadable programs for the PicoMite (RP2350). The image header, the start block a program is called with,
 * and the table of firmware services it uses. The firmware (misc/Loader.c) and the SDK (tools/sdk) are both built
 * from this file.
 *
 * MIT License. Copyright (c) 2026 Peter Mather.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this file, to deal in it without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of it, and to permit persons to whom it is furnished to do so, subject to the following condition:
 * the above copyright notice and this permission notice shall be included in all copies or substantial portions.
 * THE FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE FILE OR THE USE OR OTHER DEALINGS IN IT.
 *
 * The file, as RUN "name.app" reads it:
 *   the header (64 bytes, below);
 *   image_size bytes: the program as linked, from link_base: its code, constants, init/fini arrays and data;
 *   reloc_count words: what to adjust when the image is not loaded at link_base.
 * A relocation word is the offset, from the image's start, of the word to adjust, with its kind in the top 4 bits.
 * A Cortex-M33 build loads addresses from literal pools, so an image is moved by adjusting whole words, by any
 * multiple of its sections' largest alignment (align_log2). An image with MOVT relocations (hand-written assembly)
 * is flagged, and is moved only by multiples of 64 KB. tools/sdk/mkimage.py makes the file from an ELF linked with
 * --emit-relocs.
 *
 * The names of the types and the services are the Colour Maximite 2's (its cmm2img.h), so that a program's source
 * builds for either: cmm2->con_puts(), cmm2_start->heap. The layout of the table and its version numbers are the
 * PicoMite's own, as the binaries can never be shared.
 *
 * A program runs on its own stack (the process stack), 32 KB of the SRAM heap unless its header asks for another
 * size, with its heap between its .bss and the end of the PSRAM it borrows. It returns from its entry, or calls
 * leave(), to go back to MMBasic.
 */
#ifndef PMIMG_H
#define PMIMG_H
#include <stddef.h>
#include <stdint.h>

#define PMIMG_MAGIC          "PICOIMG"         /* the header's first 8 bytes, its NUL included */
#define PMIMG_HDR_VERSION    1
#define PMIMG_API_VERSION    1                 /* the services below. Version 1 grows until the first release that
                                                  has it; after that each addition raises it */
#define PMIMG_TARGET         0x2350            /* the header's target: an RP2350 */
#define PMIMG_KIND_APP       1                 /* an application: it runs, then MMBasic goes on */
#define PMIMG_KIND_MODULE    2                 /* a module (a USER module): kept beside the BASIC program */
#define PMIMG_FLAG_PSRAM     0x01              /* needs PSRAM (mkimage --psram) */
#define PMIMG_FLAG_MOVT      0x04              /* has MOVT relocations: moved only by multiples of 64 KB */
#define PMIMG_BASE           0x11000000u       /* where a program is linked: the bottom of the PSRAM, where it usually
                                                  loads with nothing moved */
#define PMIMG_STACK          0x00008000u       /* the stack when the header asks for none: 32 KB */

#define PMIMG_REL_ABS32      0u                /* add the difference to the word */
#define PMIMG_REL_MOVT       1u                /* add the difference's top half to a Thumb-2 MOVT's immediate */
#define PMIMG_REL_KIND(r)    ((uint32_t)(r) >> 28)
#define PMIMG_REL_OFFSET(r)  ((uint32_t)(r) & 0x0FFFFFFFu)

typedef struct {
    char     magic[8];                         /* "PICOIMG\0" */
    uint16_t hdr_version;                      /* PMIMG_HDR_VERSION */
    uint16_t api_version;                      /* the version of the services it needs */
    uint8_t  kind;                             /* PMIMG_KIND_APP */
    uint8_t  flags;                            /* PMIMG_FLAG_... */
    uint16_t target;                           /* PMIMG_TARGET */
    uint32_t link_base;                        /* the address it was linked at */
    uint32_t image_size;                       /* the bytes after the header that are loaded */
    uint32_t bss_size;                         /* zeroed after the image */
    uint32_t entry;                            /* the entry point's offset (odd: a Thumb address) */
    uint32_t stack_size;                       /* 0: PMIMG_STACK */
    uint32_t heap_min;                         /* the least heap it can run with */
    uint32_t reloc_count;                      /* relocation words after the image */
    uint32_t crc32;                            /* of the image and the relocation words (zlib's CRC-32) */
    char     name[15];                         /* NUL padded */
    uint8_t  align_log2;                       /* its sections' largest alignment, as a power of 2 */
} pmimg_header;                                /* 64 bytes */

struct cmm2_services;
struct cmm2_basic;
typedef struct {
    const struct cmm2_services *svc;
    int      argc;
    char   **argv;                             /* argv[0] the file's name, then the words of RUN's second argument */
    void    *heap;                             /* the program's heap: from its .bss to the end of its area, 8-byte
                                                  aligned */
    uint32_t heap_size;
    uint32_t base;                             /* where the image is */
    void    *heap2;                            /* a second block, or NULL (always NULL on the PicoMite so far) */
    uint32_t heap2_size;
    const struct cmm2_basic *basic;            /* a USER module's table of the interpreter's routines; NULL for an
                                                  application */
} cmm2img_start;

typedef struct {                               /* the display after video_mode */
    uint8_t *fb;                               /* its first pixel; a line is pitch bytes on from the one before */
    uint32_t width, height, pitch, bpp;
    uint32_t copies;                           /* 1: the PicoMite stores each line once (its doubling is the display's) */
} cmm2_video;

typedef struct {                               /* a file's or directory's details (file_stat) */
    uint32_t size;                             /* 0 for a directory */
    uint8_t  is_dir;
    uint8_t  month, day, hour, minute, second; /* its last change (all 0 on A:, and for a drive's root) */
    uint16_t year;
} cmm2_stat;

typedef struct {                               /* one directory entry (dir_read) */
    char     name[256];
    uint32_t size;
    uint8_t  is_dir;
} cmm2_dirent;

#define CMM2_O_READ    0x01                    /* file_open's modes */
#define CMM2_O_WRITE   0x02
#define CMM2_O_CREATE  0x04                    /* with WRITE: created, or emptied if it is there */
#define CMM2_O_APPEND  0x08                    /* with WRITE: created if need be, and written at its end */

/* The services. A program must never meet an MMBasic error: each returns a status instead, negative for an error
   (for the files, minus FatFs's FRESULT, A:'s littlefs errors given as the nearest FRESULT), with the firmware's
   message from io_error() for -4. The table only grows: check version before using a later entry. */
struct cmm2_services {
    uint32_t version;                          /* PMIMG_API_VERSION */
    uint32_t size;                             /* sizeof(struct cmm2_services) in the firmware */
    /* the console: the serial console and the screen, as PRINT */
    void     (*con_putc)(int c);
    void     (*con_puts)(const char *s);
    int      (*con_getc)(void);                /* the next key, or -1: it never waits */
    void     (*con_screen)(int on);            /* 0: con_putc and con_puts to the serial console only, not drawn on
                                                  the screen; 1 (the start): both */
    /* time */
    uint64_t (*micros)(void);                  /* microseconds since start-up */
    void     (*delay_us)(uint32_t us);         /* waits, without polling */
    void     (*datetime)(int t[6]);            /* year, month, day, hour, minute, second */
    /* the system */
    int      (*poll)(void);                    /* the USB devices, keyboards and network seen to; 1 once Ctrl-C has been
                                                  pressed: the program should end, and the BASIC program that ran it
                                                  stops too */
    void     (*leave)(int code);               /* back to MMBasic from anywhere, as if the entry had returned code */
    uint32_t (*cpu_hz)(void);
    int      (*break_take)(void);              /* 1 if Ctrl-C has been pressed since the last call, which clears it:
                                                  the program deals with it (a BASIC program that ran it goes on);
                                                  poll() reports it too, until it is taken */
    void    *(*alloc)(uint32_t size);          /* a block of MMBasic's heap, zeroed, or NULL: freed at the end */
    void     (*release)(void *p);
    /* the firmware's own copy and fill, which the SDK's memcpy, memset and memmove call */
    void    *(*mem_copy)(void *d, const void *s, size_t n);
    void    *(*mem_fill)(void *d, int c, size_t n);
    void    *(*mem_move)(void *d, const void *s, size_t n);
    const char *(*io_error)(void);             /* the firmware's message for the last error a service returned */
    /* the PicoMite's own */
    uint32_t (*psram_size)(void);              /* the PSRAM's size in bytes, 0 if there is none */
    /* files: names as MMBasic takes them (A:/dir/name, B:..., C:..., or relative to the current drive and directory);
       handles 0-7. What a program leaves open is closed when it ends */
    int      (*file_open)(const char *name, int mode);
    int      (*file_close)(int h);
    int32_t  (*file_read)(int h, void *buf, uint32_t n);          /* the bytes read */
    int32_t  (*file_write)(int h, const void *buf, uint32_t n);   /* the bytes written */
    int      (*file_seek)(int h, uint32_t pos);
    int32_t  (*file_tell)(int h);
    int32_t  (*file_size)(int h);
    int      (*file_sync)(int h);
    int      (*file_truncate)(int h);          /* the file ends at its position */
    int      (*file_remove)(const char *name); /* a file, or an empty directory */
    int      (*file_rename)(const char *from, const char *to);
    int      (*file_stat)(const char *name, cmm2_stat *st);   /* 0, or minus FatFs's FRESULT (4: no such file) */
    int      (*dir_make)(const char *name);
    int      (*dir_open)(const char *path);    /* a handle 0-3 */
    int      (*dir_read)(int h, cmm2_dirent *e);   /* 1: an entry, 0: the end */
    int      (*dir_close)(int h);
    int      (*dir_change)(const char *path);  /* DRIVE and CHDIR's: MMBasic's current drive and directory, which names
                                                  are relative to, and which stay when the program ends */
    int      (*dir_current)(char *buf, uint32_t size);   /* CWD$'s ("A:/GAMES"): 0, or -3 if size is too small */
    /* the screen, by the PicoMite's MODE numbers. The mode, MAP's colours, the font and the colours BASIC had are put
       back when the program ends */
    int      (*video_mode)(int mode, int depth, cmm2_video *v);   /* MODE mode, depth the mode's own (MODE 1: 1, MODE 2
                                                  and 3: 4, MODE 4: 16, RGB555; MODE 5: 8, MAP's 256 colours): 0 with v
                                                  filled and the page cleared, or negative */
    int      (*video_palette)(int start, int count, const uint8_t *rgb);   /* MODE 5's colours (MAP's) from R,G,B bytes,
                                                  shown from the next frame: 0, or -1 if the mode has none (MODE 5 at
                                                  1280x720 and the like shows each byte as RGB332) */
    void     (*video_wait)(void);              /* until the display starts its next frame (50 ms at most) */
    /* the keyboard and the gamepads */
    int      (*keys_held)(int keys[7]);        /* KEYDOWN's: [0]-[5] the keys held (MMBasic's codes), [6] the modifier
                                                  bits; how many keys are held */
    int      (*keys_raw)(uint8_t report[8]);   /* the USB (or Bluetooth) keyboard's last report: [0] its modifier bits,
                                                  [2]-[7] the HID usage codes of the keys held; how many are held */
    int      (*gamepad)(int n, int *lx, int *ly);   /* the pad in USB channel n (1-4): DEVICE(GAMEPAD n, B)'s bitmap, and
                                                  its left stick (LX and LY) in lx and ly if they are not NULL, 128 on a
                                                  generic pad, which has none; -1 if no pad is there */
    /* sound, through OPTION AUDIO's output (PWM, an SPI DAC or I2S). The stream is 16-bit stereo, made by the
       program's fill, which the firmware calls from an interrupt (below every other) each time a buffer is wanted:
       1024 frames, 23 ms, the first two before audio_start returns. fill runs on MMBasic's stack, not the program's:
       it must be quick, keep its locals small and call no service, malloc or stdio. A fill that is late makes the
       last buffer play again. Its sound goes out at PLAY VOLUME's level. */
    int      (*audio_start)(void (*fill)(int16_t *buf, uint32_t frames));   /* the rate in Hz (44.1 kHz as the output
                                                  makes it), or negative: -1 no output for it (no OPTION AUDIO, or a
                                                  VS1053), -2 sound already playing, -3 no room */
    void     (*audio_stop)(void);              /* (ended for it when the program ends) */
    void     (*audio_lock)(void);              /* fill held off, while the program changes what fill reads: briefly */
    void     (*audio_unlock)(void);
    /* the mouse, and a gamepad's other axes */
    int      (*mouse)(int m[4]);               /* 1 with m: x, y, the buttons (1 left, 2 right, 4 middle), the wheel's
                                                  count; 0 if there is no mouse (m all 0). A USB or PS/2 mouse, as
                                                  DEVICE(MOUSE 2, ...) gives it, kept within the screen */
    void     (*mouse_set)(int x, int y);       /* its position moved there */
    int      (*gamepad_axes)(int n, int v[6]); /* gamepad's buttons, and in v DEVICE(GAMEPAD n, ...)'s LX, LY, RX, RY
                                                  (0-255, 128 centred: always 128 on a generic pad) and L, R (the
                                                  analogue triggers, 0-255; 0 on a generic pad); -1 if no pad is there */
    /* I/O, by GP numbers. Each returns 0 (or a value) or a negative error: -1 no such GP (or one the board keeps, as
       the PC3 its HDMI's), bus or port, -2 its pins are in use (the system's, or a bus already open), -3 not possible (a bus with no pins set, or not open...), -4 the
       firmware's error, whose message io_error() gives. A bus's pins are the ones SETPIN or OPTION SYSTEM gave it, or
       the ones bus_pins names. What a program sets up is undone when it ends: its PWM stopped, its buses and ports
       closed (their pins then unconfigured, as I2C CLOSE leaves them), the pins it set up unconfigured */
    int      (*pin_setup)(int gp, int mode, int pull);   /* CMM2_PIN_*; pull CMM2_PULL_* for an input or an open-drain
                                                  output (an input that pin_write drives low for 0) */
    int      (*pin_read)(int gp);              /* 0 or 1 (an output's level too) */
    void     (*pin_write)(int gp, int v);      /* an output's level (an open-drain one: 1 lets go) */
    int      (*adc_read)(int gp, uint32_t *uv);   /* an analog input (CMM2_PIN_ANALOG): 0-65535, and its microvolts
                                                  (from 3.3 V or OPTION VCC's) */
    int      (*pwm_set)(int gp, uint32_t freq, uint32_t duty);   /* freq Hz (a slice's A and B share theirs), duty
                                                  0-65535; the GP set up for PWM first if need be */
    int      (*pwm_stop)(int gp);              /* that output stopped (the slice, when neither runs) */
    int      (*bus_pins)(int kind, int bus, int a, int b, int c);   /* SETPIN's, for a bus: PM_BUS_I2C (bus 0 I2C,
                                                  1 I2C2; a SDA, b SCL), PM_BUS_SPI (0 SPI, 1 SPI2; a RX, b TX, c
                                                  SCK), PM_BUS_COM (1 COM1, 2 COM2; a RX, b TX) */
    int      (*i2c_open)(int bus, int khz, int timeout_ms);   /* bus 0: I2C, 1: I2C2; 100, 400 or 1000 kHz; timeout 0
                                                  (none) or 100 ms up. A bus already open (OPTION SYSTEM I2C's, or
                                                  BASIC's I2C OPEN): 0, and it is used as it is */
    int      (*i2c_close)(int bus);
    int      (*i2c_xfer)(int bus, int addr, const uint8_t *w, uint32_t wn, uint8_t *r, uint32_t rn, int stop);   /*
                                                  wn bytes written, then rn read (a repeated start between), the bus
                                                  released at the end if stop; neither: the address alone. 0, 1 a NACK,
                                                  2 timed out (MM.I2C's), or negative */
    int      (*spi_open)(int bus, uint32_t hz, int mode);   /* bus 0: SPI, 1: SPI2; mode 0-3, 8 bits */
    int      (*spi_close)(int bus);
    int      (*spi_xfer)(int bus, const uint8_t *tx, uint8_t *rx, uint32_t n);   /* n bytes, tx NULL: 0s out, rx
                                                  NULL: what comes in dropped */
    int      (*com_open)(int com, uint32_t baud, int bits, int parity, int stop, uint32_t rxbuf);   /* OPEN "COMn:"'s
                                                  numbers: 1 or 2, 3-6 a USB serial device; bits 7 or 8, parity 0
                                                  none, 1 even, 2 odd, stop 1 or 2; rxbuf 0: 1024 */
    int      (*com_close)(int com);
    int      (*com_write)(int com, const uint8_t *buf, uint32_t n);   /* queued (waiting while the queue is full) */
    int      (*com_read)(int com, uint8_t *buf, uint32_t n);   /* the bytes waiting, up to n: 0 if none (no wait) */
    int      (*com_avail)(int com);            /* the bytes waiting */
    int      (*com_connected)(int com);        /* 1 if its device is there (COM1 and COM2 always), 0 if a USB serial
                                                  device is not, open or not; -1 if there is no such port */
    /* data, not a service: the firmware's double-precision routines, in loader/pmdouble.h's order, which
       tools/sdk/fwdouble.S puts in place of libgcc's and newlib's */
    const void *const *doubles;
    /* the clock and the watchdog */
    int      (*datetime_set)(const int t[6]);  /* the clock (DATE$ and TIME$) set to year (2000-2099), month, day, hour,
                                                  minute, second, and the RTC with it (OPTION RTC's DS3231...), as RTC
                                                  SETTIME sets it: 0, 1 if no RTC answered (the clock alone), -3 a bad
                                                  date or time */
    void     (*watchdog)(int ms);              /* MMBasic's WATCHDOG: the board restarts unless this is called again within
                                                  ms; 0 stops it. It goes on counting after the program ends, as
                                                  WATCHDOG in a BASIC program does */
    /* the network: the Colour Maximite 2's services (its version 21), the same names and meanings, on the firmware's
       Wi-Fi (OPTION WIFI joins at power-up) and lwIP (misc/LoaderNet.c), as eight BSD-style handles over TCP and UDP.
       The network moves in poll() and net_poll: call net_poll while waiting on it. Errors are negative errno values
       (Linux's numbers, as MicroPython's): -11 EAGAIN nothing yet, -19 ENODEV no Wi-Fi (or a build without it), -107
       ENOTCONN, -111 ECONNREFUSED... A program's handles are closed when it ends; its joins and leaves change no
       option */
    int      (*net_poll)(void);                /* the network pumped: 0 no Wi-Fi here, 1 not joined, 2 joined (with an
                                                  address) */
    int      (*net_join)(const char *ssid, const char *password);   /* that network joined (password "" or NULL: an open
                                                  one), waiting up to 30 s for an address: 0, -13 a wrong password,
                                                  -110 timed out, -113 not joined */
    int      (*net_leave)(void);               /* the network left */
    int      (*net_ifconfig)(uint8_t a[16]);   /* address, mask, gateway and DNS server, 4 bytes each */
    int      (*net_dns)(const char *name, uint8_t ip[4]);   /* a name's address (or a dotted address's): 0 in ip; 1
                                                  asked: net_poll, then ask again with the same name; -2 not found */
    int      (*sock_open)(int type);           /* 1 TCP, 2 UDP: a handle 0-7, or -24 none free */
    int      (*sock_bind)(int h, int port);    /* the local port (any address) */
    int      (*sock_connect)(int h, const uint8_t ip[4], int port);   /* TCP: started, sock_state says when it is made
                                                  (or its error); UDP: the peer for sock_send without an address */
    int      (*sock_listen)(int h, int backlog);   /* TCP: listening on its bound port */
    int      (*sock_accept)(int h, uint8_t ip[4], int *port);   /* a connection to a listening handle: its own handle
                                                  and the peer's address, or -11 none yet */
    int      (*sock_send)(int h, const uint8_t *buf, int n, const uint8_t *ip, int port);   /* TCP: the bytes taken (0:
                                                  no room now; ip unused); UDP: one datagram to ip:port, or the
                                                  connected peer if ip is NULL */
    int      (*sock_recv)(int h, uint8_t *buf, int n, uint8_t *ip, int *port);   /* TCP: up to n bytes, 0 once the
                                                  peer has closed and all is taken; UDP: one datagram (cut to n) and,
                                                  if ip and port are not NULL, its sender; -11 nothing yet */
    int      (*sock_state)(int h);             /* bits: 1 connected, 2 data (or, listening, a connection) waiting, 4
                                                  room to send, 8 the peer closed; or its error */
    int      (*sock_close)(int h);
};

#define CMM2_PIN_OFF     0                     /* pin_setup's modes */
#define CMM2_PIN_IN      1
#define CMM2_PIN_OUT     2
#define CMM2_PIN_OD      3                     /* open drain */
#define CMM2_PIN_ANALOG  4
#define CMM2_PULL_NONE   0
#define CMM2_PULL_UP     1
#define CMM2_PULL_DOWN   2
#define PM_BUS_I2C       0                     /* bus_pins' kinds (the PicoMite's own: PM_, as
                                                  the CMM2 has no bus_pins) */
#define PM_BUS_SPI       1
#define PM_BUS_COM       2

/* A module's table (PMIMG_KIND_MODULE: build.py --module), the one its module_main() returns, starts with this */
typedef struct {
    uint32_t magic;                            /* CMM2MOD_MAGIC */
    uint32_t version;                          /* the table's version, the firmware command's to check */
    void     (*forget)(void);                  /* called before the module goes: its files closed, its work given up */
} cmm2mod_head;
#define CMM2MOD_MAGIC        0x444F4D43u       /* "CMOD" */

#endif
