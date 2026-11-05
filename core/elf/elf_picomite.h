#ifndef PICOMITE_ELF_ADAPTER_H
#define PICOMITE_ELF_ADAPTER_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ELF_CSUB_MAGIC 0x53434c45u /* "ELCS" */

uint32_t elf_picomite_firmware_id(void);
bool elf_picomite_is_elf(const char *filename);
unsigned char *elf_picomite_csub_source(unsigned char *p, unsigned short endtoken, char *line, int size);
bool elf_picomite_csub_single_object(const char *line, const unsigned char *prog);
uint32_t *elf_picomite_csub_compile(const unsigned char *decl, const char *line, uint32_t base,
                                    const unsigned char *prog, int *words, char *err);
void elf_picomite_csub_check(const uint32_t *code);
bool elf_picomite_csub_stale(void);
void elf_picomite_csub_rebuild(void);
/* A RUN invoked without TinyCC (RP2040); RP2350 uses the established
   runtime stack and error trapping but no compiler for ELF execution. */
void elf_picomite_run_file(const char *filename, const char *args);
int elf_picomite_run_argv(int argc, char **argv);
void elf_picomite_abandoned(void);
#endif
