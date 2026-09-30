"""textjust.py PORT - TEXT's justification written as a bare word (first V7.0.00b1 report).

TEXT x, y, s$, CM worked in 6.03 because cmd_text reads the argument's own
letters before trying it as a string expression.  From V7 a saved program
stores every name as a symbol (core/Symbols.h), and TEXT is one of the commands
given its statement with the symbols left in, so the letters were gone and
the bare form stopped with "Expected a string" (the prompt, which is not
stored as symbols, still worked).  GetJustificationArg reads a lone symbol's
spelling.  Everything else must behave as in 6.03: quoted, string variables,
expressions, an empty justification, and the errors for a word or number that
is not one.  Run it with OPTION COMPILE OFF and ON on an RP2350.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

PROG = '''Dim s$
Dim J$ = "RB"
Dim C As STRING = "LT"
Text 10, 10, "a", C
Text 10, 20, "b", CM
Text 10, 30, "c", cm
Text 10, 40, "d", RBV
Text 10, 50, "e", "CM"
Text 10, 60, "f", J$
s$ = "L" : Text 10, 70, "g", s$ + "T"
Text 10, 80, "h", , 1
Text 10, 90, "i", LT, 1, 1
Print "all ok"
On Error Skip
Text 10, 100, "j", XYZ
Print "bad word: "; MM.ErrMsg$
On Error Skip
Text 10, 110, "k", 5
Print "number: "; MM.ErrMsg$
'''
WANT = "all ok | bad word: Expected a string | number: Expected a string"

b = pc3.PC3(sys.argv[1])
b.attention()
prompt = pc3.ANSI.sub("", b.cmd('Text 10, 120, "p", C : ? "prompt ok"', 10))
b.upload(PROG, 30)
b.drain(0.1)
out = pc3.ANSI.sub("", b.run(30)).replace("\r", "")
got = " | ".join(l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">"))
print("prompt:", "ok" if "prompt ok" in prompt else prompt.strip())
print("program:", got)
print("TEXTJUST", "PASS" if got == WANT and "prompt ok" in prompt else "FAIL")
b.close()
