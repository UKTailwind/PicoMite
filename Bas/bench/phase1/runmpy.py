"""runmpy.py PORT MODULE OUTFILE [TIMEOUT] - run one bench module on Peter's MicroPython port.

Imports MODULE fresh at the friendly REPL (it runs on import) and saves
everything printed until the REPL prompt returns after BENCHEND, or after a
Traceback. Bas/bench/mpy_bench.py 'run' waits for the anchor's "Time taken"
line, so it does not suit the BENCH suites. Upload first with
  mpy_bench.py PORT put bench_gfx.py bench_gfx.py
(relative paths only on that board).
"""
import sys, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/bench")
from mpy_bench import Repl


def main():
    port, module, outf = sys.argv[1:4]
    timeout = float(sys.argv[4]) if len(sys.argv) > 4 else 300.0
    r = Repl(port)
    r.interrupt()
    r.s.write(b"import sys\r")
    time.sleep(0.2)
    r.s.write(("sys.modules.pop(%r, None)\r" % module).encode())
    time.sleep(0.2)
    r.s.read(65536)
    r.s.write(("import %s\r" % module).encode())
    buf, t0 = b"", time.time()
    while time.time() - t0 < timeout:
        chunk = r.s.read(4096)
        if not chunk:
            continue
        buf += chunk
        if buf.rstrip().endswith(b">>>") and (b"BENCHEND" in buf or b"Traceback" in buf):
            break
    else:
        buf += b"\n[runmpy: timeout]\n"
    text = buf.decode(errors="replace").replace("\r", "")
    open(outf, "w", encoding="utf-8").write(text)
    print(text[-6000:])
    print("[runmpy: %.1f s]" % (time.time() - t0))


if __name__ == "__main__":
    main()
