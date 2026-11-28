/* cmm2_net.c: BSD sockets for a loadable program, on the firmware's lwIP through its network services (loader/pmimg.h:
   net_poll, net_dns and sock_*), for libcurl in curl.app. The Colour Maximite 2's apps/curl/port/cmm2_net.c, where
   they are services version 21; the PicoMite's table has had them since its version 1 (NET_API, make.py). MIT
   License, as pmimg.h.

   - A socket's descriptor is SOCK_FD0 plus its slot (16-23), clear of the files' (3-10, tools/sdk/syscalls.c) and
     under newlib's FD_SETSIZE, so select() takes both. closesocket() closes one (curl's HAVE_CLOSESOCKET).
   - The services never wait; these calls wait as a blocking socket would (net_poll meanwhile), unless the socket
     is non-blocking: setsockopt(SOL_SOCKET, SO_NONBLOCK), as curl turns it on (HAVE_SETSOCKOPT_SO_NONBLOCK).
   - select() is the one place that waits for several: it loops on net_poll and sock_state until something is
     ready or the time is up. While anything waits here the network is pumped every pass, as services 21 require.
   - Ctrl-C while waiting ends the program, as Ctrl-C ends a BASIC program.
   - The services' errors are Linux's errno numbers; newlib's differ, so they are turned into newlib's here.
   - getaddrinfo asks the firmware's DNS (net_dns), IPv4 only. MSG_PEEK keeps the byte it read. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "cmm2.h"

#define SOCK_FD0  16
#define NSOCK     8

#define ST_CONNECTED  1                         /* sock_state's bits */
#define ST_DATA       2
#define ST_ROOM       4
#define ST_PEERCLOSED 8

typedef struct {
    uint8_t  used, type, nonblock, connecting, listening, peeked;
    uint8_t  peek;                              /* the byte MSG_PEEK read, while peeked */
    int8_t   h;                                 /* the services' handle */
    int      err;                               /* SO_ERROR's, newlib's errno */
    uint8_t  peer[4];
    uint16_t peerport, localport;
} sock_t;

static sock_t sk[NSOCK];
static uint64_t lastpoll, lastnet;

/* the network pumped if 250 us have passed since it last was: while data keeps coming nothing waits, and without this the
   radio would be looked at only when the data already in hand ran out (370 KB/s, against 1.1 MB/s, 27 Nov 2026) */
static void net_tick(void) {
    uint64_t now = cmm2->micros();
    if(now - lastnet >= 250) {
        lastnet = now;
        cmm2->net_poll();
    }
}

/* the network pumped, and every 20 ms the USB devices too: Ctrl-C there ends the program. (Every 2 ms, the USB poll's time
   kept the radio waiting: 387 KB/s through these sockets against 540 KB/s on the services alone, 27 Nov 2026) */
static void pump(void) {
    uint64_t now;
    cmm2->net_poll();
    lastnet = now = cmm2->micros();
    if(now - lastpoll >= 20000) {
        lastpoll = now;
        if(cmm2->poll()) {
            fputs("\n^C\n", stderr);
            exit(130);
        }
    }
}

/* a service's error (minus a Linux errno) as newlib's errno */
static int lx_errno(int r) {
    switch(-r) {
    case 2: return ENOENT;
    case 9: return EBADF;
    case 11: return EAGAIN;
    case 12: return ENOMEM;
    case 13: return EACCES;
    case 19: return ENETDOWN;                   /* ENODEV: no Wi-Fi */
    case 22: return EINVAL;
    case 24: return EMFILE;
    case 32: return EPIPE;
    case 98: return EADDRINUSE;
    case 101: return ENETUNREACH;
    case 103: return ECONNABORTED;
    case 104: return ECONNRESET;
    case 105: return ENOBUFS;
    case 106: return EISCONN;
    case 107: return ENOTCONN;
    case 110: return ETIMEDOUT;
    case 111: return ECONNREFUSED;
    case 113: return EHOSTUNREACH;
    case 114: return EALREADY;
    case 115: return EINPROGRESS;
    default: return EIO;
    }
}

static int fail(int e) {
    errno = e;
    return -1;
}

/* the first time the network is wanted: the services and Wi-Fi there, and joined (a restart may still be joining). 0, or
   -1 with the reason printed once */
