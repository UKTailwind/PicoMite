/* sqlcore.c: SQLite on the Colour Maximite 2's services. MIT License.

   SQLite is built with SQLITE_OS_OTHER (sqlite_cmm2.h), so its OS layer is this file: a VFS ("cmm2") whose files are
   the firmware's (file_open and the rest, FatFs underneath, on the SD card or a USB drive), its time MMBasic's clock,
   its sleep delay_us. One program uses a database at a time, so there are no locks: SQLite runs in exclusive locking
   mode, and the journal (a rollback journal beside the database, "name-journal") is what keeps a database whole when
   power is lost. Its memory is one pool (SQLITE_CONFIG_HEAP: MEMSYS5, a buddy allocator), never malloc's.

   PicoMite: the Colour Maximite 2's apps/sql/sqlcore.c (5e9721f), with the PicoMite's services (the same names:
   tools/sdk/cmm2.h). Its databases, journals and temporary files are on B: (the SD card) or C: (a USB drive) only,
   FatFs drives (Peter, 24 Nov 2026): A: is littlefs in the flash, which rewrites a file from the block that changed to
   its end, so each commit would rewrite most of a database. The program itself may be loaded from A:. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cmm2.h"
#include "sqlcore.h"

#define PATHMAX 255                         /* SQLite's mxPathname: FatFs's longest name, with the drive */
#define FR_NO_FILE 4                        /* (FatFs's FRESULT, as the services return it negated) */
#ifdef PMIMG_API_VERSION
#define HAVE_TRUNCATE 1                     /* PicoMite: file_truncate in every version of its services */
#define DB_DRIVE(d) (((d) & ~0x20) == 'B' || ((d) & ~0x20) == 'C')   /* a drive a database may be on (above) */
#else
#define HAVE_TRUNCATE (cmm2->version >= 19)
#define DB_DRIVE(d) 1
#endif

int sqlcore_pool = 512 * 1024;
int sqlcore_cache = -64;
volatile int sqlcore_broken;

typedef struct {
    sqlite3_file base;                      /* (first: SQLite's view of it) */
    int h;                                  /* the services' handle, -1 when closed */
    int32_t pos;                            /* where the handle is in the file, -1 if not known */
    int flags;                              /* SQLITE_OPEN_* as opened */
    char name[PATHMAX + 1];                 /* (to reopen it emptied, and to remove it at close) */
} cfile;

static int c_seek(cfile *c, sqlite3_int64 off) {   /* 0, or -1; c->pos where the handle is after it */
    if(off > 0x7FFFFFFF) return -1;         /* (the services' positions are 31 bits: 2 GB) */
    if(c->pos == off) return 0;
    if(cmm2->file_seek(c->h, (uint32_t)off) < 0) {
        c->pos = -1;
        return -1;
    }
    c->pos = cmm2->file_tell(c->h);         /* (opened for reading only, a seek stops at the end) */
    return 0;
}

static int c_close(sqlite3_file *f) {
    cfile *c = (cfile *)f;
    int r = SQLITE_OK;
    if(c->h >= 0 && cmm2->file_close(c->h) < 0) r = SQLITE_IOERR_CLOSE;
    c->h = -1;
    if(c->flags & SQLITE_OPEN_DELETEONCLOSE) cmm2->file_remove(c->name);
    return r;
}

