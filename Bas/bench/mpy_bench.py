"""Phase 0 anchor driver for Peter's MicroPython PC3 port.

usage:
  mpy_bench.py PORT put LOCAL REMOTE
  mpy_bench.py PORT run MODULE RUNS [LABEL]

'put' writes a file through the raw REPL in checked chunks.  'run' imports the
module fresh RUNS times (it calls main() on import), answering the five
input() calls with the reference inputs once the output goes quiet, and
prints one JSON line per run with the time and the golden check.
"""
import json, re, serial, sys, time

INPUTS = ["12,1,2000", "39,40,36", "-104,57,12", "1644", "30"]
GOLDEN = ["2451904.14541560", "2451904.19690359", "2451904.25420764"]


class Repl:
    def __init__(self, port):
        self.s = serial.Serial(port, 115200, timeout=0.05, write_timeout=5)

    def read_until(self, tok, timeout):
        buf, end = b"", time.time() + timeout
        while time.time() < end:
            buf += self.s.read(4096)
            if tok in buf:
                return buf
        raise TimeoutError("waiting for %r; got %r" % (tok, buf[-300:]))

    def interrupt(self):
        self.s.write(b"\r\x03\x03")
        time.sleep(0.3)
        self.s.read(65536)

    def raw_exec(self, code, timeout=30):
        """Run code in the raw REPL; return (stdout, stderr)."""
        self.s.write(b"\x01")                     # enter raw REPL
        self.read_until(b"raw REPL; CTRL-B to exit\r\n>", 5)
        data = code.encode()
        for i in range(0, len(data), 256):
            self.s.write(data[i:i + 256])
            time.sleep(0.01)
        self.s.write(b"\x04")
        out = self.read_until(b"\x04>", timeout)  # "OK" + stdout + \x04 + stderr + \x04>
        self.s.write(b"\x02")                     # back to the friendly REPL
        self.read_until(b">>> ", 5)
        body = out[out.index(b"OK") + 2:-2]
        o, _, e = body.partition(b"\x04")
        return o.decode(errors="replace"), e.decode(errors="replace")

    def put(self, local, remote):
        data = open(local, "rb").read()
        o, e = self.raw_exec("f=open(%r,'wb')\nf.close()" % remote)
        if e:
            raise IOError(e)
        for i in range(0, len(data), 1024):
            o, e = self.raw_exec("f=open(%r,'ab')\nf.write(%r)\nf.close()" % (remote, data[i:i + 1024]))
            if e:
                raise IOError(e)
        o, e = self.raw_exec("import os\nprint(os.stat(%r)[6])" % remote)
        return int(o.strip()), len(data)

    def run_module(self, module, timeout=240):
        self.s.write(("import sys\r").encode()); time.sleep(0.2)
        self.s.write(("sys.modules.pop(%r, None)\r" % module).encode()); time.sleep(0.2)
        self.s.read(65536)
        self.s.write(("import %s\r" % module).encode())
        buf, sent, last, t0 = b"", 0, time.time(), time.time()
        while time.time() - t0 < timeout:
            b = self.s.read(4096)
            if b:
                buf += b
                last = time.time()
                if buf.rstrip().endswith(b">>>") and (b"Time taken" in buf or b"Traceback" in buf):
                    break  # finished, or died with a traceback
                continue
            if sent < len(INPUTS) and time.time() - last > 0.6 and b"input" in buf.lower():
                self.s.write((INPUTS[sent] + "\r").encode())
                sent += 1
                last = time.time()
        text = buf.decode(errors="replace")
        m = re.search(r"Time taken\s*:\s*([0-9.]+)", text)
        err = re.search(r"Traceback[\s\S]*", text)
        return {"seconds": float(m.group(1)) if m else None,
                "jd": re.findall(r"24519\d\d\.\d+", text),
                "golden": all(g in text for g in GOLDEN),
                "error": err.group(0)[-300:] if err else None,
                "tail": "" if m else text[-400:]}


def main():
    port, what = sys.argv[1], sys.argv[2]
    r = Repl(port)
    r.interrupt()
    if what == "put":
        got, want = r.put(sys.argv[3], sys.argv[4])
        print("put %s -> %s: device %d bytes, local %d" % (sys.argv[3], sys.argv[4], got, want))
    elif what == "run":
        module, runs = sys.argv[3], int(sys.argv[4])
        label = sys.argv[5] if len(sys.argv) > 5 else module
        o, e = r.raw_exec("import sys, machine\nprint(sys.implementation._machine, machine.freq())")
        for n in range(runs):
            res = r.run_module(module)
            res.update({"label": label, "run": n + 1, "device": o.strip()})
            print(json.dumps(res), flush=True)


if __name__ == "__main__":
    main()
