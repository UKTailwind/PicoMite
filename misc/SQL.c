/***************************************************************************
PicoMite MMBasic

SQL.c

The SQL command and the SQL( function: an SQL database (SQLite) for BASIC programs, as on the Colour Maximite 2 (its
Src/SQL.c, 62a6e5b: the same commands, so that a program runs on either). SQLite is the module sql.mod (apps/sql:
sql_mod.c over sqlcore.c), loaded into the heap by the first SQL OPEN and kept until the heap goes (RUN, NEW, CHAIN:
Memory.c's InitHeap, Loader.c's LoaderSqlDrop). It runs on a stack of its own (Loader.c's LoaderSqlCall). This file is
the BASIC side: the commands, the values bound to a statement's parameters, the rows into variables, arrays and
structures. Only in the builds with the loader (CMakeLists.txt LOADER_VARIANTS): the PicoMitePC3 and PicoMiteHDMIUSB.

    SQL OPEN file$ [, poolKB [, cacheKB]]   the database (made if it is not there), on B: or C: (a database on A:, the
                                    flash's littlefs, is refused by the module: each commit would rewrite most of it),
                                    the module loaded for it from A:/sql/sql.mod or else B:/sql/sql.mod: its pool 2 MB
                                    of the heap unless poolKB says, an eighth of it (at most 256 KB) the page cache
                                    unless cacheKB says. One database at a time: an OPEN closes the last one
    SQL CLOSE                       the database closed and the module unloaded (its memory given back); SQL CLOSE QUERY
                                    ends a query before its end (its memory and its tables let go)
    SQL EXEC sql$ [, value ...]     statements, the values bound to their parameters (? and ?n by place, :name from a
                                    structure given among the values); rows a statement gives are not kept. A statement
                                    that finds its table held by a query not read to its end ends that query and runs
    SQL QUERY sql$ [, value ...]    one statement, kept as the cursor: SQL(NEXT) steps it
    SQL CREATE type [, table$]      CREATE TABLE IF NOT EXISTS from a TYPE (the table named after the type)
    SQL INSERT var [, table$]       a structure, or a whole array of them var(), inserted by its members' names
    SQL(NEXT)                       1 if the cursor is on its next row, 0 when it has none (the query then ends at once)
    SQL(n)                          the row's column n (from 1): an integer, a float or a string as the column holds it
    SQL(FETCH var)                  as NEXT, the row put into the structure var by its members' names
    SQL(SELECT sql$, array() [, value ...])   the rows into a 1 or 2 dimensional array (row, column), or an array of
                                    structures: how many it held
    SQL(COLUMNS), SQL(NAME n), SQL(ISNULL n), SQL(ROWS), SQL(ID)
    SQL(TYPE n)                     column n's type in this row: 1 INTEGER, 2 FLOAT, 3 TEXT, 4 BLOB, 5 NULL (SQLite's)
    SQL(LONGSTRING n, a%())         column n into the LONGSTRING a%(): its length (text of any length; SQL(n) has 255)

A LONGSTRING a%() can also be a statement (sql$), and a value after one (its text bound). Ctrl-C (the break key) in a
statement, or between the statements or rows of one SQL command, is the error "SQL: stopped by Ctrl-C", which ON ERROR
takes, and not a break as well.

Copyright 2026 Peter Mather. (The firmware's licence: see the other files.)
***************************************************************************/
#ifdef LOADER
#include <string.h>
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "Loader.h"
#include "loader/sqlmod.h"

static const char *const sqlmodfiles[] = {"A:/sql/sql.mod", "B:/sql/sql.mod"};   // where SQL OPEN looks, in turn
#define SQL_POOL_KB  2048                       // SQLite's pool unless SQL OPEN says (sql.app's: 2 MB if there is room)
#define SQL_CLIB     0x10000                    // the module's C library's share of its heap, beside the pool
#define SQL_VALUES   20                         // values after a statement
#define SQL_FIELDS   64                         // a structure's members, nested ones and arrays' elements counted

static const sqlmod *sq;                        // the module's table while it is there (sqlmod_now() checks)
static int sqlopen;                             // a database is open
static void *sqlcur;                            // SQL QUERY's statement
static int sqlrow;                              // the cursor is on a row
static int sqldone;                             // the query has given all its rows (and is ended)
static int sqlinsavepoint;                      // an SQL INSERT of an array is in its savepoint

#define CALL(T, f) ((T)LoaderSqlFn((const void *)(f)))
#define U(s) ((unsigned char *)(s))

// the module's table, or NULL: forgotten with the heap, and the cursor and the open database with it
static const sqlmod *sqlmod_now(void)
{
    if (sq && LoaderSqlTable() != (const void *)sq)
    {
        sq = NULL;
        sqlopen = 0;
        sqlcur = NULL;
        sqlrow = 0;
        sqldone = 0;
        sqlinsavepoint = 0;
    }
    return sq;
}

