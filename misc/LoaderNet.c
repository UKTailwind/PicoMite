/* LoaderNet.c: the network for loadable programs (loader/pmimg.h's net_ and sock_ services): MicroPython's socket and
   network modules, and any program built with the SDK. The Colour Maximite 2's Src/net/LoaderNet.c (its services
   version 21, the same names and meanings), on the PicoMite's own Wi-Fi (OPTION WIFI, which joins at power-up:
   net/WiFi.c) and lwIP, given to a program as eight BSD-style handles over lwIP's raw API: TCP (connect, or listen and
   accept) and UDP.
     - While a program runs, the network is pumped by its poll() (CheckAbort's ProcessWeb) and by net_poll, which it
       calls while it waits on the network. lwIP's callbacks below run only inside those, in the foreground (lwIP's
       poll mode): nothing here runs from an interrupt.
     - What arrives is kept until the program takes it: TCP's pbufs in a chain (the receive window opened again as it
       is taken, altcp_recved), UDP's datagrams in a queue of four (more are dropped, as a full socket buffer drops
       them), a listening handle's connections in a queue of four, each given a handle at once (lwIP's default receive
       callback would close a connection nobody had taken yet).
     - Errors are negative errno values, Linux's numbers as MicroPython has them.
     - A program's handles are all closed when it ends (Loader.c, ld_tidy: LoaderNetClose).
   The joins here do not change the options, and start none of MMBasic's servers: OPTION WIFI's network is joined
   again by WEB CONNECT, or at power-up. Builds without Wi-Fi answer -19 (ENODEV), net_poll 0.
   MIT License, as pmimg.h. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "LoaderNet.h"

#define E_NOENT        2                        // errno, Linux's numbers (MicroPython's MP_E*)
#define E_BADF         9
#define E_AGAIN        11
#define E_NOMEM        12
#define E_ACCES        13
#define E_NODEV        19
#define E_INVAL        22
#define E_MFILE        24
#define E_ADDRINUSE    98
#define E_CONNABORTED  103
#define E_CONNRESET    104
#define E_NOBUFS       105
#define E_ISCONN       106
#define E_NOTCONN      107
#define E_TIMEDOUT     110
#define E_CONNREFUSED  111
#define E_HOSTUNREACH  113
#define E_ALREADY      114
#define E_INPROGRESS   115

#ifdef PICOMITEWEB
#include "pico/cyw43_arch.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "net/WiFi.h"

#define LN_SOCKS 8
#define LN_QUEUE 4

#define LN_TCP 1
#define LN_UDP 2

#define F_CONNECTED  1                          // (sock_state's bits)
#define F_DATA       2
#define F_ROOM       4
#define F_PEERCLOSED 8
#define F_LISTEN     16                         // (internal)
#define F_CONNECTING 32

typedef struct
{
    uint8_t type;                               // 0 free, LN_TCP, LN_UDP
    uint8_t flags;
    int16_t err;                                // a pending error (negative errno), 0 none
    void *pcb;                                  // struct altcp_pcb * (TCP) or struct udp_pcb * (UDP); NULL once gone
    struct pbuf *rx;                            // TCP: what arrived and is not taken yet
    uint8_t qhead, qn;                          // UDP's datagrams, or a listening handle's connections (handles)
    struct pbuf *uq[LN_QUEUE];
    ip_addr_t uaddr[LN_QUEUE];
    uint16_t uport[LN_QUEUE];
    int8_t aq[LN_QUEUE];
} ln_sock;

static ln_sock lns[LN_SOCKS];

static int ln_errno(err_t e)
{
    switch (e)
    {
    case ERR_OK:
        return 0;
    case ERR_MEM:
        return -E_NOMEM;
    case ERR_BUF:
        return -E_NOBUFS;
    case ERR_TIMEOUT:
        return -E_TIMEDOUT;
    case ERR_RTE:
        return -E_HOSTUNREACH;
    case ERR_INPROGRESS:
        return -E_INPROGRESS;
    case ERR_WOULDBLOCK:
        return -E_AGAIN;
    case ERR_USE:
        return -E_ADDRINUSE;
    case ERR_ALREADY:
        return -E_ALREADY;
    case ERR_ISCONN:
        return -E_ISCONN;
    case ERR_CONN:
    case ERR_CLSD:
        return -E_NOTCONN;
    case ERR_ABRT:
        return -E_CONNABORTED;
    case ERR_RST:
        return -E_CONNRESET;
    default:
        return -E_INVAL;
    }
}

static ln_sock *ln_get(int h) { return (h >= 0 && h < LN_SOCKS && lns[h].type) ? &lns[h] : NULL; }

static int ln_new(int type)
{
    for (int h = 0; h < LN_SOCKS; h++)
        if (!lns[h].type)
        {
            memset(&lns[h], 0, sizeof(ln_sock));
            lns[h].type = type;
            return h;
        }
    return -E_MFILE;
}

/* ---- lwIP's callbacks (inside the network's poll) ---- */
static err_t ln_tcp_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    ln_sock *s = arg;
    if (p == NULL)
    { // the peer's end
        s->flags |= F_PEERCLOSED;
        return ERR_OK;
    }
    if (s->rx)
        pbuf_cat(s->rx, p);
    else
        s->rx = p;
    return ERR_OK; // (altcp_recved as the program takes it)
}