static int ensure_net(void) {
    static int done;                            /* 1 joined, -1 not there */
    uint64_t end;
    int st;
    if(done) return done > 0 ? 0 : -1;
    done = -1;
    if(cmm2->version < NET_API) {
        fputs("This needs firmware with the network services (V8)\n", stderr);
        return -1;
    }
    st = cmm2->net_poll();
    if(st == 0) {
        fputs("No Wi-Fi: it needs a Pico 2 W (or an RM2) and OPTION WIFI\n", stderr);
        return -1;
    }
    end = cmm2->micros() + 15000000u;
    while(st != 2) {
        if(cmm2->micros() >= end) {
            fputs("Wi-Fi not joined (OPTION WIFI's network)\n", stderr);
            return -1;
        }
        pump();
        st = cmm2->net_poll();
    }
    done = 1;
    return 0;
}

static sock_t *get(int fd) {
    int i = fd - SOCK_FD0;
    if(i < 0 || i >= NSOCK || !sk[i].used) return NULL;
    return &sk[i];
}

int cmm2_is_socket(int fd) { return get(fd) != NULL; }

/* the state of a socket, the byte kept by MSG_PEEK counted as data */
static int state(sock_t *s) {
    int st = cmm2->sock_state(s->h);
    if(st >= 0 && s->peeked) st |= ST_DATA;
    return st;
}

static int ready_read(sock_t *s) {
    int st = state(s);
    if(st < 0) return 1;                        /* (an error: read() says which) */
    if(s->type == SOCK_DGRAM || s->listening) return (st & ST_DATA) != 0;
    return (st & (ST_DATA | ST_PEERCLOSED)) != 0;
}

static int ready_write(sock_t *s) {
    int st = state(s);
    if(st < 0) return 1;                        /* (a connect that failed: SO_ERROR says why) */
    if(s->type == SOCK_DGRAM) return 1;
    return (st & ST_CONNECTED) && (st & ST_ROOM);
}

static int has_error(sock_t *s) {
    return state(s) < 0;
}

int socket(int domain, int type, int protocol) {
    int i, h, t;
    (void)protocol;
    if(ensure_net()) return fail(ENETDOWN);
    if(domain != AF_INET) return fail(EAFNOSUPPORT);
    type &= 0xF;                                /* (Linux's SOCK_NONBLOCK and SOCK_CLOEXEC bits, if given) */
    if(type == SOCK_STREAM) t = 1;
    else if(type == SOCK_DGRAM) t = 2;
    else return fail(EPROTONOSUPPORT);
    for(i = 0; i < NSOCK && sk[i].used; i++) {}
    if(i == NSOCK) return fail(EMFILE);
    if((h = cmm2->sock_open(t)) < 0) return fail(lx_errno(h));
    memset(&sk[i], 0, sizeof(sk[i]));
    sk[i].used = 1;
    sk[i].type = type;
    sk[i].h = h;
    return SOCK_FD0 + i;
}

int closesocket(int fd) {
    sock_t *s = get(fd);
    if(s == NULL) return fail(EBADF);
    cmm2->sock_close(s->h);
    s->used = 0;
    return 0;
}

static int addr_in(const struct sockaddr *addr, socklen_t len, uint8_t ip[4], uint16_t *port) {
    const struct sockaddr_in *a = (const struct sockaddr_in *)addr;
    if(addr == NULL || len < sizeof(struct sockaddr_in) || a->sin_family != AF_INET) return -1;
    memcpy(ip, &a->sin_addr.s_addr, 4);
    *port = ntohs(a->sin_port);
    return 0;
}

static void fill_in(struct sockaddr *addr, socklen_t *len, const uint8_t ip[4], uint16_t port) {
    struct sockaddr_in a;
    if(addr == NULL || len == NULL) return;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    memcpy(&a.sin_addr.s_addr, ip, 4);
    memcpy(addr, &a, *len < sizeof(a) ? *len : sizeof(a));
    *len = sizeof(a);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len) {
    sock_t *s = get(fd);
    int r, st;
    if(s == NULL) return fail(EBADF);
    if(addr_in(addr, len, s->peer, &s->peerport) < 0) return fail(EAFNOSUPPORT);
    if((r = cmm2->sock_connect(s->h, s->peer, s->peerport)) < 0) return fail(lx_errno(r));
    if(s->type == SOCK_DGRAM) return 0;
    if(s->nonblock) {
        s->connecting = 1;
        return fail(EINPROGRESS);
    }
    for(;;) {                                   /* (blocking: until made, or refused) */
        st = cmm2->sock_state(s->h);
        if(st < 0) return fail(lx_errno(st));
        if(st & ST_CONNECTED) return 0;
        pump();
    }
}

