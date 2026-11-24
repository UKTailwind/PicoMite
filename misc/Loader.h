/* Loader.h: loadable programs (RUN "name.app"). Only in builds with LOADER defined (CMakeLists.txt LOADER_VARIANTS):
   docs/Loader_Plan.md has the design, loader/pmimg.h the file format and the services. */
#ifndef LOADER_H
#define LOADER_H
#ifdef LOADER
extern volatile int LoaderActive;           // 1 while a program runs: CheckAbort leaves a Ctrl-C to it
int LoaderIsApp(const char *fname);         // the name ends in .APP (any case)
void LoaderRun(char *fname, char *args);    // runs it with args's words as argv[1] on; returns when it ends
const void *LoaderModuleLoad(char *fname, const void *basic);   // a USER module loaded (the last one dropped first): its
                                            // table, or an MMBasic error
const void *LoaderModuleTable(void);        // the USER module's table, or NULL if none is loaded (the heap gone since)
void LoaderModuleDrop(void);                // the USER module gone: its forget(), its files closed, its memory freed
extern volatile int LoaderModBusy;          // 1 while the SQL module runs: CheckAbort leaves a Ctrl-C to its poll
const void *LoaderSqlLoad(char *fname, uint32_t heap);   // the SQL module loaded (the last one dropped first), heap its
                                            // C library's heap: its table, or an MMBasic error
const void *LoaderSqlTable(void);           // the SQL module's table, or NULL if none is loaded (the heap gone since)
uint32_t LoaderSqlHeapSize(void);           // its C library's heap's bytes
void *LoaderSqlFn(const void *fn);          // fn of the SQL module, to be called on its own stack: cast to fn's type
void LoaderSqlDrop(void);                   // the SQL module gone: its forget() (the database closed), its memory freed
void cmd_sql(void);                         // SQL and SQL( (SQL.c)
void fun_sql(void);
void cmd_user(void);                        // USER and USER( (User.c)
void fun_user(void);
#endif
#endif
