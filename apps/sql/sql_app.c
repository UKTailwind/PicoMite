/* sql_app.c: sql.app, SQLite run from MMBasic's RUN, a statement or a file of them at a time. MIT License.

   Step 1 of docs/SQL_Plan.md: the SQLite of the module to come (sqlcore.c), run as an application, to try it on the
   card and to measure what a statement costs.

       RUN "A:/sql/sql.app", [-h] [-t] [-log] database "statement; ..."
       RUN "A:/sql/sql.app", [-h] [-t] [-log] database @file.sql
       RUN "A:/sql/sql.app", [-t] database -bench [n]

   The words after the database are the statement (in double quotes it may hold anything but a double quote: SQL's
   strings are in single quotes). Each statement's rows are printed a line each, the columns separated by "|", a NULL
   as nothing (SQLite's shell's list mode); -h prints the columns' names first. -t prints the time each took. -log prints what SQLite
   reports as it goes (an allocation it could not make, with its size). A
   database is made if it is not there. An error is printed, and ends the program with code 1. -bench times opening,
   n single inserts each its own transaction (default 100), 1000 in one, and some queries, in a table "bench".

   PicoMite: the Colour Maximite 2's apps/sql/sql_app.c (5e9721f). RUN's second argument is a string, whose words are
   the program's arguments (a word in double quotes may hold spaces; the words of the statement are put back together
   with a space between, so it needs no quotes):

       RUN "sql.app", "[-h] [-t] [-log] [-stack] database statement; ..."
       RUN "sql.app", "[-h] [-t] [-log] [-stack] database @file.sql"
       RUN "sql.app", "[-t] [-stack] database -bench [n]"

   The program may be on any drive; the database must be on B: or C: (sqlcore.c). -stack prints the most of the
   program's stack that SQLite used (make.py's SQL_STACK), to size the SQL module's own stack. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmm2.h"
#include "sqlcore.h"

static int headers, timing, logging;

#ifdef SQL_STACK
/* -stack: the stack below main's frame filled with a pattern at the start, and at the end the deepest word that
   changed. main is called near the stack's top (crt0 and the loader use little of it), so the pattern goes from 1 KB
   below here to 2 KB above the stack's bottom */
#define STACK_MARK 0x5AC3E10Fu
static uint32_t *mark_lo, *mark_hi;

static void __attribute__((noinline)) stack_mark(void) {
    uint32_t sp, *p;
    __asm volatile("mov %0, sp" : "=r"(sp));
    mark_hi = (uint32_t *)((sp - 1024) & ~3u);
    mark_lo = (uint32_t *)((sp - SQL_STACK + 2048) & ~3u);
    for(p = mark_lo; p < mark_hi; p++) *p = STACK_MARK;
}

static unsigned stack_used(void) {          /* bytes from the pattern's top down to the deepest word changed */
    uint32_t *p = mark_lo;
    while(p < mark_hi && *p == STACK_MARK) p++;
    return (unsigned)((char *)mark_hi - (char *)p) + 1024;
}
#endif

/* -log: what SQLite reports as it goes (an allocation that failed, and its size; a statement it had to compile again) */
static void logged(void *arg, int code, const char *msg) { (void)arg; fprintf(stderr, "sqlite log %d: %s\n", code, msg); }

static unsigned long ms_since(uint64_t t0) { return (unsigned long)((cmm2->micros() - t0 + 500) / 1000); }

static int fail(sqlite3 *db, const char *what) {
    fprintf(stderr, "%s: %s\n", what, sqlcore_broken ? "stopped by Ctrl-C" : sqlite3_errmsg(db));
    return 1;
}

