/* netdb.h for a loadable program (port/cmm2_net.c): getaddrinfo on the firmware's DNS (services version 21's net_dns),
   IPv4 only. MIT License, as cmm2img.h. */
#ifndef CMM2_NETDB_H
#define CMM2_NETDB_H
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

struct hostent {
    char  *h_name;
    char **h_aliases;
    int    h_addrtype;
    int    h_length;
    char **h_addr_list;
};
#define h_addr h_addr_list[0]

#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define AI_NUMERICSERV  0x08
#define AI_V4MAPPED     0x10
#define AI_ALL          0x20
#define AI_ADDRCONFIG   0x40

#define NI_MAXHOST      1025
#define NI_MAXSERV      32

#define EAI_AGAIN       (-3)
#define EAI_BADFLAGS    (-1)
#define EAI_FAIL        (-4)
#define EAI_FAMILY      (-6)
#define EAI_MEMORY      (-10)
#define EAI_NONAME      (-2)
#define EAI_SERVICE     (-8)
#define EAI_SOCKTYPE    (-7)
#define EAI_SYSTEM      (-11)

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int err);
struct hostent *gethostbyname(const char *name);
int gethostname(char *name, size_t len);

#ifdef __cplusplus
}
#endif
#endif
