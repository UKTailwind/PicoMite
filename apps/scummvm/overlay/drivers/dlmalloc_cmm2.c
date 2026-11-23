/* dlmalloc.c with the options frank-quest's CMakeLists.txt gives it (its COMPILE_DEFINITIONS for that file), set here
   because the CMM2's build.py has no per-file flags. GPL-2.0-or-later, as frank-quest. */
#define ONLY_MSPACES 1
#define HAVE_MMAP 0
#define HAVE_MORECORE 0
#define NO_MALLOC_STATS 1
#define USE_LOCKS 0
#define ABORT_ON_ASSERT_FAILURE 0
#define MALLOC_FAILURE_ACTION
#define LACKS_SYS_MMAN_H 1
#define LACKS_UNISTD_H 1
#define LACKS_SYS_PARAM_H 1
#define FOOTERS 1
#define PROCEED_ON_ERROR 0
#define MALLOC_INSPECT_ALL 1
#include "dlmalloc.c"