static void sqlneedopen(void)
{
    if (!sqlmod_now() || !sqlopen)
        error("No database: SQL OPEN first");
}

static void sqlfinal(void *st)
{
    if (st)
        CALL(int (*)(void *), sq->finalize)(st);
}

// SQL QUERY's statement ended (finalised): a query not read to its end holds memory and its tables
static void sqlendquery(void)
{
    sqlfinal(sqlcur);
    sqlcur = NULL;
    sqlrow = 0;
    sqldone = 0;
}

// an SQL INSERT of an array stopped part way (an error, or Ctrl-C): its rows undone, as its one transaction, and the
// savepoint let go (left open, it kept every later change uncommitted, and SQL CLOSE then lost them)
static void sqlundo(void)
{
    static const char *const undo[] = {"ROLLBACK TO sql_insert", "RELEASE sql_insert"};
    int i;
    if (!sqlinsavepoint)
        return;
    sqlinsavepoint = 0;
    for (i = 0; i < 2; i++)
    {
        void *st = NULL;
        if (CALL(int (*)(const char *, void **, const char **), sq->prepare)(undo[i], &st, NULL) == 0 && st)
        {
            CALL(int (*)(void *), sq->step)(st);
            sqlfinal(st);
        }
    }
}

// an SQLite error as MMBasic's, the statement st finalised first: it never returns. A statement stopped by Ctrl-C (the
// module's poll saw MMBasic's break) is "SQL: stopped by Ctrl-C", and the break is taken by it: left set, it would end
// the program at the next statement, past ON ERROR
static void __attribute__((noreturn)) sqlfail(int rc, void *st)
{
    char m[STRINGSIZE];
    const char *e = CALL(const char *(*)(void), sq->errmsg)();
    strncpy(m, e ? e : "error", sizeof(m) - 1);
    m[sizeof(m) - 1] = 0;
    sqlfinal(st);
    if (st && st == sqlcur)
    {
        sqlcur = NULL;
        sqlrow = 0;
    }
    sqlundo();
    if (rc == SQLMOD_INTERRUPT)
        MMAbort = 0;
    error("SQL: $", m);
    for (;;)
    {
    } // (error() does not return)
}

// Ctrl-C between the statements or the rows of one SQL command: the same error as in a statement
static void __attribute__((noreturn)) sqlbroken(void)
{
    sqlundo();
    MMAbort = 0;
    error("SQL: stopped by Ctrl-C");
    for (;;)
    {
    }
}

// the LONGSTRING named by text p (the whole of it: name(), an integer array of one dimension): the array (its length
// in element 0, its bytes after); NULL if p is not an integer array
static int64_t *sqllong(unsigned char *p)
{
    unsigned char *e;
    int64_t *a;
    skipspace(p);
    if (!isnamestart(*p))
        return NULL;
    e = skipvar(p, false);
    skipspace(e);
    if (*e || e - p < 3 || e[-1] != ')' || e[-2] != '(')
        return NULL; // (an expression, or an element)
    if ((a = findvar(p, V_FIND | V_NOFIND_NULL | V_EMPTY_OK)) == NULL)
        return NULL;
    if ((VREC(g_VarIndex)->type & T_STRUCT) || !(VREC(g_VarIndex)->type & T_INT))
        return NULL;
    if (!DimIsAllocated(RAW_DIM((*VREC(g_VarIndex)), 0)) || !DimIsEnd(RAW_DIM((*VREC(g_VarIndex)), 1)))
        error("SQL: a LONGSTRING is an integer array of one dimension");
    if (a[0] < 0 || a[0] > (int64_t)(DimElements(RAW_DIM((*VREC(g_VarIndex)), 0)) - 1) * 8)
        error("SQL: not a LONGSTRING");
    return a;
}

// a statement's text from p: a string expression, or a LONGSTRING (a statement longer than 255 characters)
static char *sqltext(unsigned char *p)
{
    int64_t *a = sqllong(p);
    char *s;
    if (a == NULL)
        return (char *)getCstring(p);
    s = GetTempMemory((int)a[0] + 1);
    memcpy(s, a + 1, (size_t)a[0]);
    s[a[0]] = 0;
    return s;
}

/* ---- structures: a TYPE's members as columns ---- */
typedef struct
{
    char name[MAXVARLEN * 2 + 8];
    int type, offset, size;
} sqlfield;

