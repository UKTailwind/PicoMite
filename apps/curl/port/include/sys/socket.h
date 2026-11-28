/* sys/socket.h for a loadable program: the BSD socket calls that port/cmm2_net.c gives on the firmware's lwIP (services
   version 21: eight TCP/UDP handles, IPv4 only). newlib has none. MIT License, as cmm2img.h. */
#ifndef CMM2_SYS_SOCKET_H
#define CMM2_SYS_SOCKET_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char        ss_pad[126];
} __attribute__((aligned(8)));

struct linger {
    int l_onoff;
    int l_linger;
};

#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        10
#define PF_UNSPEC       AF_UNSPEC
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define SOL_SOCKET      1
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_LINGER       13
#define SO_RCVTIMEO     20
#define SO_SNDTIMEO     21
#define SO_NONBLOCK     0x4000          /* (not Linux's: how curl turns non-blocking on, HAVE_SETSOCKOPT_SO_NONBLOCK) */

#define MSG_PEEK        0x02
#define MSG_DONTWAIT    0x40

#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

#define SOMAXCONN       4

int socket(int domain, int type, int protocol);
int closesocket(int fd);
int connect(int fd, const struct sockaddr *addr, socklen_t len);
int bind(int fd, const struct sockaddr *addr, socklen_t len);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, socklen_t *len);
ssize_t send(int fd, const void *buf, size_t n, int flags);
ssize_t recv(int fd, void *buf, size_t n, int flags);
ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *from, socklen_t *fromlen);
int getsockopt(int fd, int level, int name, void *val, socklen_t *len);
int setsockopt(int fd, int level, int name, const void *val, socklen_t len);
int getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int shutdown(int fd, int how);

#ifdef __cplusplus
}
#endif
#endif