int bind(int fd, const struct sockaddr *addr, socklen_t len) {
    sock_t *s = get(fd);
    uint8_t ip[4];
    uint16_t port;
    int r;
    if(s == NULL) return fail(EBADF);
    if(addr_in(addr, len, ip, &port) < 0) return fail(EAFNOSUPPORT);
    if((r = cmm2->sock_bind(s->h, port)) < 0) return fail(lx_errno(r));
    s->localport = port;
    return 0;
}

int listen(int fd, int backlog) {
    sock_t *s = get(fd);
    int r;
    if(s == NULL) return fail(EBADF);
    if((r = cmm2->sock_listen(s->h, backlog)) < 0) return fail(lx_errno(r));
    s->listening = 1;
    return 0;
}

int accept(int fd, struct sockaddr *addr, socklen_t *len) {
    sock_t *s = get(fd);
    uint8_t ip[4];
    int port, h, i;
    if(s == NULL) return fail(EBADF);
    for(;;) {
        h = cmm2->sock_accept(s->h, ip, &port);
        if(h >= 0) break;
        if(h != -11) return fail(lx_errno(h));
        if(s->nonblock) return fail(EAGAIN);
        pump();
    }
    for(i = 0; i < NSOCK && sk[i].used; i++) {}
    if(i == NSOCK) {
        cmm2->sock_close(h);
        return fail(EMFILE);
    }
    memset(&sk[i], 0, sizeof(sk[i]));
    sk[i].used = 1;
    sk[i].type = SOCK_STREAM;
    sk[i].h = h;
    memcpy(sk[i].peer, ip, 4);
    sk[i].peerport = port;
    fill_in(addr, len, ip, port);
    return SOCK_FD0 + i;
}

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t tolen) {
    sock_t *s = get(fd);
    uint8_t ip[4];
    uint16_t port = 0;
    int r;
    if(s == NULL) return fail(EBADF);
    if(to && addr_in(to, tolen, ip, &port) < 0) return fail(EAFNOSUPPORT);
    if(n > 0x7FFFFFFF) n = 0x7FFFFFFF;
    net_tick();
    for(;;) {
        r = cmm2->sock_send(s->h, buf, (int)n, to ? ip : NULL, port);
        if(r < 0) return fail(lx_errno(r));
        if(r > 0 || n == 0) return r;
        if(s->nonblock || (flags & MSG_DONTWAIT)) return fail(EAGAIN);
        pump();                                 /* (no room: lwIP's buffers are full until the peer acknowledges) */
    }
}

ssize_t send(int fd, const void *buf, size_t n, int flags) {
    return sendto(fd, buf, n, flags, NULL, 0);
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *from, socklen_t *fromlen) {
    sock_t *s = get(fd);
    uint8_t ip[4];
    int port = 0, r;
    if(s == NULL) return fail(EBADF);
    if(n == 0) return 0;
    if(n > 0x7FFFFFFF) n = 0x7FFFFFFF;
    net_tick();
    if(s->type == SOCK_STREAM && s->peeked) {   /* the byte MSG_PEEK kept, first */
        *(uint8_t *)buf = s->peek;
        if(!(flags & MSG_PEEK)) s->peeked = 0;
        fill_in(from, fromlen, s->peer, s->peerport);
        return 1;
    }
    for(;;) {
        if(flags & MSG_PEEK && s->type == SOCK_STREAM) {
            r = cmm2->sock_recv(s->h, &s->peek, 1, NULL, NULL);
            if(r == 1) {
                s->peeked = 1;
                *(uint8_t *)buf = s->peek;
            }
        } else
            r = cmm2->sock_recv(s->h, buf, (int)n, ip, &port);
        if(r >= 0) {
            if(s->type == SOCK_DGRAM) fill_in(from, fromlen, ip, (uint16_t)port);
            else fill_in(from, fromlen, s->peer, s->peerport);
            return r;
        }
        if(r != -11) return fail(lx_errno(r));
        if(s->nonblock || (flags & MSG_DONTWAIT)) return fail(EAGAIN);
        pump();
    }
}

ssize_t recv(int fd, void *buf, size_t n, int flags) {
    return recvfrom(fd, buf, n, flags, NULL, NULL);
}

