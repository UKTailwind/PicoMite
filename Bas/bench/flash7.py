"""flash7.py PORT UF2 - flash.py for a native USB CDC board (the RP2040 VGA rig):
after the copy, wait for the port to go and come back, then settle, before
anything opens it.  Opening it while the board re-enumerates wedged Windows'
CDC driver on 2026-09-28 (the board needed a hardware reset)."""
import subprocess, sys, time, os
import serial.tools.list_ports as lp

port, uf2 = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
r = subprocess.run([sys.executable, os.path.join(here, "flash.py"), port, uf2])
if r.returncode:
    sys.exit(r.returncode)
present = lambda: any(p.device == port for p in lp.comports())
t0 = time.time()
while present() and time.time() - t0 < 15:  # the reboot drops it
    time.sleep(0.25)
t1 = time.time()
while not present() and time.time() - t1 < 30:
    time.sleep(0.25)
if not present():
    sys.exit("%s did not come back" % port)
time.sleep(5)  # let the firmware finish booting before the port is opened
print("%s back %.1fs after the copy" % (port, time.time() - t0))
