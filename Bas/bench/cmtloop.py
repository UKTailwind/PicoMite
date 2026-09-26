"""cmtloop.py PORT - loops and GOSUBs whose opening line ends in a comment.

NEXT, LOOP and RETURN come back into the middle of the opening line, onto the
comment.  ExecuteProgram's comment fast path used to take that comment as a
whole-line comment of the line last crossed and skip to the line after it,
out of the loop."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

SRC = """For i = 1 To 3 : ' c
  Print "i="; i
Next
Gosub sb : ' back here
Print "after gosub"
Do : ' d
  k = k + 1
  ' a comment-only line inside the loop
Loop Until k = 2
Print "k="; k
For j = 1 To 2 : ' outer
  For m = 1 To 2 : ' inner
    n = n + 1
  Next m
Next j
Print "n="; n
x = 1 : ' trailing, reached in order
' a comment-only line
Print "x="; x
End
sb:
Print "in sb"
Return
"""
WANT = ["i= 1", "i= 2", "i= 3", "in sb", "after gosub", "k= 2", "n= 4", "x= 1"]

b = pc3.PC3(sys.argv[1])
b.attention()
print("upload:", b.upload(SRC, 30))
out = pc3.ANSI.sub("", b.run(30))
print(out)
got = [l.strip() for l in out.splitlines() if l.strip() and l.strip() not in ("RUN", ">")]
print("CMTLOOP", "PASS" if got == WANT else "FAIL (want %s)" % WANT)
b.close()
