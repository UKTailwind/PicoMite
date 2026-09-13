"""Minimal PicoMite console driver for the PC3 on a UART bridge.

Rules from the project notes: pace characters (~2 ms) and lines (>=25 ms),
wait for the real "> " prompt (ANSI stripped) before the next command,
finish AUTOSAVE with Ctrl-Z and wait for the flash write.

Usage:
  pc3.py probe                       board identity, options, memory
  pc3.py cmd "PRINT MM.VER" [...]    run interactive commands
  pc3.py run file.bas [timeout_s]    AUTOSAVE the file into program memory, RUN, capture output
  pc3.py put local devpath           send a file to the device over XMODEM
"""
import os, re, sys, time
import serial
import serial.tools.list_ports

# The UART bridge re-enumerates on a different number whenever it is moved
# to another USB socket, so find it rather than insisting on one name.
def find_port(preferred="COM3"):
    ports = list(serial.tools.list_ports.comports())
    names = [p.device for p in ports]
    # An explicit choice wins: there can be more than one CH340 on the bench,
    # and talking to the wrong board is worse than refusing to guess.
    forced = os.environ.get("PC3_PORT")
    if forced:
        if forced not in names:
            raise IOError("PC3_PORT=%s is not present; found %s" % (forced, names))
        return forced
    if preferred in names:
        return preferred
    ch340 = [p.device for p in ports if "1A86:7523" in (p.hwid or "").upper()]
    if len(ch340) == 1:
        return ch340[0]
    if len(names) == 1:
        return names[0]
    raise IOError("cannot tell which port the PC3 is on: %s - set PC3_PORT" % (names or "none"))


PORT = None
BAUD = 115200
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[=>]|\r")
CHAR_GAP = 0.002
LINE_GAP = 0.03
FAST_LINE_GAP = 0.006   # AUTOSAVE N: no echo coming back, so lines can go out quickly


class PC3:
    def __init__(self, port=None):
        self.port = port or find_port()
        self.s = serial.Serial(self.port, BAUD, timeout=0.05, write_timeout=2)
        self.log = []

    def close(self):
        self.s.close()

    def _read(self):
        b = self.s.read(4096)
        if b:
            t = b.decode("latin-1")
            self.log.append(t)
            return t
        return ""

    def drain(self, quiet=0.2):
        out, last = "", time.time()
        while time.time() - last < quiet:
            t = self._read()
            if t:
                out += t
                last = time.time()
        return out

    def wait_prompt(self, timeout=10.0):
        """Collect output until the console shows its '> ' prompt."""
        buf, t0 = "", time.time()
        while time.time() - t0 < timeout:
            t = self._read()
            if t:
                buf += t
                clean = ANSI.sub("", buf)
                if clean.rstrip(" ").endswith("\n>") or clean.endswith("> ") or clean == ">" or clean.endswith("\n> "):
                    # make sure nothing else is queued behind the prompt
                    time.sleep(0.05)
                    tail = self._read()
                    if not tail:
                        return ANSI.sub("", buf)
                    buf += tail
        raise TimeoutError("no prompt after %.0fs; last output:\n%s" % (timeout, ANSI.sub("", buf)[-800:]))

    def send_raw(self, text):
        for ch in text:
            self.s.write(ch.encode("latin-1"))
            time.sleep(CHAR_GAP)

    def send_line(self, line):
        self.send_raw(line + "\r")
        time.sleep(LINE_GAP)

    def attention(self):
        """Ctrl-C twice, then wait for a prompt."""
        self.s.write(b"\x03")
        time.sleep(0.3)
        self.s.write(b"\x03")
        time.sleep(0.3)
        self.drain(0.3)
        self.s.write(b"\r")
        return self.wait_prompt(5)

    def cmd(self, line, timeout=10.0):
        self.drain(0.05)
        self.send_line(line)
        out = self.wait_prompt(timeout)
        # strip the echo of the command and the trailing prompt
        lines = out.split("\n")
        body = [l for l in lines if l.strip() not in ("", ">", line.strip())]
        return "\n".join(body)

    def upload(self, source, timeout=30.0):
        """AUTOSAVE the source into program memory."""
        self.drain(0.05)
        # The N suppresses the console echo for the transfer, so the device is
        # not sending every character back while we are still talking.  That
        # removes the reason for pacing characters, and a whole line can go out
        # in a single write.
        self.send_line("AUTOSAVE N")
        time.sleep(0.4)
        self.drain(0.2)
        n = 0
        for raw in source.splitlines():
            self.s.write((raw.rstrip("\r\n") + "\r").encode("latin-1"))
            n += 1
            time.sleep(FAST_LINE_GAP)
            if n % 32 == 0:
                self._read()
        time.sleep(0.2)
        self.s.write(b"\x1a")  # Ctrl-Z
        out = self.wait_prompt(timeout)
        m = re.search(r"Saved\s+(\d+)\s+bytes", out)
        return (int(m.group(1)) if m else None, n)

    def run(self, timeout=300.0):
        self.drain(0.05)
        self.send_line("RUN")
        out = self.wait_prompt(timeout)
        return out

    # ---------------------------------------------------------------- XMODEM
    def _read_exact(self, n, deadline):
        buf = b""
        while len(buf) < n and time.time() < deadline:
            buf += self.s.read(n - len(buf))
        return buf

    def xmodem_receive(self, timeout=120.0):
        """Receive one file from the device's XMODEM SEND (CRC mode, 128 or 1024 byte packets)."""
        SOH, STX, EOT, ACK, NAK, CAN = 1, 2, 4, 6, 0x15, 0x18
        data, expected, start = b"", 1, time.time()
        self.s.reset_input_buffer()
        self.s.write(b"C")
        while time.time() - start < timeout:
            hdr = self._read_exact(1, time.time() + 1.5)
            if not hdr:
                if not data:
                    self.s.write(b"C")      # keep asking until the first packet arrives
                continue
            c = hdr[0]
            if c == EOT:
                self.s.write(bytes([ACK]))
                return data
            if c == CAN:
                raise IOError("sender cancelled")
            if c not in (SOH, STX):
                continue                    # console noise before the transfer starts
            size = 128 if c == SOH else 1024
            rest = self._read_exact(2 + size + 2, time.time() + 3.0)
            if len(rest) < 2 + size + 2:
                self.s.write(bytes([NAK]))
                continue
            blk, nblk = rest[0], rest[1]
            payload = rest[2:2 + size]
            crc_rx = (rest[2 + size] << 8) | rest[3 + size]
            if blk != (~nblk & 0xFF) or crc16(payload) != crc_rx:
                self.s.write(bytes([NAK]))
                continue
            if blk == (expected & 0xFF):
                data += payload
                expected += 1
            self.s.write(bytes([ACK]))      # duplicate of the previous block: ACK, don't store
        raise TimeoutError("XMODEM receive timed out with %d bytes" % len(data))

    def xmodem_send(self, devpath, data, timeout=180.0):
        """Send one file to the device: XMODEM R on the console, we do the sending.

        The receiver asks with 'C' for CRC mode or NAK for the old checksum, and
        we answer in whichever it asked for.  Short files are padded to the
        128-byte block with ctrl-Z, which is what every XMODEM has always done.
        """
        SOH, EOT, ACK, NAK, CAN = 1, 4, 6, 0x15, 0x18
        self.drain(0.05)
        self.send_line('XMODEM R "%s"' % devpath)
        time.sleep(0.3)
        crc, t0 = None, time.time()
        while time.time() - t0 < 30:
            b = self.s.read(1)
            if not b:
                continue
            if b[0] == ord("C"):
                crc = True
                break
            if b[0] == NAK:
                crc = False
                break
        if crc is None:
            raise TimeoutError("the device never asked for the file")
        blk, pos = 1, 0
        while pos < len(data):
            chunk = data[pos:pos + 128]
            chunk = chunk + b"" * (128 - len(chunk))
            pkt = bytes([SOH, blk & 0xFF, (~blk) & 0xFF]) + chunk
            if crc:
                c = crc16(chunk)
                pkt += bytes([c >> 8, c & 0xFF])
            else:
                pkt += bytes([sum(chunk) & 0xFF])
            for _ in range(10):
                self.s.write(pkt)
                r = self._read_exact(1, time.time() + 5)
                if r and r[0] == ACK:
                    break
                if r and r[0] == CAN:
                    raise IOError("the device cancelled the transfer")
            else:
                raise IOError("block %d was never acknowledged" % blk)
            pos += 128
            blk += 1
        for _ in range(5):
            self.s.write(bytes([EOT]))
            r = self._read_exact(1, time.time() + 3)
            if r and r[0] == ACK:
                break
        return self.wait_prompt(30)

    def grab(self, devpath, timeout=180.0):
        """Fetch a file from the device: XMODEM SEND on the console, receive it here."""
        self.drain(0.05)
        self.send_line('XMODEM S "%s"' % devpath)
        time.sleep(0.5)
        data = self.xmodem_receive(timeout)
        self.wait_prompt(15)
        return data