// the members of TYPE st (nested ones flattened, PARENT_CHILD; an array member's elements NAME_1, NAME_2...) from
// fields[n], at offsets from base: the count
static int sqlfields(int st, int base, const char *prefix, sqlfield *f, int n)
{
    struct s_structdef *d = g_structtbl[st];
    int m, k, count, esize;
    for (m = 0; m < d->num_members; m++)
    {
        struct s_structmember *mb = &d->members[m];
        count = 1;
        if (DimIsAllocated(mb->dimtbl[0]))
        {
            if (!DimIsEnd(mb->dimtbl[1]))
                error("SQL: member $ has more than one dimension", mb->name);
            count = DimElements(mb->dimtbl[0]);
        }
        esize = mb->type == T_STR ? mb->size + 1 : mb->type == T_STRUCT ? g_structtbl[mb->size]->total_size : 8;
        for (k = 0; k < count; k++)
        {
            char nm[sizeof(f->name)];
            if (count > 1 || DimIsAllocated(mb->dimtbl[0]))
                snprintf(nm, sizeof(nm), "%s%s_%d", prefix, (char *)mb->name, k + g_OptionBase);
            else
                snprintf(nm, sizeof(nm), "%s%s", prefix, (char *)mb->name);
            if (mb->type == T_STRUCT)
            {
                strcat(nm, "_");
                n = sqlfields(mb->size, base + mb->offset + k * esize, nm, f, n);
            }
            else
            {
                if (n >= SQL_FIELDS)
                    error("SQL: more than % members", SQL_FIELDS);
                strcpy(f[n].name, nm);
                f[n].type = mb->type;
                f[n].offset = base + mb->offset + k * esize;
                f[n].size = mb->size;
                n++;
            }
        }
    }
    return n;
}

static int sqlfield_find(sqlfield *f, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcasecmp(f[i].name, name))
            return i;
    return -1;
}

// a structure variable named by text p (the whole of it: a name, name(), name(i) or name.member that is a structure):
// its data, its TYPE in *st, and in *count its elements if p is a whole array, name(); NULL if p is no such thing
static char *sqlstruct(unsigned char *p, int *st, int *count)
{
    unsigned char *e;
    char *v;
    int idx, whole;
    skipspace(p);
    if (!isnamestart(*p))
        return NULL;
    e = skipvar(p, false);
    skipspace(e);
    if (*e)
        return NULL; // (an expression, not a variable alone)
    whole = e - p >= 2 && e[-1] == ')' && e[-2] == '(';
    if ((v = findvar(p, V_FIND | V_NOFIND_NULL | V_EMPTY_OK)) == NULL)
        return NULL;
    idx = g_VarIndex;
    if (!(VREC(idx)->type & T_STRUCT))
        return NULL;
    if (g_StructMemberType != 0 && g_StructMemberType != T_STRUCT)
        return NULL; // (a member that is a number or string)
    *st = g_StructMemberType == T_STRUCT ? g_StructMemberSize : (int)VREC(idx)->size;
    *count = 1;
    if (whole)
    {
        if (!DimIsAllocated(RAW_DIM((*VREC(idx)), 0)) || !DimIsEnd(RAW_DIM((*VREC(idx)), 1)))
            error("SQL: a structure array of one dimension");
        *count = DimElements(RAW_DIM((*VREC(idx)), 0));
    }
    return v;
}

/* ---- values to parameters ---- */

// value i (from 1) of statement st from the expression text p, or the text of a LONGSTRING a%()
static void sqlbindone(void *st, int i, unsigned char *p)
{
    MMFLOAT f = 0;
    long long int n = 0;
    unsigned char *s = NULL;
    int t = T_NOTYPE, rc;
    int64_t *a = sqllong(p);
    if (a)
    {
        if ((rc = CALL(int (*)(void *, int, const char *, int), sq->bind_text)(st, i, (const char *)(a + 1), (int)a[0])))
            sqlfail(rc, st);
        return;
    }
    evaluate(p, &f, &n, &s, &t, false);
    if (t & T_STR)
        rc = CALL(int (*)(void *, int, const char *, int), sq->bind_text)(st, i, (const char *)s + 1, *s);
    else if (t & T_INT)
        rc = CALL(int (*)(void *, int, int64_t), sq->bind_int)(st, i, n);
    else
        rc = CALL(int (*)(void *, int, double), sq->bind_double)(st, i, f);
    if (rc)
        sqlfail(rc, st);
}

// a structure's member as the value of parameter i
static void sqlbindfield(void *st, int i, char *data, sqlfield *f, int id0null)
{
    int rc;
    char *v = data + f->offset;
    if (f->type == T_INT)
    {
        if (id0null && *(long long int *)v == 0)
            rc = CALL(int (*)(void *, int), sq->bind_null)(st, i);
        else
            rc = CALL(int (*)(void *, int, int64_t), sq->bind_int)(st, i, *(long long int *)v);
    }
    else if (f->type == T_NBR)
        rc = CALL(int (*)(void *, int, double), sq->bind_double)(st, i, *(MMFLOAT *)v);
    else
        rc = CALL(int (*)(void *, int, const char *, int), sq->bind_text)(st, i, v + 1, (unsigned char)*v);
    if (rc)
        sqlfail(rc, st);
}

