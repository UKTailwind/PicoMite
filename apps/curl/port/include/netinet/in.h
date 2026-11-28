/* netinet/in.h for a loadable program (port/cmm2_net.c): IPv4 addresses and byte order. MIT License, as cmm2img.h. */
#ifndef CMM2_NETINET_IN_H
#define CMM2_NETINET_IN_H
#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;                   /* network order */
};

struct sockaddr_in {
    sa_family_t    sin_family;
    in_port_t      sin_port;            /* network order */
    struct in_addr sin_addr;
    char           sin_zero[8];
};

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_LOOPBACK  ((in_addr_t)0x7F000001)
#define INADDR_NONE      ((in_addr_t)0xFFFFFFFF)
#define INADDR_BROADCAST ((in_addr_t)0xFFFFFFFF)

#define INET_ADDRSTRLEN 16

/* the M7 is little-endian; the network is big-endian */
#define htons(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
#define ntohs(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
#define htonl(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))
#define ntohl(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))

#ifdef __cplusplus
}
#endif
#endif
