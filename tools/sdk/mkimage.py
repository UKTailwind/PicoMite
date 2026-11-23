"""mkimage.py ELF OUT.app [--name NAME] [--stack BYTES] [--heap BYTES] [--api N] [--psram] [--module] [--fixed]

Makes a loadable program for the PicoMite (loader/pmimg.h) from an ELF that app.ld linked with -Wl,--emit-relocs:
the 64-byte header, the image (every allocated section with contents, from the link address, gaps zeroed), then the
relocation words. Kept are the relocations that hold an absolute address of the image: R_ARM_ABS32 and R_ARM_TARGET1
words (PMIMG_REL_ABS32), which the loader adjusts by any difference, and R_ARM_THM_MOVT_ABS instructions
(PMIMG_REL_MOVT), which set PMIMG_FLAG_MOVT: such an image is moved only by multiples of 64 KB, which leave a MOVW as
it is. A Cortex-M33 build of C and C++ has none (its addresses are in literal pools); hand-written assembly may.
PC-relative relocations need nothing, as the image moves whole; one against an absolute or undefined symbol is left
alone. Any other kind stops the build here, so it never fails at load time. The header records the sections' largest
alignment, which every move keeps. Standard library only.

An SRAM part (app.ld's .sram and .sram_bss, linked at PMIMG_SRAM_LINK) makes a version 2 header, its extension
(pmimg_sram) saying where its bytes are in the image (after the rest, where app.ld stores them), how many, how many to
zero after them and their alignment. The loader copies them into a block of the SRAM heap and moves every relocated
word that holds an address of the SRAM part by that block's difference, the others by the image's. A call between the
parts is a branch to a veneer of the linker's (ldr.w pc, [pc, #-0] and the target's address), whose word is added to
the relocations here; any other PC-relative reference from one part to the other stops the build (the parts move by
different amounts), but for the unwinding tables' (PREL31), which no program here uses.
"""
import argparse, re, struct, sys, zlib

SHF_ALLOC, SHT_NOBITS, SHT_SYMTAB, SHT_REL, SHT_RELA = 2, 8, 2, 9, 4
SHN_UNDEF, SHN_LORESERVE, SHN_ABS = 0, 0xFF00, 0xFFF1
PT_LOAD = 1
R_ABS32, R_TARGET1, R_MOVW, R_MOVT = 2, 38, 47, 48
PC_RELATIVE = {
    0: 'NONE', 1: 'PC24', 3: 'REL32', 10: 'THM_CALL', 11: 'THM_PC8', 28: 'CALL', 29: 'JUMP24', 30: 'THM_JUMP24',
    40: 'V4BX', 42: 'PREL31', 49: 'THM_MOVW_PREL_NC', 50: 'THM_MOVT_PREL', 51: 'THM_JUMP19', 52: 'THM_JUMP6',
    53: 'THM_ALU_PREL_11_0', 54: 'THM_PC12', 102: 'THM_JUMP11', 103: 'THM_JUMP8',
}
BRANCHES = {10, 28, 29, 30}                     # (between the parts: through a veneer, which the linker must have made)
PREL31 = 42
HEADER = struct.Struct('<8sHHBBHIIIIIIII15sB')
assert HEADER.size == 64
SRAM_EXT = struct.Struct('<IIIIB3x')            # pmimg_sram: link, offset, size, bss, align_log2
assert SRAM_EXT.size == 20
MAGIC = b'PICOIMG\0'
TARGET = 0x2350
FLAG_PSRAM, FLAG_MOVT = 0x01, 0x04
SRAM_LO, SRAM_HI = 0x20000000, 0x30000000       # where an SRAM part is linked (PMIMG_SRAM_LINK, 0x20000000)
VENEER = b'\x5f\xf8\x00\xf0'                    # ldr.w pc, [pc, #-0]: the linker's Thumb-2 long branch


def in_sram(addr):
    return SRAM_LO <= addr < SRAM_HI


def sections(d):
    shoff, = struct.unpack_from('<I', d, 32)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 46)
    secs = []
    for i in range(shnum):
        f = struct.unpack_from('<10I', d, shoff + i * shentsize)
        secs.append(dict(name=f[0], type=f[1], flags=f[2], addr=f[3], offset=f[4], size=f[5], link=f[6], info=f[7],
                         align=f[8]))
    strtab = secs[shstrndx]
    for s in secs:
        end = d.index(b'\0', strtab['offset'] + s['name'])
        s['name'] = d[strtab['offset'] + s['name']:end].decode()
    return secs


def load_address(d, addr):
    """where the section at run address addr is stored (its LMA), from the program headers"""
    phoff, = struct.unpack_from('<I', d, 28)
    phentsize, phnum = struct.unpack_from('<HH', d, 42)
    for i in range(phnum):
        ptype, _, vaddr, paddr, _, memsz = struct.unpack_from('<6I', d, phoff + i * phentsize)
        if ptype == PT_LOAD and vaddr <= addr < vaddr + memsz:
            return paddr + addr - vaddr
    sys.exit('no program header holds 0x%08X' % addr)