// statement st's parameters from the n values in v: a structure among them gives the :name parameters (by its members'
// names), the others the rest in order
static void sqlbind(void *st, unsigned char **v, int n)
{
    int np = CALL(int (*)(void *), sq->bind_count)(st), i, k, next = 0, sti, cnt, nf = 0;
    char *sdata = NULL;
    sqlfield *f = NULL;
    for (k = 0; k < n; k++)
    { // (a structure: its members by name)
        char *d = sqlstruct(v[k], &sti, &cnt);
        if (d)
        {
            if (cnt != 1)
                error("SQL: one structure, not an array, for a statement's names");
            f = (sqlfield *)GetTempMemory(SQL_FIELDS * sizeof(sqlfield));
            nf = sqlfields(sti, 0, "", f, 0);
            sdata = d;
            v[k] = NULL;
        }
    }
    for (i = 1; i <= np; i++)
    {
        const char *name = CALL(const char *(*)(void *, int), sq->bind_name)(st, i);
        if (name && (name[0] == ':' || name[0] == '@' || name[0] == '$'))
        {
            int j = sdata ? sqlfield_find(f, nf, name + 1) : -1;
            if (j < 0)
            {
                sqlfinal(st);
                error("SQL: no value for $", name);
            }
            sqlbindfield(st, i, sdata, &f[j], 0);
        }
        else
        {
            while (next < n && v[next] == NULL)
                next++;
            if (next >= n)
            {
                sqlfinal(st);
                error("SQL: % values for % parameters", n - (sdata != NULL), np);
            }
            sqlbindone(st, i, v[next++]);
        }
    }
}

/* ---- rows to variables ---- */

// column c of statement st into a variable of type t at v (a string of at most size characters)
static void sqlput(void *st, int c, int t, char *v, int size)
{
    int ct = CALL(int (*)(void *, int), sq->column_type)(st, c);
    if (t == T_INT)
        *(long long int *)v = ct == SQLMOD_NULL ? 0 : CALL(int64_t (*)(void *, int), sq->column_int)(st, c);
    else if (t == T_NBR)
        *(MMFLOAT *)v = ct == SQLMOD_NULL ? 0 : CALL(double (*)(void *, int), sq->column_double)(st, c);
    else
    {
        int n = 0;
        const char *s = ct == SQLMOD_NULL ? "" : CALL(const char *(*)(void *, int, int *), sq->column_text)(st, c, &n);
        if (n > size)
        { // (SQL QUERY's cursor stays on its row, so that the column can be read again by SQL(LONGSTRING); SQL(SELECT's
            if (st != sqlcur) // own statement is ended)
                sqlfinal(st);
            error("SQL: a string of % characters, longer than %", n, size);
        }
        memcpy(v + 1, s, n);
        *v = n;
    }
}

// the row of st into the structure of TYPE sti at d, by the columns' names; members without a column cleared
static void sqlrowstruct(void *st, char *d, int sti)
{
    sqlfield *f = (sqlfield *)GetTempMemory(SQL_FIELDS * sizeof(sqlfield));
    int nf = sqlfields(sti, 0, "", f, 0), nc = CALL(int (*)(void *), sq->columns)(st), c, j;
    memset(d, 0, g_structtbl[sti]->total_size);
    for (c = 0; c < nc; c++)
    {
        const char *name = CALL(const char *(*)(void *, int), sq->column_name)(st, c);
        if (name && (j = sqlfield_find(f, nf, name)) >= 0)
            sqlput(st, c, f[j].type, d + f[j].offset, f[j].size);
    }
}

/* ---- the command ---- */

// the statements of sql run, each with the values in v bound to its parameters, rows not kept
static void sqlrun(const char *sql, unsigned char **v, int n)
{
    const char *tail = sql;
    while (*tail)
    {
        void *st = NULL;
        int rc = CALL(int (*)(const char *, void **, const char **), sq->prepare)(tail, &st, &tail);
        if (rc)
            sqlfail(rc, st);
        if (st == NULL)
            continue;
        sqlbind(st, v, n);
        while ((rc = CALL(int (*)(void *), sq->step)(st)) == SQLMOD_ROW)
        {
        }
        if (rc == SQLMOD_LOCKED && sqlcur)
        {                  // a query not read to its end still holds its table (a DROP or CREATE INDEX on it): the
            sqlendquery(); // query ends, and the statement is tried again
            CALL(int (*)(void *), sq->reset)(st);
            while ((rc = CALL(int (*)(void *), sq->step)(st)) == SQLMOD_ROW)
            {
            }
        }
        if (rc != SQLMOD_DONE)
            sqlfail(rc, st);
        sqlfinal(st);
        if (MMAbort && *tail)
            sqlbroken(); // (Ctrl-C with statements still to run)
    }
}

