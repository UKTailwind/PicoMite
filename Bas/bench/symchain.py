"""symchain.py PORT - CHAIN keeps variables: check the chained program still
binds each name to its own variable when the two programs number their
symbols differently.

Program A (in program memory) uses x more than y, so x gets the lower symbol
id; the chained file uses y more than x, so there the ids are the other way
round.  The chained program must print x= 5 y= 7."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

A = """Dim x = 5, y = 7
x = x + 0 : x = x + 0 : x = x + 0
Print "A: x="; x; " y="; y
Chain "A:/chb.bas"
"""
B = """y = y + 0 : y = y + 0 : y = y + 0
Print "B: x="; x; " y="; y
Dim z = x * 10 + y
Print "B: z="; z
End
"""
# SAVE CONTEXT / LOAD CONTEXT inside one running program: the bindings are
# rebuilt twice while it runs
C = """Dim x = 5, y = 7
x = x + 0 : x = x + 0
Save Context
x = 1 : y = 2 : Dim q = 3
Load Context
Print "C: x="; x; " y="; y
Dim w = x * 10 + y
Print "C: w="; w
y = y + 0 : y = y + 0 : y = y + 0
Print "C: y="; y
End
"""
b = pc3.PC3(sys.argv[1])
b.attention()
print(b.xmodem_send("A:/chb.bas", B.replace("\n", "\r\n").encode("latin-1")).strip()[-80:])
print("upload:", b.upload(A, 30))
out = pc3.ANSI.sub("", b.run(30))
print(out)
ok = "B: x= 5 y= 7" in out and "B: z= 57" in out
print("CHAIN-SYMBOLS", "PASS" if ok else "FAIL")
print("upload:", b.upload(C, 30))
out = pc3.ANSI.sub("", b.run(30))
print(out)
ok = "C: x= 5 y= 7" in out and "C: w= 57" in out and "C: y= 7" in out
print("CONTEXT-SYMBOLS", "PASS" if ok else "FAIL")
b.close()
