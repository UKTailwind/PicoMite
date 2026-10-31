/* ARM EABI varargs for RP2350 TinyCC. */
#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;
#define _tcc_alignof(type) ((int)&((struct {char c;type x;} *)0)->x)
#define _tcc_align(addr,type) (((unsigned)addr + _tcc_alignof(type) - 1) \
                               & ~(_tcc_alignof(type) - 1))
#define va_start(ap,last) ap = ((char *)&(last)) + ((sizeof(last)+3)&~3)
#define va_arg(ap,type) (ap = (void *) ((_tcc_align(ap,type)+sizeof(type)+3) \
                        &~3), *(type *)(ap - ((sizeof(type)+3)&~3)))
#define va_copy(dest, src) (dest) = (src)
#define va_end(ap) ((void)0)

typedef va_list __gnuc_va_list;
#define _VA_LIST_DEFINED

#endif
