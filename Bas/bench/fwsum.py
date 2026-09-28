"""fwsum.py PORT FIRMWARE.uf2 - is the firmware in the board's flash still the
image that was flashed?  Sums the image's 32-bit words in 64 KB blocks on the
host, has the board sum the same bytes with PEEK (OPTION COMPILE OFF, so the
check does not lean on Route B), and for a block that differs, sums its 4 KB
sectors on both sides to say which sectors changed and what they now hold (an
erased sector reads all &HFFFFFFFF words).  Prints FWSUM OK or FWSUM BAD.
Written after a PC3 lost its firmware in the middle of a test chain: the
fault in project memory (a flash erase that latches the wrong address) would
show as whole sectors of 0xFF."""
import os, re, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

BASE = 0x10000000


def image(path):
    data = open(path, "rb").read()
    mem = {}
    for off in range(0, len(data), 512):
        blk = data[off:off + 512]
        magic0, magic1, flags, addr, size, _, _, family = struct.unpack("<IIIIIIII", blk[:32])
        if magic0 != 0x0A324655 or magic1 != 0x9E5D5157 or (flags & 1):
            continue  # not a UF2 block, or "not main flash"
        if (flags & 0x2000) and family not in (0xE48BFF59, 0xE48BFF56):
            continue  # not RP2350 ARM or RP2040 code: the RP2350's E10 "absolute" block, say
        if BASE <= addr < BASE + 0x1000000:
            mem[addr] = blk[32:32 + size]
    lo, hi = min(mem), max(a + len(d) for a, d in mem.items())
    img = bytearray(b"\xff" * (hi - lo))
    for a, d in mem.items():
        img[a - lo:a - lo + len(d)] = d
    return lo, img


def sums(lo, img, start, end, step):
    out = []
    for b in range(start, end, step):
        chunk = img[b - lo:min(b + step, end) - lo]
        out.append(sum(struct.unpack("<%dI" % (len(chunk) // 4), chunk)))
    return out


def board_sums(b, start, end, step):
    src = """Option Default Integer
Dim b, i, s, e = &H%X
For b = &H%X To e - 1 Step %d
  s = 0
  For i = b To Min(b + %d, e) - 4 Step 4
    s = s + (Peek(Word i) And &HFFFFFFFF)
  Next
  Print "FS "; s
Next
Print "FSEND"
""" % (end, start, step, step)
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(300)).replace("\r", "")
    if "FSEND" not in out:
        sys.exit("FWSUM ERROR: " + " ".join(out.split())[-300:])
    return [int(x) for x in re.findall(r"^FS +(\d+)", out, re.M)]


def main():
    port, uf2 = sys.argv[1], sys.argv[2]
    lo, img = image(uf2)
    end = lo + len(img)
    b = pc3.PC3(port)
    b.attention()
    b.cmd("OPTION COMPILE OFF", 10)
    host = sums(lo, img, lo, end, 65536)
    brd = board_sums(b, lo, end, 65536)
    bad = [i for i, (h, d) in enumerate(zip(host, brd)) if h != d]
    if len(brd) != len(host):
        bad = bad or [len(brd)]
    for i in bad:
        start = lo + i * 65536
        stop = min(start + 65536, end)
        hs = sums(lo, img, start, stop, 4096)
        bs = board_sums(b, start, stop, 4096)
        for j, (h, d) in enumerate(zip(hs, bs)):
            if h != d:
                n = min(4096, stop - start - j * 4096) // 4
                what = " (erased)" if d == n * 0xFFFFFFFF else ""
                print("sector %08X differs: image %d, flash %d%s" % (start + j * 4096, h, d, what))
    print("FWSUM", "OK" if not bad else "BAD", "(%d bytes, %08X-%08X)" % (len(img), lo, end))
    b.close()


main()
