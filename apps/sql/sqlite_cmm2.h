/* sqlite_cmm2.h: SQLite's build settings on the CMM2, included ahead of every file (make.py: -include). They are
   usqlite's (lib/usqlite/usqlite_config.h, MIT, Copyright (c) 2021 Elvin Slavik), as the MicroPython port builds
   SQLite 3.47.0 and ran its tests on a G2 (2 Nov 2026), with these changes (docs/SQL_Plan.md):
   - the progress callback is kept (usqlite leaves it out), so that Ctrl-C can stop a long statement;
   - UTF-16 is kept (below);
   - the pool is sqlcore.c's sqlcore_pool, chosen at start-up by the memory there is, as the MicroPython port's
     cmm2_sqlite_pool: 2 MB on a G2, 512 KB on a G1. */
#ifndef SQLITE_CMM2_H
#define SQLITE_CMM2_H

#define SQLITE_OS_OTHER 1                   /* the OS layer is sqlcore.c's, over the firmware's file services */

#define SQLITE_OMIT_ANALYZE 1
#define SQLITE_OMIT_AUTHORIZATION 1
#define SQLITE_OMIT_AUTOINIT 1              /* (sqlcore_init calls sqlite3_initialize once the pool is set) */
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_OMIT_EXPLAIN 1
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_LOCALTIME 1
#define SQLITE_OMIT_LOOKASIDE 1
#define SQLITE_OMIT_QUICKBALANCE 1
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_OMIT_TCL_VARIABLE 1
/* UTF-16 is kept (usqlite leaves it out): without it a database made in UTF-16 on a PC opened, but every query of it
   was "malformed database schema", and PRAGMA encoding gave nothing (homa, Back Shed, 15 Nov 2026). Text still comes
   to BASIC as UTF-8. */
#define SQLITE_OMIT_WAL 1                   /* no shared memory: rollback journals */

#define SQLITE_UNTESTABLE 1

#define SQLITE_DEFAULT_SYNCHRONOUS 2        /* FULL: the journal synced before the database is written */
#define SQLITE_DEFAULT_WAL_SYNCHRONOUS 1
#define SQLITE_THREADSAFE 0
#define SQLITE_LIKE_DOESNT_MATCH_BLOBS 1
#define SQLITE_MAX_EXPR_DEPTH 0
#define SQLITE_DEFAULT_LOCKING_MODE 1       /* exclusive: one program, no locks to take */
#undef SQLITE_ENABLE_RTREE

#define SQLITE_ENABLE_MEMORY_MANAGEMENT 1
#define SQLITE_DEFAULT_MEMSTATUS 1          /* sqlite3_memory_used() and _highwater() */
#define SQLITE_ZERO_MALLOC 1                /* no malloc of its own: one pool */
#define SQLITE_ENABLE_MEMSYS5 1             /* the pool, a buddy allocator (SQLITE_CONFIG_HEAP) */

#ifndef __ASSEMBLER__
extern int sqlcore_pool;                    /* the pool's bytes (sqlcore.c) */
extern int sqlcore_cache;                   /* the page cache's size as SQLite's cache_size takes it: -KB */
#endif
/* the page cache: an eighth of the pool, at most 256 KB (usqlite's rule: a small cache makes big sorts spill to the
   card early, rather than fill the pool), or the size asked for (sqlcore_init's cache, in slots of its own) */
#define SQLITE_DEFAULT_CACHE_SIZE sqlcore_cache
/* the sorter keeps at most 256 KB of rows before it writes them out, whatever the cache's size (which it follows):
   sqlite3.c's own definition is guarded by make.py's one patch (13 Nov 2026) */
#define SQLITE_MAX_PMASZ (256 * 1024)

#endif
