#ifndef PICOMITE_PM_RUNTIME_H
#define PICOMITE_PM_RUNTIME_H
#include <stdbool.h>
#include <stddef.h>
void *pm_runtime_malloc(size_t size);
void *pm_runtime_calloc(size_t nmemb, size_t size);
void *pm_runtime_realloc(void *ptr, size_t size);
void pm_runtime_free(void *ptr);
bool pm_runtime_owns_range(const void *ptr, size_t size);
int pm_runtime_call_main(int (*entry)(int, char **), int argc, char **argv);
void pm_runtime_begin(void);
void pm_runtime_end(void);
bool pm_runtime_is_running(void);
void pm_runtime_tick_service(void);
void pm_runtime_try_catch(const char *msg, int err);
bool pm_runtime_csub_forbidden(const char *name);
const void *pm_runtime_resolve_symbol(const char *name);
unsigned pm_runtime_firmware_id(void);
#endif
