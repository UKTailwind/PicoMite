"""Remove C comments from TinyCC ROM-header bytes, not from source headers.

Preserve quoted literals and line endings outside comments. Each C comment
is a single space; the newline terminating a // comment is preserved.
"""
import sys
from pathlib import Path


def strip_comments(data: bytes) -> bytes:
    result = bytearray()
    i = 0
    size = len(data)
    while i < size:
        c = data[i]
        if c in (ord('"'), ord("'")):
            quote = c
            result.append(c)
            i += 1
            while i < size:
                c = data[i]
                result.append(c)
                i += 1
                if c == 92 and i < size:  # backslash-escaped byte
                    result.append(data[i])
                    i += 1
                elif c == quote:
                    break
            continue
        if c == 47 and i + 1 < size and data[i + 1] in (42, 47):
            block = data[i + 1] == 42
            result.append(32)  # a comment is whitespace in C
            i += 2
            closed = not block
            while i < size:
                if block and data[i:i + 2] == b'*/':
                    i += 2
                    closed = True
                    break
                if not block and data[i] in (10, 13):
                    result.append(data[i])
                if not block and data[i] == 10:
                    i += 1
                    break
                i += 1
            if not closed:
                raise ValueError("unterminated block comment")
            continue
        result.append(c)
        i += 1
    return bytes(result)


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: tcc_strip_header_comments.py output_dir header...")
    destination = Path(sys.argv[1])
    destination.mkdir(parents=True, exist_ok=True)
    for n, filename in enumerate(sys.argv[2:]):
        original = Path(filename).read_bytes()
        (destination / f"{n}.h").write_bytes(strip_comments(original))


if __name__ == "__main__":
    main()