static void ln_tcp_err(void *arg, err_t err)
{
    ln_sock *s = arg; // (the pcb is freed already)
    s->pcb = NULL;
    s->err = err == ERR_RST && (s->flags & F_CONNECTING) ? -E_CONNREFUSED : ln_errno(err);
    if (s->err == 0)
        s->err = -E_CONNABORTED;
    s->flags &= ~(F_CONNECTED | F_CONNECTING);
}

static err_t ln_tcp_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    ln_sock *s = arg;
    s->flags &= ~F_CONNECTING;
    if (err != ERR_OK)
        s->err = ln_errno(err);
    else
        s->flags |= F_CONNECTED;
    return ERR_OK;
}

static void ln_tcp_setup(ln_sock *s, struct altcp_pcb *pcb)
{
    s->pcb = pcb;
    altcp_arg(pcb, s);
    altcp_recv(pcb, ln_tcp_recv);
    altcp_err(pcb, ln_tcp_err);
}

static err_t ln_tcp_accept(void *arg, struct altcp_pcb *newpcb, err_t err)
{
    ln_sock *l = arg;
    if (err != ERR_OK || newpcb == NULL)
        return ERR_VAL;
    int h = l->qn < LN_QUEUE ? ln_new(LN_TCP) : -1;
    if (h < 0)
    { // (no room: refused)
        altcp_abort(newpcb);
        return ERR_ABRT;
    }
    lns[h].flags = F_CONNECTED;
    ln_tcp_setup(&lns[h], newpcb);
    l->aq[(l->qhead + l->qn++) % LN_QUEUE] = h;
    return ERR_OK;
}

static void ln_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    ln_sock *s = arg;
    if (s->qn == LN_QUEUE)
    { // (a full queue drops it)
        pbuf_free(p);
        return;
    }
    int i = (s->qhead + s->qn++) % LN_QUEUE;
    s->uq[i] = p;
    ip_addr_copy(s->uaddr[i], *addr);
    s->uport[i] = port;
}

/* ---- the services ---- */
int LoaderNetPoll(void)
{
    if (!startupcomplete)
        return 0;
    ProcessWeb(1);
    struct netif *n = netif_list;
    return (WIFIconnected && n && netif_is_link_up(n) && !ip4_addr_isany_val(*netif_ip4_addr(n))) ? 2 : 1;
}

// that network joined, as WEB CONNECT joins OPTION WIFI's (net/WiFi.c's WebConnect: the first join after a restart can
// fail with PICO_ERROR_CONNECT_FAILED where the next succeeds, so that error is tried three times)
int LoaderNetJoin(const char *ssid, const char *password)
{
    if (!startupcomplete)
        return -E_NODEV;
    if (ssid == NULL || !*ssid || strlen(ssid) > 32 || (password && strlen(password) > 63))
        return -E_INVAL;
    int open = password == NULL || !*password, r;
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
    for (int attempt = 1;; attempt++)
    {
        r = cyw43_arch_wifi_connect_timeout_ms(ssid, open ? NULL : password, open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_MIXED_PSK,
                                               30000);
        if (r != PICO_ERROR_CONNECT_FAILED || attempt == 3)
            break;
        for (int i = 0; i < 100; i++)
        {
            cyw43_arch_poll();
            uSec(10000);
        }
    }
    if (r)
    {
        WIFIconnected = 0;
        LastWifiErr = r == PICO_ERROR_BADAUTH ? CYW43_LINK_BADAUTH : CYW43_LINK_FAIL;
        return r == PICO_ERROR_BADAUTH ? -E_ACCES : r == PICO_ERROR_TIMEOUT ? -E_TIMEDOUT : -E_HOSTUNREACH;
    }
    WIFIconnected = 1;
    LastWifiErr = 0;
    return 0;
}

