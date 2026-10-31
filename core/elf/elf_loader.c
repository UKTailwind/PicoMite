#include "elf_loader.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* Avoid the host compiler's Elf32_* definitions and unaligned dereferences. */
typedef struct { unsigned char ident[16]; uint16_t type, machine; uint32_t version;
    uint32_t entry, phoff, shoff, flags; uint16_t ehsize, phentsize, phnum,
    shentsize, shnum, shstrndx; } Ehdr;
typedef struct { uint32_t name, type, flags, addr, offset, size, link, info,
    align, entsize; } Shdr;
typedef struct { uint32_t name, value, size; unsigned char info, other;
    uint16_t shndx; } Sym;
typedef struct { uint32_t offset, info; } Rel;

#define ET_REL 1u
#define EM_ARM 40u
#define SHT_SYMTAB 2u
#define SHT_STRTAB 3u
#define SHT_RELA 4u
#define SHT_NOBITS 8u
#define SHT_REL 9u
#define SHF_WRITE 1u
#define SHF_ALLOC 2u
#define SHN_UNDEF 0u
#define SHN_ABS 0xfff1u
#define R_ARM_NONE 0u
#define R_ARM_ABS32 2u
#define R_ARM_REL32 3u
#define R_ARM_THM_CALL 10u
#define R_ARM_THM_JUMP24 30u
#define R_ARM_PREL31 42u
#define R_ARM_THM_MOVW_ABS_NC 47u
#define R_ARM_THM_MOVT_ABS 48u
#define MAX_SECTIONS 128u
#define MAX_IMAGE_SIZE (8u * 1024u * 1024u)

typedef struct {
    const uint8_t *src;
    size_t len;
    Ehdr eh;
    Shdr sh[MAX_SECTIONS];
    size_t at[MAX_SECTIONS];
    size_t code_end;
    size_t end;
} Layout;

static bool fail(char *err, size_t cap, const char *why) {
    if (cap) snprintf(err, cap, "%s", why);
    return false;
}
static bool span(size_t size, uint32_t off, uint32_t count) {
    return (size_t)off <= size && (size_t)count <= size - (size_t)off;
}
static uint16_t rd16(const void *p) { uint16_t x; memcpy(&x, p, 2); return x; }
static uint32_t rd32(const void *p) { uint32_t x; memcpy(&x, p, 4); return x; }
static void wr16(void *p, uint16_t x) { memcpy(p, &x, 2); }
static void wr32(void *p, uint32_t x) { memcpy(p, &x, 4); }
static bool align_up(size_t *v, uint32_t a) {
    size_t add;
    if (!a) a = 1;
    if (a & (a - 1)) return false;
    add = (size_t)a - 1;
    if (*v > SIZE_MAX - add) return false;
    *v = (*v + add) & ~add;
    return true;
}

/* ARM build attributes: RP2040 is ARMv6-M, RP2350 is ARMv8-M mainline.
   An RP2040 image may run on RP2350, but a Thumb-2 image must never be
   dispatched to RP2040.  Reject objects with unverifiable attributes there. */
