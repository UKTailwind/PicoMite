/* sql_mod.c: the SQL module, sql.mod: SQLite (sqlcore.c) behind the firmware's SQL command. MIT License.

   Built with build.py --module (make.py), so it is loaded into MMBasic's heap beside the BASIC program by the first
   SQL OPEN and kept until the heap goes (RUN, NEW, CHAIN: forget). Its functions are sqlmod.h's, each a thin layer
   over SQLite's own: the firmware does the BASIC side (the values, the arrays and the structures), this the
   database's. One database is open at a time; ATTACH reaches others through it.

   PicoMite: the Colour Maximite 2's apps/sql/sql_mod.c (62a6e5b), unchanged but for sqlmod.h's place (loader/). */
#include <stdlib.h>
#include <string.h>
#include "cmm2.h"
#include "sqlcore.h"
#include "../../loader/sqlmod.h"

static sqlite3 *db;
static int started;                                     /* SQLite started, with its pool */
static char openmsg[128];                               /* why the last open failed (its handle is gone) */

static void shut(void) {                                /* the database closed, its statements finalised first */
    sqlite3_stmt *s;
    if(db == NULL) return;
    while((s = sqlite3_next_stmt(db, NULL)) != NULL) sqlite3_finalize(s);
    sqlite3_close(db);
    db = NULL;
}

static void m_forget(void) { shut(); }

static int m_open(const char *name, uint32_t pool, uint32_t cache) {
    int r;
    shut();
    if(!started) {
        void *p = malloc(pool);
        if(p == NULL) return SQLITE_NOMEM;
        if((r = sqlcore_init(p, (int)pool, (int)cache)) != SQLITE_OK) { free(p); return r; }
        started = 1;
    }
    sqlcore_broken = 0;
    openmsg[0] = 0;
    if((r = sqlite3_open_v2(name, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) != SQLITE_OK) {
        strncpy(openmsg, sqlite3_errmsg(db), sizeof(openmsg) - 1);
        sqlite3_close(db);
        db = NULL;
        return r;
    }
    sqlcore_watch(db);
    return SQLITE_OK;
}

static int m_close(void) { shut(); return SQLITE_OK; }

static const char *m_errmsg(void) {
    if(sqlcore_broken) return "stopped by Ctrl-C";
    return db ? sqlite3_errmsg(db) : openmsg[0] ? openmsg : "no database is open";
}

static int m_prepare(const char *sql, void **stmt, const char **tail) {
    sqlcore_broken = 0;
    return sqlite3_prepare_v2(db, sql, -1, (sqlite3_stmt **)stmt, tail);
}

static int m_bind_int(void *s, int i, int64_t v) { return sqlite3_bind_int64(s, i, v); }
static int m_bind_double(void *s, int i, double v) { return sqlite3_bind_double(s, i, v); }
static int m_bind_text(void *s, int i, const char *t, int n) { return sqlite3_bind_text(s, i, t, n, SQLITE_TRANSIENT); }
static int m_bind_null(void *s, int i) { return sqlite3_bind_null(s, i); }
static int m_bind_count(void *s) { return sqlite3_bind_parameter_count(s); }
static const char *m_bind_name(void *s, int i) { return sqlite3_bind_parameter_name(s, i); }
static int m_step(void *s) { sqlcore_broken = 0; return sqlite3_step(s); }
static int m_reset(void *s) { return sqlite3_reset(s); }
static int m_finalize(void *s) { return sqlite3_finalize(s); }
static int m_columns(void *s) { return sqlite3_column_count(s); }
static int m_column_type(void *s, int i) { return sqlite3_column_type(s, i); }
static int64_t m_column_int(void *s, int i) { return sqlite3_column_int64(s, i); }
static double m_column_double(void *s, int i) { return sqlite3_column_double(s, i); }
static const char *m_column_text(void *s, int i, int *n) {
    const char *t = sqlite3_column_type(s, i) == SQLITE_BLOB ? (const char *)sqlite3_column_blob(s, i)
                                                             : (const char *)sqlite3_column_text(s, i);
    *n = sqlite3_column_bytes(s, i);                    /* (after the text or blob: SQLite's rule) */
    return t ? t : "";
}
static const char *m_column_name(void *s, int i) { return sqlite3_column_name(s, i); }
static const char *m_column_decltype(void *s, int i) { return sqlite3_column_decltype(s, i); }
static int64_t m_changes(void) { return db ? sqlite3_changes64(db) : 0; }
static int64_t m_last_id(void) { return db ? sqlite3_last_insert_rowid(db) : 0; }

static const sqlmod table = {
    { CMM2MOD_MAGIC, SQLMOD_VERSION, m_forget },
    m_open, m_close, m_errmsg, m_prepare,
    m_bind_int, m_bind_double, m_bind_text, m_bind_null, m_bind_count, m_bind_name,
    m_step, m_reset, m_finalize,
    m_columns, m_column_type, m_column_int, m_column_double, m_column_text, m_column_name, m_column_decltype,
    m_changes, m_last_id
};

const void *module_main(void) { return &table; }