int LoaderNetLeave(void)
{
    if (!startupcomplete)
        return -E_NODEV;
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    WIFIconnected = 0;
    return 0;
}

int LoaderNetIfconfig(uint8_t a[16])
{
    struct netif *n = netif_list;
    memset(a, 0, 16);
    if (!startupcomplete || n == NULL)
        return -E_NODEV;
    memcpy(a, netif_ip4_addr(n), 4);
    memcpy(a + 4, netif_ip4_netmask(n), 4);
    memcpy(a + 8, netif_ip4_gw(n), 4);
    const ip_addr_t *d = dns_getserver(0);
    if (d)
        memcpy(a + 12, ip_2_ip4(d), 4);
    return 0;
}

// DNS: one lookup at a time (the caller asks again with the same name until it is answered)
static char ln_dnsname[64];
static int ln_dnsstate; // 0 none, 1 asked, 2 found, 3 not found
static ip_addr_t ln_dnsaddr;
static int ln_dnsgen;

static void ln_dns_found(const char *name, const ip_addr_t *addr, void *arg)
{
    if ((int)(intptr_t)arg != ln_dnsgen)
        return; // (an old lookup's answer)
    if (addr)
    {
        ip_addr_copy(ln_dnsaddr, *addr);
        ln_dnsstate = 2;
    }
    else
        ln_dnsstate = 3;
}

int LoaderNetDns(const char *name, uint8_t ip[4])
{
    ip4_addr_t a;
    if (name == NULL || !*name || strlen(name) >= sizeof(ln_dnsname))
        return -E_INVAL;
    if (ip4addr_aton(name, &a))
    {
        memcpy(ip, &a, 4);
        return 0;
    }
    if (!startupcomplete)
        return -E_NODEV;
    if (ln_dnsstate && strcmp(name, ln_dnsname) == 0)
    {
        if (ln_dnsstate == 1)
            return 1;
        int r = ln_dnsstate == 2 ? 0 : -E_NOENT;
        if (r == 0)
            memcpy(ip, ip_2_ip4(&ln_dnsaddr), 4);
        ln_dnsstate = 0;
        return r;
    }
    strcpy(ln_dnsname, name);
    ln_dnsgen++;
    ip_addr_t addr;
    err_t e = dns_gethostbyname(name, &addr, ln_dns_found, (void *)(intptr_t)ln_dnsgen);
    if (e == ERR_OK)
    {
        ln_dnsstate = 0;
        memcpy(ip, ip_2_ip4(&addr), 4);
        return 0;
    }
    if (e == ERR_INPROGRESS)
    {
        ln_dnsstate = 1;
        return 1;
    }
    ln_dnsstate = 0;
    return -E_NOENT;
}

int LoaderSockOpen(int type)
{
    if (!startupcomplete)
        return -E_NODEV;
    if (type != LN_TCP && type != LN_UDP)
        return -E_INVAL;
    int h = ln_new(type);
    if (h < 0)
        return h;
    if (type == LN_TCP)
    {
        struct altcp_pcb *pcb = altcp_tcp_new_ip_type(IPADDR_TYPE_V4);
        if (pcb == NULL)
        {
            lns[h].type = 0;
            return -E_NOMEM;
        }
        ln_tcp_setup(&lns[h], pcb);
    }
    else
    {
        struct udp_pcb *pcb = udp_new_ip_type(IPADDR_TYPE_V4);
        if (pcb == NULL)
        {
            lns[h].type = 0;
            return -E_NOMEM;
        }
        lns[h].pcb = pcb;
        udp_recv(pcb, ln_udp_recv, &lns[h]);
    }
    return h;
}

int LoaderSockBind(int h, int port)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (s->pcb == NULL)
        return s->err ? s->err : -E_INVAL;
    err_t e = s->type == LN_TCP ? altcp_bind(s->pcb, IP_ANY_TYPE, port) : udp_bind(s->pcb, IP_ANY_TYPE, port);
    return ln_errno(e);
}

