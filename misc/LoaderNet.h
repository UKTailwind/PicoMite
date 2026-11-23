/* LoaderNet.h: the network for loadable programs (misc/LoaderNet.c; loader/pmimg.h's net_ and sock_ services).
   MIT License, as pmimg.h. */
#ifndef LOADERNET_H
#define LOADERNET_H
#include <stdint.h>
int LoaderNetPoll(void);
int LoaderNetJoin(const char *ssid, const char *password);
int LoaderNetLeave(void);
int LoaderNetIfconfig(uint8_t a[16]);
int LoaderNetDns(const char *name, uint8_t ip[4]);
int LoaderSockOpen(int type);
int LoaderSockBind(int h, int port);
int LoaderSockConnect(int h, const uint8_t ip[4], int port);
int LoaderSockListen(int h, int backlog);
int LoaderSockAccept(int h, uint8_t ip[4], int *port);
int LoaderSockSend(int h, const uint8_t *buf, int n, const uint8_t *ip, int port);
int LoaderSockRecv(int h, uint8_t *buf, int n, uint8_t *ip, int *port);
int LoaderSockState(int h);
int LoaderSockClose(int h);
void LoaderNetClose(void);  /* a program's end: its handles closed */
#endif