static int c_read(sqlite3_file *f, void *buf, int amt, sqlite3_int64 off) {
    cfile *c = (cfile *)f;
    int32_t got = 0;
    if(c_seek(c, off) < 0) return SQLITE_IOERR_READ;
    if(c->pos == off) {                     /* (else off is past the end: nothing to read) */
        if((got = cmm2->file_read(c->h, buf, (uint32_t)amt)) < 0) {
            c->pos = -1;
            return SQLITE_IOERR_READ;
        }
        c->pos += got;
    }
    if(got < amt) {                         /* SQLite wants the rest zeroed, and to be told */
        memset((char *)buf + got, 0, (size_t)(amt - got));
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int c_write(sqlite3_file *f, const void *buf, int amt, sqlite3_int64 off) {
    cfile *c = (cfile *)f;
    int32_t put;
    if(c_seek(c, off) < 0 || c->pos != off) return SQLITE_IOERR_WRITE;   /* (a seek past the end makes a file opened
                                                                              for writing longer: FatFs's f_lseek) */
    if((put = cmm2->file_write(c->h, buf, (uint32_t)amt)) < 0) {
        c->pos = -1;
        return SQLITE_IOERR_WRITE;
    }
    c->pos += put;
    return put < amt ? SQLITE_FULL : SQLITE_OK;   /* (FatFs writes less when the card is full) */
}

/* A file cut short by file_truncate (services version 19, from 7.00.00RC6). Before it there was no such service: emptied, a file
   is opened again created; cut short otherwise, it is left longer: SQLite takes a database's size from its header, so
   the pages after it are never read, and are written over as the database grows again. */
static int c_truncate(sqlite3_file *f, sqlite3_int64 size) {
    cfile *c = (cfile *)f;
    int32_t now = cmm2->file_size(c->h);
    if(now < 0) return SQLITE_IOERR_TRUNCATE;
    if(size >= now) return SQLITE_OK;
    if(HAVE_TRUNCATE) {
        if(cmm2->file_seek(c->h, (uint32_t)size) < 0 || cmm2->file_truncate(c->h) < 0) {
            c->pos = -1;
            return SQLITE_IOERR_TRUNCATE;
        }
        c->pos = (int32_t)size;
        return SQLITE_OK;
    }
    if(size == 0) {
        cmm2->file_close(c->h);
        c->pos = -1;
        if((c->h = cmm2->file_open(c->name, CMM2_O_READ | CMM2_O_WRITE | CMM2_O_CREATE)) < 0) {
            c->h = -1;
            return SQLITE_IOERR_TRUNCATE;
        }
        c->pos = 0;
    }
    return SQLITE_OK;
}

static int c_sync(sqlite3_file *f, int flags) {
    cfile *c = (cfile *)f;
    (void)flags;
    return cmm2->file_sync(c->h) < 0 ? SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int c_size(sqlite3_file *f, sqlite3_int64 *size) {
    cfile *c = (cfile *)f;
    int32_t n = cmm2->file_size(c->h);
    if(n < 0) return SQLITE_IOERR_FSTAT;
    *size = n;
    return SQLITE_OK;
}

static int c_lock(sqlite3_file *f, int level) { (void)f; (void)level; return SQLITE_OK; }
static int c_reserved(sqlite3_file *f, int *out) { (void)f; *out = 0; return SQLITE_OK; }
static int c_control(sqlite3_file *f, int op, void *arg) { (void)f; (void)op; (void)arg; return SQLITE_NOTFOUND; }
static int c_sector(sqlite3_file *f) { (void)f; return 512; }
static int c_device(sqlite3_file *f) { (void)f; return 0; }

static const sqlite3_io_methods c_methods = {
    1, c_close, c_read, c_write, c_truncate, c_sync, c_size, c_lock, c_lock, c_reserved, c_control, c_sector, c_device
};

static uint64_t rnd;                        /* xorshift64*, seeded from the clock */
static uint64_t rnd_next(void) {
    if(rnd == 0) rnd = cmm2->micros() * 0x9E3779B97F4A7C15ull | 1;
    rnd ^= rnd >> 12;
    rnd ^= rnd << 25;
    rnd ^= rnd >> 27;
    return rnd * 0x2545F4914F6CDD1Dull;
}

/* the current directory's drive, as "A" */
static char drive(void) {
    char cwd[PATHMAX + 1];
    if(cmm2->dir_current(cwd, sizeof(cwd)) < 0 || cwd[0] == 0 || cwd[1] != ':') return 'A';
    return cwd[0];
}

static char tmpdrive = 'B';                 /* PicoMite: temporary files on the last database's drive (never A:) */

static int v_open(sqlite3_vfs *vfs, sqlite3_filename zname, sqlite3_file *f, int flags, int *outflags) {
    cfile *c = (cfile *)f;
    cmm2_stat st;
    int mode, there;
    (void)vfs;
    memset(c, 0, sizeof(*c));               /* (pMethods NULL: nothing to close if this fails) */
    c->h = -1;
    c->pos = -1;
    if(zname == NULL) {                     /* a temporary file (a big sort's spill): a name of its own at the root */
#ifdef PMIMG_API_VERSION
        snprintf(c->name, sizeof(c->name), "%c:/sqlite_%08lx.tmp", tmpdrive, (unsigned long)(rnd_next() >> 32));
#else
        snprintf(c->name, sizeof(c->name), "%c:/sqlite_%08lx.tmp", drive(), (unsigned long)(rnd_next() >> 32));
#endif
        flags |= SQLITE_OPEN_DELETEONCLOSE;
    } else {
        if(strlen(zname) > PATHMAX) return SQLITE_CANTOPEN;
        strcpy(c->name, zname);             /* (whole: v_full made it so) */
        if(c->name[1] != ':' || !DB_DRIVE(c->name[0])) return SQLITE_CANTOPEN;
        if(flags & SQLITE_OPEN_MAIN_DB) tmpdrive = c->name[0];
    }
    c->flags = flags;
    there = cmm2->file_stat(c->name, &st) == 0;
    if(there && st.is_dir) return SQLITE_CANTOPEN;
    if(flags & SQLITE_OPEN_READONLY) mode = CMM2_O_READ;
    else {
        if(there && (flags & SQLITE_OPEN_EXCLUSIVE)) return SQLITE_CANTOPEN;
        mode = CMM2_O_READ | CMM2_O_WRITE;  /* (WRITE alone opens a file that is there, as it is) */
        if(!there) {
            if(!(flags & SQLITE_OPEN_CREATE)) return SQLITE_CANTOPEN;
            mode |= CMM2_O_CREATE;
        }
    }
    if((c->h = cmm2->file_open(c->name, mode)) < 0) {
        c->h = -1;
        return SQLITE_CANTOPEN;
    }
    c->pos = 0;
    if(outflags) *outflags = flags;
    c->base.pMethods = &c_methods;
    return SQLITE_OK;
}

static int v_delete(sqlite3_vfs *vfs, const char *name, int syncdir) {
    int r = cmm2->file_remove(name);
    (void)vfs;
    (void)syncdir;
    if(r == -FR_NO_FILE) return SQLITE_IOERR_DELETE_NOENT;
    return r < 0 ? SQLITE_IOERR_DELETE : SQLITE_OK;
}

/* there, as the Unix VFS has it: an empty file counts as none (an empty journal is not a hot one) */
static int v_access(sqlite3_vfs *vfs, const char *name, int flags, int *out) {
    cmm2_stat st;
    (void)vfs;
    *out = cmm2->file_stat(name, &st) == 0 && (flags != SQLITE_ACCESS_EXISTS || st.is_dir || st.size > 0);
    return SQLITE_OK;
}

/* a name as FatFs takes it, whole: "X:/..." as it is, "/..." on the current drive, else after the current directory */
static int v_full(sqlite3_vfs *vfs, const char *in, int n, char *out) {
    char cwd[PATHMAX + 1];
    int len;
    (void)vfs;
    if(in[0] && in[1] == ':') len = snprintf(out, (size_t)n, "%s", in);
    else if(in[0] == '/' || in[0] == '\\') len = snprintf(out, (size_t)n, "%c:%s", drive(), in);
    else {
        if(cmm2->dir_current(cwd, sizeof(cwd)) < 0) return SQLITE_CANTOPEN;
        len = snprintf(out, (size_t)n, "%s%s%s", cwd, cwd[0] && cwd[strlen(cwd) - 1] == '/' ? "" : "/", in);
    }
    if(len < 0 || len >= n) return SQLITE_CANTOPEN;
    for(; *out; out++) if(*out == '\\') *out = '/';
    return SQLITE_OK;
}

/* 1 if a database may be called name: made whole as SQLite makes it, on a drive DB_DRIVE allows (":memory:" and "",
   SQLite's own, always) */
int sqlcore_name_ok(const char *name) {
    char full[PATHMAX + 1];
    if(name[0] == 0 || strcmp(name, ":memory:") == 0) return 1;
    if(v_full(NULL, name, sizeof(full), full) != SQLITE_OK) return 0;
    return full[1] == ':' && DB_DRIVE(full[0]);
}

static int v_random(sqlite3_vfs *vfs, int n, char *out) {
    int i;
    (void)vfs;
    for(i = 0; i < n; i++) out[i] = (char)(rnd_next() >> 56);
    return n;
}

static int v_sleep(sqlite3_vfs *vfs, int us) {
    (void)vfs;
    if(us > 0) cmm2->delay_us((uint32_t)us);
    return us;
}

/* now, as SQLite counts: the Julian day, times 86,400,000 (milliseconds), from MMBasic's clock */
static int v_now64(sqlite3_vfs *vfs, sqlite3_int64 *now) {
    int t[6];
    long long a, y, m, jdn;
    (void)vfs;
    cmm2->datetime(t);
    a = (14 - t[1]) / 12;
    y = t[0] + 4800 - a;
    m = t[1] + 12 * a - 3;
    jdn = t[2] + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;   /* the day number, from noon */
    *now = jdn * 86400000LL - 43200000LL + ((long long)t[3] * 3600 + t[4] * 60 + t[5]) * 1000LL;
    return SQLITE_OK;
}

static int v_now(sqlite3_vfs *vfs, double *now) {
    sqlite3_int64 ms;
    v_now64(vfs, &ms);
    *now = ms / 86400000.0;
    return SQLITE_OK;
}

static int v_error(sqlite3_vfs *vfs, int n, char *out) {
    (void)vfs;
    if(n > 0) out[0] = 0;
    return 0;
}

static sqlite3_vfs cmm2_vfs = {
    2, sizeof(cfile), PATHMAX, NULL, "cmm2", NULL,
    v_open, v_delete, v_access, v_full, NULL, NULL, NULL, NULL,
    v_random, v_sleep, v_now, v_error, v_now64, NULL, NULL, NULL
};

int sqlite3_os_init(void) { return sqlite3_vfs_register(&cmm2_vfs, 1); }
int sqlite3_os_end(void) { return SQLITE_OK; }

/* The pool: 2 MB on both boards (Peter, 13 Nov 2026: on the MicroPython port too much memory given to SQLite locked it
   up, so 2 MB to start), or all of a smaller heap but 512 KB (the C library's buffers, and a file of statements read
   whole), at least 256 KB. Not MicroPython's 512 KB on a G1: a big sort merges what it wrote to temporary files with a
   4 KB buffer for each piece, so an index on 26 MB of rows, in 64 KB pieces, wanted 1.6 MB (13 Nov, on the PC); with
   2 MB the pieces are 256 KB and the buffers 400 KB. */
int sqlcore_pool_for(unsigned heap) {
    unsigned p = heap > 768u * 1024 ? heap - 512u * 1024 : 256u * 1024;
    return (int)(p > 2u * 1024 * 1024 ? 2u * 1024 * 1024 : p) & ~7;
}

/* The sorter (ORDER BY, GROUP BY, DISTINCT, CREATE INDEX) keeps rows in memory until it has the larger of the cache's
   size and 250 pages (SQLITE_SORTER_PMASZ: about 1 MB), then writes them to a temporary file. A 1 MB block is more
   than a G1's 512 KB pool has, and in a G2's 2 MB a buddy allocator seldom has one free: a sort of 200,000 rows
   failed "out of memory" (13 Nov 2026, on the PC). So the least is a sixteenth of the pool, in 4 KB pages: 128 KB on
   a G2 (with its 256 KB cache, the sorter keeps 256 KB), 32 KB on a G1. */
/* cache bytes of the pool (0: an eighth of it, at most 256 KB) are the page cache, in slots of its own of exactly a 4 KB
   page and SQLite's header for it (SQLITE_CONFIG_PAGECACHE): in the buddy allocator each page would be an 8 KB block,
   so a 1.5 MB cache in a 3 MB pool ran out of memory at once (13 Nov 2026, the G2), and the default cache took twice
   its size. (A page of another size, or more pages than slots, come from the rest of the pool.) */
int sqlcore_init(void *pool, int size, int cache) {
    int r, pages, hdr = 0;
    sqlcore_pool = size;
    sqlcore_cache = size / 8 < 256 * 1024 ? -(size / 8 / 1024) : -256;
    if(cache <= 0) cache = -sqlcore_cache * 1024;
    if(cache < size) {
        int slot, n;
        sqlite3_config(SQLITE_CONFIG_PCACHE_HDRSZ, &hdr);
        slot = (4096 + hdr + 7) & ~7;
        n = cache / slot;
        if((r = sqlite3_config(SQLITE_CONFIG_PAGECACHE, pool, slot, n)) != SQLITE_OK) return r;
        pool = (char *)pool + n * slot;
        size -= n * slot;
        sqlcore_cache = -(n * 4096 / 1024);
    }
    pages = size / 16 / 4096;
    if((r = sqlite3_config(SQLITE_CONFIG_HEAP, pool, size, 64)) != SQLITE_OK) return r;   /* (64: usqlite's atom) */
    if((r = sqlite3_config(SQLITE_CONFIG_PMASZ, (unsigned)(pages > 0 ? pages : 1))) != SQLITE_OK) return r;
    return sqlite3_initialize();
}

/* every 1,000 of SQLite's virtual machine's steps: the USB devices kept up; non-zero (Ctrl-C) ends the statement */
static int c_progress(void *arg) {
    (void)arg;
    if(cmm2->poll()) sqlcore_broken = 1;
    return sqlcore_broken;
}

void sqlcore_watch(sqlite3 *db) { sqlite3_progress_handler(db, 1000, c_progress, NULL); }
