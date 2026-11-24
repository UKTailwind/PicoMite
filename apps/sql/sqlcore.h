/* sqlcore.h: SQLite on the CMM2's services (sqlcore.c), shared by sql.app and, later, the SQL module. MIT License. */
#ifndef SQLCORE_H
#define SQLCORE_H
#include "sqlite3.h"

/* SQLite started with its memory in pool (size bytes, 8-byte aligned), cache bytes of it a page cache of its own (0:
   an eighth of the pool, at most 256 KB): SQLITE_OK, or SQLite's error */
int sqlcore_init(void *pool, int size, int cache);

/* the pool's size for the memory there is: 2 MB if heap bytes allow it (a G2), else 512 KB (a G1) */
int sqlcore_pool_for(unsigned heap);

/* db watched while a statement runs: the USB devices kept up, and Ctrl-C ends the statement (SQLITE_INTERRUPT) */
void sqlcore_watch(sqlite3 *db);

/* 1 if a database may be called name: on the PicoMite, on B: or C: only (A: is littlefs in the flash) */
int sqlcore_name_ok(const char *name);

/* 1 once Ctrl-C has stopped a statement */
extern volatile int sqlcore_broken;

#endif
