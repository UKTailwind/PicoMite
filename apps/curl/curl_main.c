/* curl_main.c: curl.app, the curl tool (curl 8.23.0) for the PicoMite (the PC3 build), from the Colour Maximite 2's
   apps/curl (CMM2V1.5 v8-dev; its manual chapter is the CMM2's docs/apps/text/14_curl.md). curl's own
   main (src/tool_main.c) is renamed curl_tool_main by port/curl_config.h, and called from here.
       RUN "A:/curl/curl.app", [options] url ...
   as curl takes them ("--help all" lists them).
   - The CA bundle: --cacert's, else CURL_CA_BUNDLE's, else cacert.pem in the app's own folder if it is there (as the
     programs zip has it), else A:/cacert.pem (the manual's WEB TLS CA example; port/curl_config.h).
   - When a certificate is refused and the clock is before 2026, it says the clock may be why.
   Two options of its own, for measuring:
       RUN "curl.app", --cmm2-bench a.b.c.d port /path
   what net_poll costs, and an HTTP download through the services directly and through port/cmm2_net.c's sockets;
       RUN "curl.app", --cmm2-mem [curl's options] url ...
   curl as usual, then the heap's high-water mark. MIT License, as pmimg.h. */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cmm2.h"

int curl_tool_main(int argc, char *argv[]);

static uint8_t buf[16384];

static void report(const char *what, uint64_t bytes, uint64_t us, uint32_t polls) {
    printf("%-34s %8lu bytes in %6lu ms: %5lu KB/s, %lu net_polls\n", what, (unsigned long)bytes,
           (unsigned long)(us / 1000), (unsigned long)(us ? bytes * 1000000 / us / 1024 : 0), (unsigned long)polls);
}

/* the benches' connect, given 15 s (the network polled meanwhile); 0, and the handle closed, if it failed */
static int bench_connect(int h, const uint8_t ip[4], int port) {
    uint64_t t0 = cmm2->micros();
    int st;
    cmm2->sock_connect(h, ip, port);
    while(!((st = cmm2->sock_state(h)) & 1)) {
        if(st < 0 || cmm2->micros() - t0 > 15000000u) {
            if(st < 0) printf("connect %d\n", st);
            else puts("connect: no answer in 15 s");
            cmm2->sock_close(h);
            return 0;
        }
        cmm2->net_poll();
    }
    return 1;
}

/* 1 (and said) once nothing has come for 15 s: a dead network ends a bench rather than hanging it */
static int bench_stalled(uint64_t last, uint64_t bytes) {
    if(cmm2->micros() - last <= 15000000u) return 0;
    printf("stalled: nothing for 15 s after %lu bytes\n", (unsigned long)bytes);
    return 1;
}

/* an HTTP GET through the services alone: mode 0 polls before every sock_recv, mode 1 only when nothing came */
static void bench_raw(const uint8_t ip[4], int port, const char *req, int mode) {
    int h = cmm2->sock_open(1), r;
    uint64_t t0, last, bytes = 0;
    uint32_t polls = 0;
    if(h < 0) { printf("sock_open %d\n", h); return; }
    if(!bench_connect(h, ip, port)) return;
    cmm2->sock_send(h, (const uint8_t *)req, strlen(req), NULL, 0);
    t0 = last = cmm2->micros();
    for(;;) {
        if(mode == 0) { cmm2->net_poll(); polls++; }
        r = cmm2->sock_recv(h, buf, sizeof(buf), NULL, NULL);
        if(r > 0) { bytes += r; last = cmm2->micros(); continue; }
        if(r == 0) break;
        if(r != -11) { printf("recv %d\n", r); break; }
        if(bench_stalled(last, bytes)) break;
        if(mode == 1) { cmm2->net_poll(); polls++; }
    }
    report(mode ? "services, poll when empty" : "services, poll before each recv", bytes, cmm2->micros() - t0, polls);
    cmm2->sock_close(h);
}