int LoaderSockConnect(int h, const uint8_t ip[4], int port)
{
    ln_sock *s = ln_get(h);
    ip_addr_t a;
    if (s == NULL)
        return -E_BADF;
    if (s->pcb == NULL)
        return s->err ? s->err : -E_INVAL;
    IP_ADDR4(&a, ip[0], ip[1], ip[2], ip[3]);
    if (s->type == LN_UDP)
        return ln_errno(udp_connect(s->pcb, &a, port));
    if (s->flags & F_CONNECTED)
        return -E_ISCONN;
    if (s->flags & F_CONNECTING)
        return -E_ALREADY;
    err_t e = altcp_connect(s->pcb, &a, port, ln_tcp_connected);
    if (e != ERR_OK)
        return ln_errno(e);
    s->flags |= F_CONNECTING;
    return 0;
}

int LoaderSockListen(int h, int backlog)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (s->type != LN_TCP || s->pcb == NULL)
        return -E_INVAL;
    err_t e = ERR_OK;
    struct altcp_pcb *l = altcp_listen_with_backlog_and_err(s->pcb, backlog < 1 ? 1 : backlog > 255 ? 255 : backlog, &e);
    if (l == NULL)
        return ln_errno(e ? e : ERR_MEM);
    s->pcb = l; // (the old pcb is freed by the listen)
    s->flags |= F_LISTEN;
    altcp_arg(l, s);
    altcp_accept(l, ln_tcp_accept);
    return 0;
}

int LoaderSockAccept(int h, uint8_t ip[4], int *port)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (!(s->flags & F_LISTEN))
        return -E_INVAL;
    if (s->qn == 0)
        return s->err ? s->err : -E_AGAIN;
    int n = s->aq[s->qhead];
    s->qhead = (s->qhead + 1) % LN_QUEUE;
    s->qn--;
    if (ip || port)
    {
        ip_addr_t a;
        u16_t p = 0;
        ip_addr_set_zero(&a);
        if (lns[n].pcb)
            altcp_get_tcp_addrinfo(lns[n].pcb, 0, &a, &p);
        if (ip)
            memcpy(ip, ip_2_ip4(&a), 4);
        if (port)
            *port = p;
    }
    return n;
}

int LoaderSockSend(int h, const uint8_t *buf, int n, const uint8_t *ip, int port)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (n < 0)
        return -E_INVAL;
    if (s->type == LN_UDP)
    {
        if (s->pcb == NULL)
            return -E_BADF;
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
        if (p == NULL)
            return -E_NOMEM;
        memcpy(p->payload, buf, n);
        err_t e;
        if (ip)
        {
            ip_addr_t a;
            IP_ADDR4(&a, ip[0], ip[1], ip[2], ip[3]);
            e = udp_sendto(s->pcb, p, &a, port);
        }
        else
            e = udp_send(s->pcb, p);
        pbuf_free(p);
        return e == ERR_OK ? n : ln_errno(e);
    }
    if (s->pcb == NULL)
        return s->err ? s->err : -E_NOTCONN;
    if (!(s->flags & F_CONNECTED))
        return (s->flags & F_CONNECTING) ? -E_AGAIN : -E_NOTCONN;
    int room = altcp_sndbuf(s->pcb);
    if (n > room)
        n = room;
    if (n > 0xFFFF)
        n = 0xFFFF;
    if (n == 0)
        return 0;
    err_t e = altcp_write(s->pcb, buf, (u16_t)n, TCP_WRITE_FLAG_COPY);
    if (e == ERR_MEM)
        return 0; // (no room after all: try again)
    if (e != ERR_OK)
        return ln_errno(e);
    altcp_output(s->pcb);
    return n;
}

