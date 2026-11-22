/* modtest.c: a USER module that tries the services from inside a statement (tools/sdk). MIT License, as pmimg.h.

     python build.py out/modtest.mod examples/user/modtest.c --user

     USER LOAD "modtest"
     USER SVC            the console, the time, alloc and release, a file on A: written, read back and removed through
                         the services; PASS or FAIL for each
     USER KEEP           opens A:/modtest.txt for writing and leaves it open: USER CLOSE (or RUN...) closes it
     USER LEAVE          cmm2->leave(3): MMBasic's error "A USER module cannot leave()"
     USER SPIN           calls poll() until Ctrl-C, which breaks the statement as in any command
     PRINT USER(MICROS)  the microseconds since start-up, from the services
     PRINT USER(FMT$ "%g", 1e6)   snprintf's formatting (the firmware's) of a number */
#include <stdio.h>
#include <string.h>
#include "cmm2user.h"

static const struct cmm2_basic *B;
static int keep = -1;                           /* the file USER KEEP left open */

static void check(const char *what, int ok) {
    char b[120];
    snprintf(b, sizeof(b), "%s %s\r\n", ok ? "PASS" : "FAIL", what);
    B->print(b);
}

static void c_svc(char *p) {
    char buf[16];
    void *m;
    int h, n;
    (void)p;
    cmm2->con_puts("con_puts from a module\r\n");
    check("micros counts", cmm2->micros() > 0);
    m = cmm2->alloc(1000);
    check("alloc 1000", m != NULL);
    cmm2->release(m);
    h = cmm2->file_open("A:/modtest.txt", CMM2_O_WRITE | CMM2_O_CREATE);
    check("file_open A:/modtest.txt for writing", h >= 0);
    if(h < 0) return;
    check("file_write 5", cmm2->file_write(h, "hello", 5) == 5);
    check("file_close", cmm2->file_close(h) == 0);
    h = cmm2->file_open("A:/modtest.txt", CMM2_O_READ);
    n = h >= 0 ? cmm2->file_read(h, buf, sizeof(buf)) : -1;
    check("read back hello", n == 5 && memcmp(buf, "hello", 5) == 0);
    if(h >= 0) cmm2->file_close(h);
    check("file_remove", cmm2->file_remove("A:/modtest.txt") == 0);
    check("file_open of nothing: an error, -4 or below", cmm2->file_open("A:/no/such.txt", CMM2_O_READ) < 0);
}

static void c_keep(char *p) {
    (void)p;
    keep = cmm2->file_open("A:/modtest.txt", CMM2_O_WRITE | CMM2_O_CREATE);
    if(keep < 0) B->error("Cannot open A:/modtest.txt");
    cmm2->file_write(keep, "kept", 4);
    B->print("A:/modtest.txt left open\r\n");
}

static void c_leave(char *p) { (void)p; cmm2->leave(3); B->print("not reached\r\n"); }

static void c_spin(char *p) {
    (void)p;
    B->print("poll() until Ctrl-C\r\n");
    for(;;) if(cmm2->poll()) { B->print("poll() returned 1\r\n"); return; }
}

static void f_micros(char *p, cmm2_value *r) {
    (void)p;
    r->type = CMM2_T_INT;
    r->i = (int64_t)cmm2->micros();
}

typedef struct { const char *name; void (*fn)(char *p); } command_t;
static const command_t commands[] = {{"SVC", c_svc}, {"KEEP", c_keep}, {"LEAVE", c_leave}, {"SPIN", c_spin}};

static void command(char *line) {
    char *p;
    for(unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        if((p = B->checkstring(line, commands[i].name)) != NULL) { commands[i].fn(p); return; }
    B->error("Unknown USER command");
}

/* USER(FMT$ format$, x): snprintf's (the firmware's) formatting of x */
static void f_fmt(char *p, cmm2_value *r) {
    char *argv[3];
    int argc, n;
    B->getargs(&p, 3, ",", argv, &argc);
    if(argc != 3) B->error("Argument count");
    n = snprintf(r->s + 1, CMM2_STRINGSIZE - 1, B->get_cstring(argv[0]), B->get_number(argv[2]));
    r->s[0] = (char)(n > 254 ? 254 : n);
    r->type = CMM2_T_STR;
}

static void function(char *args, cmm2_value *r) {
    char *p;
    if((p = B->checkstring(args, "MICROS")) != NULL) { f_micros(p, r); return; }
    if((p = B->checkstring(args, "FMT$")) != NULL) { f_fmt(p, r); return; }
    B->error("Unknown USER function");
}

static void forget(void) {}                     /* (a file left open is the firmware's to close) */

static const cmm2user_head table = {{CMM2MOD_MAGIC, CMM2USER_VERSION, forget}, CMM2USER_MAGIC, command, function};

const void *module_main(void) {
    B = cmm2_start->basic;
    if(B == NULL || B->version < CMM2USER_VERSION || B->size < sizeof(struct cmm2_basic)) return NULL;
    return &table;
}
