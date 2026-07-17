/***********************************************************************************************************************
PicoMite MMBasic

MMping.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
    on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
    by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
    without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
// WEB PING address$ [, count] [, timeout_ms] [, avgvar]
//
// Sends ICMP echo requests (default 4, 1..100) to a hostname or dotted IP address and
// prints a reply/timeout line for each, plus a summary (suppressed by OPTION STATUS OFF
// like the other WEB commands). The optional timeout is per-reply in ms (default 1000).
// The optional 4th argument names a float variable that receives the average round-trip
// time in ms, or -1 if no replies were received - so a program can test connectivity
// without parsing console output. Requires LWIP_RAW (enabled in lwipopts).

#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/dns.h"

#define PING_DATA_SIZE 32
#define PING_ID 0xB2B2

typedef struct
{
    ip_addr_t addr;
    volatile int dnsdone;
    volatile int gotreply;
    volatile uint32_t rtt_us;
    uint16_t seq;
    uint64_t sent_us;
} PING_T;
static PING_T pingst;
// File-scope so a previous invocation aborted by error() (longjmp) is cleaned up on the
// next call instead of leaking the pcb.
static struct raw_pcb *ping_pcb = NULL;

// Called with the result of the DNS lookup
static void ping_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    PING_T *state = (PING_T *)arg;
    if (ipaddr)
    {
        state->addr = *ipaddr;
        state->dnsdone = 1;
    }
    else
        web_async_set_error("ping dns request failed");
}

// Raw ICMP receive: claim our echo replies, pass everything else on
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    PING_T *state = (PING_T *)arg;
    if (p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)) && pbuf_remove_header(p, PBUF_IP_HLEN) == 0)
    {
        struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
        if (ICMPH_TYPE(iecho) == ICMP_ER && iecho->id == PING_ID && iecho->seqno == lwip_htons(state->seq))
        {
            state->rtt_us = (uint32_t)(time_us_64() - state->sent_us);
            state->gotreply = 1;
            pbuf_free(p);
            return 1; // eaten
        }
        pbuf_add_header(p, PBUF_IP_HLEN); // not ours - restore and pass on
    }
    return 0;
}

static void ping_send(struct raw_pcb *pcb, PING_T *state)
{
    size_t len = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;
    struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
    if (!p)
        return; // treated as a lost ping by the timeout
    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = PING_ID;
    iecho->seqno = lwip_htons(state->seq);
    for (size_t i = 0; i < PING_DATA_SIZE; i++)
        ((char *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    iecho->chksum = inet_chksum(iecho, len); // raw pcbs don't fill in the ICMP checksum
    state->gotreply = 0;
    state->sent_us = time_us_64();
    raw_sendto(pcb, p, &state->addr);
    pbuf_free(p);
}

void cmd_ping(unsigned char *tp)
{
    getcsargs(&tp, 7);
    if (!(argc == 1 || argc == 3 || argc == 5 || argc == 7))
        SyntaxError();
    if (ping_pcb) // previous invocation ended in error() - clean it up
    {
        raw_remove(ping_pcb);
        ping_pcb = NULL;
    }
    char *IP = GetTempStrMemory();
    strcpy(IP, (char *)getCstring(argv[0]));
    int count = 4, timeout = 1000;
    MMFLOAT *outavg = NULL;
    if (argc >= 3 && *argv[2])
        count = getint(argv[2], 1, 100);
    if (argc >= 5 && *argv[4])
        timeout = getint(argv[4], 100, 10000);
    if (argc == 7)
    {
        outavg = findvar(argv[6], V_FIND);
        if (!(g_vartbl[g_VarIndex].type & T_NBR))
            StandardError(6);
    }
    PING_T *state = &pingst;
    memset(state, 0, sizeof(PING_T));
    // Resolve the target: dotted IP directly, otherwise DNS (same pattern as WEB NTP)
    ip4_addr_t remote_addr;
    int dots = 0;
    for (const char *p = IP; *p; p++)
        if (*p == '.')
            dots++;
    if (dots == 3 && ip4addr_aton(IP, &remote_addr))
    {
        state->addr = remote_addr;
    }
    else
    {
        int err = dns_gethostbyname(IP, &remote_addr, ping_dns_found, state);
        if (err == ERR_OK)
            state->addr = remote_addr;
        else if (err == ERR_INPROGRESS)
        {
            Timer4 = 5000;
            while (!state->dnsdone && Timer4)
                if (startupcomplete)
                    cyw43_arch_poll();
            web_async_check_error();
            if (!Timer4)
                error("Failed to convert web address");
        }
        else
            error("Failed to resolve address");
    }
    char buff[STRINGSIZE] = {0};
    ping_pcb = raw_new(IP_PROTO_ICMP);
    if (!ping_pcb)
        error("failed to create pcb");
    raw_recv(ping_pcb, ping_recv, state);
    raw_bind(ping_pcb, IP_ADDR_ANY);
    int got = 0;
    uint64_t total_us = 0;
    for (int i = 0; i < count; i++)
    {
        state->seq++;
        ping_send(ping_pcb, state);
        Timer4 = timeout;
        while (!state->gotreply && Timer4)
        {
            if (startupcomplete)
                cyw43_arch_poll();
            web_async_check_error();
        }
        if (state->gotreply)
        {
            got++;
            total_us += state->rtt_us;
            sprintf(buff, "Reply from %s: seq=%d time=%u.%03ums\r\n", ip4addr_ntoa(&state->addr),
                    state->seq, (unsigned int)(state->rtt_us / 1000), (unsigned int)(state->rtt_us % 1000));
        }
        else
            sprintf(buff, "Request timed out: seq=%d\r\n", state->seq);
        if (!optionsuppressstatus)
            MMPrintString(buff);
        if (i < count - 1) // pace the pings; keep servicing the network meanwhile
        {
            Timer4 = 250;
            while (Timer4)
                if (startupcomplete)
                    cyw43_arch_poll();
        }
    }
    raw_remove(ping_pcb);
    ping_pcb = NULL;
    if (!optionsuppressstatus)
    {
        if (got)
            sprintf(buff, "Ping statistics for %s: sent=%d received=%d lost=%d average=%u.%03ums\r\n",
                    ip4addr_ntoa(&state->addr), count, got, count - got,
                    (unsigned int)(total_us / got / 1000), (unsigned int)((total_us / got) % 1000));
        else
            sprintf(buff, "Ping statistics for %s: sent=%d received=0\r\n", ip4addr_ntoa(&state->addr), count);
        MMPrintString(buff);
    }
    if (outavg)
        *outavg = got ? (MMFLOAT)((double)total_us / got / 1000.0) : -1.0;
}