// SQL CREATE: the CREATE TABLE for TYPE sti
static void sqlcreate(int sti, char *table)
{
    sqlfield *f = (sqlfield *)GetTempMemory(SQL_FIELDS * sizeof(sqlfield));
    int nf = sqlfields(sti, 0, "", f, 0), i;
    char *s = GetTempMemory(SQL_FIELDS * (sizeof(f->name) + 24) + 128);
    sprintf(s, "CREATE TABLE IF NOT EXISTS \"%s\"(", table);
    for (i = 0; i < nf; i++)
        sprintf(s + strlen(s), "%s\"%s\" %s", i ? ", " : "", f[i].name,
                f[i].type == T_INT ? (!strcasecmp(f[i].name, "ID") ? "INTEGER PRIMARY KEY" : "INTEGER") : f[i].type == T_NBR ? "REAL"
                                                                                                                          : "TEXT");
    strcat(s, ")");
    sqlrun(s, NULL, 0);
}

// SQL INSERT: count structures of TYPE sti from d, in one savepoint (undone if it stops part way: sqlundo); an ID member
// that is 0 given SQLite's new key
static void sqlinsert(char *d, int sti, int count, char *table)
{
    sqlfield *f = (sqlfield *)GetTempMemory(SQL_FIELDS * sizeof(sqlfield));
    int nf = sqlfields(sti, 0, "", f, 0), i, k, rc, id = -1, size = g_structtbl[sti]->total_size;
    char *s = GetTempMemory(SQL_FIELDS * (sizeof(f->name) + 8) + 128);
    void *st = NULL;
    sprintf(s, "INSERT INTO \"%s\"(", table);
    for (i = 0; i < nf; i++)
    {
        sprintf(s + strlen(s), "%s\"%s\"", i ? ", " : "", f[i].name);
        if (f[i].type == T_INT && !strcasecmp(f[i].name, "ID"))
            id = i;
    }
    strcat(s, ") VALUES(");
    for (i = 0; i < nf; i++)
        strcat(s, i ? ", ?" : "?");
    strcat(s, ")");
    if (count > 1)
    {
        sqlrun("SAVEPOINT sql_insert", NULL, 0);
        sqlinsavepoint = 1;
    }
    rc = CALL(int (*)(const char *, void **, const char **), sq->prepare)(s, &st, NULL);
    if (rc || st == NULL)
        sqlfail(rc, st);
    for (k = 0; k < count; k++)
    {
        char *e = d + k * size;
        for (i = 0; i < nf; i++)
            sqlbindfield(st, i + 1, e, &f[i], i == id);
        if ((rc = CALL(int (*)(void *), sq->step)(st)) != SQLMOD_DONE)
            sqlfail(rc, st);
        if (id >= 0 && *(long long int *)(e + f[id].offset) == 0)
            *(long long int *)(e + f[id].offset) = CALL(int64_t (*)(void), sq->last_id)();
        CALL(int (*)(void *), sq->reset)(st);
        if (MMAbort && k + 1 < count)
        { // (Ctrl-C with rows still to go: none of them kept)
            sqlfinal(st);
            sqlbroken();
        }
    }
    sqlfinal(st);
    if (count > 1)
    {
        sqlinsavepoint = 0;
        sqlrun("RELEASE sql_insert", NULL, 0);
    }
}

// the database closed and the module unloaded: its memory given back
static void sqlclose(void)
{
    if (sqlmod_now())
    {
        if (sqlopen)
        {
            sqlendquery();
            CALL(int (*)(void), sq->close)();
        }
        LoaderSqlDrop(); // (a USER module stays)
    }
    sq = NULL;
    sqlopen = 0;
    sqldone = 0;
    sqlinsavepoint = 0;
}

// the module's file: A:/sql/sql.mod, else B:/sql/sql.mod
static char *sqlmodfile(void)
{
    unsigned i;
    for (i = 0; i < sizeof(sqlmodfiles) / sizeof(sqlmodfiles[0]); i++)
        if (ExistsFile((char *)sqlmodfiles[i]) == 1)
            return (char *)sqlmodfiles[i];
    error("SQL: sql.mod is not in A:/sql or B:/sql");
    return NULL;
}

