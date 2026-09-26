"""argleak16.py PORT - an error in a FUNCTION's arguments under ON ERROR
IGNORE must not leak memory (bug report item 16).

The error handler undid the call's frame but freed nothing: on the RP2040 the
2.8 KB argument block of every such call was lost (about 35 errors used up the
VGA's heap), and on both chips the copy of a string argument.  50 skipped
errors in a numeric argument and 50 after a string argument must leave the heap
where it was, and a call must still work.  (The nested-call case,
Foo(Bar(1), 1/z), is left as it is: see the report.)"""
import sys, os, re, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """Function Foo(a, b)
  Foo = a + b
End Function
Function Fs(a$, b)
  Fs = Len(a$) + b
End Function
Dim i, y, z = 0, h0, h1
y = Foo(1, 2) : y = Fs("x", 1)
h0 = MM.Info(HEAP)
On Error Ignore
For i = 1 To 50
  y = Foo(1 / z, 1)
  y = Fs("abc", 1 / z)
Next
On Error Abort
h1 = MM.Info(HEAP)
Print "HEAP"; h0 - h1
Print "CALL"; Foo(1, 2); Fs("abc", 1)
"""

b = pc3.PC3(sys.argv[1])
b.attention()
b.upload(SRC, 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(60))
lines = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
print(" | ".join(lines))
ok = len(lines) == 2 and lines[0] == "HEAP 0" and re.findall(r"-?\d+", lines[1]) == ["3", "4"]
print("ARGLEAK16", "PASS" if ok else "FAIL")
b.close()
