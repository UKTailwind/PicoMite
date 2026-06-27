#!/usr/bin/env python3
"""
vid2rgb121.py  -  Convert any video into a PicoMite RGB121 framebuffer video.

Output (.vc) format:
    For every frame, in order:
        uint32  blobLen   (little-endian, = 4 + len(rle))
        ---- blob (exactly what BLIT MEMORY expects) ----
        uint16  width  | 0x8000      (0x8000 = "RLE compressed")
        uint16  height
        bytes   rle...               each byte = (colour<<4) | count, count 1..15

The RLE stream is the whole frame, row-major, as one continuous run of nibbles
(runs may cross scanline boundaries) - this is exactly how the firmware's
getnextnibble()/docompressed() decoder consumes it.

RGB121 nibble layout (== framebuffer colour index):  R<<3 | G(0..3)<<1 | B
    R = 1 bit, G = 2 bits (levels 0,0x40,0x80,0xFF), B = 1 bit.
    black = 0, white = 15.

Requires ffmpeg on PATH.

Usage:
    python vid2rgb121.py input.mp4 ba.vc [--w 320] [--h 240] [--fps 22]
"""
import argparse, subprocess, struct, sys, shutil, os

def find_ffmpeg():
    """Prefer ffmpeg on PATH; fall back to the binary bundled by imageio-ffmpeg."""
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return None

# Green reconstruction levels for the 2-bit green channel, and their thresholds.
G_LEVELS = (0x00, 0x40, 0x80, 0xFF)
# midpoints between adjacent levels: 32, 96, 191
def g_quant(g):
    if g < 32:  return 0
    if g < 96:  return 1
    if g < 191: return 2
    return 3

def rgb_to_nibble(r, g, b):
    R = 1 if r >= 128 else 0
    B = 1 if b >= 128 else 0
    G = g_quant(g)
    return (R << 3) | (G << 1) | B

def build_nibble_lut():
    """256^? is too big; build per-channel then combine on the fly is enough,
    but precompute the full 24-bit->nibble is 16M entries. Instead precompute
    per-channel contributions for speed."""
    rl = bytes(0x08 if r >= 128 else 0 for r in range(256))      # R<<3
    bl = bytes(0x01 if b >= 128 else 0 for b in range(256))      # B
    gl = bytes(g_quant(g) << 1 for g in range(256))              # G<<1
    return rl, gl, bl

def build_rgb332_lut():
    """Per-channel contributions for RGB332 = RRRGGGBB, matching the firmware's
    RGB332(): (c>>16 & 0xE0) | (c>>11 & 0x1C) | (c>>6 & 0x03)."""
    rl = bytes((r & 0xE0) for r in range(256))                   # bits 7-5
    gl = bytes(((g & 0xE0) >> 3) for g in range(256))            # bits 4-2
    bl = bytes(((b & 0xC0) >> 6) for b in range(256))            # bits 1-0
    return rl, gl, bl

def rle_encode(nibbles):
    """nibbles: bytes/bytearray of values 0..15. Emit (colour<<4)|count, count<=15."""
    out = bytearray()
    n = len(nibbles)
    i = 0
    while i < n:
        c = nibbles[i]
        run = 1
        # max run we *could* take, capped so we never emit count 0
        while i + run < n and nibbles[i + run] == c and run < 15:
            run += 1
        out.append((c << 4) | run)
        i += run
    return out

def rle332_encode(px):
    """px: bytes of RGB332 pixels. Emit [count 1..255][value] runs (runs may
    cross scanline boundaries). The firmware decoder fills w*h pixels with
    memset per run."""
    out = bytearray()
    n = len(px)
    i = 0
    while i < n:
        v = px[i]
        run = 1
        while i + run < n and px[i + run] == v and run < 255:
            run += 1
        out.append(run)
        out.append(v)
        i += run
    return out