def symbols(d, secs):
    """(name, value, section index) of every symbol"""
    out = []
    for s in secs:
        if s['type'] != SHT_SYMTAB:
            continue
        strtab = secs[s['link']]
        for i in range(s['size'] // 16):
            name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', d, s['offset'] + i * 16)
            end = d.index(b'\0', strtab['offset'] + name)
            out.append((d[strtab['offset'] + name:end].decode('latin-1'), value, shndx))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('out')
    ap.add_argument('--name', default='')
    ap.add_argument('--stack', type=int, default=0)
    ap.add_argument('--heap', type=int, default=0)
    ap.add_argument('--api', type=int, default=1)     # the services' version it uses: older firmware refuses it
    ap.add_argument('--psram', action='store_true')   # PMIMG_FLAG_PSRAM: a board without PSRAM refuses it
    ap.add_argument('--module', action='store_true')  # PMIMG_KIND_MODULE: loaded beside a BASIC program
    ap.add_argument('--fixed', action='store_true')   # no relocations: it can only run where it is linked
    a = ap.parse_args()
    d = open(a.elf, 'rb').read()
    if d[:4] != b'\x7fELF' or d[4] != 1 or d[5] != 1:
        sys.exit('%s: not a 32-bit little-endian ELF' % a.elf)
    entry, = struct.unpack_from('<I', d, 24)
    secs = sections(d)
    alloc = [s for s in secs if s['flags'] & SHF_ALLOC and s['size']]
    data = [s for s in alloc if s['type'] != SHT_NOBITS and not in_sram(s['addr'])]
    bss = [s for s in alloc if s['type'] == SHT_NOBITS and not in_sram(s['addr'])]
    sdata = [s for s in alloc if s['type'] != SHT_NOBITS and in_sram(s['addr'])]
    sbss = [s for s in alloc if s['type'] == SHT_NOBITS and in_sram(s['addr'])]
    base = min(s['addr'] for s in data)
    end = max(s['addr'] + s['size'] for s in data)
    if base & 7:
        sys.exit('the image starts at 0x%08X: it must be linked at a multiple of 8' % base)
    align = max([8] + [s['align'] for s in alloc if not in_sram(s['addr'])])
    if align & (align - 1) or align > 0x10000:
        sys.exit('a section is aligned to %d bytes: at most 64 KB, a power of 2' % align)
    if base % align:
        sys.exit('the image starts at 0x%08X, not a multiple of its sections\' alignment (%d)' % (base, align))

    # the SRAM part: its bytes after the rest of the image, where app.ld stores them
    sram = bool(sdata or sbss)
    slink = ssize = sbsz = soff = 0
    salign = 8
    if sram:
        if a.module:
            sys.exit('a module cannot have an SRAM part (.sram, .sram_bss)')
        if a.fixed:
            sys.exit('a program with an SRAM part needs its relocations: not --fixed')
        slink = min(s['addr'] for s in sdata + sbss)
        sdend = max([slink] + [s['addr'] + s['size'] for s in sdata])
        send = max(s['addr'] + s['size'] for s in sdata + sbss)
        for s in sbss:
            if s['addr'] < sdend:
                sys.exit('%s (0x%08X) is not above the SRAM part\'s bytes' % (s['name'], s['addr']))
        ssize, sbsz = sdend - slink, send - sdend
        salign = max([8] + [s['align'] for s in sdata + sbss])
        if salign & (salign - 1) or salign > 0x1000 or slink % salign:
            sys.exit('the SRAM part at 0x%08X, aligned to %d: at most 4 KB, a power of 2 it is a multiple of' %
                     (slink, salign))
        soff = (load_address(d, slink) - base) if sdata else (end - base + 7) & ~7
        if soff < end - base or soff & 7:
            sys.exit('the SRAM part is stored at +0x%X, inside the rest of the image or unaligned' % soff)
    image = bytearray(soff + ssize if sram else end - base)
    for s in data:
        image[s['addr'] - base:s['addr'] - base + s['size']] = d[s['offset']:s['offset'] + s['size']]
    for s in sdata:
        o = soff + s['addr'] - slink
        image[o:o + s['size']] = d[s['offset']:s['offset'] + s['size']]
    iend = base + len(image)
    for s in bss:
        if s['addr'] < iend:
            sys.exit('%s (0x%08X) is not above the image' % (s['name'], s['addr']))
    bss_end = max([iend] + [s['addr'] + s['size'] for s in bss])

    def part(addr):                             # 'p' the image, 's' the SRAM part, None neither
        if base <= addr <= bss_end:
            return 'p'
        if sram and slink <= addr <= slink + ssize + sbsz:
            return 's'
        return None

    def offset(addr):                           # the image's offset of a word at run address addr, or None
        if base <= addr < end:
            return addr - base
        if sram and slink <= addr < slink + ssize:
            return soff + addr - slink
        return None

    relocs, skipped, failed, movt, unwind = set(), 0, [], False, 0
    for s in secs:
        if s['type'] == SHT_RELA:
            sys.exit('%s: RELA relocations are not expected on ARM' % s['name'])
        if s['type'] != SHT_REL:
            continue
        target = secs[s['info']]
        if not target['flags'] & SHF_ALLOC or target['type'] == SHT_NOBITS:
            continue                                    # (debug information)
        symtab = secs[s['link']]
        for i in range(s['size'] // 8):
            off, info = struct.unpack_from('<II', d, s['offset'] + i * 8)
            kind, symi = info & 0xFF, info >> 8
            value, = struct.unpack_from('<I', d, symtab['offset'] + symi * 16 + 4)
            shndx, = struct.unpack_from('<H', d, symtab['offset'] + symi * 16 + 14)
            if kind in PC_RELATIVE:
                if sram and shndx not in (SHN_UNDEF, SHN_ABS) and shndx < SHN_LORESERVE and secs[shndx]['flags'] & SHF_ALLOC:
                    there = 's' if in_sram(secs[shndx]['addr']) else 'p'
                    here = 's' if in_sram(off) else 'p'
                    if here != there and kind not in BRANCHES:
                        if kind == PREL31:
                            unwind += 1
                        else:
                            failed.append('%s: a %s reference at 0x%08X from one part to the other' %
                                          (s['name'], PC_RELATIVE[kind], off))
                continue
            if kind in (R_ABS32, R_TARGET1, R_MOVW, R_MOVT) and shndx in (SHN_UNDEF, SHN_ABS):
                skipped += 1                            # an absolute address (a register, say), or an undefined weak: 0
                continue
            o = offset(off)
            if o is None:
                failed.append('%s: offset 0x%08X outside the image' % (s['name'], off))
            elif kind in (R_ABS32, R_TARGET1):
                word, = struct.unpack_from('<I', image, o)
                if part(word) is None:
                    print('warning: %s+0x%X holds 0x%08X, outside the image' % (target['name'], off - target['addr'], word))
                relocs.add(o)
            elif kind == R_MOVT:
                relocs.add(o | (1 << 28))
                movt = True
            elif kind == R_MOVW:
                movt = True                             # (a 64 KB step never changes its low half)
            else:
                failed.append('%s: relocation type %d at 0x%08X' % (s['name'], kind, off))

    veneers = 0
    if sram:                                    # the linker's veneers between the parts: their words relocated
        if movt:
            sys.exit('MOVT relocations and an SRAM part cannot go together')
        for name, value, shndx in symbols(d, secs):
            if not re.match(r'__.+_veneer$', name):
                continue
            o = offset(value & ~1)
            if o is None or image[o:o + 4] != VENEER:
                failed.append('%s at 0x%08X: a veneer of a kind this does not know' % (name, value))
                continue
            word, = struct.unpack_from('<I', image, o + 4)
            if part(word) is None:
                failed.append('%s at 0x%08X goes to 0x%08X, outside the program' % (name, value, word))
            relocs.add(o + 4)
            veneers += 1
    if failed:
        sys.exit('cannot make a loadable image:\n  ' + '\n  '.join(failed[:20]))
    if a.fixed:
        relocs = set()
    rel = b''.join(struct.pack('<I', r) for r in sorted(relocs, key=lambda r: r & 0x0FFFFFFF))
    crc = zlib.crc32(bytes(image) + rel) & 0xFFFFFFFF
    name = (a.name or a.out.replace('\\', '/').split('/')[-1].rsplit('.', 1)[0])[:14].encode()
    flags = (FLAG_PSRAM if a.psram else 0) | (FLAG_MOVT if movt and not a.fixed else 0)
    hdr = HEADER.pack(MAGIC, 2 if sram else 1, a.api, 2 if a.module else 1, flags, TARGET, base, len(image),
                      bss_end - iend, entry - base, a.stack, a.heap, len(relocs), crc, name, align.bit_length() - 1)
    ext = SRAM_EXT.pack(slink, soff, ssize, sbsz, salign.bit_length() - 1) if sram else b''
    open(a.out, 'wb').write(hdr + ext + bytes(image) + rel)
    print('%s: %d bytes of image at 0x%08X, %d of .bss, entry +0x%X, %d relocations (%d absolute left alone), '
          'aligned %d%s, CRC %08X' % (a.out, len(image), base, bss_end - iend, entry - base, len(relocs), skipped,
                                      align, ', MOVT: moved by 64 KB steps' if flags & FLAG_MOVT else '', crc))
    if sram:
        print('  SRAM part: %d bytes (at +0x%X) and %d zeroed, aligned %d; %d veneers between the parts%s' %
              (ssize, soff, sbsz, salign, veneers,
               ', %d unwinding entries for SRAM code left as they are' % unwind if unwind else ''))


if __name__ == '__main__':
    main()
