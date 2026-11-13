#ifndef PICOMITE_ELF_LOADER_H
#define PICOMITE_ELF_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Pure ELF32/ARM ET_REL linker: no TinyCC, FatFS or MMBasic dependencies.
   All addresses are final execution addresses, not addresses of a temporary
   writable buffer (important for CSUB images destined for flash). */
typedef bool (*elf_symbol_resolver)(const char *name, uintptr_t *value, void *ctx);

/* Code and data are copied into image.  The first 'prefix' bytes are reserved
   for the caller.  Each branch relocation may reserve a 16-byte AAPCS-safe Thumb veneer. */
size_t elf_workspace_size(void);
bool elf_abi_supported(const void *object, size_t object_size, bool target_rp2350,
                       char *err, size_t err_size);
bool elf_image_size(void *workspace, const void *object, size_t object_size, size_t prefix,
                    bool writable, size_t *image_size, char *err, size_t err_size);
bool elf_link_image(void *workspace, const void *object, size_t object_size,
                    void *image, size_t image_size, uintptr_t run_base,
                    size_t prefix, bool writable, const char *entry,
                    elf_symbol_resolver resolver, void *ctx, uintptr_t *entry_address,
                    char *err, size_t err_size);

#endif