/* every statement of sql, its rows printed: 0, or 1 after an error */
static int run(sqlite3 *db, const char *sql) {
    const char *tail = sql;
    while(*tail) {
        sqlite3_stmt *st;
        uint64_t t0 = cmm2->micros();
        int i, nc, r;
        if(sqlite3_prepare_v2(db, tail, -1, &st, &tail) != SQLITE_OK) return fail(db, "SQL");
        if(st == NULL) continue;            /* (only spaces or a comment left) */
        nc = sqlite3_column_count(st);
        if(headers && nc) {
            for(i = 0; i < nc; i++) printf("%s%s", i ? "|" : "", sqlite3_column_name(st, i));
            printf("\n");
        }
        while((r = sqlite3_step(st)) == SQLITE_ROW) {
            for(i = 0; i < nc; i++) {
                const unsigned char *v = sqlite3_column_text(st, i);
                printf("%s%s", i ? "|" : "", v ? (const char *)v : "");
            }
            printf("\n");
        }
        if(r != SQLITE_DONE) {
            fail(db, "SQL");
            sqlite3_finalize(st);
            return 1;
        }
        sqlite3_finalize(st);
        if(timing) printf("(%lu ms)\n", ms_since(t0));
    }
    return 0;
}

/* a file's whole text, from malloc, or NULL */
static char *slurp(const char *name) {
    FILE *f = fopen(name, "rb");
    long n;
    char *s;
    if(f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(n < 0 || (s = malloc((size_t)n + 1)) == NULL) { fclose(f); return NULL; }
    if(fread(s, 1, (size_t)n, f) != (size_t)n) { free(s); fclose(f); return NULL; }
    s[n] = 0;
    fclose(f);
    return s;
}

static int exec(sqlite3 *db, const char *sql) {
    if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) return fail(db, sql);
    return 0;
}

static int bench(sqlite3 *db, int n) {
    sqlite3_stmt *ins, *get;
    uint64_t t0;
    long long sum = 0;
    int i, rows = 0;
    t0 = cmm2->micros();
    if(exec(db, "DROP TABLE IF EXISTS bench; CREATE TABLE bench(id INTEGER PRIMARY KEY, name TEXT, n INTEGER)")) return 1;
    printf("table made:            %6lu ms\n", ms_since(t0));
    if(sqlite3_prepare_v2(db, "INSERT INTO bench(name, n) VALUES(?, ?)", -1, &ins, NULL) != SQLITE_OK) return fail(db, "insert");
    t0 = cmm2->micros();
    for(i = 0; i < n; i++) {                /* each its own transaction: journal made, synced, removed */
        sqlite3_bind_text(ins, 1, "single", -1, SQLITE_STATIC);
        sqlite3_bind_int(ins, 2, i);
        if(sqlite3_step(ins) != SQLITE_DONE) { sqlite3_finalize(ins); return fail(db, "insert"); }
        sqlite3_reset(ins);
    }
    printf("%5d inserts, each committed: %6lu ms (%lu ms each)\n", n, ms_since(t0), n ? ms_since(t0) / (unsigned long)n : 0);
    t0 = cmm2->micros();
    if(exec(db, "BEGIN")) return 1;
    for(i = 0; i < 1000; i++) {
        sqlite3_bind_text(ins, 1, "batch", -1, SQLITE_STATIC);
        sqlite3_bind_int(ins, 2, n + i);
        if(sqlite3_step(ins) != SQLITE_DONE) { sqlite3_finalize(ins); return fail(db, "insert"); }
        sqlite3_reset(ins);
    }
    if(exec(db, "COMMIT")) return 1;
    sqlite3_finalize(ins);
    printf(" 1000 inserts, one commit:   %6lu ms\n", ms_since(t0));
    t0 = cmm2->micros();
    if(sqlite3_prepare_v2(db, "SELECT count(*), sum(n) FROM bench", -1, &get, NULL) != SQLITE_OK) return fail(db, "count");
    if(sqlite3_step(get) == SQLITE_ROW) { rows = sqlite3_column_int(get, 0); sum = sqlite3_column_int64(get, 1); }
    sqlite3_finalize(get);
    printf("count and sum (%d, %ld):    %6lu ms\n", rows, (long)sum, ms_since(t0));   /* (newlib-nano: no %lld) */
    t0 = cmm2->micros();
    if(sqlite3_prepare_v2(db, "SELECT name, n FROM bench ORDER BY n DESC", -1, &get, NULL) != SQLITE_OK) return fail(db, "sort");
    for(i = 0; sqlite3_step(get) == SQLITE_ROW; i++) {}
    sqlite3_finalize(get);
    printf("%5d rows sorted:             %6lu ms\n", i, ms_since(t0));
    t0 = cmm2->micros();
    if(exec(db, "CREATE INDEX bench_n ON bench(n)")) return 1;
    printf("index made:                  %6lu ms\n", ms_since(t0));
    t0 = cmm2->micros();
    if(sqlite3_prepare_v2(db, "SELECT name FROM bench WHERE n = ?", -1, &get, NULL) != SQLITE_OK) return fail(db, "find");
    for(i = 0; i < 1000; i++) {
        sqlite3_bind_int(get, 1, (i * 7919) % (n + 1000));
        if(sqlite3_step(get) != SQLITE_ROW) { sqlite3_finalize(get); return fail(db, "find"); }
        sqlite3_reset(get);
    }
    sqlite3_finalize(get);
    printf(" 1000 rows found by index:   %6lu ms\n", ms_since(t0));
    printf("pool: %d KB, most used %ld KB\n", sqlcore_pool / 1024, (long)(sqlite3_memory_highwater(0) / 1024));
    return 0;
}

