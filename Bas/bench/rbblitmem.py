"""rbblitmem.py PORT - Route B: BLIT MEMORY through the command splice, and
SPRITE MEMORY, which the tokeniser turns into it.  Its address (GetPeekAddr,
which is getinteger), x and y (getinteger) and transparent colour (getint) are
values: from an array element as PETSCII Robots passes them, floats rounded
as getinteger rounds them, clipped at every edge, the fast even-x copy and
the nibble path.  Every program runs with OPTION COMPILE OFF, ON and SHADOW:
all three must print the same, errors included; where the forms should
compile, the ON run's statements run as text (RAN - CODE) must stay under the
limit (the loop's 160 blits would pass it as text).  Leaves OPTION COMPILE as
it found it, and the display in MODE 1."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
TILE = """Dim Integer t(20), ti(3)
a = Peek(VarAddr t())
Poke Short a, 8 : Poke Short a + 2, 6
For k = 0 To 23 : Poke Byte a + 4 + k, (k * 37 + 11) And 255 : Next
ti(1) = a
"""
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("BLIT MEMORY and SPRITE MEMORY: edges, floats, colours", """Mode 2
CLS
Dim Integer k, ckx, cky, ck, a
Dim Float fx
""" + TILE + """For k = 0 To 39
  Blit Memory a, k * 7 - 4, (k * 13) Mod 230 - 3
  Sprite Memory ti(1), 2 * k + 1, 100 + (k Mod 5), k Mod 16
  fx = k * 2.6
  Blit Memory a + 0, fx, 150.4 + k / 3, 5
  Blit Memory ti(1), 316 - k \\ 4, 236 + (k Mod 3)
Next
zst$ = MM.Info(COMPILE)
For cky = 0 To 239 : For ckx = 0 To 319 : ck = ck + Pixel(ckx, cky) * (1 + ((ckx + 3 * cky) And 15)) : Next : Next
Mode 1
Print "CK"; ck
Print "STAT "; zst$
""", 45),
    ("BLIT MEMORY with two arguments (SyntaxError)", """Dim Integer k, a
""" + TILE + """Blit Memory a, 1
Print "not reached"
""", None),
    ("BLIT MEMORY colour 16 (getint's error)", """Mode 2
Dim Integer k, a
""" + TILE + """Blit Memory a, 1, 1, 16
Print "not reached"
""", None),
    ("BLIT MEMORY with a string x (stays text: getinteger's error)", """Mode 2
Dim Integer k, a
""" + TILE + """Blit Memory a, "5", 1
Print "not reached"
""", None),
]

b = pc3.PC3(sys.argv[1])
b.attention()
was = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
    b.cmd("MODE 1", 10)
    lines = [l for l in out.split("\n") if l.strip() and l.strip() not in ("RUN", ">")]
    stat = [l for l in lines if l.startswith("STAT ")]
    return [l for l in lines if not l.startswith("STAT ")], (stat[0][5:] if stat else "")


for name, src, most_text in PROGS:
    outs = {}
    tail = "" if "zst$" in src else STAT
    for mode in ("OFF", "ON", "SHADOW"):
        b.cmd("OPTION COMPILE " + mode, 10)
        outs[mode] = run(src + tail)
    same = outs["OFF"][0] == outs["ON"][0] == outs["SHADOW"][0]
    m = re.search(r"RAN (\d+) MISS \d+ CODE (\d+)", outs["ON"][1])
    text = int(m.group(1)) - int(m.group(2)) if m else -1
    good = same and (most_text is None or 0 <= text <= most_text)
    ok = ok and good
    print("%-4s %-60s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBBLITMEM", "PASS" if ok else "FAIL")
b.close()
sys.exit(0 if ok else 1)
