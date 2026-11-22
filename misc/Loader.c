/***********************************************************************************************************************
Loadable programs (Peter, 16 Nov 2026; ported from the Colour Maximite 2's Src/Loader.c). docs/Loader_Plan.md has the
design, loader/pmimg.h the file format and the services, tools/sdk the tools that build a program.

  RUN "name.app" [, args$]   loads the program into the PSRAM, runs it, and goes on with the next statement when it
                             ends (typed at the prompt: back to the prompt). The words of args$ are its argv[1] on.

Only a name ending in .APP is looked at, so RUN of a BASIC program is as it was, and nothing of the BASIC program in
memory is touched.

Its memory: the largest free run of the PSRAM heap, all but LD_RESERVE of it, borrowed while the program runs and
given back after. The heap is allocated from the top down, so at the prompt that run is the bottom of the PSRAM, where
programs are linked (PMIMG_BASE), and nothing is moved. Elsewhere the image is moved by its relocations. The program's
stack is a block of the SRAM heap (the top of its PSRAM if there is no room), and its heap is the rest of its PSRAM.

The program runs on its own stack (the process stack, PSPLIM catching an overflow), so the interrupts keep the main
stack. LoaderCall switches to it and calls the entry with the start block; the program comes back by returning, or
from any depth through leave(), which goes to LoaderReturn: that puts back the main stack and the registers
LoaderCall saved. Each service that is not a pure function runs on the main stack, through ld_veneer: a flash write
(A:, the options) turns off the whole XIP window, the PSRAM's included, and the firmware's code expects its own stack.
The services never raise an MMBasic error (its longjmp would land with the program's stack still selected): each runs
under ld_try and returns a status. What a program leaves open is closed and freed when it ends. A fault in it restarts
the board, as any fault does.
***********************************************************************************************************************/
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include <stdarg.h>
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "ff.h"
#include "loader/pmimg.h"
#include "Loader.h"

#ifdef LOADER
extern uint32_t PSRAMsize;
extern volatile int MMAbort;
extern volatile char ConsoleRxBuf[];
extern volatile int ConsoleRxBufHead, ConsoleRxBufTail;
extern volatile unsigned int WDTimer;
extern uint32_t DefinedSubFunMem;
extern unsigned int SBitsGet(unsigned char *addr);
extern void SBitsSet(unsigned char *addr, int bits);
extern uint_fast64_t gettimefromepoch(int *year, int *month, int *day, int *hour, int *minute, int *second);
extern void CloseAudio(int all);
extern void chdir(char *p);                                         // CHDIR's (FileIO.c)
extern int KeyDown[7];                                              // KEYDOWN's (KeyboardMap.c)
extern void setmode(int mode, bool clear);                          // MODE's (Draw.c)
extern void cmd_setpin(void), cmd_pwm(void), cmd_spi(void), cmd_spi2(void);   // SETPIN's, PWM's, SPI's (I/O services)
#ifndef USBKEYBOARD
extern bool mouse0;                                                 // a PS/2 mouse is there (mouse.c)
#endif

#define LD_BLOCKS  64                                               // the alloc service's blocks
#define LD_FILES   8                                                // the file services' handles
#define LD_DIRS    4                                                // and the directories'
#define LD_MOD_HEAP 0x10000                                         // a USER module's C library's heap if it asks none
#define LD_RESERVE 0x40000                                          // the PSRAM run's top left free: the services' blocks
#define LD_CHUNK   4096                                             // the image read through SRAM this much at a time

volatile int LoaderActive;
uint8_t LoaderKeyReport[8];                                         // the keyboard's last report (process_kbd_report)
static uint32_t ldmsp __attribute__((used));                        // the main stack inside LoaderCall, for LoaderReturn

// a file or directory the program has open, on A: (littlefs, fs 0) or on B: or C: (FatFs): the file systems' own, not
// MMBasic's numbered files, which stay the BASIC program's
typedef struct {
    int fs;
    union {
        FIL fat;
        lfs_file_t lfs;
    } u;
    DWORD *map;                                                     // a FatFs file's cluster map (ld_map), or NULL
} ldfile_t;
typedef struct {
    int fs;
    union {
        DIR fat;
        lfs_dir_t lfs;
    } u;
} lddir_t;

// what a program has while it runs: a block of the SRAM heap, so that the loader costs a few words of BSS
typedef struct {
    void *block[LD_BLOCKS];                                         // its blocks of MMBasic's heap (alloc)
    ldfile_t *file[LD_FILES];
    lddir_t *dir[LD_DIRS];
    char errmsg[MAXERRMSG];                                         // io_error()'s
    int screen;                                                     // the console's output drawn on the screen too
    int vmode, palette;                                             // it set a mode; it changed MAP's colours
    int oldmode, oldfont, oldfc, oldbc;                             // the screen as BASIC had it
    uint16_t oldmap[256];
    uint64_t pins, od, pwmpins;                                     // its GPs (bit n: GPn), its open-drain outputs, its PWM
    uint32_t pwmon;                                                 // its PWM outputs running (bit slice * 2 + B)
    uint16_t pwmduty[24];
    uint32_t pwmfreq[12];
    uint8_t buses, coms;                                            // bit 0-1: I2C, I2C2; 2-3: SPI, SPI2; bit n: COMn
    cmm2img_start start;
    char *argv[];                                                   // then the words themselves
} ldstate_t;
static ldstate_t *ld;

/*******************************************************************************************
 the services
*******************************************************************************************/
// fn(arg) with an MMBasic error caught: 0, or -4 with the message in errmsg. The error's state (MM.ERRNO,
// MM.ERRMSG$, a SUB call's arguments) and the current drive are left as the BASIC program had them; a file error is
// left in FSerror for fn to see (OPTION FILE ERROR CONTINUE's way), not raised.
static int ld_try(void (*fn)(void *), void *arg)
{
    jmp_buf save;
    char msg[MAXERRMSG];
    int skip = OptionErrorSkip, en = MMerrno, abort = OptionFileErrorAbort;
    int fs = FatFSFileSystem, fssave = FatFSFileSystemSave;
    uint32_t sub = DefinedSubFunMem;
    volatile int failed = 0;
    memcpy(save, ErrNext, sizeof(jmp_buf));
    memcpy(msg, MMErrMsg, sizeof(msg));
    OptionErrorSkip = 1;                                            // (error() then goes to ErrNext, printing nothing,
    DefinedSubFunMem = 0;                                           // and leaves a SUB call's arguments alone)
    OptionFileErrorAbort = 0;
    if (setjmp(ErrNext) == 0)
        fn(arg);
    else
    {
        failed = 1;
        snprintf(ld->errmsg, sizeof(ld->errmsg), "%s", MMErrMsg);
    }
    memcpy(ErrNext, save, sizeof(jmp_buf));
    memcpy(MMErrMsg, msg, sizeof(msg));
    OptionErrorSkip = skip;
    OptionFileErrorAbort = abort;
    MMerrno = en;
    DefinedSubFunMem = sub;
    FatFSFileSystem = fs;
    FatFSFileSystemSave = fssave;
    return failed ? -4 : 0;
}
static const char *ld_ioerror(void) { return ld ? ld->errmsg : ""; }

// the console
static void ld_putcbody(void *a)
{
    int c = *(int *)a;
    if (ld->screen)
        MMputchar(c, 1);
    else
        SerialConsolePutC(c, 1);
}
static __attribute__((used, noinline)) void ld_putc(int c) { ld_try(ld_putcbody, &c); }
static void ld_putsbody(void *a)
{
    if (ld->screen)
        MMPrintString((char *)a);
    else
        SSPrintString((char *)a);
}
static __attribute__((used, noinline)) void ld_puts(const char *s) { ld_try(ld_putsbody, (void *)s); }
static void ld_conscreen(int on) { ld->screen = on != 0; }
// the console's queue without getConsole's CheckAbort: Ctrl-C is poll()'s to report
static int ld_getc(void)
{
    int c = -1;
    if (ConsoleRxBufHead != ConsoleRxBufTail)
    {
        c = ConsoleRxBuf[ConsoleRxBufTail];
        ConsoleRxBufTail = (ConsoleRxBufTail + 1) % CONSOLE_RX_BUF_SIZE;
    }
    return c;
}

