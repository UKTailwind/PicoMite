#!/usr/bin/env python3
"""
Measure RGB332 compression on an existing raw .r332 file (no firmware needed).

Two candidate schemes, per frame, runs allowed to cross scanline boundaries:

  A) count+value RLE  (the proposed scheme)
       [count 1..255][value]   -> 'count' copies of 'value'   (2 bytes/run)
       Simple, but a run of length 1 costs 2 bytes -> 2x blow-up on noisy data.

  B) PackBits-style    (for comparison)
       control byte n:
         n = 0..127  -> copy next (n+1) literal bytes
         n = 129..255-> repeat next byte (257-n) times   (2..128)
         n = 128     -> unused
       Worst case overhead ~1/128, so it never blows up badly.
"""
import sys, os
from itertools import groupby

FRAME = 320 * 240  # 76800 bytes per RGB332 frame

def runs(frame):
    """maximal (value,length) runs over the whole frame."""
    for v, g in groupby(frame):
        yield v, sum(1 for _ in g)

def size_countvalue(frame):
    total = 0
    for _, L in runs(frame):
        total += 2 * ((L + 254) // 255)   # ceil(L/255) packets, 2 bytes each
    return total

def size_packbits(frame):
    total = 0
    lit = 0  # pending literal bytes
    def flush_lit():
        nonlocal lit, total
        while lit > 0:
            k = min(lit, 128)
            total += 1 + k           # control + k literal bytes
            lit -= k
    for _, L in runs(frame):
        if L >= 2:
            flush_lit()
            total += 2 * ((L + 127) // 128)   # ceil(L/128) repeat packets
        else:
            lit += 1
    flush_lit()
    return total

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "sample-2.r332"
    fps = float(sys.argv[2]) if len(sys.argv) > 2 else 22.0
    data = open(path, "rb").read()
    nframes = len(data) // FRAME
    if nframes == 0:
        sys.exit("file smaller than one 320x240 frame")

    raw = nframes * FRAME
    cv_total = pb_total = 0
    cv_max = pb_max = 0
    for f in range(nframes):
        fr = data[f*FRAME:(f+1)*FRAME]
        cv = size_countvalue(fr)
        pb = size_packbits(fr)
        cv_total += cv; pb_total += pb
        cv_max = max(cv_max, cv); pb_max = max(pb_max, pb)
        if (f+1) % 50 == 0:
            print(f"  {f+1}/{nframes}", end="\r")

    print(" " * 30, end="\r")
    READ = 0.95e6   # measured SD read throughput, bytes/sec (~0.95 MB/s)
    def row(name, total, mx):
        avg = total / nframes
        print(f"{name:18} {total/1024/1024:7.2f} MB  "
              f"ratio {raw/total:5.2f}x  "
              f"avg {avg:7.0f} B/frame  max {mx:6d} B  "
              f"~{READ/avg:5.1f} fps sustainable")
    print(f"file: {path}   {nframes} frames @ {fps:g} fps   raw {raw/1024/1024:.2f} MB "
          f"({FRAME} B/frame, {READ/FRAME:.1f} fps sustainable)\n")
    row("count+value RLE", cv_total, cv_max)
    row("PackBits-style",  pb_total, pb_max)

if __name__ == "__main__":
    main()
