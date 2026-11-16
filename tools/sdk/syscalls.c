/* syscalls.c: newlib's system calls for a loadable program (tools/sdk), on the firmware's services. MIT License, as
   pmimg.h. Ported from the Colour Maximite 2's.

   File descriptors 0-2 are the console (a newline goes out as CR LF while OPOST and ONLCR are on; a read waits for
   one key, echoes it, and gives Enter as a newline), or a file put in its place by dup2, as a shell's < and > would; 3
   up are the services' file handles 0-7. The console has a termios (sys/termios.h): ECHO, ICRNL, ONLCR, and with
   ICANON off VMIN and VTIME, so that a read can return at once with no key, as an INKEY$ does. The heap (malloc) is
   the start block's, between the program's .bss and its stack. time() and gettimeofday() read MMBasic's clock (DATE$
   and TIME$) once and add the microseconds since; clock_gettime's CLOCK_MONOTONIC, times() and clock() count the
   microseconds since start-up (time.h declares what newlib leaves out). opendir and the rest (sys/dirent.h), stat,
   chdir, getcwd and ftruncate are the services'; fork, exec and wait refuse, as one program cannot start
   another. An error sets errno as a program reads it (fail()). memcpy, memset and memmove are the firmware's. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include "cmm2.h"

#undef errno
extern int errno;

/* An error: errno set where each reader looks for it. newlib's reentrant wrappers (_open_r and the rest, under open,
   read, stat...) copy the plain global errno into the program's own, which is errno.h's *__errno(); but the calls a
   program makes here directly (mkdir, rename, opendir, chdir, dup2...) have no wrapper, so their errno never reached
   it. Both are set (12 Nov 2026). */
static int fail(int e) {
    errno = e;
    *__errno() = e;
    return -1;
}

/* a service's status (minus FatFs's FRESULT) as errno */
static int fr_errno(int r) {
    switch(-r) {
    case 4: case 5: return ENOENT;                  /* FR_NO_FILE, FR_NO_PATH */
    case 6: case 19: return EINVAL;                 /* FR_INVALID_NAME, FR_INVALID_PARAMETER */
    case 7: return EACCES;                          /* FR_DENIED: read-only, a directory not empty, or no room */
    case 8: return EEXIST;                          /* FR_EXIST */
    case 9: return EBADF;                           /* FR_INVALID_OBJECT */
    case 10: return EROFS;                          /* FR_WRITE_PROTECTED */
    case 11: case 12: case 13: return ENODEV;       /* FR_INVALID_DRIVE, FR_NOT_ENABLED, FR_NO_FILESYSTEM */
    case 16: return EBUSY;                          /* FR_LOCKED */
    case 17: return ENOMEM;                         /* FR_NOT_ENOUGH_CORE */
    case 18: return EMFILE;                         /* FR_TOO_MANY_OPEN_FILES */
    default: return EIO;                            /* the card or drive itself */
    }
}

#define FD_FILE 3                                       /* the first file's descriptor: handle 0 */
#define HANDLES 8                                       /* the services' file handles */

static int8_t stdh[FD_FILE] = {-1, -1, -1};             /* 0-2: the handle dup2 put in the console's place, or -1 */
static uint8_t refs[HANDLES];                           /* the descriptors open on each handle */

/* memcpy, memset and memmove (Peter, 4 Nov 2026): newlib-nano's are byte loops, and a program, newlib itself and GCC's
   structure copies all call them. They are the firmware's, bound by cmm2_mem_bind when _start has the services;
   before that, these byte loops. */
#define NOLIBCALL __attribute__((optimize("no-tree-loop-distribute-patterns")))   /* (a loop must not become a call) */
static void *NOLIBCALL byte_copy(void *d, const void *s, size_t n) {
    uint8_t *dp = d;
    const uint8_t *sp = s;
    while(n--) *dp++ = *sp++;
    return d;
}
static void *NOLIBCALL byte_fill(void *d, int c, size_t n) {
    uint8_t *dp = d;
    while(n--) *dp++ = (uint8_t)c;
    return d;
}
static void *NOLIBCALL byte_move(void *d, const void *s, size_t n) {
    uint8_t *dp = d;
    const uint8_t *sp = s;
    if(dp <= sp || dp >= sp + n) return byte_copy(d, s, n);
    dp += n;
    sp += n;
    while(n--) *--dp = *--sp;
    return d;
}
static void *(*mem_copy)(void *, const void *, size_t) = byte_copy;
static void *(*mem_fill)(void *, int, size_t) = byte_fill;
static void *(*mem_move)(void *, const void *, size_t) = byte_move;

