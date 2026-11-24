/* sqlite_unit.c: SQLite's amalgamation compiled as one unit, as usqlite's usqlite.c compiles it (MIT, Elvin Slavik):
   the warnings that sqlite3.c gives with the settings in sqlite_cmm2.h turned off, and what SQLITE_OMIT_ANALYZE leaves
   the amalgamation's parser asking for. (The amalgamation is made with every statement in its grammar: an OMIT
   option leaves the parser calling what it took out.) */
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wempty-body"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "sqlite3.c"                        /* (the warnings stay off to the end: the last of them come there) */

#ifdef SQLITE_OMIT_ANALYZE
/* ANALYZE: an error, not nothing (usqlite's stub does nothing) */
SQLITE_PRIVATE void sqlite3Analyze(Parse *pParse, Token *pName1, Token *pName2) {
    (void)pName1;
    (void)pName2;
    sqlite3ErrorMsg(pParse, "ANALYZE is not available");
}
#endif
