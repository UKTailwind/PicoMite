/* PicoMite TinyCC: <assert.h> */
#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
int printf(const char *fmt, ...);
void abort(void);
#define assert(e) ((e) ? (void)0 : (printf("%s:%d: assertion failed: %s\n", \
                    __FILE__, __LINE__, #e), abort()))
#endif