// time
static uint64_t ld_micros(void) { return time_us_64(); }
static void ld_delay(uint32_t us) { busy_wait_us(us); }
static __attribute__((used, noinline)) void ld_datetime(int t[6])
{
    gettimefromepoch(&t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
}

// the system: what the interpreter does between statements (the USB devices and keyboards, the network, Bluetooth),
// with CheckAbort's Ctrl-C left to the program (LoaderActive); 1 once Ctrl-C has been pressed. MMAbort is left set,
// so when the program ends the BASIC program that ran it stops as well, as at any Ctrl-C
static void ld_pollbody(void *a)
{
    (void)a;
    stepper_poll_events();
    CheckAbort();
}
static __attribute__((used, noinline)) int ld_poll(void)
{
    if (!LoaderActive)                                              // a USER module, inside a statement: Ctrl-C breaks
        ld_pollbody(NULL);                                          // it, as in any command (not under ld_try, which
    else                                                            // would leave ON ERROR SKIP's state on at the prompt)
        ld_try(ld_pollbody, NULL);
    return MMAbort != 0;
}
static uint32_t ld_cpuhz(void) { return clock_get_hz(clk_sys); }
static int ld_breaktake(void)
{
    int b = MMAbort != 0;
    MMAbort = 0;
    return b;
}
static __attribute__((used, noinline)) void *ld_alloc(uint32_t size)
{
    int i;
    for (i = 0; i < LD_BLOCKS && ld->block[i]; i++)
    {
    }
    if (i == LD_BLOCKS || size == 0 || size > 0x7FFFFFFF)
        return NULL;
    return ld->block[i] = GetMemoryNull((int)size);
}
static __attribute__((used, noinline)) void ld_release(void *p)
{
    for (int i = 0; i < LD_BLOCKS; i++)
        if (p != NULL && ld->block[i] == p)
        {
            FreeMemory(p);
            ld->block[i] = NULL;
        }
}
static uint32_t ld_psramsize(void) { return PSRAMsize ? PSRAMsize + PSRAM_RESERVE : 0; }

/* The files. A name is taken as MMBasic's commands take it (getfullfilename: a drive, the current drive's directory),
   and its drive mounted as a command would (InitSDCard). Each operation runs in ld_filebody under the trap, so a
   failure of either is an MMBasic error caught: -4. A file system's own error is returned as minus FatFs's FRESULT,
   littlefs's (A:) turned into the nearest one, as the programs' C library expects (syscalls.c's errno). */
enum { LF_OPEN, LF_CLOSE, LF_READ, LF_WRITE, LF_SEEK, LF_TELL, LF_SIZE, LF_SYNC, LF_TRUNC, LF_REMOVE, LF_RENAME,
       LF_STAT, LF_MKDIR, LF_OPENDIR, LF_READDIR, LF_CLOSEDIR, LF_CHDIR, LF_GETCWD };
typedef struct {
    int op, h, mode;
    const char *s, *s2;
    void *buf;
    uint32_t n;
    int32_t r;
} ldfop_t;

static int ld_lfserr(int e)
{
    switch (e)
    {
    case 0:
        return 0;
    case LFS_ERR_NOENT:
        return -FR_NO_FILE;
    case LFS_ERR_EXIST:
        return -FR_EXIST;
    case LFS_ERR_NOTDIR:
        return -FR_NO_PATH;
    case LFS_ERR_ISDIR:
    case LFS_ERR_NOTEMPTY:
    case LFS_ERR_NOSPC:
    case LFS_ERR_FBIG:
        return -FR_DENIED;
    case LFS_ERR_INVAL:
    case LFS_ERR_NAMETOOLONG:
        return -FR_INVALID_NAME;
    case LFS_ERR_NOMEM:
        return -FR_NOT_ENOUGH_CORE;
    case LFS_ERR_BADF:
        return -FR_INVALID_OBJECT;
    default:
        return e > 0 ? 0 : -FR_DISK_ERR;
    }
}

// name's drive made current (the trap puts the BASIC program's back) and mounted, its full path in q: the drive (0 A:,
// 1 B:, 2 C:), or -1 if it would not mount (FSerror says why)
static int ld_resolve(const char *name, char *q)
{
    unsigned char *line = cmdline;
    char n[FF_MAX_LFN];
    snprintf(n, sizeof(n), "%s", name);
    memset(q, 0, FF_MAX_LFN);                                       // (getfullfilename copies the directory unterminated)
    cmdline = (unsigned char *)" ";                                 // (getfullfilename takes a drive only with a cmdline)
    getfullfilename(n, q);
    cmdline = line;
    FatFSFileSystemSave = FatFSFileSystem;                          // (InitSDCard's ErrorThrow sets FatFSFileSystem to it)
    if (FatFSFileSystem && !InitSDCard())
        return -1;
    return FatFSFileSystem;
}

// a FatFs file opened as it is (read, or read and written in place) is given a map of its clusters (FatFs's fast seek),
// as the CMM2's are: a seek, or the next cluster, is then found in the table, not by following the FAT from the file's
// start over the card's SPI each time (Doom seeks to each lump of a 28 MB WAD: 65 s to start without it). A write or a
// seek that would make the file longer, or a truncate, drops it first (with it a file cannot change its length). 64
// entries first, then as many as FatFs says it needs; without room the file is read as before
static void ld_map(ldfile_t *f)
{
    DWORD n = 64, *t;
    FRESULT r;
    while ((t = GetMemoryNull(n * sizeof(DWORD))) != NULL)
    {
        t[0] = n;
        f->u.fat.cltbl = t;
        if ((r = f_lseek(&f->u.fat, CREATE_LINKMAP)) == FR_OK)
        {
            f->map = t;
            return;
        }
        f->u.fat.cltbl = NULL;
        n = t[0];
        FreeMemory(t);
        if (r != FR_NOT_ENOUGH_CORE)
            return;
    }
}
static void ld_unmap(ldfile_t *f)
{
    if (f->map)
    {
        f->u.fat.cltbl = NULL;
        FreeMemory(f->map);
        f->map = NULL;
    }
}

static void ld_filebody(void *a)
{
    ldfop_t *o = a;
    ldfile_t *f = (o->h >= 0 && o->h < LD_FILES) ? ld->file[o->h] : NULL;
    lddir_t *d = (o->h >= 0 && o->h < LD_DIRS) ? ld->dir[o->h] : NULL;
    char q[FF_MAX_LFN], q2[FF_MAX_LFN];
    int fs;
    UINT got;
    switch (o->op)
    {
    case LF_OPEN:
        for (o->h = 0; o->h < LD_FILES && ld->file[o->h]; o->h++)
        {
        }
        if (o->h == LD_FILES)
        {
            o->r = -FR_TOO_MANY_OPEN_FILES;
            return;
        }
        if ((fs = ld_resolve(o->s, q)) < 0)
        {
            o->r = -(FSerror > 0 ? FSerror : FR_NOT_READY);
            return;
        }
        if ((f = GetMemoryNull(sizeof(ldfile_t))) == NULL)
        {
            o->r = -FR_NOT_ENOUGH_CORE;
            return;
        }
        f->fs = fs;
        if (fs)
        {
            BYTE m = (o->mode & CMM2_O_READ) ? FA_READ : 0;
            if (o->mode & CMM2_O_WRITE)
                m |= FA_WRITE | ((o->mode & CMM2_O_APPEND) ? FA_OPEN_APPEND : (o->mode & CMM2_O_CREATE) ? FA_CREATE_ALWAYS : FA_OPEN_EXISTING);
            o->r = -(int)f_open(&f->u.fat, q, m);
            if (o->r == 0 && !(m & (FA_CREATE_ALWAYS | FA_OPEN_APPEND)))
                ld_map(f);
        }
        else
        {
            int m = (o->mode & CMM2_O_WRITE) ? ((o->mode & CMM2_O_READ) ? LFS_O_RDWR : LFS_O_WRONLY) : LFS_O_RDONLY;
            if (o->mode & CMM2_O_WRITE)
                m |= (o->mode & CMM2_O_APPEND) ? (LFS_O_CREAT | LFS_O_APPEND) : (o->mode & CMM2_O_CREATE) ? (LFS_O_CREAT | LFS_O_TRUNC) : 0;
            o->r = ld_lfserr(lfs_file_open(&lfs, &f->u.lfs, q, m));
        }
        if (o->r)
            FreeMemory(f);
        else
            ld->file[o->r = o->h] = f;
        return;
    case LF_OPENDIR:
        for (o->h = 0; o->h < LD_DIRS && ld->dir[o->h]; o->h++)
        {
        }
        if (o->h == LD_DIRS)
        {
            o->r = -FR_TOO_MANY_OPEN_FILES;
            return;
        }
        if ((fs = ld_resolve(o->s, q)) < 0)
        {
            o->r = -(FSerror > 0 ? FSerror : FR_NOT_READY);
            return;
        }
        if ((d = GetMemoryNull(sizeof(lddir_t))) == NULL)
        {
            o->r = -FR_NOT_ENOUGH_CORE;
            return;
        }
        d->fs = fs;
        o->r = fs ? -(int)f_opendir(&d->u.fat, q) : ld_lfserr(lfs_dir_open(&lfs, &d->u.lfs, q));
        if (o->r)
            FreeMemory(d);
        else
            ld->dir[o->r = o->h] = d;
        return;
    case LF_REMOVE:
    case LF_MKDIR:
    case LF_STAT:
        if ((fs = ld_resolve(o->s, q)) < 0)
        {
            o->r = -(FSerror > 0 ? FSerror : FR_NOT_READY);
            return;
        }
        if (o->op == LF_REMOVE)
            o->r = fs ? -(int)f_unlink(q) : ld_lfserr(lfs_remove(&lfs, q));
        else if (o->op == LF_MKDIR)
            o->r = fs ? -(int)f_mkdir(q) : ld_lfserr(lfs_mkdir(&lfs, q));
        else
        {
            cmm2_stat *st = o->buf;
            memset(st, 0, sizeof(*st));
            if (strcmp(q, "/") == 0)                                // (a drive's root, which neither has an entry for)
                st->is_dir = 1, o->r = 0;
            else if (fs)
            {
                FILINFO fno;
                if ((o->r = -(int)f_stat(q, &fno)) == 0)
                {
                    st->is_dir = (fno.fattrib & AM_DIR) != 0;
                    st->size = st->is_dir ? 0 : (uint32_t)fno.fsize;
                    st->year = (fno.fdate >> 9) + 1980;
                    st->month = (fno.fdate >> 5) & 15;
                    st->day = fno.fdate & 31;
                    st->hour = fno.ftime >> 11;
                    st->minute = (fno.ftime >> 5) & 63;
                    st->second = (fno.ftime & 31) * 2;
                }
            }
            else
            {
                struct lfs_info inf;
                if ((o->r = ld_lfserr(lfs_stat(&lfs, q, &inf))) == 0)
                {
                    st->is_dir = inf.type == LFS_TYPE_DIR;
                    st->size = st->is_dir ? 0 : inf.size;
                }
            }
        }
        return;
    case LF_RENAME:
        if ((fs = ld_resolve(o->s2, q2)) < 0 || ld_resolve(o->s, q) != fs)
        {
            o->r = fs < 0 ? -(FSerror > 0 ? FSerror : FR_NOT_READY) : -FR_INVALID_DRIVE;
            return;
        }
        o->r = fs ? -(int)f_rename(q, q2) : ld_lfserr(lfs_rename(&lfs, q, q2));
        return;
    case LF_CHDIR:
    {
        int waste = 0, t = drivecheck((char *)o->s, &waste);
        FSerror = 0;
        if (t - 1 != FatFSFileSystem)                               // (DRIVE's: CHDIR stays on the current drive)
        {
            FatFSFileSystem = FatFSFileSystemSave = t - 1;
            if (FatFSFileSystem && !InitSDCard())
            {
                o->r = -(FSerror > 0 ? FSerror : FR_NOT_READY);
                return;
            }
        }
        snprintf(q, sizeof(q), "%s", o->s);
        if (q[waste])
            chdir(q);
        o->r = FSerror > 0 ? -FSerror : FSerror < 0 ? ld_lfserr(FSerror) : 0;
        if (o->r == 0)
            o->h = FatFSFileSystem;                                 // (kept after the trap: ld_chdir)
        return;
    }
    case LF_GETCWD:
    {
        char *c = GetCWD();
        o->r = strlen(c) + 1 > o->n ? -3 : 0;
        if (o->r == 0)
            strcpy(o->buf, c);
        ClearSpecificTempMemory(c);
        return;
    }
    case LF_READDIR:
        if (d == NULL)
            break;
        if (d->fs)
        {
            FILINFO fno;
            cmm2_dirent *e = o->buf;
            do
                o->r = -(int)f_readdir(&d->u.fat, &fno);
            while (o->r == 0 && fno.fname[0] && (fno.fattrib & AM_SYS));
            if (o->r == 0 && fno.fname[0])
            {
                snprintf(e->name, sizeof(e->name), "%s", fno.fname);
                e->size = (uint32_t)fno.fsize;
                e->is_dir = (fno.fattrib & AM_DIR) != 0;
                o->r = 1;
            }
        }
        else
        {
            struct lfs_info inf;
            cmm2_dirent *e = o->buf;
            do
                o->r = lfs_dir_read(&lfs, &d->u.lfs, &inf);
            while (o->r > 0 && (strcmp(inf.name, ".") == 0 || strcmp(inf.name, "..") == 0));
            if (o->r > 0)
            {
                snprintf(e->name, sizeof(e->name), "%s", inf.name);
                e->size = inf.type == LFS_TYPE_DIR ? 0 : inf.size;
                e->is_dir = inf.type == LFS_TYPE_DIR;
                o->r = 1;
            }
            else
                o->r = ld_lfserr(o->r);
        }
        return;
    case LF_CLOSEDIR:
        if (d == NULL)
            break;
        o->r = d->fs ? -(int)f_closedir(&d->u.fat) : ld_lfserr(lfs_dir_close(&lfs, &d->u.lfs));
        ld->dir[o->h] = NULL;
        FreeMemory(d);
        return;
    default:                                                        // the operations on an open file
        if (f == NULL)
            break;
        switch (o->op)
        {
        case LF_CLOSE:
            o->r = f->fs ? -(int)f_close(&f->u.fat) : ld_lfserr(lfs_file_close(&lfs, &f->u.lfs));
            ld->file[o->h] = NULL;
            FreeMemory(f->map);
            FreeMemory(f);
            return;
        case LF_READ:
            if (f->fs)
                o->r = (o->r = -(int)f_read(&f->u.fat, o->buf, o->n, &got)) ? o->r : (int32_t)got;
            else
                o->r = (o->r = lfs_file_read(&lfs, &f->u.lfs, o->buf, o->n)) < 0 ? ld_lfserr(o->r) : o->r;
            return;
        case LF_WRITE:
            if (f->fs && f->map && f_tell(&f->u.fat) + o->n > f_size(&f->u.fat))
                ld_unmap(f);                                        // (it grows)
            if (f->fs)
                o->r = (o->r = -(int)f_write(&f->u.fat, o->buf, o->n, &got)) ? o->r : (int32_t)got;
            else
                o->r = (o->r = lfs_file_write(&lfs, &f->u.lfs, o->buf, o->n)) < 0 ? ld_lfserr(o->r) : o->r;
            return;
        case LF_SEEK:
            if (f->fs && f->map && o->n > f_size(&f->u.fat))
                ld_unmap(f);                                        // (past the end: a seek there grows it)
            o->r = f->fs ? -(int)f_lseek(&f->u.fat, o->n) : (o->r = lfs_file_seek(&lfs, &f->u.lfs, o->n, LFS_SEEK_SET)) < 0 ? ld_lfserr(o->r) : 0;
            return;
        case LF_TELL:
            o->r = f->fs ? (int32_t)f_tell(&f->u.fat) : lfs_file_tell(&lfs, &f->u.lfs);
            return;
        case LF_SIZE:
            o->r = f->fs ? (int32_t)f_size(&f->u.fat) : lfs_file_size(&lfs, &f->u.lfs);
            return;
        case LF_SYNC:
            o->r = f->fs ? -(int)f_sync(&f->u.fat) : ld_lfserr(lfs_file_sync(&lfs, &f->u.lfs));
            return;
        case LF_TRUNC:
            if (f->fs)
                ld_unmap(f);
            o->r = f->fs ? -(int)f_truncate(&f->u.fat) : ld_lfserr(lfs_file_truncate(&lfs, &f->u.lfs, lfs_file_tell(&lfs, &f->u.lfs)));
            return;
        }
    }
    o->r = -FR_INVALID_OBJECT;                                      // (no such handle)
}
static int32_t ld_fop(ldfop_t *o)
{
    o->r = -FR_INT_ERR;
    return ld_try(ld_filebody, o) < 0 ? -4 : o->r;
}
static __attribute__((used, noinline)) int ld_open(const char *name, int mode)
{
    ldfop_t o = {.op = LF_OPEN, .s = name, .mode = mode};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_close(int h)
{
    ldfop_t o = {.op = LF_CLOSE, .h = h};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int32_t ld_read(int h, void *buf, uint32_t n)
{
    ldfop_t o = {.op = LF_READ, .h = h, .buf = buf, .n = n};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int32_t ld_write(int h, const void *buf, uint32_t n)
{
    ldfop_t o = {.op = LF_WRITE, .h = h, .buf = (void *)buf, .n = n};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_seek(int h, uint32_t pos)
{
    ldfop_t o = {.op = LF_SEEK, .h = h, .n = pos};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int32_t ld_tell(int h)
{
    ldfop_t o = {.op = LF_TELL, .h = h};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int32_t ld_size(int h)
{
    ldfop_t o = {.op = LF_SIZE, .h = h};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_sync(int h)
{
    ldfop_t o = {.op = LF_SYNC, .h = h};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_truncate(int h)
{
    ldfop_t o = {.op = LF_TRUNC, .h = h};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_remove(const char *name)
{
    ldfop_t o = {.op = LF_REMOVE, .s = name};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_rename(const char *from, const char *to)
{
    ldfop_t o = {.op = LF_RENAME, .s = from, .s2 = to};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_stat(const char *name, cmm2_stat *st)
{
    ldfop_t o = {.op = LF_STAT, .s = name, .buf = st};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_mkdir(const char *name)
{
    ldfop_t o = {.op = LF_MKDIR, .s = name};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_opendir(const char *path)
{
    ldfop_t o = {.op = LF_OPENDIR, .s = path};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_readdir(int h, cmm2_dirent *e)
{
    ldfop_t o = {.op = LF_READDIR, .h = h, .buf = e};
    return ld_fop(&o);
}
static __attribute__((used, noinline)) int ld_closedir(int h)
{
    ldfop_t o = {.op = LF_CLOSEDIR, .h = h};
    return ld_fop(&o);
}
// DRIVE and CHDIR's: the current drive kept after the trap, which otherwise puts the BASIC program's back
static __attribute__((used, noinline)) int ld_chdir(const char *path)
{
    ldfop_t o = {.op = LF_CHDIR, .s = path, .h = -1};
    int r = ld_fop(&o);
    if (r == 0 && o.h >= 0)
        FatFSFileSystem = FatFSFileSystemSave = o.h;
    return r;
}
static __attribute__((used, noinline)) int ld_getcwd(char *buf, uint32_t size)
{
    ldfop_t o = {.op = LF_GETCWD, .buf = buf, .n = size};
    return ld_fop(&o);
}

/* The screen: MODE (setmode) and MAP's colours (map256, which the display reads each line) by the PicoMite's own
   numbers. BASIC's mode, colours, font and console colours are kept as the program starts and put back if it changed
   them. */
static const uint8_t ld_bpp[6] = {0, 1, 4, 4, 16, 8};               // each MODE's bits a pixel
typedef struct {
    int mode, depth;
    cmm2_video *v;
} ldvid_t;
static void ld_vmodebody(void *a)
{
    ldvid_t *o = a;
    setmode(o->mode, true);
    ld->vmode = 1;
    o->v->fb = WriteBuf;
    o->v->width = HRes;
    o->v->height = VRes;
    o->v->bpp = o->depth;
    o->v->pitch = HRes * o->depth / 8;
    o->v->copies = 1;
    o->mode = 0;
}
static __attribute__((used, noinline)) int ld_vmode(int mode, int depth, cmm2_video *v)
{
    ldvid_t o = {mode, depth, v};
    if (mode < 1 || mode > 5 || depth != ld_bpp[mode] || v == NULL)
        return -1;
    return ld_try(ld_vmodebody, &o) < 0 ? -4 : 0;
}
// the start of the display's next frame, or 50 ms
static void ld_frame(void)
{
    uint64_t dl = time_us_64() + 50000;
    while (v_scanline == 0 && time_us_64() < dl)
    {
    }
    while (v_scanline != 0 && time_us_64() < dl)
    {
    }
}
static int ld_palette(int start, int count, const uint8_t *rgb)
{
    if (DISPLAY_TYPE != SCREENMODE5 || !FullColour || start < 0 || count < 0 || start + count > 256)
        return -1;
    ld_frame();
    for (int i = 0; i < count; i++, rgb += 3)
        map256[start + i] = RGB555((uint32_t)rgb[0] << 16 | rgb[1] << 8 | rgb[2]);
    ld->palette = 1;
    return 0;
}
static void ld_screensave(void)
{
    ld->oldmode = DISPLAY_TYPE - SCREENMODE1 + 1;
    ld->oldfont = gui_font;
    ld->oldfc = gui_fcolour;
    ld->oldbc = gui_bcolour;
    memcpy(ld->oldmap, map256, sizeof(ld->oldmap));
}
static void ld_screenbody(void *a)
{
    (void)a;
    if (ld->vmode)
    {
        setmode(ld->oldmode, true);
        SetFont(ld->oldfont);
        gui_fcolour = ld->oldfc;
        gui_bcolour = ld->oldbc;
        ClearScreen(gui_bcolour);
        CurrentX = CurrentY = 0;
    }
    if (ld->vmode || ld->palette)
    {
        ld_frame();
        memcpy(map256, ld->oldmap, sizeof(ld->oldmap));
    }
}

// the keyboard and the gamepads
static int ld_keysheld(int keys[7])
{
    int n = 0;
    for (int i = 0; i < 7; i++)
    {
        keys[i] = KeyDown[i];
        if (i < 6 && KeyDown[i])
            n++;
    }
    return n;
}
static int ld_keysraw(uint8_t report[8])
{
    int n = 0;
    memcpy(report, LoaderKeyReport, 8);
    for (int i = 2; i < 8; i++)
        if (report[i])
            n++;
    return n;
}
// the pad in USB channel n (1-4): DEVICE(GAMEPAD n, ...)'s LX, LY, RX, RY, L and R in v, and its buttons; -1 if there
// is none. A generic pad (SNES, 130) has no sticks: its d-pad is in the buttons, and its LX and LY stay 0, which would
// be a stick held up and left, so its sticks read 128 and its triggers 0 (as on the CMM2)
static int ld_padaxes(int n, int v[6])
{
    int t;
    if (n < 1 || n > 4 || !HID[n - 1].active)
        return -1;
    t = HID[n - 1].Device_type;
    if (t < 128 || t == 133 || t == 134)                            // (a keyboard, a mouse, a touch screen: USBKeyboard.c)
        return -1;
    v[0] = t == 130 ? 128 : nunstruct[n].ax;
    v[1] = t == 130 ? 128 : nunstruct[n].ay;
    v[2] = t == 130 ? 128 : nunstruct[n].Z;
    v[3] = t == 130 ? 128 : nunstruct[n].C;
    v[4] = t == 130 ? 0 : nunstruct[n].L;
    v[5] = t == 130 ? 0 : nunstruct[n].R;
    return nunstruct[n].x0;
}
static int ld_gamepad(int n, int *lx, int *ly)
{
    int v[6], b = ld_padaxes(n, v);
    if (b >= 0 && lx)
        *lx = v[0];
    if (b >= 0 && ly)
        *ly = v[1];
    return b;
}

// the mouse: DEVICE(MOUSE 2, ...)'s, where a USB mouse (or a PS/2 one) puts it, kept within the screen by the firmware
static int ld_mouse(int m[4])
{
#ifdef USBKEYBOARD
    int on = HID[1].Device_type == 2;
#else
    int on = mouse0;
#endif
    m[0] = on ? nunstruct[2].ax : 0;
    m[1] = on ? nunstruct[2].ay : 0;
    m[2] = on ? (nunstruct[2].L ? 1 : 0) | (nunstruct[2].R ? 2 : 0) | (nunstruct[2].C ? 4 : 0) : 0;
    m[3] = on ? nunstruct[2].az : 0;
    return on;
}
static void ld_mouseset(int x, int y)
{
    nunstruct[2].ax = x < 0 ? 0 : x >= HRes ? HRes - 1 : x;
    nunstruct[2].ay = y < 0 ? 0 : y >= VRes ? VRes - 1 : y;
}

// sound: Audio.c's AppSound, the stream that the program's fill makes (audio_start and audio_stop on the main stack,
// through the veneer below: they allocate and free, and CloseAudio waits)
static void ld_alock(void) { AppSoundHold(1); }
static void ld_aunlock(void) { AppSoundHold(0); }

/* I/O, by GP numbers. SETPIN, PWM, I2C OPEN, SPI OPEN and OPEN "COMn:" run as the interpreter runs them, their arguments
   as text, under ld_try (so their checks and their errors are BASIC's); the transfers are the SDK's calls on the buses
   they opened. What the program sets up is noted in ld (pins, pwmon, buses, coms) and undone by ld_iotidy when it
   ends. A bus's pins are the ones SETPIN or OPTION SYSTEM gave it, or the ones bus_pins names. */
extern void i2cEnable(unsigned char *p), i2c2Enable(unsigned char *p);
extern bool tuh_cdc_mounted(uint8_t idx);
#ifdef USBKEYBOARD
#define LD_COMS 6                                                   // COM1, COM2, and COM3-6 the USB serial devices
#else
#define LD_COMS 2
#endif
typedef struct {
    void (*cmd)(void);                                              // a command, which reads cmdline
    void (*fn)(unsigned char *);                                    // or a routine given the text
    char a[64];
} ldcmd_t;
static void ld_cmdbody(void *p)
{
    ldcmd_t *c = p;
    if (c->fn)
        c->fn((unsigned char *)c->a);
    else
    {
        cmdline = (unsigned char *)c->a;
        c->cmd();
    }
}
static int ld_cmd(void (*cmd)(void), void (*fn)(unsigned char *), const char *fmt, ...)
{
    ldcmd_t c = {cmd, fn, ""};
    unsigned char *line = cmdline;
    va_list v;
    int r;
    va_start(v, fmt);
    vsnprintf(c.a, sizeof(c.a), fmt, v);
    va_end(v);
    r = ld_try(ld_cmdbody, &c);
    cmdline = line;
    return r;
}
static int ld_pin(int gp)                                           // MMBasic's pin, 0: none (or one the board keeps)
{
    return gp < 0 || gp > (rp2350a ? 29 : 47) || IsInvalidPin(PINMAP[gp]) ? 0 : PINMAP[gp];
}
static int ld_inuse(int pin) { return pin == 99 || ExtCurrentConfig[pin] >= EXT_COM_RESERVED; }   // (99: not set)

static __attribute__((used, noinline)) int ld_pinsetup(int gp, int mode, int pull)
{
    static const char *const m[] = {"OFF", "DIN", "DOUT", "DIN", "AIN"};   // (open drain: an input, driven low for 0)
    int pin = ld_pin(gp), r;
    uint64_t b = 1ull << gp;
    if (!pin)
        return -1;
    if (mode < CMM2_PIN_OFF || mode > CMM2_PIN_ANALOG || pull < CMM2_PULL_NONE || pull > CMM2_PULL_DOWN)
        return -3;
    if (ld_inuse(pin))
        return -2;
    r = ld_cmd(cmd_setpin, NULL, "GP%d,%s%s", gp, m[mode],
               (mode != CMM2_PIN_IN && mode != CMM2_PIN_OD) || !pull ? "" : pull == CMM2_PULL_UP ? ",PULLUP" : ",PULLDOWN");
    if (r == 0)
    {
        ld->pins = mode ? ld->pins | b : ld->pins & ~b;
        ld->od = mode == CMM2_PIN_OD ? ld->od | b : ld->od & ~b;
        ld->pwmpins &= ~b;
        if (mode == CMM2_PIN_OD)
            gpio_put(gp, 0);                                        // (what it drives when pin_write gives 0)
    }
    return r;
}
static int ld_pinread(int gp)
{
    int pin = ld_pin(gp);
    if (!pin)
        return -1;
    return ExtCurrentConfig[pin] == EXT_DIG_OUT ? gpio_get_out_level(gp) : gpio_get(gp);
}
static void ld_pinwrite(int gp, int v)
{
    int pin = ld_pin(gp);
    if (pin && (ld->od >> gp & 1))
        gpio_set_dir(gp, !v);                                       // (low, or let go)
    else if (pin && ExtCurrentConfig[pin] == EXT_DIG_OUT)
        gpio_put(gp, v != 0);
}
static int ld_adcread(int gp, uint32_t *uv)
{
    int pin = ld_pin(gp), a;
    if (!pin)
        return -1;
    if (ExtCurrentConfig[pin] != EXT_ANA_IN || (a = (int)ExtInp(pin)) < 0)
        return -3;
    if (uv)
        *uv = (uint32_t)(a * VCC * 1e6 / 4095.0 + 0.5);
    return (a * 65535 + 2047) / 4095;
}

// PWM: a slice's A and B share its frequency. The slice's other output, if the program has it running, is given its
// duty again, as PWM leaves an output it is not given at its old count, made for the old frequency
static __attribute__((used, noinline)) int ld_pwmset(int gp, uint32_t freq, uint32_t duty)
{
    char d[2][12] = {"", ""};
    int pin = ld_pin(gp), s, o, r;
    uint64_t b = 1ull << gp;
    if (!pin)
        return -1;
    if (freq < 1)
        return -3;
    s = pwm_gpio_to_slice_num(gp);
    o = s * 2 + pwm_gpio_to_channel(gp);
    if (!(ld->pwmpins & b))
    {
        if (ld_inuse(pin))
            return -2;
        if ((r = ld_cmd(cmd_setpin, NULL, "GP%d,PWM", gp)) < 0)
            return r;
        ld->pins |= b;
        ld->pwmpins |= b;
    }
    ld->pwmduty[o] = duty > 65535 ? 65535 : duty;
    ld->pwmon |= 1u << o;
    for (int i = 0; i < 2; i++)
        if (ld->pwmon >> (s * 2 + i) & 1)
        {
            uint32_t p = (ld->pwmduty[s * 2 + i] * 10000u + 32767) / 65535;   // (hundredths of a percent)
            snprintf(d[i], sizeof(d[i]), "%u.%02u", (unsigned)(p / 100), (unsigned)(p % 100));
        }
    r = ld_cmd(cmd_pwm, NULL, "%d,%u,%s,%s", s, (unsigned)freq, d[0], d[1]);
    if (r < 0)
        ld->pwmon &= ~(1u << o);
    else
        ld->pwmfreq[s] = freq;
    return r;
}
static __attribute__((used, noinline)) int ld_pwmstop(int gp)
{
    int s, o;
    if (!ld_pin(gp))
        return -1;
    s = pwm_gpio_to_slice_num(gp);
    o = s * 2 + pwm_gpio_to_channel(gp);
    if (!(ld->pwmon >> o & 1))
        return 0;
    ld->pwmon &= ~(1u << o);
    if (ld->pwmon >> (o ^ 1) & 1)                                   // the other output runs: this one at 0%
        return ld_cmd(cmd_pwm, NULL, o & 1 ? "%d,%u,,0" : "%d,%u,0", s, (unsigned)ld->pwmfreq[s]);
    return ld_cmd(cmd_pwm, NULL, "%d,OFF", s);
}

// a bus's pins (SETPIN's), a bus open (by anyone), and the buses' own
static int ld_busopen(int kind, int k)
{
    if (kind == CMM2_BUS_I2C)
        return k ? I2C2_enabled : I2C_enabled;
    if (kind == CMM2_BUS_SPI)
    {
        int tx = k ? SPI1TXpin : SPI0TXpin;                         // (open, or the system's: reserved)
        return tx != 99 && ExtCurrentConfig[tx] >= EXT_COM_RESERVED;
    }
    return k ? com2 : com1;
}
static __attribute__((used, noinline)) int ld_buspins(int kind, int bus, int a, int b, int c)
{
    static const char *const name[3][2] = {{"I2C", "I2C2"}, {"SPI", "SPI2"}, {"COM1", "COM2"}};
    int k = kind == CMM2_BUS_COM ? bus - 1 : bus, r;
    if (kind < CMM2_BUS_I2C || kind > CMM2_BUS_COM || k < 0 || k > 1 || !ld_pin(a) || !ld_pin(b) ||
        (kind == CMM2_BUS_SPI && !ld_pin(c)))
        return -1;
    if (ld_busopen(kind, k))
        return -2;
    if (kind == CMM2_BUS_SPI)
        r = ld_cmd(cmd_setpin, NULL, "GP%d,GP%d,GP%d,%s", a, b, c, name[kind][k]);
    else
        r = ld_cmd(cmd_setpin, NULL, "GP%d,GP%d,%s", a, b, name[kind][k]);
    if (r == 0)
        ld->pins |= 1ull << a | 1ull << b | (kind == CMM2_BUS_SPI ? 1ull << c : 0);
    return r;
}

// I2C: bus 0 is I2C, 1 is I2C2. One already open (OPTION SYSTEM I2C's, or BASIC's I2C OPEN) is used as it is
static __attribute__((used, noinline)) int ld_i2cclose(int bus)
{
    if (bus < 0 || bus > 1)
        return -1;
    if (ld->buses >> bus & 1)
    {
        ld->buses &= ~(1 << bus);
        if (bus)
            i2c2_disable();
        else
            i2c_disable();
    }
    return 0;
}
static __attribute__((used, noinline)) int ld_i2copen(int bus, int khz, int timeout)
{
    int r;
    if (bus < 0 || bus > 1)
        return -1;
    ld_i2cclose(bus);
    if (ld_busopen(CMM2_BUS_I2C, bus))
        return 0;
    if ((bus ? I2C1SDApin : I2C0SDApin) == 99)
        return -3;
    r = ld_cmd(NULL, bus ? i2c2Enable : i2cEnable, "%d,%d", khz, timeout);
    if (r == 0)
        ld->buses |= 1 << bus;
    return r;
}
static int ld_i2cxfer(int bus, int addr, const uint8_t *w, uint32_t wn, uint8_t *r, uint32_t rn, int stop)
{
    i2c_inst_t *i = bus ? i2c1 : i2c0;
    uint32_t t;
    uint8_t probe;
    int e = 0;
    if (bus < 0 || bus > 1)
        return -1;
    if (!ld_busopen(CMM2_BUS_I2C, bus) || addr < 0 || addr > 127)
        return -3;
    t = (bus ? I2C1locked : I2C0locked) ? 1000 : bus ? I2C2_Timeout : I2C_Timeout;   // (ms: the system's bus as BASIC's
    t = t ? t * 1000 : 60000000;                                    // I2C WRITE has it; 0, none)
    if (wn)
        e = i2c_write_timeout_us(i, addr, w, wn, rn || !stop, t);
    if (e >= 0 && (rn || !wn))                                      // (neither: a byte read, as a scan does)
        e = i2c_read_timeout_us(i, addr, rn ? r : &probe, rn ? rn : 1, !stop, t);
    return e == PICO_ERROR_TIMEOUT ? 2 : e < 0 ? 1 : 0;
}

// SPI: bus 0 is SPI, 1 is SPI2, 8 bits. One already open (the system's, or BASIC's) is refused
static __attribute__((used, noinline)) int ld_spiclose(int bus)
{
    if (bus < 0 || bus > 1)
        return -1;
    if (ld->buses >> (bus + 2) & 1)
    {
        ld->buses &= ~(4 << bus);
        if (bus)
            SPI2Close();
        else
            SPIClose();
    }
    return 0;
}
static __attribute__((used, noinline)) int ld_spiopen(int bus, uint32_t hz, int mode)
{
    int r;
    if (bus < 0 || bus > 1)
        return -1;
    ld_spiclose(bus);
    if (ld_busopen(CMM2_BUS_SPI, bus))
        return -2;
    if ((bus ? SPI1TXpin : SPI0TXpin) == 99)
        return -3;
    r = ld_cmd(bus ? cmd_spi2 : cmd_spi, NULL, "OPEN %u,%d,8", (unsigned)hz, mode & 3);
    if (r == 0)
        ld->buses |= 4 << bus;
    return r;
}
static int ld_spixfer(int bus, const uint8_t *tx, uint8_t *rx, uint32_t n)
{
    spi_inst_t *s = bus ? spi1 : spi0;
    uint8_t junk[16];
    if (bus < 0 || bus > 1)
        return -1;
    if (!(ld->buses >> (bus + 2) & 1))
        return -3;
    if (tx && rx)
        spi_write_read_blocking(s, tx, rx, n);
    else if (tx)
        spi_write_blocking(s, tx, n);
    else if (rx)
        spi_read_blocking(s, 0, rx, n);
    else
        for (uint32_t k; n; n -= k)
            spi_read_blocking(s, 0, junk, k = n > sizeof(junk) ? sizeof(junk) : n);
    return 0;
}

// COM: OPEN "COMn:"'s numbers, COM3-6 the USB serial devices
static __attribute__((used, noinline)) int ld_comclose(int com)
{
    if (com < 1 || com > LD_COMS)
        return -1;
    if (ld->coms >> com & 1)
    {
        ld->coms &= ~(1 << com);
        SerialClose(com);
    }
    return 0;
}
static __attribute__((used, noinline)) int ld_comopen(int com, uint32_t baud, int bits, int parity, int stop,
                                                       uint32_t rxbuf)
{
    int r;
    if (com < 1 || com > LD_COMS)
        return -1;
    ld_comclose(com);
    r = ld_cmd(NULL, SerialOpen, "COM%d:%u,%u%s%s%s", com, (unsigned)baud, (unsigned)(rxbuf ? rxbuf : COM_DEFAULT_BUF_SIZE),
               parity == 1 ? ",EVEN" : parity == 2 ? ",ODD" : "", stop == 2 ? ",S2" : "", bits == 7 ? ",7BIT" : "");
    if (r == 0)
        ld->coms |= 1 << com;
    return r;
}
typedef struct {
    int com;
    const uint8_t *buf;
    uint32_t n;
} ldcomw_t;
static void ld_comwbody(void *p)
{
    ldcomw_t *w = p;
    for (uint32_t i = 0; i < w->n; i++)
        SerialPutchar(w->com, w->buf[i]);
}
static __attribute__((used, noinline)) int ld_comwrite(int com, const uint8_t *buf, uint32_t n)
{
    ldcomw_t w = {com, buf, n};
    if (com < 1 || com > LD_COMS)
        return -1;
    if (!(ld->coms >> com & 1))
        return -3;
    return ld_try(ld_comwbody, &w) < 0 ? -4 : (int)n;
}
static int ld_comread(int com, uint8_t *buf, uint32_t n)
{
    uint32_t i = 0;
    int c;
    if (com < 1 || com > LD_COMS)
        return -1;
    if (!(ld->coms >> com & 1))
        return -3;
    while (i < n && (c = SerialGetchar(com)) >= 0)
        buf[i++] = c;
    return (int)i;
}
static int ld_comavail(int com)
{
    if (com < 1 || com > LD_COMS)
        return -1;
    if (!(ld->coms >> com & 1))
        return -3;
    return SerialRxStatus(com);
}
static int ld_comconnected(int com)
{
    if (com < 1 || com > LD_COMS)
        return -1;
    return com <= 2 ? 1 : tuh_cdc_mounted(com - 3);
}

// what the program set up, undone (its end): its PWM stopped, its buses and ports closed, then its pins unconfigured
static void ld_iotidy(void)
{
    for (int s = 0; s < 12; s++)
        if (ld->pwmon >> (s * 2) & 3)
            ld_cmd(cmd_pwm, NULL, "%d,OFF", s);
    for (int b = 0; b < 2; b++)
    {
        ld_i2cclose(b);
        ld_spiclose(b);
    }
    for (int c = 1; c <= LD_COMS; c++)
        ld_comclose(c);
    for (int gp = 0; gp < 48; gp++)
        if ((ld->pins >> gp & 1) && !ld_inuse(PINMAP[gp]))
            ld_cmd(cmd_setpin, NULL, "GP%d,OFF", gp);
}

// what a program or a USER module leaves, given back: its sound stopped (its fill is in the memory freed next), its
// PWM, buses, ports and pins, its files and directories closed, its blocks of the heap freed
static void ld_tidy(void)
{
    AppSoundStop();
    AppSoundHold(0);
    ld_iotidy();
    for (int i = 0; i < LD_FILES; i++)                              // (A:'s are in littlefs's list of open files)
        if (ld->file[i])
            ld_close(i);
    for (int i = 0; i < LD_DIRS; i++)
        if (ld->dir[i])
            ld_closedir(i);
    for (int i = 0; i < LD_BLOCKS; i++)
        FreeMemory(ld->block[i]);
}

/*******************************************************************************************
 calling the program on its own stack
*******************************************************************************************/
// LoaderCall(entry, start, psp, psplim): the registers a call must keep and d8-d15 saved on the main stack, which is
// noted in ldmsp; then the process stack set to psp, its limit to psplim, and taken (CONTROL.SPSEL), and entry(start)
// called. Its return, or a jump to LoaderReturn from leave(), comes back with r0 the code: the main stack again, the
// limit off, the registers restored.
__attribute__((naked)) static int LoaderCall(void *entry, cmm2img_start *start, uint32_t psp, uint32_t psplim)
{
    __asm volatile(
        "push   {r4-r11, lr}        \n"
        "vpush  {d8-d15}            \n"
        "sub    sp, sp, #4          \n" /* (36 + 64 + 4 bytes: the main stack stays 8-byte aligned) */
        "ldr    r12, =ldmsp         \n"
        "str    sp, [r12]           \n"
        "msr    psp, r2             \n"
        "msr    psplim, r3          \n"
        "mrs    r12, control        \n"
        "orr    r12, r12, #2        \n"
        "msr    control, r12        \n"
        "isb                        \n"
        "mov    r12, r0             \n"
        "mov    r0, r1              \n"
        "blx    r12                 \n"
        ".global LoaderReturn       \n"
        ".thumb_func                \n"
        "LoaderReturn:              \n"
        "mrs    r3, control         \n"
        "bic    r3, r3, #2          \n"
        "msr    control, r3         \n"
        "isb                        \n"
        "movs   r3, #0              \n"
        "msr    psplim, r3          \n"
        "ldr    r3, =ldmsp          \n"
        "ldr    sp, [r3]            \n"
        "add    sp, sp, #4          \n"
        "vpop   {d8-d15}            \n"
        "pop    {r4-r11, pc}        \n"
        ".ltorg                     \n");
}
static __attribute__((used, noinline)) void ld_leaveerr(void) { error("A USER module cannot leave()"); }
__attribute__((naked, noreturn)) static void ld_leave(int code)
{
    __asm volatile(
        "ldr    r1, =LoaderActive   \n" /* a program's: back to MMBasic (r0 is already the code) */
        "ldr    r1, [r1]            \n"
        "cbz    r1, 1f              \n"
        "b      LoaderReturn        \n"
        "1:                         \n" /* a USER module's (MMBasic's stack, inside a statement): an error */
        "b      ld_leaveerr         \n"
        ".ltorg                     \n");
}

// A service run on the main stack: the program calls a stub, which puts the service in ip and branches here. Its
// arguments are r0-r3 and up to four words on the program's stack, which are copied to the main stack (a service has
// at most eight). The main stack is where LoaderCall left it (ldmsp: the interrupts that came meanwhile have put it
// back), so the firmware runs on its own stack, in the SRAM; a flash write there cannot take it away. A USER module
// runs on the main stack already: its call goes straight to the service.
__attribute__((naked, used)) static void ld_veneerfn(void)
{
    __asm volatile(
        ".thumb_func                \n"
        "ld_veneer:                 \n"
        "push   {r4, r5, r6, lr}    \n"
        "mrs    r6, control         \n"
        "tst    r6, #2              \n"
        "bne    1f                  \n"
        "pop    {r4, r5, r6, lr}    \n" /* the main stack (a USER module): the service as it is */
        "bx     ip                  \n"
        "1:                         \n"
        "mov    r4, sp              \n" /* the process stack: the stacked arguments at r4+16 */
        "mov    r5, ip              \n"
        "bic    r6, r6, #2          \n"
        "msr    control, r6         \n"
        "isb                        \n"
        "sub    sp, sp, #16         \n"
        "ldr    r6, [r4, #16]       \n"
        "str    r6, [sp, #0]        \n"
        "ldr    r6, [r4, #20]       \n"
        "str    r6, [sp, #4]        \n"
        "ldr    r6, [r4, #24]       \n"
        "str    r6, [sp, #8]        \n"
        "ldr    r6, [r4, #28]       \n"
        "str    r6, [sp, #12]       \n"
        "blx    r5                  \n"
        "add    sp, sp, #16         \n"
        "mrs    r6, control         \n"
        "orr    r6, r6, #2          \n"
        "msr    control, r6         \n"
        "isb                        \n" /* (the process stack again: r4) */
        "pop    {r4, r5, r6, pc}    \n");
}
#define LD_VENEER(stub, fn)                                                       \
    __attribute__((naked)) static void stub(void)                                \
    {                                                                             \
        __asm volatile("ldr ip, =" #fn "\n b ld_veneer\n .ltorg\n");             \
    }
LD_VENEER(v_putc, ld_putc)
LD_VENEER(v_puts, ld_puts)
LD_VENEER(v_datetime, ld_datetime)
LD_VENEER(v_poll, ld_poll)
LD_VENEER(v_alloc, ld_alloc)
LD_VENEER(v_release, ld_release)
LD_VENEER(v_open, ld_open)
LD_VENEER(v_close, ld_close)
LD_VENEER(v_read, ld_read)
LD_VENEER(v_write, ld_write)
LD_VENEER(v_seek, ld_seek)
LD_VENEER(v_tell, ld_tell)
LD_VENEER(v_size, ld_size)
LD_VENEER(v_sync, ld_sync)
LD_VENEER(v_truncate, ld_truncate)
LD_VENEER(v_remove, ld_remove)
LD_VENEER(v_rename, ld_rename)
LD_VENEER(v_stat, ld_stat)
LD_VENEER(v_mkdir, ld_mkdir)
LD_VENEER(v_opendir, ld_opendir)
LD_VENEER(v_readdir, ld_readdir)
LD_VENEER(v_closedir, ld_closedir)
LD_VENEER(v_chdir, ld_chdir)
LD_VENEER(v_getcwd, ld_getcwd)
LD_VENEER(v_vmode, ld_vmode)
LD_VENEER(v_astart, AppSoundStart)
LD_VENEER(v_astop, AppSoundStop)
LD_VENEER(v_pinsetup, ld_pinsetup)
LD_VENEER(v_pwmset, ld_pwmset)
LD_VENEER(v_pwmstop, ld_pwmstop)
LD_VENEER(v_buspins, ld_buspins)
LD_VENEER(v_i2copen, ld_i2copen)
LD_VENEER(v_i2cclose, ld_i2cclose)
LD_VENEER(v_spiopen, ld_spiopen)
LD_VENEER(v_spiclose, ld_spiclose)
LD_VENEER(v_comopen, ld_comopen)
LD_VENEER(v_comclose, ld_comclose)
LD_VENEER(v_comwrite, ld_comwrite)

static const struct cmm2_services ldsvc = {
    .version = PMIMG_API_VERSION,
    .size = sizeof(struct cmm2_services),
    .con_putc = (void (*)(int))v_putc,
    .con_puts = (void (*)(const char *))v_puts,
    .con_getc = ld_getc,
    .con_screen = ld_conscreen,
    .micros = ld_micros,
    .delay_us = ld_delay,
    .datetime = (void (*)(int *))v_datetime,
    .poll = (int (*)(void))v_poll,
    .leave = ld_leave,
    .cpu_hz = ld_cpuhz,
    .break_take = ld_breaktake,
    .alloc = (void *(*)(uint32_t))v_alloc,
    .release = (void (*)(void *))v_release,
    .mem_copy = memcpy,
    .mem_fill = memset,
    .mem_move = memmove,
    .io_error = ld_ioerror,
    .psram_size = ld_psramsize,
    .file_open = (int (*)(const char *, int))v_open,
    .file_close = (int (*)(int))v_close,
    .file_read = (int32_t (*)(int, void *, uint32_t))v_read,
    .file_write = (int32_t (*)(int, const void *, uint32_t))v_write,
    .file_seek = (int (*)(int, uint32_t))v_seek,
    .file_tell = (int32_t (*)(int))v_tell,
    .file_size = (int32_t (*)(int))v_size,
    .file_sync = (int (*)(int))v_sync,
    .file_truncate = (int (*)(int))v_truncate,
    .file_remove = (int (*)(const char *))v_remove,
    .file_rename = (int (*)(const char *, const char *))v_rename,
    .file_stat = (int (*)(const char *, cmm2_stat *))v_stat,
    .dir_make = (int (*)(const char *))v_mkdir,
    .dir_open = (int (*)(const char *))v_opendir,
    .dir_read = (int (*)(int, cmm2_dirent *))v_readdir,
    .dir_close = (int (*)(int))v_closedir,
    .dir_change = (int (*)(const char *))v_chdir,
    .dir_current = (int (*)(char *, uint32_t))v_getcwd,
    .video_mode = (int (*)(int, int, cmm2_video *))v_vmode,
    .video_palette = ld_palette,
    .video_wait = ld_frame,
    .keys_held = ld_keysheld,
    .keys_raw = ld_keysraw,
    .gamepad = ld_gamepad,
    .audio_start = (int (*)(void (*)(int16_t *, uint32_t)))v_astart,
    .audio_stop = (void (*)(void))v_astop,
    .audio_lock = ld_alock,
    .audio_unlock = ld_aunlock,
    .mouse = ld_mouse,
    .mouse_set = ld_mouseset,
    .gamepad_axes = ld_padaxes,
    .pin_setup = (int (*)(int, int, int))v_pinsetup,
    .pin_read = ld_pinread,
    .pin_write = ld_pinwrite,
    .adc_read = ld_adcread,
    .pwm_set = (int (*)(int, uint32_t, uint32_t))v_pwmset,
    .pwm_stop = (int (*)(int))v_pwmstop,
    .bus_pins = (int (*)(int, int, int, int, int))v_buspins,
    .i2c_open = (int (*)(int, int, int))v_i2copen,
    .i2c_close = (int (*)(int))v_i2cclose,
    .i2c_xfer = ld_i2cxfer,
    .spi_open = (int (*)(int, uint32_t, int))v_spiopen,
    .spi_close = (int (*)(int))v_spiclose,
    .spi_xfer = ld_spixfer,
    .com_open = (int (*)(int, uint32_t, int, int, int, uint32_t))v_comopen,
    .com_close = (int (*)(int))v_comclose,
    .com_write = (int (*)(int, const uint8_t *, uint32_t))v_comwrite,
    .com_read = ld_comread,
    .com_avail = ld_comavail,
    .com_connected = ld_comconnected,
};

/*******************************************************************************************
 RUN "name.app"
*******************************************************************************************/
int LoaderIsApp(const char *fname)
{
    size_t n = strlen(fname);
    return n > 4 && fname[n - 4] == '.' && mytoupper(fname[n - 3]) == 'A' && mytoupper(fname[n - 2]) == 'P' &&
           mytoupper(fname[n - 1]) == 'P';
}

// what loading a program needs, from the file to the place it runs (ld_load, under FileTry)
typedef struct {
    char *fname;
    int module;                                                     // a USER module, not a program
    pmimg_header h;
    unsigned char *area;                                            // the PSRAM borrowed (a module's block), or NULL
    uint32_t areasize;
    unsigned char *stack;                                           // its stack in the SRAM heap, or NULL
    uint32_t stacksize;
    unsigned char *base;                                            // where the image goes
    unsigned char *heap;
    uint32_t heapsize;
    uint32_t psp, psplim;
    char msg[MAXERRMSG];                                            // why it was refused
} ldload_t;

// the largest free run of the PSRAM heap less LD_RESERVE, marked as one block (FreeMemory gives it back): its start,
// or NULL with *size the most there is
static unsigned char *ld_claim(uint32_t *size)
{
    unsigned char *a, *run = NULL, *best = NULL;
    unsigned char *const top = (unsigned char *)(PSRAMbase + PSRAMsize);
    uint32_t n = 0, bestn = 0, i;
    for (a = (unsigned char *)PSRAMbase; a < top; a += PAGESIZE)
    {
        if (SBitsGet(a) & PUSED)
            n = 0;
        else
        {
            if (n++ == 0)
                run = a;
            if (n > bestn)
            {
                bestn = n;
                best = run;
            }
        }
    }
    n = bestn > LD_RESERVE / PAGESIZE ? bestn - LD_RESERVE / PAGESIZE : 0;
    *size = n * PAGESIZE;
    if (n < 2)
        return NULL;
    for (i = 0; i < n; i++)
        SBitsSet(best + i * PAGESIZE, i == n - 1 ? (PUSED | PLAST) : PUSED);
    return best;
}

// the open file's next n bytes into dst, through an SRAM buffer (an SD card's DMA never writes the PSRAM): 1, or 0 if
// the file is shorter or cannot be read
static int ld_get(int fnbr, void *dst, uint32_t n, unsigned char *buf)
{
    unsigned int got;
    while (n)
    {
        uint32_t k = n > LD_CHUNK ? LD_CHUNK : n;
        got = 0;
        if (FileGetData(fnbr, buf, k, &got) != 0 || MMerrno || got != k)
            return 0;
        memcpy(dst, buf, k);
        dst = (unsigned char *)dst + k;
        n -= k;
    }
    return 1;
}

static bool ld_refuse(ldload_t *L, const char *msg)
{
    strncpy(L->msg, msg, sizeof(L->msg) - 1);
    return false;
}

// a Thumb-2 MOVT (T1) given another immediate: hw1 11110 i 101100 imm4, hw2 0 imm3 Rd imm8
static uint16_t ld_movt_imm(uint16_t *ins)
{
    return ((ins[0] & 0xF) << 12) | ((ins[0] & 0x400) << 1) | ((ins[1] & 0x7000) >> 4) | (ins[1] & 0xFF);
}
static void ld_movt_set(uint16_t *ins, uint16_t v)
{
    ins[0] = (ins[0] & ~0x040F) | ((v >> 12) & 0xF) | ((v & 0x800) >> 1);
    ins[1] = (ins[1] & ~0x70FF) | ((v & 0x700) << 4) | (v & 0xFF);
}

// the program read in, checked and put in its place: true, or false with L->msg (or MMErrMsg) saying why. Run under
// FileTry, which closes the file if it is left open; what it borrowed is in L, given back by the caller on a failure.
static bool ld_load(void *a)
{
    ldload_t *L = a;
    pmimg_header *h = &L->h;
    unsigned char *buf;
    uint32_t fsize, end, rel, place, align, i, crc;
    int fnbr;
    fnbr = FindFreeFileNbr();
    FatFSFileSystemSave = FatFSFileSystem;
    if (!BasicFileOpen(L->fname, fnbr, FA_READ))
        return false;
    fsize = filesource[fnbr] == FATFSFILE ? f_size(FileTable[fnbr].fptr) : lfs_file_size(&lfs, FileTable[fnbr].lfsptr);
    buf = GetTempMemory(LD_CHUNK);
    if (!ld_get(fnbr, h, sizeof(*h), buf) || memcmp(h->magic, PMIMG_MAGIC, 8))
        return ld_refuse(L, "Not a loadable program");
    if (h->hdr_version != PMIMG_HDR_VERSION || h->kind != (L->module ? PMIMG_KIND_MODULE : PMIMG_KIND_APP) ||
        h->target != PMIMG_TARGET ||
        h->image_size == 0 || !(h->entry & 1) || h->entry >= h->image_size || h->align_log2 > 16 ||
        h->reloc_count > 0x1000000 || fsize != sizeof(*h) + h->image_size + h->reloc_count * 4)
        return ld_refuse(L, "Not a program this firmware can load");
    if (h->api_version > PMIMG_API_VERSION)
        return ld_refuse(L, "Needs a newer firmware");

    // its place: the image, its .bss, then its heap; the relocations read in where the heap will be
    end = (h->image_size + h->bss_size + 7) & ~7u;
    rel = h->reloc_count * 4;
    align = (h->flags & PMIMG_FLAG_MOVT) ? 0x10000 : (1u << h->align_log2) < 8 ? 8 : 1u << h->align_log2;
    if (L->module)
    {   // a module: a block of the heap for the image, its .bss and the relocations, and one for its C library's heap.
        // It has no stack of its own: its functions run on MMBasic's
        L->heapsize = ((h->heap_min ? h->heap_min : LD_MOD_HEAP) + 7) & ~7u;
        if ((L->area = GetMemoryNull(end + rel + align)) == NULL || (L->heap = GetMemoryNull(L->heapsize)) == NULL)
        {
            snprintf(L->msg, sizeof(L->msg), "Needs %u KB of the heap",
                     (unsigned)((end + rel + align + L->heapsize + 1023) / 1024));
            return false;
        }
        L->base = (unsigned char *)(((uint32_t)L->area + align - 1) & ~(align - 1));
        goto read;
    }
    L->stacksize = h->stack_size ? (h->stack_size + 7) & ~7u : PMIMG_STACK;
    if ((L->stack = GetSRAMMemoryNull(L->stacksize)) == NULL) // (else the stack at the top of the PSRAM borrowed)
        place = end + (rel > L->stacksize ? rel : L->stacksize);
    else
        place = end + rel;
    L->area = ld_claim(&L->areasize);
    if (L->area)
    {
        unsigned char *areaend = L->area + L->areasize;
        if ((unsigned char *)h->link_base >= L->area && (unsigned char *)h->link_base < areaend)
            L->base = (unsigned char *)h->link_base;                // (nothing moved)
        else
            L->base = (unsigned char *)(((uint32_t)L->area + align - 1) & ~(align - 1));
        if (L->base + place <= areaend)
        {
            L->heap = L->base + end;
            L->heapsize = (uint32_t)(areaend - L->heap) - (L->stack ? 0 : L->stacksize);
            if ((L->base - L->area) + place <= L->areasize && L->heapsize >= h->heap_min)
                goto fits;
        }
    }
    snprintf(L->msg, sizeof(L->msg), "Needs %u KB, %u KB free in one block",
             (unsigned)(((uint64_t)end + (L->stack ? 0 : L->stacksize) + h->heap_min + 1023) / 1024),
             (unsigned)(L->areasize / 1024));
    return false;
fits:
    if (L->stack)
    {
        L->psplim = ((uint32_t)L->stack + 7) & ~7u;
        L->psp = ((uint32_t)L->stack + L->stacksize) & ~7u;
    }
    else
    {
        L->psp = ((uint32_t)L->area + L->areasize) & ~7u;
        L->psplim = L->psp - L->stacksize;
    }

read: // the image straight into its place, the relocations after its .bss
    if (!ld_get(fnbr, L->base, h->image_size, buf) || !ld_get(fnbr, L->base + end, rel, buf))
        return ld_refuse(L, "Cannot read the program");
    ForceFileClose(fnbr);
    crc = lfs_crc(0xFFFFFFFF, L->base, h->image_size);
    crc = lfs_crc(crc, L->base + end, rel) ^ 0xFFFFFFFF;
    if (crc != h->crc32)
        return ld_refuse(L, "The program file is damaged");
    if ((uint32_t)L->base != h->link_base)
    {
        uint32_t delta = (uint32_t)L->base - h->link_base, w, *r = (uint32_t *)(L->base + end);
        for (i = 0; i < h->reloc_count; i++)
        {
            uint32_t off = PMIMG_REL_OFFSET(r[i]);
            if (off + 4 > h->image_size)
                return ld_refuse(L, "The program file is damaged");
            if (PMIMG_REL_KIND(r[i]) == PMIMG_REL_ABS32)
            {
                memcpy(&w, L->base + off, 4);
                w += delta;
                memcpy(L->base + off, &w, 4);
            }
            else if (PMIMG_REL_KIND(r[i]) == PMIMG_REL_MOVT && !(delta & 0xFFFF))
            {
                uint16_t ins[2];
                memcpy(ins, L->base + off, 4);
                ld_movt_set(ins, ld_movt_imm(ins) + (delta >> 16));
                memcpy(L->base + off, ins, 4);
            }
            else
                return ld_refuse(L, "The program file is damaged");
        }
    }
    memset(L->base + h->image_size, 0, h->bss_size);
    return true;
}

// the words of text (a word in double quotes may hold spaces): how many, and the bytes they take with their ends
static int ld_words(const char *text, int *len)
{
    int n = 0;
    const char *p = text;
    *len = 0;
    for (;;)
    {
        while (*p == ' ')
            p++;
        if (*p == 0)
            return n;
        n++;
        if (*p == '"')
        {
            p++;
            while (*p && *p != '"')
                p++, (*len)++;
            if (*p)
                p++;
        }
        else
            while (*p && *p != ' ')
                p++, (*len)++;
        (*len)++;
    }
}

void LoaderRun(char *fname, char *args)
{
    ldload_t *L;
    ldstate_t *was = ld;                                            // a USER module's, kept while the program runs
    char *p, *s, name[STRINGSIZE];
    unsigned char *area, *stack;
    int argc, len, code;
    unsigned int wd;
    if (LoaderActive)
        error("A loadable program is running");
    if (!PSRAMsize)
        error("Needs PSRAM");
    CloseAudio(1);                                                  // (its refills would stop while the program runs)
    strncpy(name, fname, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;

    // the state and argv in one block of the SRAM heap: argv[0] the file's name, then args's words
    argc = ld_words(args, &len) + 1;
    len += strlen(name) + 1;
    ld = GetMemory(sizeof(ldstate_t) + (argc + 1) * sizeof(char *) + len);
    s = (char *)&ld->argv[argc + 1];
    ld->argv[0] = s;
    strcpy(s, name);
    s += strlen(name) + 1;
    for (argc = 1, p = args;;)
    {
        while (*p == ' ')
            p++;
        if (*p == 0)
            break;
        ld->argv[argc++] = s;
        if (*p == '"')
        {
            p++;
            while (*p && *p != '"')
                *s++ = *p++;
            if (*p)
                p++;
        }
        else
            while (*p && *p != ' ')
                *s++ = *p++;
        *s++ = 0;
    }
    ld->argv[argc] = NULL;

    // Loaded with a Ctrl-C held off (LoaderActive: CheckAbort's longjmp to the prompt would leave the PSRAM claimed),
    // and seen once the memory is given back: MMAbort stays set, so the BASIC program or the prompt stops next.
    L = GetTempMemory(sizeof(ldload_t));
    L->fname = name;
    MMAbort = 0;
    LoaderActive = 1;
    if (!FileTry(ld_load, L) || MMAbort)
    {
        char msg[MAXERRMSG];
        LoaderActive = 0;
        strcpy(msg, *L->msg ? L->msg : *MMErrMsg ? MMErrMsg : "Cannot read the program");
        FreeMemory(L->area);
        FreeMemory(L->stack);
        FreeMemory(ld);
        ld = was;
        if (MMAbort)
            return;
        error("$", msg);
    }
    LoaderActive = 0;

    ld->screen = 1;
    ld->start.svc = &ldsvc;
    ld->start.argc = argc;
    ld->start.argv = ld->argv;
    ld->start.heap = L->heap;
    ld->start.heap_size = L->heapsize;
    ld->start.base = (uint32_t)L->base;
    area = L->area;                                                 // (L is temporary memory: kept here for the end)
    stack = L->stack;
    ld_screensave();
    wd = WDTimer;                                                   // the software watchdog held while it runs
    WDTimer = 0;
    LoaderActive = 1;
    code = LoaderCall(L->base + L->h.entry, &ld->start, L->psp, L->psplim);
    LoaderActive = 0;

    // what it left: its sound, I/O, files and blocks, the memory it borrowed, the keys it had (not typed into BASIC).
    // A Ctrl-C stays: the BASIC program that ran it stops next.
    ld_tidy();
    ld_try(ld_screenbody, NULL);                                    // (BASIC's screen, if the program changed it)
    FreeMemory(area);
    FreeMemory(stack);
    FreeMemory(ld);
    ld = was;
    ConsoleRxBufTail = ConsoleRxBufHead;
    WDTimer = wd;
    if (code)
    {
        char b[STRINGSIZE + 40];
        snprintf(b, sizeof(b), "%s ended with code %d\r\n", name, code);
        MMPrintString(b);
    }
}

/*******************************************************************************************
 a USER module (misc/User.c): loaded into the heap beside the BASIC program, and kept until USER CLOSE, another USER
 LOAD, or the heap's wipe (InitHeap: RUN, NEW, LOAD, CHAIN...). Its code and .bss are one block of the heap, its C
 library's heap another; it has no stack of its own, as its functions (and its start) run on MMBasic's, inside a
 statement. Its services' state is ldmod, which is ld whenever no program is running.
*******************************************************************************************/
static ldstate_t *ldmod;
static const cmm2mod_head *ldmodhead;
static void *ldmodarea, *ldmodheap;

const void *LoaderModuleTable(void) { return ldmodhead; }

// the module gone: its forget() called (an error in it ignored), what it left given back, its memory freed
void LoaderModuleDrop(void)
{
    const cmm2mod_head *t = ldmodhead;
    if (!ldmod)
        return;
    ldmodhead = NULL;
    ld = ldmod;
    if (t && t->forget)
        ld_try((void (*)(void *))t->forget, NULL);
    ld_tidy();
    FreeMemory(ldmodarea);
    FreeMemory(ldmodheap);
    FreeMemory(ldmod);
    ldmod = NULL;
    ldmodarea = ldmodheap = NULL;
    ld = NULL;
}

// the module in fname loaded (the one loaded before dropped first), basic its start block's: its table, or an
// MMBasic error
const void *LoaderModuleLoad(char *fname, const void *basic)
{
    ldload_t *L;
    const cmm2mod_head *t;
    LoaderModuleDrop();
    if (LoaderActive)
        error("A loadable program is running");
    L = GetTempMemory(sizeof(ldload_t));
    L->fname = fname;
    L->module = 1;
    ldmod = GetMemory(sizeof(ldstate_t));
    if (!FileTry(ld_load, L))
    {
        char msg[MAXERRMSG];
        strcpy(msg, *L->msg ? L->msg : *MMErrMsg ? MMErrMsg : "Cannot read the module");
        FreeMemory(L->area);
        FreeMemory(L->heap);
        FreeMemory(ldmod);
        ldmod = NULL;
        error("$", msg);
    }
    ldmodarea = L->area;
    ldmodheap = L->heap;
    ld = ldmod;
    ld->screen = 1;
    ld->start.svc = &ldsvc;
    ld->start.heap = L->heap;
    ld->start.heap_size = L->heapsize;
    ld->start.base = (uint32_t)L->base;
    ld->start.basic = basic;
    t = ((const cmm2mod_head *(*)(cmm2img_start *))(L->base + L->h.entry))(&ld->start);
    if (t == NULL || t->magic != CMM2MOD_MAGIC)
    {
        LoaderModuleDrop();
        error("$ is not a module", fname);
    }
    ldmodhead = t;
    return t;
}
#endif
