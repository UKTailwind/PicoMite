/* sqlmod.h: the SQL module's functions (apps/sql/sql_mod.c, SQLite), as the firmware's SQL command calls them
   (misc/SQL.c). MIT License, as pmimg.h, so that the module, whatever its licence, can include it.

   The Colour Maximite 2's Inc/sqlmod.h (SQLMOD_VERSION 2), the same table, so that sql_mod.c builds for either. The
   module is a PMIMG_KIND_MODULE image, sql.mod, loaded by the first SQL OPEN and kept while the heap lasts. The
   firmware calls each function through LoaderSqlFn (misc/Loader.c), on the module's own stack: so each takes at most
   four words of arguments, all in registers (the PicoMite's ABI is softfp: a double is two of them). A statement is
   SQLite's sqlite3_stmt, kept opaque here; the codes are SQLite's (SQLITE_OK 0, SQLITE_INTERRUPT 9, SQLITE_ROW 100,
   SQLITE_DONE 101), and so are the column types (1 integer, 2 float, 3 text, 4 blob, 5 null). */
#ifndef SQLMOD_H
#define SQLMOD_H
#include <stdint.h>
#include "pmimg.h"

#define SQLMOD_VERSION     2                   /* (2: open takes the page cache's size) */
#define SQLMOD_OK          0
#define SQLMOD_LOCKED      6                   /* a table another statement of this connection holds */
#define SQLMOD_INTERRUPT   9
#define SQLMOD_ROW         100
#define SQLMOD_DONE        101
#define SQLMOD_INTEGER     1
#define SQLMOD_FLOAT       2
#define SQLMOD_TEXT        3
#define SQLMOD_BLOB        4
#define SQLMOD_NULL        5

typedef struct {
    cmm2mod_head head;                                  /* CMM2MOD_MAGIC, SQLMOD_VERSION, forget (the database closed) */
    int          (*open)(const char *name, uint32_t pool, uint32_t cache);   /* the database (made if it is not
                                                           there), the one open; the first open starts SQLite with a pool
                                                           of pool bytes, cache of them a page cache of its own (0: an
                                                           eighth of the pool, at most 256 KB) */
    int          (*close)(void);                        /* its statements finalised too */
    const char  *(*errmsg)(void);                       /* the last error's message ("stopped by Ctrl-C" for 9) */
    int          (*prepare)(const char *sql, void **stmt, const char **tail);   /* *stmt NULL: only spaces or a
                                                           comment; *tail the text after the statement */
    int          (*bind_int)(void *stmt, int i, int64_t v);     /* parameters from 1 */
    int          (*bind_double)(void *stmt, int i, double v);
    int          (*bind_text)(void *stmt, int i, const char *s, int n);   /* copied */
    int          (*bind_null)(void *stmt, int i);
    int          (*bind_count)(void *stmt);
    const char  *(*bind_name)(void *stmt, int i);       /* ":name" (with its first character), or NULL for a "?" */
    int          (*step)(void *stmt);                   /* SQLMOD_ROW, SQLMOD_DONE or an error */
    int          (*reset)(void *stmt);
    int          (*finalize)(void *stmt);
    int          (*columns)(void *stmt);
    int          (*column_type)(void *stmt, int i);     /* columns from 0 */
    int64_t      (*column_int)(void *stmt, int i);
    double       (*column_double)(void *stmt, int i);
    const char  *(*column_text)(void *stmt, int i, int *n);   /* the bytes (not ended by a 0 if a blob), n of them */
    const char  *(*column_name)(void *stmt, int i);
    const char  *(*column_decltype)(void *stmt, int i); /* the declared type ("INTEGER", "TEXT"...), or NULL */
    int64_t      (*changes)(void);                      /* rows the last INSERT, UPDATE or DELETE changed */
    int64_t      (*last_id)(void);                      /* the last row inserted's ROWID */
} sqlmod;

#endif