static bool uleb(const uint8_t **p, const uint8_t *end, uint32_t *v) {
    unsigned shift = 0;
    *v = 0;
    while (*p < end && shift < 35) {
        uint8_t x = *(*p)++;
        if (shift == 28 && (x & 0x70)) return false;
        *v |= (uint32_t)(x & 127u) << shift;
        if (!(x & 128u)) return true;
        shift += 7;
    }
    return false;
}
static bool attributes_ok(const uint8_t *p, size_t sz, bool target_rp2350) {
    const uint8_t *end = p + sz;
    bool seen = false;
    if (!sz || *p++ != 'A') return false;
    while (p < end) {
        const uint8_t *start = p, *sectend;
        uint32_t n;
        if (end - p < 5) return false;
        n = rd32(p);
        if (n < 6 || n > (size_t)(end - start)) return false;
        sectend = start + n;
        p += 4;
        {
            const uint8_t *vendor_end = memchr(p, 0, sectend - p);
            if (!vendor_end) return false;
            if ((vendor_end - p) != 5 || memcmp(p, "aeabi", 5)) {
                p = sectend;
                continue;
            }
            p = vendor_end + 1;
        }
        while (p < sectend) {
            uint8_t tag = *p++;
            const uint8_t *substart = p - 1, *subend;
            if (sectend - p < 4) return false;
            n = rd32(p); p += 4;
            if (n < 5 || n > (size_t)(sectend - substart)) return false;
            subend = substart + n;
            if (tag == 1) {
                uint32_t arch = 0, profile = 0, armisa = 0;
                while (p < subend) {
                    uint32_t key, v;
                    if (!uleb(&p, subend, &key)) return false;
                    if (key == 4 || key == 5 || key == 67) {
                        const uint8_t *z = memchr(p, 0, subend - p);
                        if (!z) return false;
                        p = z + 1;
                    } else if (key == 32) {
                        if (!uleb(&p, subend, &v)) return false;
                        const uint8_t *z = memchr(p, 0, subend - p);
                        if (!z) return false;
                        p = z + 1;
                    } else {
                        if (!uleb(&p, subend, &v)) return false;
                        if (key == 6) { arch = v; seen = true; }
                        if (key == 7) profile = v;
                        if (key == 8) armisa = v;
                    }
                }
                if (seen && (profile != 'M' || armisa ||
                    (!target_rp2350 && arch != 11 && arch != 12) ||
                    (target_rp2350 && arch != 11 && arch != 12 && arch != 16 && arch != 17)))
                    return false;
            }
            p = subend;
        }
        p = sectend;
    }
    return seen;
}

bool elf_abi_supported(const void *object, size_t object_size, bool target_rp2350,
                       char *err, size_t cap) {
    Ehdr eh;
    const uint8_t *bytes = object;
    unsigned i;
    if (object_size < sizeof(eh)) return fail(err, cap, "ELF: truncated header");
    memcpy(&eh, object, sizeof(eh));
    if (memcmp(bytes, "\177ELF", 4) || bytes[4] != 1 || bytes[5] != 1 ||
        eh.machine != EM_ARM || eh.type != ET_REL ||
        eh.shentsize != sizeof(Shdr) || !eh.shnum ||
        !span(object_size, eh.shoff, (uint32_t)eh.shnum * sizeof(Shdr)))
        return fail(err, cap, "ELF: invalid ELF32/ARM object");
    for (i = 0; i < eh.shnum; ++i) {
        Shdr sh;
        memcpy(&sh, bytes + eh.shoff + (size_t)i * sizeof(Shdr), sizeof(sh));
        if (sh.type != 0x70000003u) continue;
        if (!span(object_size, sh.offset, sh.size) ||
            !attributes_ok(bytes + sh.offset, sh.size, target_rp2350))
            return fail(err, cap, "ELF: incompatible ARM attributes");
        return true;
    }
    if (!target_rp2350) return fail(err, cap, "ELF: ARMv6-M attributes required on RP2040");
    return true; /* TinyCC objects may omit .ARM.attributes on RP2350. */
}