int getsockopt(int fd, int level, int name, void *val, socklen_t *len) {
    sock_t *s = get(fd);
    int v = 0, st;
    if(s == NULL) return fail(EBADF);
    if(level == SOL_SOCKET && name == SO_ERROR) {
        st = cmm2->sock_state(s->h);
        v = st < 0 ? lx_errno(st) : s->err;
        s->err = 0;
    } else if(level == SOL_SOCKET && name == SO_TYPE)
        v = s->type;
    else if(level == SOL_SOCKET && (name == SO_SNDBUF || name == SO_RCVBUF))
        v = 16 * 1460;                          /* (the firmware's TCP window) */
    if(val && len && *len >= sizeof(int)) {
        memcpy(val, &v, sizeof(int));
        *len = sizeof(int);
    }
    return 0;
}

int setsockopt(int fd, int level, int name, const void *val, socklen_t len) {
    sock_t *s = get(fd);
    long v = 0;
    if(s == NULL) return fail(EBADF);
    if(level == SOL_SOCKET && name == SO_NONBLOCK) {
        if(val && len >= sizeof(long)) memcpy(&v, val, sizeof(long));
        else if(val && len >= sizeof(int)) { int i; memcpy(&i, val, sizeof(int)); v = i; }
        s->nonblock = v != 0;
    }
    return 0;                                   /* (the rest, TCP_NODELAY, SO_KEEPALIVE...: lwIP's are the firmware's) */
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len) {
    sock_t *s = get(fd);
    uint8_t a[16];
    if(s == NULL) return fail(EBADF);
    if(cmm2->net_ifconfig(a) < 0) memset(a, 0, 4);
    fill_in(addr, len, a, s->localport);
    return 0;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len) {
    sock_t *s = get(fd);
    if(s == NULL) return fail(EBADF);
    if(s->type == SOCK_STREAM && !(cmm2->sock_state(s->h) & ST_CONNECTED)) return fail(ENOTCONN);
    fill_in(addr, len, s->peer, s->peerport);
    return 0;
}

int shutdown(int fd, int how) {
    (void)how;
    return get(fd) ? 0 : fail(EBADF);           /* (lwIP's half-close is not given: closesocket closes both ways) */
}

/* select: sockets and the console. A file's descriptor is always ready, as on a Unix. */
int select(int nfds, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv) {
    fd_set r0, w0, e0;
    uint64_t end = 0;
    int fd, n;
    sock_t *s;
    if(nfds < 0 || nfds > FD_SETSIZE) return fail(EINVAL);
    if(tv) end = cmm2->micros() + (uint64_t)tv->tv_sec * 1000000u + (uint64_t)tv->tv_usec;
    FD_ZERO(&r0); FD_ZERO(&w0); FD_ZERO(&e0);
    if(rd) r0 = *rd;
    if(wr) w0 = *wr;
    if(ex) e0 = *ex;
    net_tick();
    for(;;) {
        if(rd) FD_ZERO(rd);
        if(wr) FD_ZERO(wr);
        if(ex) FD_ZERO(ex);
        n = 0;
        for(fd = 0; fd < nfds; fd++) {
            int want_r = FD_ISSET(fd, &r0), want_w = FD_ISSET(fd, &w0), want_e = FD_ISSET(fd, &e0);
            if(!(want_r || want_w || want_e)) continue;
            if((s = get(fd)) != NULL) {
                if(want_r && ready_read(s)) { FD_SET(fd, rd); n++; }
                if(want_w && ready_write(s)) { FD_SET(fd, wr); n++; }
                if(want_e && has_error(s)) { FD_SET(fd, ex); n++; }
            } else if(fd >= 3 && fd < SOCK_FD0) {
                if(want_r) { FD_SET(fd, rd); n++; }     /* (a file) */
                if(want_w) { FD_SET(fd, wr); n++; }
            } else if(fd == 0 && want_r) {
                /* (the console: not looked at; curl does not wait on it) */
            } else if(fd < 3 && want_w) {
                FD_SET(fd, wr); n++;
            } else if(want_r || want_w) {
                return fail(EBADF);
            }
        }
        if(n > 0) return n;
        if(tv && cmm2->micros() >= end) return 0;
        pump();
    }
}