void cmd_sql(void)
{
    unsigned char *p;
    if ((p = checkstring(cmdline, U("OPEN"))))
    {
        char name[STRINGSIZE];
        uint32_t pool = SQL_POOL_KB * 1024, cache = 0, heap;
        int rc;
        getargs(&p, 5, U(","));
        if (argc != 1 && argc != 3 && argc != 5)
            error("Argument count");
        strcpy(name, (char *)getCstring(argv[0]));
        if (name[0] && strcmp(name, ":memory:"))
        { // B: or C: (the module refuses any other drive: this says why)
            int drive = name[1] == ':' ? mytoupper(name[0]) - 'A' : FatFSFileSystem;
            if (drive != 1 && drive != 2)
                error("SQL: a database must be on B: or C:");
        }
        if (argc >= 3 && *argv[2])
            pool = getint(argv[2], 256, 16384) * 1024;
        if (argc == 5)
            cache = getint(argv[4], 64, 16384) * 1024;
        if (cache >= pool)
            error("SQL: a cache of % KB in a pool of % KB", (int)(cache / 1024), (int)(pool / 1024));
        sqlclose(); // (one at a time: the last one closed)
        sq = (const sqlmod *)LoaderSqlLoad(sqlmodfile(), pool + SQL_CLIB);
        if (sq->head.version != SQLMOD_VERSION)
        {
            sqlclose();
            error("SQL: sql.mod is not this firmware's");
        }
        heap = LoaderSqlHeapSize();
        pool = heap - SQL_CLIB;
        rc = CALL(int (*)(const char *, uint32_t, uint32_t), sq->open)(name, pool, cache);
        if (rc)
        {
            char m[STRINGSIZE];
            strncpy(m, CALL(const char *(*)(void), sq->errmsg)(), sizeof(m) - 1);
            m[sizeof(m) - 1] = 0;
            sqlclose();
            error("SQL: $", m);
        }
        sqlopen = 1;
    }
    else if ((p = checkstring(cmdline, U("CLOSE"))))
    {
        if (checkstring(p, U("QUERY")))
        { // SQL CLOSE QUERY: the query ended before its end (its memory and its tables let go)
            if (sqlmod_now() && sqlopen)
                sqlendquery();
            return;
        }
        sqlclose();
    }
    else if ((p = checkstring(cmdline, U("EXEC"))))
    {
        getargs(&p, SQL_VALUES * 2 + 1, U(","));
        if (argc < 1 || !(argc & 1))
            error("Argument count");
        sqlneedopen();
        char *sql = sqltext(argv[0]);
        unsigned char *v[SQL_VALUES];
        int i, n = argc / 2;
        for (i = 0; i < n; i++)
            v[i] = argv[2 + 2 * i];
        sqlrun(sql, v, n);
    }
    else if ((p = checkstring(cmdline, U("QUERY"))))
    {
        getargs(&p, SQL_VALUES * 2 + 1, U(","));
        if (argc < 1 || !(argc & 1))
            error("Argument count");
        sqlneedopen();
        char *sql = sqltext(argv[0]);
        unsigned char *v[SQL_VALUES];
        const char *tail = NULL;
        int i, n = argc / 2, rc;
        void *st = NULL;
        for (i = 0; i < n; i++)
            v[i] = argv[2 + 2 * i];
        sqlendquery();
        rc = CALL(int (*)(const char *, void **, const char **), sq->prepare)(sql, &st, &tail);
        if (rc)
            sqlfail(rc, st);
        if (st == NULL)
            error("SQL: no statement");
        while (tail && (*tail == ' ' || *tail == ';' || *tail == '\t'))
            tail++;
        if (tail && *tail)
        {
            sqlfinal(st);
            error("SQL QUERY: one statement");
        }
        sqlbind(st, v, n);
        sqlcur = st;
    }
    else if ((p = checkstring(cmdline, U("CREATE"))))
    {
        char table[MAXVARLEN + 1];
        int sti;
        getargs(&p, 3, U(","));
        if (argc != 1 && argc != 3)
            error("Argument count");
        sqlneedopen();
        unsigned char *t = argv[0];
        skipspace(t);
        if ((sti = FindStructType(t)) < 0)
            error("SQL: no TYPE $", t);
        snprintf(table, sizeof(table), "%s", argc == 3 ? (char *)getCstring(argv[2]) : (char *)g_structtbl[sti]->name);
        sqlcreate(sti, table);
    }
    else if ((p = checkstring(cmdline, U("INSERT"))))
    {
        char table[STRINGSIZE], *d;
        int sti, count;
        getargs(&p, 3, U(","));
        if (argc != 1 && argc != 3)
            error("Argument count");
        sqlneedopen();
        if ((d = sqlstruct(argv[0], &sti, &count)) == NULL)
            error("SQL INSERT: a structure, or an array of them");
        strcpy(table, argc == 3 ? (char *)getCstring(argv[2]) : (char *)g_structtbl[sti]->name);
        sqlinsert(d, sti, count, table);
    }
    else
        error("Syntax");
}

/* ---- the function ---- */