static bool layout_init(Layout *l, const void *object, size_t len, uintptr_t base, size_t prefix,
                        bool writable, char *err, size_t cap) {
    size_t cursor = prefix, veneers = 0;
    const uint8_t *p = (const uint8_t *)object;
    unsigned i;
    memset(l, 0, sizeof(*l));
    if (len < sizeof(Ehdr)) return fail(err, cap, "ELF: truncated header");
    l->src = p;
    l->len = len;
    memcpy(&l->eh, p, sizeof(Ehdr));
    if (memcmp(p, "\177ELF", 4) || p[4] != 1 || p[5] != 1 || p[6] != 1 ||
        (p[7] != 0 && p[7] != 97) || l->eh.type != ET_REL ||
        l->eh.machine != EM_ARM || l->eh.version != 1 ||
        l->eh.ehsize != sizeof(Ehdr) || l->eh.shentsize != sizeof(Shdr) ||
        !l->eh.shnum || l->eh.shnum > MAX_SECTIONS ||
        (l->eh.flags & 0x400u)) /* EF_ARM_ABI_FLOAT_HARD */
        return fail(err, cap, "ELF: expected soft-float ELF32/ARM ET_REL");
    if (!span(len, l->eh.shoff, (uint32_t)l->eh.shnum * sizeof(Shdr)))
        return fail(err, cap, "ELF: section table outside file");
    for (i = 0; i < l->eh.shnum; i++) {
        Shdr *s = &l->sh[i];
        memcpy(s, p + l->eh.shoff + i * sizeof(Shdr), sizeof(*s));
        if (s->type != SHT_NOBITS && !span(len, s->offset, s->size))
            return fail(err, cap, "ELF: section outside file");
        if (s->type == SHT_RELA) return fail(err, cap, "ELF: RELA not supported");
        if ((s->flags & SHF_ALLOC) == 0) continue;
        if (!writable && (s->flags & SHF_WRITE))
            return fail(err, cap, "ELF: CSUB cannot have writable sections");
        if (s->align > 4096u || base > UINT32_MAX - cursor) return fail(err, cap, "ELF: invalid alignment");
        {
            size_t absolute = base + cursor;
            if (!align_up(&absolute, s->align)) return fail(err, cap, "ELF: invalid alignment");
            cursor = absolute - base;
        }
        l->at[i] = cursor;
        if (s->size > MAX_IMAGE_SIZE - cursor)
            return fail(err, cap, "ELF: image too large");
        cursor += s->size;
    }
    l->code_end = cursor;
    if (!align_up(&cursor, 4)) return fail(err, cap, "ELF: image too large");
    for (i = 0; i < l->eh.shnum; i++) {
        const Shdr *s = &l->sh[i];
        if (s->type != SHT_REL) continue;
        if (s->info >= l->eh.shnum || s->link >= l->eh.shnum ||
            l->sh[s->link].type != SHT_SYMTAB || !s->entsize ||
            s->entsize != sizeof(Rel) || s->size % sizeof(Rel))
            return fail(err, cap, "ELF: malformed relocation table");
        if (!(l->sh[s->info].flags & SHF_ALLOC)) continue;
        if ((size_t)s->size / sizeof(Rel) > (MAX_IMAGE_SIZE - veneers) / 16)
            return fail(err, cap, "ELF: too many relocations");
        veneers += ((size_t)s->size / sizeof(Rel)) * 16;
    }
    if (veneers > MAX_IMAGE_SIZE - cursor)
        return fail(err, cap, "ELF: image too large");
    l->end = cursor + veneers;
    return true;
}

size_t elf_workspace_size(void) { return sizeof(Layout); }

bool elf_image_size(void *workspace, const void *object, size_t object_size, size_t prefix,
                    bool writable, size_t *image_size, char *err, size_t cap) {
    Layout *l = workspace;
    size_t slack = 0;
    unsigned i;
    if (!layout_init(l, object, object_size, 0, prefix, writable, err, cap)) return false;
    /* The caller does not yet know the final malloc address.  Allow for
       every section requiring additional padding from that address. */
    for (i = 0; i < l->eh.shnum; ++i)
        if (l->sh[i].flags & SHF_ALLOC)
            slack += l->sh[i].align ? l->sh[i].align - 1u : 0u;
    if (slack > MAX_IMAGE_SIZE - l->end) return fail(err, cap, "ELF: image too large");
    *image_size = l->end + slack;
    return true;
}

static bool elf_symbol(const Layout *l, const Shdr *symtab, const Sym *sym,
                       uintptr_t base, elf_symbol_resolver resolver, void *ctx,
                       uintptr_t *out, char *err, size_t cap) {
    const Shdr *str;
    const char *name;
    if (sym->shndx == SHN_ABS) { *out = sym->value; return true; }
    if (sym->shndx != SHN_UNDEF) {
        if (sym->shndx >= l->eh.shnum ||
            !(l->sh[sym->shndx].flags & SHF_ALLOC) ||
            sym->value > l->sh[sym->shndx].size)
            return fail(err, cap, "ELF: symbol outside allocated section");
        *out = base + l->at[sym->shndx] + sym->value;
        return true;
    }
    if (symtab->link >= l->eh.shnum) return fail(err, cap, "ELF: bad symbol strings");
    str = &l->sh[symtab->link];
    if (str->type != SHT_STRTAB || sym->name >= str->size)
        return fail(err, cap, "ELF: bad symbol name");
    name = (const char *)l->src + str->offset + sym->name;
    if (!memchr(name, 0, str->size - sym->name))
        return fail(err, cap, "ELF: unterminated symbol name");
    if (!*name) { *out = 0; return true; }
    if (resolver && resolver(name, out, ctx)) return true;
    if ((sym->info >> 4) == 2) { *out = 0; return true; } /* weak undefined */
    if (cap) snprintf(err, cap, "ELF: undefined symbol %.80s", name);
    return false;
}