/* the same through port/cmm2_net.c's blocking sockets */
static void bench_sock(const uint8_t ip[4], int port, const char *req) {
    struct sockaddr_in a;
    int fd = socket(AF_INET, SOCK_STREAM, 0), r;
    uint64_t t0, bytes = 0;
    if(fd < 0) { printf("socket errno %d\n", errno); return; }
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    memcpy(&a.sin_addr.s_addr, ip, 4);
    if(connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { printf("connect errno %d\n", errno); closesocket(fd); return; }
    send(fd, req, strlen(req), 0);
    t0 = cmm2->micros();
    while((r = recv(fd, buf, sizeof(buf), 0)) > 0) bytes += r;
    report("cmm2_net.c sockets", bytes, cmm2->micros() - t0, 0);
    closesocket(fd);
}

/* a download through the services that leaves the radio alone for gap_ms after each 512 KB, as a program busy with
   other work does (27 Nov 2026: the browser's network died after some pages) */
static void bench_gap(const uint8_t ip[4], int port, const char *req, int gap_ms) {
    int h = cmm2->sock_open(1), r, gaps = 0;
    uint64_t t0, last, bytes = 0, next = 512 * 1024, g;
    if(h < 0) { printf("sock_open %d\n", h); return; }
    if(!bench_connect(h, ip, port)) return;
    cmm2->sock_send(h, (const uint8_t *)req, strlen(req), NULL, 0);
    t0 = last = cmm2->micros();
    for(;;) {
        r = cmm2->sock_recv(h, buf, sizeof(buf), NULL, NULL);
        if(r > 0) {
            bytes += r;
            last = cmm2->micros();
            if(bytes >= next) {                 /* (busy, the radio not looked at) */
                next += 512 * 1024;
                g = cmm2->micros() + (uint64_t)gap_ms * 1000u;
                while(cmm2->micros() < g) {}
                gaps++;
            }
            continue;
        }
        if(r == 0) break;
        if(r != -11) { printf("recv %d\n", r); break; }
        if(bench_stalled(last, bytes)) break;
        cmm2->net_poll();
    }
    printf("gaps of %d ms: %lu bytes in %lu ms, %d gaps\n", gap_ms, (unsigned long)bytes,
           (unsigned long)((cmm2->micros() - t0) / 1000), gaps);
    cmm2->sock_close(h);
}

static int bench(int argc, char **argv) {
    struct in_addr a;
    uint8_t ip[4];
    char req[300];
    uint64_t t0;
    int i, port;
    if(argc < 5 || !inet_aton(argv[2], &a)) {
        puts("RUN \"curl.app\", --cmm2-bench a.b.c.d port /path [gap_ms]");
        return 2;
    }
    memcpy(ip, &a.s_addr, 4);
    port = atoi(argv[3]);
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", argv[4], argv[2]);
    if(cmm2->version < NET_API || cmm2->net_poll() != 2) { puts("no network"); return 1; }
    if(argc > 5) {
        bench_gap(ip, port, req, atoi(argv[5]));
        return 0;
    }
    t0 = cmm2->micros();
    for(i = 0; i < 1000; i++) cmm2->net_poll();
    printf("net_poll, idle: %lu us each\n", (unsigned long)((cmm2->micros() - t0) / 1000));
    bench_raw(ip, port, req, 0);
    bench_raw(ip, port, req, 1);
    bench_sock(ip, port, req);
    return 0;
}

/* cacert.pem in the app's own folder, given to curl's own lookup (src/tool_operate.c, cacertpaths) as CURL_CA_BUNDLE,
   unless the user has set that: --cacert still wins over it */
static void ca_beside_app(const char *app) {
    static char path[300];
    struct stat st;
    const char *slash = strrchr(app, '/');
    size_t n;
    if(getenv("CURL_CA_BUNDLE")) return;
    if(!slash) slash = strrchr(app, '\\');
    n = slash ? (size_t)(slash - app + 1) : (app[0] && app[1] == ':' ? 2 : 0);
    if(n + sizeof("cacert.pem") > sizeof(path)) return;
    memcpy(path, app, n);
    strcpy(path + n, "cacert.pem");
    if(stat(path, &st) == 0 && !S_ISDIR(st.st_mode)) setenv("CURL_CA_BUNDLE", path, 1);
}

/* after a refused certificate (curl's 60) or a failed handshake (35): a clock before 2026 is the likely reason, as a
   certificate is valid only between two dates */
static void clock_hint(int r) {
    time_t t = time(NULL);
    struct tm tm;
    if(r != 60 && r != 35) return;
    gmtime_r(&t, &tm);
    if(tm.tm_year + 1900 >= 2026) return;
    fprintf(stderr, "The clock says %04d-%02d-%02d, and certificates are checked against it: WEB NTP sets it.\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

int main(int argc, char **argv) {
    int r, mem = 0;
    if(argc > 1 && !strcmp(argv[1], "--cmm2-bench")) return bench(argc, argv);
    if(argc > 0) ca_beside_app(argv[0]);
    if(argc > 1 && !strcmp(argv[1], "--cmm2-mem")) {           /* curl as usual, then the heap's high-water mark */
        mem = 1;
        argv[1] = argv[0];
        argv++;
        argc--;
    }
    r = curl_tool_main(argc, argv);
    fflush(stdout);
    clock_hint(r);
    fflush(stderr);
    if(mem) {
        struct mallinfo mi = mallinfo();
        fprintf(stderr, "heap: %lu KB taken from the system at most, %lu KB in use at the end\n",
                (unsigned long)(mi.arena / 1024), (unsigned long)(mi.uordblks / 1024));
    }
    return r;
}