int main(int argc, char **argv) {
    sqlite3 *db = NULL;
    const char *name = NULL;
    char *sql = NULL, *pool;
    int a = 1, n = 100, r = 0, size, len, i, stack = 0;
    uint64_t t0;
    while(a < argc && argv[a][0] == '-' && strcmp(argv[a], "-bench")) {
        if(!strcmp(argv[a], "-h")) headers = 1;
        else if(!strcmp(argv[a], "-t")) timing = 1;
        else if(!strcmp(argv[a], "-log")) logging = 1;
#ifdef SQL_STACK
        else if(!strcmp(argv[a], "-stack")) stack = 1;
#endif
        else { fprintf(stderr, "sql: %s is not an option\n", argv[a]); return 1; }
        a++;
    }
    if(a >= argc) {
        fprintf(stderr, "RUN \"sql.app\", \"[-h] [-t] [-stack] database statement; ... | @file.sql | -bench [n]\"\n");
        return 1;
    }
    name = argv[a++];
    if(!sqlcore_name_ok(name)) {
        fprintf(stderr, "sql: %s: a database must be on B: or C:\n", name);
        return 1;
    }
#ifdef SQL_STACK
    if(stack) stack_mark();
#endif
    size = sqlcore_pool_for(cmm2_start->heap_size);
    if((pool = malloc((size_t)size)) == NULL) { fprintf(stderr, "sql: no room for a %d KB pool\n", size / 1024); return 1; }
    if(logging) sqlite3_config(SQLITE_CONFIG_LOG, logged, NULL);   /* (before SQLite starts) */
    if((r = sqlcore_init(pool, size, 0)) != SQLITE_OK) { fprintf(stderr, "sql: SQLite would not start (%d)\n", r); return 1; }
    t0 = cmm2->micros();
    if(sqlite3_open_v2(name, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        r = fail(db, name);
        sqlite3_close(db);
        return r;
    }
    sqlcore_watch(db);
    if(a < argc && !strcmp(argv[a], "-bench")) {
        if(a + 1 < argc) n = atoi(argv[a + 1]);
        exec(db, "SELECT count(*) FROM sqlite_schema");   /* (the schema read: what opening costs) */
        printf("opened:                      %6lu ms\n", ms_since(t0));
        r = bench(db, n);
    } else if(a < argc && argv[a][0] == '@') {
        if((sql = slurp(argv[a] + 1)) == NULL) { fprintf(stderr, "sql: cannot read %s\n", argv[a] + 1); r = 1; }
        else r = run(db, sql);
    } else {                                /* the words that are left, a space between */
        for(len = 1, i = a; i < argc; i++) len += (int)strlen(argv[i]) + 1;
        if((sql = malloc((size_t)len)) == NULL) r = 1;
        else {
            sql[0] = 0;
            for(i = a; i < argc; i++) { if(i > a) strcat(sql, " "); strcat(sql, argv[i]); }
            r = run(db, sql);
        }
    }
    free(sql);
    if(sqlite3_close(db) != SQLITE_OK) r = fail(db, "close");
    if(timing) printf("(%lu ms in all)\n", ms_since(t0));
#ifdef SQL_STACK
    if(stack) printf("stack: %u bytes used of %u\n", stack_used(), (unsigned)SQL_STACK);
#endif
    return r;
}