static int32_t decode_bl(uint16_t a, uint16_t b) {
    uint32_t S = (a >> 10) & 1, J1 = (b >> 13) & 1, J2 = (b >> 11) & 1;
    uint32_t I1 = (~(J1 ^ S)) & 1, I2 = (~(J2 ^ S)) & 1;
    uint32_t n = (S << 24) | (I1 << 23) | (I2 << 22) | ((a & 1023u) << 12) | ((b & 2047u) << 1);
    return (int32_t)(n << 7) >> 7;
}
static void encode_bl(uint8_t *p, int32_t off, bool jump) {
    uint32_t v = (uint32_t)off, S = (v >> 24) & 1;
    uint32_t J1 = (~(((v >> 23) & 1) ^ S)) & 1;
    uint32_t J2 = (~(((v >> 22) & 1) ^ S)) & 1;
    wr16(p, (uint16_t)(0xf000 | (S << 10) | ((v >> 12) & 1023)));
    wr16(p + 2, (uint16_t)((jump ? 0x9000u : 0xd000u) | (J1 << 13) | (J2 << 11) | ((v >> 1) & 2047)));
}
static bool fits_bl(int64_t off) { return !(off & 1) && off >= -(1 << 24) && off < (1 << 24); }

bool elf_link_image(void *workspace, const void *object, size_t object_size, void *image,
                    size_t image_size, uintptr_t run_base, size_t prefix,
                    bool writable, const char *entry, elf_symbol_resolver resolver,
                    void *ctx, uintptr_t *entry_address, char *err, size_t cap) {
    Layout *l = workspace;
    uint8_t *dst = (uint8_t *)image;
    size_t veneer;
    unsigned i;
    if (!layout_init(l, object, object_size, run_base, prefix, writable, err, cap)) return false;
    if (image_size < l->end || run_base > UINT32_MAX || l->end > UINT32_MAX - run_base)
        return fail(err, cap, "ELF: output image out of bounds");
    memset(image, 0, l->end);
    for (i = 0; i < l->eh.shnum; i++) {
        const Shdr *s = &l->sh[i];
        if (!(s->flags & SHF_ALLOC) || s->type == SHT_NOBITS) continue;
        memcpy(dst + l->at[i], l->src + s->offset, s->size);
    }
    veneer = (l->code_end + 3u) & ~3u;
    for (i = 0; i < l->eh.shnum; i++) {
        const Shdr *relsec = &l->sh[i], *target, *symtab;
        size_t count, j;
        if (relsec->type != SHT_REL) continue;
        target = &l->sh[relsec->info];
        if (!(target->flags & SHF_ALLOC)) continue;
        symtab = &l->sh[relsec->link];
        if (!symtab->entsize || symtab->entsize != sizeof(Sym) ||
            symtab->size % sizeof(Sym) || symtab->link >= l->eh.shnum)
            return fail(err, cap, "ELF: malformed symbol table");
        count = relsec->size / sizeof(Rel);
        for (j = 0; j < count; j++) {
            Rel r;
            Sym sym;
            uintptr_t S, P;
            uint32_t kind, symi;
            uint8_t *where;
            memcpy(&r, l->src + relsec->offset + j * sizeof(Rel), sizeof(r));
            kind = r.info & 255;
            symi = r.info >> 8;
            if (r.offset > target->size || target->size - r.offset < 4 ||
                symi >= symtab->size / sizeof(Sym))
                return fail(err, cap, "ELF: relocation outside target");
            memcpy(&sym, l->src + symtab->offset + (size_t)symi * sizeof(Sym), sizeof(sym));
            if (!elf_symbol(l, symtab, &sym, run_base, resolver, ctx, &S, err, cap)) return false;
            where = dst + l->at[relsec->info] + r.offset;
            P = run_base + l->at[relsec->info] + r.offset;
            switch (kind) {
            case R_ARM_NONE: break;
            case R_ARM_ABS32: wr32(where, rd32(where) + (uint32_t)S); break;
            case R_ARM_REL32: wr32(where, rd32(where) + (uint32_t)S - (uint32_t)P); break;
            case R_ARM_PREL31: {
                int32_t addend = (int32_t)(rd32(where) << 1) >> 1;
                int64_t value = (int64_t)addend + (int64_t)(uint32_t)S - (int64_t)P;
                if (value < -(1ll << 30) || value >= (1ll << 30))
                    return fail(err, cap, "ELF: PREL31 out of range");
                wr32(where, (rd32(where) & 0x80000000u) | ((uint32_t)value & 0x7fffffffu));
                break;
            }
            case R_ARM_THM_CALL:
            case R_ARM_THM_JUMP24: {
                uint16_t a = rd16(where), b = rd16(where + 2);
                int64_t offset;
                if ((a & 0xf800) != 0xf000 ||
                    (kind == R_ARM_THM_CALL && (b & 0xd000) != 0xd000) ||
                    (kind == R_ARM_THM_JUMP24 && (b & 0xd000) != 0x9000))
                    return fail(err, cap, "ELF: invalid Thumb branch instruction");
                offset = (int64_t)decode_bl(a, b) + ((uint32_t)S & ~1u) - (int64_t)P;
                if (!fits_bl(offset)) {
                    /* Thumb-1-compatible veneer, usable in RAM and XIP.
                       Preserve r0-r3: the fourth C argument lives in r3.
                       r12 (ip) is the AAPCS inter-procedure scratch register.
                       push {r3}; ldr r3,[pc,#8]; mov ip,r3; pop {r3};
                       bx ip; nop; .word target|1 (16 bytes). */
                    int64_t to_veneer = (int64_t)(run_base + veneer) - (int64_t)P - 4;
                    if (veneer + 16 > image_size || !fits_bl(to_veneer))
                        return fail(err, cap, "ELF: veneer out of branch range");
                    wr16(dst + veneer,      0xb408u); /* push {r3}       */
                    wr16(dst + veneer + 2,  0x4b02u); /* ldr r3,[pc,#8]  */
                    wr16(dst + veneer + 4,  0x469cu); /* mov ip,r3       */
                    wr16(dst + veneer + 6,  0xbc08u); /* pop {r3}        */
                    wr16(dst + veneer + 8,  0x4760u); /* bx ip           */
                    wr16(dst + veneer + 10, 0xbf00u); /* nop (alignment) */
                    wr32(dst + veneer + 12, (uint32_t)S | 1u);
                    veneer += 16;
                    offset = (int64_t)decode_bl(a, b) + (int64_t)(run_base + veneer - 16) - (int64_t)P;
                }
                if (!fits_bl(offset)) return fail(err, cap, "ELF: Thumb branch out of range");
                encode_bl(where, (int32_t)offset, kind == R_ARM_THM_JUMP24);
                break;
            }
            default: return fail(err, cap, "ELF: unsupported ARM relocation");
            }
        }
    }
    *entry_address = 0;
    if (entry) {
        for (i = 0; i < l->eh.shnum; i++) {
            const Shdr *tab = &l->sh[i];
            size_t j;
            if (tab->type != SHT_SYMTAB || tab->entsize != sizeof(Sym) ||
                tab->link >= l->eh.shnum || l->sh[tab->link].type != SHT_STRTAB)
                continue;
            for (j = 0; j < tab->size / sizeof(Sym); j++) {
                Sym sym;
                uintptr_t address;
                const Shdr *str = &l->sh[tab->link];
                const char *name;
                memcpy(&sym, l->src + tab->offset + j * sizeof(Sym), sizeof(sym));
                if (sym.name >= str->size || sym.shndx == SHN_UNDEF) continue;
                name = (const char *)l->src + str->offset + sym.name;
                if (!memchr(name, 0, str->size - sym.name)) return fail(err, cap, "ELF: bad symbol name");
                if (strcmp(name, entry) != 0) continue;
                if (!elf_symbol(l, tab, &sym, run_base, resolver, ctx, &address, err, cap)) return false;
                *entry_address = address | 1u;
                return true;
            }
        }
        if (cap) snprintf(err, cap, "ELF: missing entry %.70s", entry);
        return false;
    }
    return true;
}
