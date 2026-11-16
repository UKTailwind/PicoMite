/* Loader.h: loadable programs (RUN "name.app"). Only in builds with LOADER defined (CMakeLists.txt LOADER_VARIANTS):
   docs/Loader_Plan.md has the design, loader/pmimg.h the file format and the services. */
#ifndef LOADER_H
#define LOADER_H
#ifdef LOADER
extern volatile int LoaderActive;           // 1 while a program runs: CheckAbort leaves a Ctrl-C to it
int LoaderIsApp(const char *fname);         // the name ends in .APP (any case)
void LoaderRun(char *fname, char *args);    // runs it with args's words as argv[1] on; returns when it ends
#endif
#endif