def crc16(buf):
    crc = 0
    for b in buf:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def trim_bmp(data):
    if data[:2] == b"BM" and len(data) >= 6:
        n = int.from_bytes(data[2:6], "little")
        if 0 < n <= len(data):
            return data[:n]
    return data.rstrip(b"\x1a\x00")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    b = PC3()
    try:
        b.attention()
        what = sys.argv[1]
        if what == "probe":
            for c in ["PRINT MM.VER", "PRINT MM.DEVICE$", "PRINT MM.INFO(CPUSPEED)", "PRINT MM.INFO(PSRAM SIZE)",
                      "PRINT MM.HRES, MM.VRES", "PRINT MM.INFO(HEAP)", "MEMORY", "OPTION LIST"]:
                print("--- " + c)
                print(b.cmd(c, 15))
        elif what == "cmd":
            for c in sys.argv[2:]:
                print("--- " + c)
                print(b.cmd(c, 30))
        elif what == "run":
            src = open(sys.argv[2], encoding="utf-8").read()
            to = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0
            saved, n = b.upload(src)
            print("uploaded %d lines, device saved %s bytes" % (n, saved))
            if saved is None:
                print("WARNING: no 'Saved' confirmation")
            print(b.run(to))
        elif what == "put":
            # pc3.py put <local file> <device path>
            local, dev = sys.argv[2], sys.argv[3]
            data = open(local, "rb").read()
            b.xmodem_send(dev, data)
            print("%s -> %s (%d bytes)" % (local, dev, len(data)))
        elif what == "grab":
            # pc3.py grab <device path> <local file> [more pairs...]; .bmp is also written as .png
            pairs = sys.argv[2:]
            for i in range(0, len(pairs) - 1, 2):
                dev, local = pairs[i], pairs[i + 1]
                data = trim_bmp(b.grab(dev))
                open(local, "wb").write(data)
                msg = "%s -> %s (%d bytes)" % (dev, local, len(data))
                if local.lower().endswith(".bmp"):
                    try:
                        from PIL import Image
                        png = local[:-4] + ".png"
                        Image.open(local).convert("RGB").save(png)
                        msg += " + " + png
                    except Exception as ex:
                        msg += " (png conversion failed: %s)" % ex
                print(msg)
    finally:
        b.close()


if __name__ == "__main__":
    main()