def extract_audio(ffmpeg, src, dst, rate, mono):
    """Extract the source audio track to an 8-bit PCM WAV (PicoMite PLAY WAV).
    Returns True if a .wav was written, False if the source has no audio."""
    cmd = [
        ffmpeg, "-v", "error", "-y", "-i", src,
        "-vn",                       # drop video
        "-ac", "1" if mono else "2",
        "-ar", str(rate),
        "-c:a", "pcm_u8",            # unsigned 8-bit PCM, matches ba.aud
        "-f", "wav", dst,
    ]
    r = subprocess.run(cmd, stderr=subprocess.PIPE, text=True)
    err = (r.stderr or "")
    if r.returncode != 0 or not os.path.exists(dst) or os.path.getsize(dst) <= 44:
        if os.path.exists(dst) and os.path.getsize(dst) <= 44:
            try: os.remove(dst)   # drop the empty-WAV stub
            except OSError: pass
        # ffmpeg says this when the source has no audio stream
        if "does not contain any stream" in err or "matches no streams" in err:
            return False, "source has no audio track"
        msg = err.strip().splitlines()
        return False, (msg[-1] if msg else "no audio stream")
    return True, None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--w", type=int, default=320)
    ap.add_argument("--h", type=int, default=240)
    ap.add_argument("--fps", type=float, default=22.0)
    ap.add_argument("--arate", type=int, default=22050,
                    help="audio sample rate (Hz). Must be <= 44100*(CPUspeed/126000); "
                         "22050 is safe at any CPU speed. Default 22050.")
    ap.add_argument("--stereo", action="store_true",
                    help="keep 2 channels (default: downmix to mono, like ba.aud)")
    ap.add_argument("--no-audio", action="store_true",
                    help="skip audio extraction")
    ap.add_argument("--rgb332", action="store_true",
                    help="output raw RGB332 frames (1 byte/pixel, no header, no "
                         "compression) for direct framebuffer writes in MODE 5, "
                         "instead of the default RGB121 BLIT MEMORY format")
    ap.add_argument("--rgb332-rle", dest="rgb332_rle", action="store_true",
                    help="output count+value RLE RGB332 frames (MODE 5). ~3x "
                         "smaller than raw; needs the RGB332-RLE firmware decoder")
    args = ap.parse_args()
    if args.rgb332 and args.rgb332_rle:
        sys.exit("choose either --rgb332 or --rgb332-rle, not both")

    ffmpeg = find_ffmpeg()
    if ffmpeg is None:
        sys.exit("ffmpeg not found (install it on PATH, or 'pip install imageio-ffmpeg')")

    W, H = args.w, args.h
    if W >= 0x8000 or H >= 0x8000:
        sys.exit("width/height must be < 32768")

    is332 = args.rgb332 or args.rgb332_rle
    rl, gl, bl = build_rgb332_lut() if is332 else build_nibble_lut()

    cmd = [
        ffmpeg, "-v", "error", "-i", args.input,
        "-vf", f"fps={args.fps},scale={W}:{H}:flags=area",
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-",
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    frame_bytes = W * H * 3
    hdr = struct.pack("<HH", W | 0x8000, H)
    frames = 0
    total_out = 0
    with open(args.output, "wb") as f:
        while True:
            raw = proc.stdout.read(frame_bytes)
            if len(raw) < frame_bytes:
                break
            px = bytearray(W * H)
            j = 0
            for p in range(0, frame_bytes, 3):
                px[j] = rl[raw[p]] | gl[raw[p + 1]] | bl[raw[p + 2]]
                j += 1
            if args.rgb332:
                # raw 1 byte/pixel, no header, no length prefix - just the frame
                f.write(px)
                frames += 1
                total_out += len(px)
                if frames % 100 == 0:
                    print(f"  {frames} frames, {total_out/1024:.0f} KB", end="\r")
                continue
            if args.rgb332_rle:
                # [uint32 blobLen][uint16 w][uint16 h][count][value]...
                blob = struct.pack("<HH", W, H) + rle332_encode(px)
                f.write(struct.pack("<I", len(blob)))
                f.write(blob)
                frames += 1
                total_out += 4 + len(blob)
                if frames % 100 == 0:
                    print(f"  {frames} frames, {total_out/1024:.0f} KB", end="\r")
                continue
            rle = rle_encode(px)
            blob = hdr + rle
            f.write(struct.pack("<I", len(blob)))
            f.write(blob)
            frames += 1
            total_out += 4 + len(blob)
            if frames % 100 == 0:
                print(f"  {frames} frames, {total_out/1024:.0f} KB", end="\r")

    proc.wait()
    print(f"\nDone: {frames} frames, {total_out/1024:.1f} KB -> {args.output}")

    # --- audio: extract the source track in real time so it stays in sync ---
    aud_path = os.path.splitext(args.output)[0] + ".aud"
    aud_ok = False
    if not args.no_audio:
        ok, why = extract_audio(ffmpeg, args.input, aud_path, args.arate, not args.stereo)
        if ok:
            ch = "stereo" if args.stereo else "mono"
            kb = os.path.getsize(aud_path) / 1024
            print(f"Audio: {ch} {args.arate} Hz 8-bit -> {aud_path} ({kb:.0f} KB)")
            aud_ok = True
        else:
            print(f"Audio: skipped ({why})")

    if args.rgb332:
        print(f"\nFormat: raw RGB332, {W}x{H}x8 = {W*H} bytes/frame, no header. "
              f"Use MODE 5; copy each {W*H}-byte frame straight to the framebuffer.")
    elif args.rgb332_rle:
        ratio = (frames * W * H) / total_out if total_out else 0
        print(f"\nFormat: RGB332 count+value RLE, MODE 5 (~{ratio:.1f}x vs raw). "
              f"Needs the RGB332-RLE firmware decoder.")
    else:
        print(f"\nPlayer needs: {W}x{H} RGB121 framebuffer, totalFrames = {frames}")
    print(f"  totalFrames = {frames}")
    print(f"  vidFile = \"{os.path.basename(args.output)}\"")
    if aud_ok:
        print(f"  audFile = \"{os.path.basename(aud_path)}\"   (PLAY WAV)")
    print(f"  pace frames to {args.fps:g} fps so audio stays in sync "
          f"(targetTime = {1000.0/args.fps:.2f} ms/frame)")

if __name__ == "__main__":
    main()