void cmm2_mem_bind(void) {
    mem_copy = cmm2->mem_copy;
    mem_fill = cmm2->mem_fill;
    mem_move = cmm2->mem_move;
}

void *memcpy(void *d, const void *s, size_t n) { return mem_copy(d, s, n); }
void *memset(void *d, int c, size_t n) { return mem_fill(d, c, n); }
void *memmove(void *d, const void *s, size_t n) { return mem_move(d, s, n); }

/* the services' handle a descriptor stands for, or -1: the console */
static int handle(int fd) {
    if(fd < 0) return -1;
    return fd < FD_FILE ? stdh[fd] : fd - FD_FILE;
}

/* the console's line discipline (tcgetattr, tcsetattr): at the start, a key at a time, echoed, Enter a newline */
static struct termios contio = {ICRNL, OPOST | ONLCR, CS8 | CREAD, ISIG | ICANON | ECHO, {[VMIN] = 1}};

int _write(int fd, const char *p, int n) {
    int i, k = 0, h = handle(fd);
    int32_t r;
    char buf[129];
    if(h < 0 && (fd == 1 || fd == 2)) {                 /* (con_puts in pieces of the buffer's size: con_putc for each
                                                           character flushed USB's console each time) */
        int nl = (contio.c_oflag & (OPOST | ONLCR)) == (OPOST | ONLCR);
        for(i = 0; i < n; i++) {
            if(p[i] == 0) {                             /* (con_puts stops at a zero: that one goes alone) */
                if(k) { buf[k] = 0; cmm2->con_puts(buf); k = 0; }
                cmm2->con_putc(0);
                continue;
            }
            if(nl && p[i] == '\n') buf[k++] = '\r';
            buf[k++] = p[i];
            if(k >= (int)sizeof(buf) - 2) { buf[k] = 0; cmm2->con_puts(buf); k = 0; }
        }
        if(k) { buf[k] = 0; cmm2->con_puts(buf); }
        return n;
    }
    if(h >= 0) {
        if((r = cmm2->file_write(h, p, n)) >= 0) return r;
        return fail(fr_errno(r));
    }
    return fail(EBADF);
}

int _read(int fd, char *p, int n) {
    int c, h = handle(fd);
    int32_t r;
    if(h < 0 && fd == 0) {
        if(n <= 0) return 0;
        if(!(contio.c_lflag & ICANON) && contio.c_cc[VMIN] == 0) {     // (a poll: VTIME tenths of a second at most)
            uint64_t end = cmm2->micros() + contio.c_cc[VTIME] * 100000ull;
            while((c = cmm2->con_getc()) < 0) {
                if(cmm2->micros() >= end) return 0;
                cmm2->poll();
            }
        } else
            while((c = cmm2->con_getc()) < 0) cmm2->poll();
        if(c == '\r' && (contio.c_iflag & ICRNL)) c = '\n';
        if(contio.c_lflag & ECHO) {
            if(c == '\n') cmm2->con_puts("\r\n");
            else cmm2->con_putc(c);
        }
        *p = c;
        return 1;
    }
    if(h >= 0) {
        if((r = cmm2->file_read(h, p, n)) >= 0) return r;
        return fail(fr_errno(r));
    }
    return fail(EBADF);
}

int _open(const char *name, int flags, int mode) {
    int acc = flags & O_ACCMODE, m = 0, h;
    (void)mode;
    if(acc != O_WRONLY) m |= CMM2_O_READ;
    if(acc != O_RDONLY) {
        m |= CMM2_O_WRITE;
        if(flags & O_APPEND) m |= CMM2_O_APPEND;
        else if(flags & (O_CREAT | O_TRUNC)) m |= CMM2_O_CREATE;
    }
    if((h = cmm2->file_open(name, m)) < 0) return fail(fr_errno(h));
    if(h < HANDLES) refs[h] = 1;
    return h + FD_FILE;
}

int creat(const char *name, mode_t mode) { return _open(name, O_WRONLY | O_CREAT | O_TRUNC, mode); }   // (newlib declares
                                                                                                    // it, defines none)

