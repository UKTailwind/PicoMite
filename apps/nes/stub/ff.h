/* ff.h: the part of FatFs frank-nes uses, on the loader's file services (ff_cmm2.c). A name from "/" is on the card, A: */
#ifndef CMM2_FF_H
#define CMM2_FF_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef char TCHAR;
typedef uint32_t FSIZE_t;

typedef enum {
    FR_OK = 0, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE, FR_NO_PATH, FR_INVALID_NAME, FR_DENIED, FR_EXIST,
    FR_INVALID_OBJECT, FR_WRITE_PROTECTED, FR_INVALID_DRIVE, FR_NOT_ENABLED, FR_NO_FILESYSTEM, FR_MKFS_ABORTED, FR_TIMEOUT,
    FR_LOCKED, FR_NOT_ENOUGH_CORE, FR_TOO_MANY_OPEN_FILES, FR_INVALID_PARAMETER
} FRESULT;

typedef struct { int unused; } FATFS;
typedef struct { int h; FSIZE_t size, pos; } FIL;
typedef struct { int h; } DIR;
typedef struct {
    FSIZE_t fsize;
    WORD fdate, ftime;
    BYTE fattrib;
    TCHAR fname[256];
} FILINFO;

#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW 0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS 0x10
#define FA_OPEN_APPEND 0x30
#define AM_RDO 0x01
#define AM_HID 0x02
#define AM_SYS 0x04
#define AM_DIR 0x10
#define AM_ARC 0x20

FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt);
#define f_unmount(path) f_mount(0, path, 0)
FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT f_close(FIL *fp);
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *br);
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *bw);
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);
FRESULT f_sync(FIL *fp);
FRESULT f_opendir(DIR *dp, const TCHAR *path);
FRESULT f_closedir(DIR *dp);
FRESULT f_readdir(DIR *dp, FILINFO *fno);
FRESULT f_stat(const TCHAR *path, FILINFO *fno);
FRESULT f_mkdir(const TCHAR *path);
FRESULT f_unlink(const TCHAR *path);
FRESULT f_rename(const TCHAR *from, const TCHAR *to);
TCHAR *f_gets(TCHAR *buf, int len, FIL *fp);
int f_puts(const TCHAR *str, FIL *fp);
#define f_size(fp) ((fp)->size)
#define f_tell(fp) ((fp)->pos)
#define f_eof(fp) ((fp)->pos >= (fp)->size)

#ifdef __cplusplus
}
#endif
#endif
