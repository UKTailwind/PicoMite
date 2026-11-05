#!/usr/bin/env python3
"""Convert FreeBSD 8x16 hex fonts to PicoMite DEFINEFONT #10 BASIC files.

Sources:
  https://github.com/emaste/fontstuff/tree/master/syscons-fonts

Uses only the Python standard library. Files contain ASCII-only BASIC source.
"""
from pathlib import Path
import re
import urllib.request

SOURCES = {
    "cp1251": "cp1251-8x16.hex",
}
BASE_URL = "https://raw.githubusercontent.com/emaste/fontstuff/master/syscons-fonts/"
FONT_NUMBER = 10
START = 0x20
COUNT = 0xE0
WIDTH = 8
HEIGHT = 16


def parse_hex(text):
    glyphs = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"([0-9a-fA-F]{4}):([0-9a-fA-F]{32})", line)
        if not match:
            raise ValueError(f"Unexpected hex glyph line: {line!r}")
        code = int(match.group(1), 16)
        glyphs[code] = bytes.fromhex(match.group(2))
    missing = [code for code in range(START, START + COUNT) if code not in glyphs]
    if missing:
        raise ValueError(f"Missing glyphs: {missing!r}")
    return glyphs


def build_font(glyphs):
    # In memory: width, height, first character, number of characters.
    header = bytes([WIDTH, HEIGHT, START, COUNT])
    bitmap = b"".join(glyphs[code] for code in range(START, START + COUNT))
    blob = header + bitmap
    assert len(blob) == 4 + COUNT * HEIGHT
    assert len(blob) % 4 == 0
    # PicoMite stores each 8-hex-digit word as uint32_t on little-endian ARM.
    words = [f"{int.from_bytes(blob[i:i+4], 'little'):08X}" for i in range(0, len(blob), 4)]
    assert words[0] == "E0201008", words[0]
    lines = [f"' 8x16 bitmap font, codes 0x20..0xFF", f"DefineFont #{FONT_NUMBER}", f"  {words[0]}"]
    for i in range(1, len(words), 8):
        lines.append("  " + " ".join(words[i:i+8]))
    lines.append("End DefineFont")
    return "\r\n".join(lines) + "\r\n"


def build_demo(codec, font_file, font_source):
    sample = "Привет, мир!"
    codes = sample.encode(codec)
    expr = "+".join(f"Chr$(&H{code:02X})" for code in codes)
    # Keep the font inside the demo: loading another program clears user fonts.
    return (f"' Demo for {font_file}\r\n"
            f"Font #{FONT_NUMBER}\r\n"
            f"Cls\r\n"
            f"Print {expr}\r\n"
            f"For i = 128 To 255\r\n"
            f"  Print Chr$(i);\r\n"
            f"  If i Mod 16 = 15 Then Print\r\n"
            f"Next i\r\n"
            f"End\r\n\r\n"
            + font_source)


def main():
    for codec, file_name in SOURCES.items():
        with urllib.request.urlopen(BASE_URL + file_name, timeout=30) as response:
            text = response.read().decode("ascii")
        glyphs = parse_hex(text)
        output = Path(f"picomite-{codec}-8x16.bas")
        font_source = build_font(glyphs)
        output.write_bytes(font_source.encode("ascii"))
        # Demo is self-contained; the font-only file is available for LIBRARY.
        demo = Path(f"picomite-{codec}-demo.bas")
        demo.write_bytes(build_demo(codec, output.name, font_source).encode("ascii"))
        print(f"{output} ({output.stat().st_size} bytes) + {demo}")


if __name__ == "__main__":
    main()