/* the name's address from the firmware's DNS (a dotted address as it is): 0, EAI_NONAME or EAI_AGAIN */
static int lookup(const char *name, uint8_t ip[4]) {
    struct in_addr a;
    uint64_t end;
    int r;
    if(inet_aton(name, &a)) {
        memcpy(ip, &a.s_addr, 4);
        return 0;
    }
    if(ensure_net()) return EAI_FAIL;
    end = cmm2->micros() + 15000000u;
    for(;;) {
        r = cmm2->net_dns(name, ip);
        if(r == 0) return 0;
        if(r != 1) return EAI_NONAME;
        if(cmm2->micros() >= end) return EAI_AGAIN;
        pump();
    }
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    struct addrinfo *ai;
    struct sockaddr_in *sa;
    uint8_t ip[4] = {0, 0, 0, 0};
    unsigned long port = 0;
    char *end;
    int fam = hints ? hints->ai_family : AF_UNSPEC, r;
    *res = NULL;
    if(fam != AF_UNSPEC && fam != AF_INET) return EAI_FAMILY;
    if(service && *service) {
        port = strtoul(service, &end, 10);
        if(*end || port > 65535) {
            if(!strcmp(service, "http")) port = 80;
            else if(!strcmp(service, "https")) port = 443;
            else return EAI_SERVICE;
        }
    }
    if(node) {
        if((r = lookup(node, ip)) != 0) return r;
    } else if(!(hints && (hints->ai_flags & AI_PASSIVE)))
        ip[0] = 127, ip[3] = 1;
    if((ai = calloc(1, sizeof(*ai) + sizeof(*sa))) == NULL) return EAI_MEMORY;
    sa = (struct sockaddr_in *)(ai + 1);
    sa->sin_family = AF_INET;
    sa->sin_port = htons((uint16_t)port);
    memcpy(&sa->sin_addr.s_addr, ip, 4);
    ai->ai_family = AF_INET;
    ai->ai_socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen = sizeof(*sa);
    ai->ai_addr = (struct sockaddr *)sa;
    if(node && hints && (hints->ai_flags & AI_CANONNAME)) ai->ai_canonname = strdup(node);
    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    struct addrinfo *next;
    for(; res; res = next) {
        next = res->ai_next;
        free(res->ai_canonname);
        free(res);
    }
}

const char *gai_strerror(int err) {
    switch(err) {
    case 0: return "Success";
    case EAI_NONAME: return "Name not found";
    case EAI_AGAIN: return "Name server did not answer";
    case EAI_FAIL: return "No network";
    case EAI_FAMILY: return "Address family not supported";
    case EAI_MEMORY: return "Out of memory";
    case EAI_SERVICE: return "Service not known";
    default: return "Name lookup failed";
    }
}

struct hostent *gethostbyname(const char *name) {
    static struct hostent he;
    static uint8_t ip[4];
    static char *list[2];
    if(lookup(name, ip) != 0) return NULL;
    list[0] = (char *)ip;
    list[1] = NULL;
    he.h_name = (char *)name;
    he.h_aliases = &list[1];
    he.h_addrtype = AF_INET;
    he.h_length = 4;
    he.h_addr_list = list;
    return &he;
}

int gethostname(char *name, size_t len) {
    strncpy(name, "cmm2", len);
    if(len) name[len - 1] = 0;
    return 0;
}

int inet_aton(const char *cp, struct in_addr *inp) {
    unsigned long v[4];
    char *end;
    int i;
    for(i = 0; i < 4; i++) {
        if(*cp < '0' || *cp > '9') return 0;
        v[i] = strtoul(cp, &end, 10);
        if(v[i] > 255) return 0;
        cp = end;
        if(i < 3) {
            if(*cp != '.') return 0;
            cp++;
        }
    }
    if(*cp) return 0;
    if(inp) inp->s_addr = htonl((uint32_t)(v[0] << 24 | v[1] << 16 | v[2] << 8 | v[3]));
    return 1;
}

/* IPv4 only: an IPv6 address is not one (0), as this network has none */
int inet_pton(int af, const char *src, void *dst) {
    struct in_addr a;
    if(af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if(!inet_aton(src, &a)) return 0;
    memcpy(dst, &a.s_addr, 4);
    return 1;
}

in_addr_t inet_addr(const char *cp) {
    struct in_addr a;
    return inet_aton(cp, &a) ? a.s_addr : INADDR_NONE;
}

char *inet_ntoa(struct in_addr in) {
    static char buf[INET_ADDRSTRLEN];
    const uint8_t *b = (const uint8_t *)&in.s_addr;
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}