int _close(int fd) {
    int h = handle(fd), r;
    if(fd >= 0 && fd < FD_FILE) stdh[fd] = -1;          /* (the console again) */
    if(h < 0) return 0;
    if(h < HANDLES && refs[h] > 1) { refs[h]--; return 0; }   /* still open on another descriptor */
    if(h < HANDLES) refs[h] = 0;
    if((r = cmm2->file_close(h)) < 0) return fail(fr_errno(r));
    return 0;
}

/* fd2 (0-2) made another name for fd's file, in place of the console, or the console again if fd is the console. Only
   the console's descriptors can be made so: a file's are the services' handles. */
int dup2(int fd, int fd2) {
    int h = handle(fd);
    if(fd < 0 || fd2 < 0 || fd2 >= FD_FILE || h >= HANDLES) return fail(EBADF);
    if(fd == fd2) return fd2;
    _close(fd2);
    if(h >= 0) {
        stdh[fd2] = h;
        refs[h] = (refs[h] ? refs[h] : 1) + 1;
    }
    return fd2;
}

off_t _lseek(int fd, off_t off, int whence) {
    int h = handle(fd);
    int32_t pos;
    if(h < 0) return 0;
    if(whence == SEEK_SET) pos = off;
    else if(whence == SEEK_CUR) pos = cmm2->file_tell(h) + off;
    else pos = cmm2->file_size(h) + off;
    if(pos < 0 || cmm2->file_seek(h, pos) < 0) return fail(EINVAL);
    return pos;
}

int _fstat(int fd, struct stat *st) {
    int h = handle(fd);
    memset(st, 0, sizeof(*st));
    if(h < 0) { st->st_mode = S_IFCHR; return 0; }
    st->st_mode = S_IFREG;
    st->st_size = cmm2->file_size(h);
    return 0;
}

int _isatty(int fd) { return handle(fd) < 0 && fd >= 0 && fd < FD_FILE; }

int tcgetattr(int fd, struct termios *tp) {
    if(!_isatty(fd)) return fail(ENOTTY);
    *tp = contio;
    return 0;
}

int tcsetattr(int fd, int when, const struct termios *tp) {
    (void)when;
    if(!_isatty(fd)) return fail(ENOTTY);
    contio = *tp;
    return 0;
}

int tcflush(int fd, int which) {
    if(!_isatty(fd)) return fail(ENOTTY);
    if(which != TCOFLUSH) while(cmm2->con_getc() >= 0) {}         // (the keys typed and not read)
    return 0;
}

void cfmakeraw(struct termios *tp) {
    tp->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tp->c_oflag &= ~OPOST;
    tp->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tp->c_cflag = (tp->c_cflag & ~(CSIZE | PARENB)) | CS8;
    tp->c_cc[VMIN] = 1;
    tp->c_cc[VTIME] = 0;
}

speed_t cfgetispeed(const struct termios *tp) { (void)tp; return B115200; }
speed_t cfgetospeed(const struct termios *tp) { (void)tp; return B115200; }
int cfsetispeed(struct termios *tp, speed_t s) { (void)tp; (void)s; return 0; }
int cfsetospeed(struct termios *tp, speed_t s) { (void)tp; (void)s; return 0; }

static char *brk;                                                   // malloc's heap: from the start block's, upward

/* size bytes (rounded up to 8) taken from the top of the heap, for a program's own allocator; malloc keeps the rest.
   NULL if malloc has already taken too much. */
void *cmm2_heap_take(uint32_t size) {
    char *top = (char *)cmm2_start->heap + cmm2_start->heap_size;
    size = (size + 7) & ~7u;
    if(brk == NULL) brk = cmm2_start->heap;
    if(size > (uint32_t)(top - brk)) return NULL;
    cmm2_start->heap_size -= size;
    return top - size;
}

void *_sbrk(ptrdiff_t incr) {
    char *prev;
    if(brk == NULL) brk = cmm2_start->heap;
    if(brk + incr > (char *)cmm2_start->heap + cmm2_start->heap_size || brk + incr < (char *)cmm2_start->heap) {
        fail(ENOMEM);
        return (void *)-1;
    }
    prev = brk;
    brk += incr;
    return prev;
}

void _exit(int code) {
    cmm2->leave(code);
    for(;;) {}
}

