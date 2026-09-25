"""tftp_bench.py PORT HOST - TFTP throughput to and from the board, first at the
prompt, then while a busy BASIC loop runs (ProcessWeb is then called from the
per-statement path). 128 KB, sent and read back, checked byte for byte."""
import os, sys, time
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/exile_tools")
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3, tftp

port, host = sys.argv[1], sys.argv[2]
data = os.urandom(128 * 1024)


def transfer(label):
    t0 = time.time()
    tftp.send(host, "tftpb.bin", data)
    t1 = time.time()
    back = tftp.fetch(host, "tftpb.bin")
    t2 = time.time()
    ok = back[:len(data)] == data
    print("%-8s send %6.1f KB/s  fetch %6.1f KB/s  %s" % (
        label, len(data) / 1024 / (t1 - t0), len(data) / 1024 / (t2 - t1), "OK" if ok else "MISMATCH"), flush=True)


b = pc3.PC3(port)
b.attention()
b.cmd('Chdir "A:/"', 10)
b.close()
for k in range(2):
    transfer("prompt")

b = pc3.PC3(port)
b.attention()
b.xmodem_send("busy.bas", b"Dim a%\r\nDo\r\n  a% = a% + 1\r\nLoop\r\n")
b.drain(0.3)
print(b.cmd('LOAD "A:/busy.bas"', 20).strip()[-40:])
b.drain(0.1)
b.send_line("RUN")
time.sleep(1.0)
try:
    for k in range(2):
        transfer("running")
finally:
    b.attention()
    b.close()