// 1 if the declared type dt (any case) has the word w (capitals) in it
static int sqlhas(const char *dt, const char *w)
{
    int n = strlen(w);
    if (dt == NULL)
        return 0;
    for (; *dt; dt++)
        if (!strncasecmp(dt, w, n))
            return 1;
    return 0;
}

static void sqlneedrow(void)
{
    sqlneedopen();
    if (!sqlcur || !sqlrow)
        error("SQL: no row (SQL QUERY, then SQL(NEXT))");
}

// the cursor's next row: 1, or 0 when there is none: the query is then ended at once (its memory and its tables let
// go), and SQL(NEXT) gives 0 again until the next SQL QUERY
static int sqlnext(void)
{
    int rc;
    sqlneedopen();
    if (!sqlcur)
    {
        if (sqldone)
            return 0;
        error("SQL: no query (SQL QUERY first)");
    }
    rc = CALL(int (*)(void *), sq->step)(sqlcur);
    if (rc == SQLMOD_ROW)
        return sqlrow = 1;
    sqlrow = 0;
    if (rc != SQLMOD_DONE)
    {
        void *st = sqlcur;
        sqlcur = NULL;
        sqlfail(rc, st);
    }
    sqlfinal(sqlcur);
    sqlcur = NULL;
    sqldone = 1;
    return 0;
}