int _kill(int pid, int sig) { (void)pid; (void)sig; return fail(EINVAL); }
int _getpid(void) { return 1; }
int _fork(void) { return fail(ENOSYS); }                       // (one program cannot start another)
int _execve(const char *name, char *const argv[], char *const env[]) {
    (void)name; (void)argv; (void)env;
    return fail(ENOSYS);
}
int _wait(int *status) { (void)status; return fail(ECHILD); }
pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; return fail(ECHILD); }
int execv(const char *name, char *const argv[]) { return _execve(name, argv, NULL); }
int execvp(const char *name, char *const argv[]) { return _execve(name, argv, NULL); }
int pipe(int fd[2]) { (void)fd; return fail(ENOSYS); }

/* us microseconds, with the USB devices looked after every 10ms meanwhile (and a Ctrl-C there seen), as MMBasic's
   PAUSE does */
int usleep(useconds_t us) {
    uint64_t end = cmm2->micros() + us, now;
    while((now = cmm2->micros()) < end) {
        cmm2->delay_us(end - now > 10000 ? 10000 : (uint32_t)(end - now));
        cmm2->poll();
    }
    return 0;
}

int fsync(int fd) {
    int h = handle(fd), r;
    if(h < 0) return 0;
    if((r = cmm2->file_sync(h)) < 0) return fail(fr_errno(r));
    return 0;
}

/* the file made len bytes long: cut short by file_truncate, or made longer by a seek past its end
   (FatFs's f_lseek, which adds clusters to a file open for writing). Its position stays where it was, or goes to len
   if it was past that (a seek past the end would make the file longer again). */
int ftruncate(int fd, off_t len) {
    int h = handle(fd), r;
    int32_t pos, size;
    if(h < 0 || len < 0) return fail(EINVAL);
    pos = cmm2->file_tell(h);
    size = cmm2->file_size(h);
    if((r = cmm2->file_seek(h, (uint32_t)len)) < 0) return fail(fr_errno(r));
    if(len < size && (r = cmm2->file_truncate(h)) < 0) return fail(fr_errno(r));
    if(len > size && cmm2->file_size(h) < len) {                    // (open for reading only: the seek stopped at the end)
        cmm2->file_seek(h, (uint32_t)pos);
        return fail(EBADF);
    }
    if(pos < len) cmm2->file_seek(h, (uint32_t)pos);
    return 0;
}

void sync(void) {}                                                  // (each file is written out as it is closed)
int rmdir(const char *name) { int r = cmm2->file_remove(name); return r < 0 ? fail(fr_errno(r)) : 0; }   // (FatFs: an empty one)
int statvfs(const char *path, struct statvfs *buf) { (void)path; (void)buf; return fail(ENOSYS); }
int _unlink(const char *name) { int r = cmm2->file_remove(name); return r < 0 ? fail(fr_errno(r)) : 0; }
/* a drive's root ("A:", "A:/", "/") is there already, as POSIX's mkdir("/") says: FatFs calls it an invalid name, and
   a program that makes each folder of a path in turn (WordGrinder's Mkdirs) stopped at the first */
static int is_root(const char *name) {
    if(name[0] && name[1] == ':') name += 2;
    else if(*name != '/' && *name != '\\') return 0;
    while(*name == '/' || *name == '\\') name++;
    return *name == 0;
}

int mkdir(const char *name, mode_t mode) {
    int r;
    (void)mode;
    if(is_root(name)) return fail(EEXIST);
    return (r = cmm2->dir_make(name)) < 0 ? fail(fr_errno(r)) : 0;
}
/* rename itself (newlib's would link and unlink): FatFs renames, and a file already called 'to' is replaced, as POSIX
   has it (FR_EXIST, 8, then the file removed and the rename tried again) */
int rename(const char *from, const char *to) {
    int r = cmm2->file_rename(from, to);
    if(r == -8 && cmm2->file_remove(to) == 0) r = cmm2->file_rename(from, to);
    return r < 0 ? fail(fr_errno(r)) : 0;
}

