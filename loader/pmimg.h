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
};

#endif
