/* sys/termios.h: the console's line discipline for a loadable program (tools/sdk), which newlib's <termios.h> includes
   and newlib does not have for this target. MIT License, as pmimg.h.

   Only the console has one (descriptors 0-2, not a file put in their place by dup2): ECHO, ICANON and ICRNL in the
   way syscalls.c's read() honours them, and VMIN and VTIME as POSIX gives them with ICANON off - VMIN 0 and VTIME 0
   is a read that returns 0 at once if no key is waiting, as an INKEY$ wants. Output always turns a newline into CR
   LF. Speeds are the console's and cannot be set. */
#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 12
struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_cc[NCCS];
};

/* c_cc */
#define VINTR   0
#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VEOF    4
#define VTIME   5
#define VMIN    6
#define VSTART  7
#define VSTOP   8
#define VSUSP   9
#define VEOL    10

/* c_iflag */
#define IGNBRK  0x0001
#define BRKINT  0x0002
#define IGNPAR  0x0004
#define PARMRK  0x0008
#define INPCK   0x0010
#define ISTRIP  0x0020
#define INLCR   0x0040
#define IGNCR   0x0080
#define ICRNL   0x0100
#define IXON    0x0400
#define IXOFF   0x1000

/* c_oflag */
#define OPOST   0x0001
#define ONLCR   0x0004

/* c_cflag */
#define CSIZE   0x0030
#define CS8     0x0030
#define CSTOPB  0x0040
#define CREAD   0x0080
#define PARENB  0x0100
#define CLOCAL  0x0800

/* c_lflag */
#define ISIG    0x0001
#define ICANON  0x0002
#define ECHO    0x0008
#define ECHOE   0x0010
#define ECHOK   0x0020
#define ECHONL  0x0040
#define NOFLSH  0x0080
#define TOSTOP  0x0100
#define IEXTEN  0x8000

/* tcsetattr */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflush */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

#define B0      0
#define B9600   9600
#define B115200 115200

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int when, const struct termios *t);
int tcflush(int fd, int which);
void cfmakeraw(struct termios *t);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int cfsetispeed(struct termios *t, speed_t s);
int cfsetospeed(struct termios *t, speed_t s);

#ifdef __cplusplus
}
#endif
#endif