/* days from 1 Jan 1970 to the date (Howard Hinnant's days_from_civil) */
static int32_t days(int y, int m, int d) {
    int32_t era, yoe, doy;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

/* names with "." and ".." in them. FatFs takes them on a FAT card, but not on an exFAT one, which has no dot entries
   (f_stat calls "A:/GAMES/.." an origin directory and fails), and the programs that list a directory stat each name
   in it, readdir's ".." too. So stat and opendir make such a name whole first, as the services would take it
   (Loader.c's ld_path: "A:..." is from A:'s root, "/..." from the current drive's, anything else from the current
   directory), with "." left out and ".." taking the part before it off: "A:/GAMES/.." is "A:/" (12 Nov 2026) */
#define PATH_LEN 264                            /* (the services' longest name: FatFs's 255 with "A:/" and the end) */

static int dot_part(const char *name) {        /* 1 if a part of the name is "." or ".." */
    const char *p = name, *s;
    if(p[0] && p[1] == ':') p += 2;
    while(*p) {
        while(*p == '/' || *p == '\\') p++;
        for(s = p; *p && *p != '/' && *p != '\\'; p++) {}
        if((p - s == 1 && s[0] == '.') || (p - s == 2 && s[0] == '.' && s[1] == '.')) return 1;
    }
    return 0;
}

static int add_parts(char *out, size_t *n, const char *p) {   /* after out's "A:" and its parts: each "/part" */
    const char *s;
    size_t len;
    while(*p) {
        while(*p == '/' || *p == '\\') p++;
        for(s = p; *p && *p != '/' && *p != '\\'; p++) {}
        len = p - s;
        if(len == 0 || (len == 1 && s[0] == '.')) continue;
        if(len == 2 && s[0] == '.' && s[1] == '.') {
            while(*n > 2 && out[--*n] != '/') {}   /* (at the root it stays there) */
            continue;
        }
        if(*n + 1 + len >= PATH_LEN) return -1;
        out[(*n)++] = '/';
        memcpy(out + *n, s, len);
        *n += len;
    }
    return 0;
}

static int whole_path(const char *name, char *out) {    /* out: PATH_LEN. 0, or -1 if too long or no getcwd (version 10) */
    char cwd[PATH_LEN];
    size_t n = 2;
    if(name[0] && name[1] == ':') {
        out[0] = name[0];
        name += 2;
    } else {
        if(cmm2->dir_current(cwd, sizeof(cwd)) < 0 || !cwd[0] || cwd[1] != ':') return -1;
        out[0] = cwd[0];
        if(*name != '/' && *name != '\\' && add_parts(out, &n, cwd + 2) < 0) return -1;
    }
    out[1] = ':';
    if(add_parts(out, &n, name) < 0) return -1;
    if(n == 2) out[n++] = '/';
    out[n] = 0;
    return 0;
}

int _stat(const char *name, struct stat *st) {
    cmm2_stat s;
    char full[PATH_LEN];
    int r;
    memset(st, 0, sizeof(*st));
    if(dot_part(name) && whole_path(name, full) == 0) name = full;
    if((r = cmm2->file_stat(name, &s)) < 0) return fail(fr_errno(r));
    st->st_mode = s.is_dir ? S_IFDIR | 0755 : S_IFREG | 0644;
    st->st_size = s.size;
    st->st_mtime = (time_t)days(s.year, s.month, s.day) * 86400 + s.hour * 3600 + s.minute * 60 + s.second;
    return 0;
}

int chdir(const char *path) {                   /* (CHDIR's statuses: -3 too long, else -4 with its message) */
    int r;
    if((r = cmm2->dir_change(path)) < 0) return fail(r == -3 ? ENAMETOOLONG : ENOENT);
    return 0;
}

/* getcwd(NULL, size) gives a block from malloc, of size bytes or, size 0, enough for any name, as glibc's and the
   BSDs' do: WordGrinder asks so, and its file browser failed with no name (12 Nov 2026) */
char *getcwd(char *buf, size_t size) {
    char *mine = NULL;
    if(buf == NULL) {
        if(size == 0) size = PATH_LEN;
        if((buf = mine = malloc(size)) == NULL) { fail(ENOMEM); return NULL; }
    }
    if(cmm2->dir_current(buf, size) < 0) {
        free(mine);
        fail(ERANGE);
        return NULL;
    }
    return buf;
}

/* readdir gives "." first, and then ".." in a directory below a drive's root, as Linux's does (FatFs gives neither):
   the file browsers of programs ported from Linux go up by "..", and had no way up without it (atari800's and
   sdltrs's, 12 Nov 2026), and WordGrinder's tests look for both. A drive's root has no "..", so that a browser stops
   there */
DIR *opendir(const char *path) {
    DIR *d;
    char full[PATH_LEN];
    int whole = whole_path(path, full) == 0, h;
    h = cmm2->dir_open(whole && dot_part(path) ? full : path);
    if(h < 0) { fail(fr_errno(h)); return NULL; }
    if((d = malloc(sizeof(DIR))) == NULL) { cmm2->dir_close(h); fail(ENOMEM); return NULL; }
    d->handle = h;
    d->dot = 1;
    d->dotdot = whole && full[3] != 0;          /* (full is "A:/" at a root) */
    return d;
}

struct dirent *readdir(DIR *d) {
    static cmm2_dirent e;
    if(d->dot || d->dotdot) {
        strcpy(d->entry.d_name, d->dot ? "." : "..");
        if(d->dot) d->dot = 0;
        else d->dotdot = 0;
        d->entry.d_type = DT_DIR;
        d->entry.d_ino = 0;
        return &d->entry;
    }
    if(cmm2->dir_read(d->handle, &e) != 1) return NULL;
    strncpy(d->entry.d_name, e.name, sizeof(d->entry.d_name) - 1);
    d->entry.d_name[sizeof(d->entry.d_name) - 1] = 0;
    d->entry.d_type = e.is_dir ? DT_DIR : DT_REG;
    d->entry.d_ino = 0;
    return &d->entry;
}

int closedir(DIR *d) {
    int r = cmm2->dir_close(d->handle);
    free(d);
    return r < 0 ? fail(fr_errno(r)) : 0;
}

/* The time: MMBasic's clock (DATE$ and TIME$) read at the first call, and the microseconds since then added to it, so
   that it never goes back. It was the clock's seconds with micros' fraction, read together each time: their seconds
   do not turn together, so it went back up to a second whenever the clock's turned first, and a program timing by it
   (PDCurses: its keys, clicks and cursor's blink) waited a second too long or not at all (12 Nov 2026). */
int _gettimeofday(struct timeval *tv, void *tz) {
    static time_t base_s;
    static uint64_t base_us;
    static int set;
    uint64_t now = cmm2->micros(), e;
    (void)tz;
    if(!set) {
        int t[6];
        cmm2->datetime(t);
        base_s = (time_t)days(t[0], t[1], t[2]) * 86400 + t[3] * 3600 + t[4] * 60 + t[5];
        base_us = now;
        set = 1;
    }
    e = now - base_us;
    tv->tv_sec = base_s + (time_t)(e / 1000000);
    tv->tv_usec = (suseconds_t)(e % 1000000);
    return 0;
}

/* times(), and so clock(): the program's time is all the processor's since start-up (it has the processor to
   itself), in CLOCKS_PER_SEC ticks. Weak, as clock_gettime */
__attribute__((weak)) clock_t _times(struct tms *t) {
    const clock_t c = (clock_t)(cmm2->micros() * (uint64_t)CLOCKS_PER_SEC / 1000000);
    if(t) {
        t->tms_utime = c;
        t->tms_stime = t->tms_cutime = t->tms_cstime = 0;
    }
    return c;
}

/* POSIX's clocks, which newlib leaves to the system: CLOCK_MONOTONIC (and the process's and thread's CPU time, the
   program having the processor to itself) the microseconds since start-up, CLOCK_REALTIME gettimeofday's. Weak, as a
   program may have its own (apps/pc386, from before these) */
__attribute__((weak)) int clock_gettime(clockid_t id, struct timespec *ts) {
    struct timeval tv;
    uint64_t us;
    if(id == CLOCK_REALTIME) {
        _gettimeofday(&tv, NULL);
        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = tv.tv_usec * 1000L;
        return 0;
    }
    if(id != CLOCK_MONOTONIC && id != CLOCK_PROCESS_CPUTIME_ID && id != CLOCK_THREAD_CPUTIME_ID) return fail(EINVAL);
    us = cmm2->micros();
    ts->tv_sec = (time_t)(us / 1000000);
    ts->tv_nsec = (long)(us % 1000000) * 1000L;
    return 0;
}

__attribute__((weak)) int clock_getres(clockid_t id, struct timespec *res) {
    (void)id;
    if(res) {
        res->tv_sec = 0;
        res->tv_nsec = 1000;                            /* (a microsecond) */
    }
    return 0;
}

__attribute__((weak)) int nanosleep(const struct timespec *req, struct timespec *rem) {
    uint64_t us;
    if(req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) return fail(EINVAL);
    us = (uint64_t)req->tv_sec * 1000000 + (uint64_t)(req->tv_nsec + 999) / 1000;
    while(us) {                                         /* (usleep's argument is 32 bits) */
        uint32_t step = us > 1000000 ? 1000000 : (uint32_t)us;
        usleep(step);
        us -= step;
    }
    if(rem) rem->tv_sec = 0, rem->tv_nsec = 0;
    return 0;
}