void fun_sql(void)
{
    unsigned char *p;
    int nc;
    int rtype = T_INT; // the result's type, made targ at the end: an argument's expression (a string's +)
                       // sets targ as it is evaluated
    if ((p = checkstring(ep, U("NEXT"))))
    {
        iret = sqlnext();
    }
    else if ((p = checkstring(ep, U("FETCH"))))
    {
        int sti, count;
        char *d = sqlstruct(p, &sti, &count);
        if (d == NULL || count != 1)
            error("SQL(FETCH): a structure");
        if ((iret = sqlnext()))
            sqlrowstruct(sqlcur, d, sti);
    }
    else if ((p = checkstring(ep, U("SELECT"))))
    {
        getargs(&p, SQL_VALUES * 2 + 3, U(","));
        if (argc < 3 || !(argc & 1))
            error("Argument count");
        sqlneedopen();
        char *sql = sqltext(argv[0]), *a;
        unsigned char *v[SQL_VALUES];
        const char *tail = NULL;
        int i, n = argc / 2 - 1, rc = SQLMOD_DONE, rows, cols = 1, sti = 0, count, idx, t, size, nr = 0;
        void *st = NULL;
        for (i = 0; i < n; i++)
            v[i] = argv[4 + 2 * i];
        if ((a = sqlstruct(argv[2], &sti, &count)) != NULL)
        {
            if (count < 1)
                error("SQL(SELECT): an array");
            rows = count;
            t = T_STRUCT;
            size = g_structtbl[sti]->total_size;
        }
        else
        {
            a = findvar(argv[2], V_FIND | V_NOFIND_ERR | V_EMPTY_OK);
            idx = g_VarIndex;
            t = VREC(idx)->type & (T_INT | T_NBR | T_STR);
            if (!t || !DimIsAllocated(RAW_DIM((*VREC(idx)), 0)))
                error("SQL(SELECT): an array");
            rows = DimElements(RAW_DIM((*VREC(idx)), 0));
            if (!DimIsEnd(RAW_DIM((*VREC(idx)), 1)))
            {
                if (!DimIsEnd(RAW_DIM((*VREC(idx)), 2)))
                    error("SQL(SELECT): one or two dimensions");
                cols = DimElements(RAW_DIM((*VREC(idx)), 1));
            }
            size = t == T_STR ? VREC(idx)->size + 1 : 8;
        }
        rc = CALL(int (*)(const char *, void **, const char **), sq->prepare)(sql, &st, &tail);
        if (rc)
            sqlfail(rc, st);
        if (st == NULL)
            error("SQL: no statement");
        sqlbind(st, v, n);
        nc = CALL(int (*)(void *), sq->columns)(st);
        while (nr < rows && (rc = CALL(int (*)(void *), sq->step)(st)) == SQLMOD_ROW)
        {
            if (t == T_STRUCT)
                sqlrowstruct(st, a + nr * size, sti);
            else
                for (i = 0; i < cols && i < nc; i++)
                    sqlput(st, i, t, a + (nr + i * rows) * size, size - 1); // (a(row, col): the first index fastest)
            nr++;
        }
        if (nr < rows && rc != SQLMOD_DONE)
            sqlfail(rc, st);
        sqlfinal(st);
        iret = nr;
    }
    else if ((p = checkstring(ep, U("COLUMNS"))))
    {
        sqlneedopen();
        iret = sqlcur ? CALL(int (*)(void *), sq->columns)(sqlcur) : 0;
    }
    else if ((p = checkstring(ep, U("NAME"))))
    {
        sqlneedopen();
        if (!sqlcur)
            error("SQL: no query (SQL QUERY first)");
        nc = CALL(int (*)(void *), sq->columns)(sqlcur);
        int c = getint(p, 1, nc);
        const char *s = CALL(const char *(*)(void *, int), sq->column_name)(sqlcur, c - 1);
        sret = GetTempMemory(STRINGSIZE);
        strncpy((char *)sret, s ? s : "", MAXSTRLEN);
        CtoM(sret);
        rtype = T_STR;
    }
    else if ((p = checkstring(ep, U("ISNULL"))))
    {
        sqlneedrow();
        nc = CALL(int (*)(void *), sq->columns)(sqlcur);
        int c = getint(p, 1, nc);
        iret = CALL(int (*)(void *, int), sq->column_type)(sqlcur, c - 1) == SQLMOD_NULL;
    }
    else if ((p = checkstring(ep, U("TYPE"))))
    { // SQL(TYPE n): what SQLite holds in column n of this row, sqlite3_column_type()'s numbers: 1 INTEGER, 2 FLOAT,
        sqlneedrow(); // 3 TEXT, 4 BLOB, 5 NULL. SQL(n) gives a BLOB as a string and 2.0 as 2
        nc = CALL(int (*)(void *), sq->columns)(sqlcur);
        int c = getint(p, 1, nc);
        iret = CALL(int (*)(void *, int), sq->column_type)(sqlcur, c - 1);
    }
    else if ((p = checkstring(ep, U("ROWS"))))
    {
        sqlneedopen();
        iret = CALL(int64_t (*)(void), sq->changes)();
    }
    else if ((p = checkstring(ep, U("ID"))))
    {
        sqlneedopen();
        iret = CALL(int64_t (*)(void), sq->last_id)();
    }
    else if ((p = checkstring(ep, U("LONGSTRING"))))
    { // SQL(LONGSTRING n, a%()): column n as text of any length into the LONGSTRING a%(): its length (SQL(n) stops at
        int64_t *dest; // 255 characters)
        const char *s = "";
        int c, room, len = 0;
        getargs(&p, 3, U(","));
        if (argc != 3)
            error("Argument count");
        sqlneedrow();
        nc = CALL(int (*)(void *), sq->columns)(sqlcur);
        c = getint(argv[0], 1, nc) - 1;
        room = (parseintegerarray(argv[2], &dest, 2, 1, NULL, true, NULL) - 1) * 8;
        if (CALL(int (*)(void *, int), sq->column_type)(sqlcur, c) != SQLMOD_NULL)
            s = CALL(const char *(*)(void *, int, int *), sq->column_text)(sqlcur, c, &len);
        if (len > room)
            error("SQL: a string of % characters, longer than the LONGSTRING's %", len, room);
        memcpy(dest + 1, s, len);
        dest[0] = len;
        iret = len;
    }
    else
    { // SQL(n): column n as its type
        sqlneedrow();
        nc = CALL(int (*)(void *), sq->columns)(sqlcur);
        int c = getint(ep, 1, nc) - 1, ct = CALL(int (*)(void *, int), sq->column_type)(sqlcur, c);
        if (ct == SQLMOD_NULL)
        { // (a NULL: 0, 0.0 or "" as the column is declared, by SQLite's own affinity rules, its words)
            const char *dt = CALL(const char *(*)(void *, int), sq->column_decltype)(sqlcur, c);
            ct = SQLMOD_INTEGER;
            if (sqlhas(dt, "CHAR") || sqlhas(dt, "TEXT") || sqlhas(dt, "CLOB"))
                ct = SQLMOD_TEXT;
            else if (sqlhas(dt, "REAL") || sqlhas(dt, "FLOA") || sqlhas(dt, "DOUB"))
                ct = SQLMOD_FLOAT;
            if (ct == SQLMOD_TEXT)
            {
                sret = GetTempMemory(STRINGSIZE);
                rtype = T_STR;
            }
            else if (ct == SQLMOD_FLOAT)
            {
                fret = 0;
                rtype = T_NBR;
            }
            else
                iret = 0;
        }
        else if (ct == SQLMOD_INTEGER)
            iret = CALL(int64_t (*)(void *, int), sq->column_int)(sqlcur, c);
        else if (ct == SQLMOD_FLOAT)
        {
            fret = CALL(double (*)(void *, int), sq->column_double)(sqlcur, c);
            rtype = T_NBR;
        }
        else
        {
            sret = GetTempMemory(STRINGSIZE);
            sqlput(sqlcur, c, T_STR, (char *)sret, MAXSTRLEN);
            rtype = T_STR;
        }
    }
    targ = rtype;
}
#endif
