"""locvars6.py PORT - OPTION LOCAL VARIABLES above the default (bug report item 6).

The option moves the barrier between local and global variables anywhere from
32 to MAXVARS-32.  g_hashlist, which records every live local, used to be sized
for the default split only, so more live locals than the default wrote past it
(a hard fault on the PC3).  It now holds as many as the option can allow.

With the option at 400: 390 live locals across a recursion work and keep their
values; more than 399 stop with "Not enough Local variable memory".  (A second
deep pass is left out: the markers freed locals leave behind can fill the
region - bug report item 30.)"""
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Option Local Variables 400
Sub R(d)
  Local a,b,c,e,f,g,h,i,j
  a = d : j = d * 2
  If d > 0 Then R d - 1
  If a <> d Or j <> d * 2 Then Print "corrupt at"; d
End Sub
R 38
Print "390 locals ok"
R 45
Print "should not get here"
"""
b = pc3.PC3(sys.argv[1])
b.attention()
print("upload:", b.upload(SRC, 30))
b.drain(0.1)
b.send_line("RUN")
time.sleep(8)
out = pc3.ANSI.sub("", b._read())
print(out)
ok = ("390 locals ok" in out and "Not enough Local variable memory" in out
      and "FAULT" not in out and "corrupt" not in out and "should not" not in out)
print("LOCVARS6", "PASS" if ok else "FAIL")
b.close()
