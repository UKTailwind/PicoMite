"""flash.py PORT UF2 - UPDATE FIRMWARE over the console, copy the image to the boot drive."""
import serial, time, shutil, sys, string, os
port, uf2 = sys.argv[1], sys.argv[2]
def boot_drive():
    for d in string.ascii_uppercase:
        p = d + ":/INFO_UF2.TXT"
        try:
            if os.path.exists(p) and "RP2" in open(p).read():
                return d + ":/"
        except Exception:
            pass
    return None
if boot_drive():
    sys.exit("a boot drive is already present: " + boot_drive())
s = serial.Serial(port, 115200, timeout=0.2, write_timeout=2)
s.write(b"\x03"); time.sleep(0.3); s.write(b"\r"); time.sleep(0.3); s.read(8192)
s.write(b"UPDATE FIRMWARE\r"); time.sleep(0.5)
try: s.read(4096); s.close()
except Exception: pass
t0 = time.time(); d = None
while time.time() - t0 < 25 and not d:
    time.sleep(0.5); d = boot_drive()
if not d:
    sys.exit("NO BOOT DRIVE after 25 s - check the USB switch / lead")
print("boot drive", d, "after %.1fs:" % (time.time() - t0), open(d + "INFO_UF2.TXT").read().split("\n")[1:3])
t1 = time.time()
for attempt in range(4):  # the drive can still be mounting when it first appears (OSError 22)
    try:
        shutil.copy(uf2, d)
        break
    except OSError as ex:
        print("copy failed (%s), retrying" % ex)
        time.sleep(3)
else:
    sys.exit("COPY FAILED - the board is in its bootloader at " + d)
print("copied %s in %.1fs" % (os.path.basename(uf2), time.time() - t1))