int LoaderSockRecv(int h, uint8_t *buf, int n, uint8_t *ip, int *port)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (n < 0)
        return -E_INVAL;
    if (s->type == LN_UDP)
    {
        if (s->qn == 0)
            return -E_AGAIN;
        int i = s->qhead;
        struct pbuf *p = s->uq[i];
        int k = p->tot_len < n ? p->tot_len : n;
        pbuf_copy_partial(p, buf, k, 0);
        if (ip)
            memcpy(ip, ip_2_ip4(&s->uaddr[i]), 4);
        if (port)
            *port = s->uport[i];
        pbuf_free(p);
        s->uq[i] = NULL;
        s->qhead = (s->qhead + 1) % LN_QUEUE;
        s->qn--;
        return k;
    }
    if (s->rx == NULL)
    {
        if (s->flags & F_PEERCLOSED)
            return 0;
        if (s->err)
            return s->err;
        return (s->flags & (F_CONNECTED | F_CONNECTING)) ? -E_AGAIN : -E_NOTCONN;
    }
    int k = s->rx->tot_len;
    if (k > n)
        k = n;
    if (k > 0xFFFF)
        k = 0xFFFF;
    pbuf_copy_partial(s->rx, buf, k, 0);
    s->rx = pbuf_free_header(s->rx, (u16_t)k); // (the pbufs taken in full freed, the next one's start moved on)
    if (s->pcb)
        altcp_recved(s->pcb, (u16_t)k);
    if (ip)
        memset(ip, 0, 4);
    if (port)
        *port = 0;
    return k;
}

int LoaderSockState(int h)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    int f = s->flags & (F_CONNECTED | F_PEERCLOSED);
    if (s->type == LN_UDP)
        return f | F_ROOM | (s->qn ? F_DATA : 0);
    if (s->flags & F_LISTEN)
        return s->qn ? F_DATA : (s->err ? s->err : 0);
    if (s->rx)
        f |= F_DATA;
    if (s->pcb && (s->flags & F_CONNECTED) && altcp_sndbuf(s->pcb) > 0)
        f |= F_ROOM;
    if (f == 0 && s->err)
        return s->err;
    return f;
}

int LoaderSockClose(int h)
{
    ln_sock *s = ln_get(h);
    if (s == NULL)
        return -E_BADF;
    if (s->type == LN_UDP)
    {
        if (s->pcb)
            udp_remove(s->pcb);
        while (s->qn)
        {
            pbuf_free(s->uq[s->qhead]);
            s->qhead = (s->qhead + 1) % LN_QUEUE;
            s->qn--;
        }
    }
    else
    {
        if (s->flags & F_LISTEN) // (connections never taken: closed with it)
            while (s->qn)
            {
                LoaderSockClose(s->aq[s->qhead]);
                s->qhead = (s->qhead + 1) % LN_QUEUE;
                s->qn--;
            }
        if (s->pcb)
        {
            struct altcp_pcb *pcb = s->pcb;
            altcp_arg(pcb, NULL);
            altcp_recv(pcb, NULL);
            altcp_err(pcb, NULL);
            if (s->flags & F_LISTEN)
                altcp_accept(pcb, NULL);
            if (altcp_close(pcb) != ERR_OK)
                altcp_abort(pcb);
        }
        if (s->rx)
            pbuf_free(s->rx);
    }
    memset(s, 0, sizeof(ln_sock));
    return 0;
}

// a program's end (Loader.c, ld_tidy): its handles closed, its DNS question forgotten
void LoaderNetClose(void)
{
    for (int h = 0; h < LN_SOCKS; h++)
        if (lns[h].type)
            LoaderSockClose(h);
    ln_dnsstate = 0;
}

#else // no Wi-Fi in this build: every service says so

int LoaderNetPoll(void) { return 0; }
int LoaderNetJoin(const char *ssid, const char *password) { return -E_NODEV; }
int LoaderNetLeave(void) { return -E_NODEV; }
int LoaderNetIfconfig(uint8_t a[16])
{
    memset(a, 0, 16);
    return -E_NODEV;
}
int LoaderNetDns(const char *name, uint8_t ip[4]) { return -E_NODEV; }
int LoaderSockOpen(int type) { return -E_NODEV; }
int LoaderSockBind(int h, int port) { return -E_BADF; }
int LoaderSockConnect(int h, const uint8_t ip[4], int port) { return -E_BADF; }
int LoaderSockListen(int h, int backlog) { return -E_BADF; }
int LoaderSockAccept(int h, uint8_t ip[4], int *port) { return -E_BADF; }
int LoaderSockSend(int h, const uint8_t *buf, int n, const uint8_t *ip, int port) { return -E_BADF; }
int LoaderSockRecv(int h, uint8_t *buf, int n, uint8_t *ip, int *port) { return -E_BADF; }
int LoaderSockState(int h) { return -E_BADF; }
int LoaderSockClose(int h) { return -E_BADF; }
void LoaderNetClose(void) {}

#endif
