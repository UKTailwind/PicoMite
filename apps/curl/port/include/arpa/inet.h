/* arpa/inet.h for a loadable program (port/cmm2_net.c). MIT License, as cmm2img.h. */
#ifndef CMM2_ARPA_INET_H
#define CMM2_ARPA_INET_H
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *inp);
char *inet_ntoa(struct in_addr in);
int inet_pton(int af, const char *src, void *dst);

#ifdef __cplusplus
}
#endif
#endif
