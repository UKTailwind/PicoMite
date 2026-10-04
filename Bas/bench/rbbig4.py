"""rbbig4.py PORT - Route B: the four largest forms items 5 and 8 left as text
(docs/Interpreter_RouteB_Coverage.html, 4 October): VAL( as the whole of
LET's right-hand side (its answer converted to the target's type in the
splice, as evaluate converts for cmd_let, so the type the string gives does
not matter); CHOICE( (the condition made an int as fun_ternary's C does it,
then only the branch it picks; both branches of one type); MM.INFO( for the
keywords whose type the compiler knows (its text spelled out as getvalue
gives it fun_info); and BLIT FLASH, FRAMEBUFFER, READ, WRITE, CLOSE and the
plain BLIT through the command splice.  Every program runs with OPTION
COMPILE OFF, ON and SHADOW: all three must print the same, errors included;
where the forms should compile, the ON run's statements run as text (RAN -
CODE) must stay under the limit.  Leaves OPTION COMPILE as it found it."""
import sys, os, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "elite_tools"))
import pc3

STAT = 'Print "STAT "; MM.Info(COMPILE)\n'
# (name, program, most statements run as text with OPTION COMPILE ON, or None)
PROGS = [
    ("VAL alone into a float and an integer", """Dim Integer k, i, n
Dim Float v, tot
Dim String s, t(7) = ("12", "-7", "2.5", "&H1F", "1e3", "9007199254740993", "", "3.7abc")
For k = 0 To 7
  s = t(k) : v = Val(s) : i = Val(s)
  Print k; " "; v; " "; i
Next
For k = 1 To 400
  s = t(k Mod 8)
  v = Val(s)
  i = Val(s)
  tot = tot + v : n = n + i
Next
Print tot; " "; n
""", 430),
    ("VAL in a single-line IF, and the forms that stay text", """Dim Integer k, i
Dim Float v, w
Dim String s = "41.5"
For k = 1 To 300
  If k Mod 2 Then v = Val(s) Else i = Val(s)
Next
w = Val(s) * 2 : i = Val(s) + 1
Print v; " "; i; " "; w
""", 20),
    ("VAL of a number: evaluate's error", """Dim Float v
v = Val(5)
Print v
""", None),
    ("CHOICE: types, truncation, one branch run", """Dim Integer k, a, cnt
Dim Float f, g
Dim String s
Function Hit(x)
  cnt = cnt + 1
  Hit = x
End Function
For k = -3 To 3
  f = k / 2
  a = Choice(f, 10, 20)
  g = Choice(k, 1.5, 2.5)
  s = Choice(k > 0, "pos", "nonpos")
  a = a + Choice(k = 0, Hit(1), Hit(2))
  Print k; " "; a; " "; g; " "; s; " "; cnt
Next
For k = 1 To 400
  a = Choice(k Mod 3, a + 1, a - 1)
  s = Choice(k And 1, s + "x", Left$(s, 3))
  g = Choice(-0.99 + k Mod 2 * 1.98, g + 0.5, g * 2)
Next
Print a; " "; s; " "; g
a = Choice(1e30, 1, 2) * 1000 + Choice(-0.99, 1, 2) * 100 + Choice(&H10000000000, 1, 2) * 10 + Choice(-1e30, 1, 2)
s = Choice(0.0, "a", "b")
Print a; " "; s
f = Choice(1, 1, 2.5) : Print f
""", 30),
    ("CHOICE: nested, in a condition", """Dim Integer k, n
For k = 1 To 300
  n = n + Choice(k Mod 2, Choice(k Mod 3, 1, 10), 100)
  If Choice(k > 150, 1, 0) Then n = n + 1000
Next
Print n
""", 10),
    ("CHOICE: two arguments (getcsargs's error)", """Dim Integer a
a = Choice(1, 2)
Print a
""", None),
    ("CHOICE: a string condition (getnumber's error)", """Dim Integer a
a = Choice("x", 1, 2)
Print a
""", None),
    ("MM.INFO keywords", """Dim Integer k, w, h, f, fc, bc, wb, fa, ds, pn, e, mc, ts
Dim Float u
Dim String p, cs, lp, si, em, ip
For k = 1 To 200
  w = MM.Info(FONTWIDTH) : h = mm.info(fontheight) : f = MM.Info(FONT)
  fc = MM.Info(FCOLOUR) : bc = MM.Info(BCOLOR) : wb = MM.Info(WRITEBUFF)
  fa = MM.Info(FLASH ADDRESS 4) : ds = MM.Info(DISK SIZE) : pn = MM.Info(PINNO GP5)
  e = MM.Info(ERRNO) : p = MM.Info(PATH) : cs = MM.Info(CPUSPEED)
  lp = MM.Info(LCDPANEL) : em = MM.Info(ERRMSG) : u = MM.Info(UPTIME)
  mc = MM.Info(MAX CONNECTIONS) : ts = MM.Info(TCPIP STATUS) : ip = MM.Info(IP ADDRESS)
Next
Print w; h; f; fc; bc; (wb <> 0); (fa <> 0); ds; pn; e
Print p; " "; cs; " "; lp; " "; em; " "; (u > 0); mc; ts; " "; ip
Print (MM.Info(HEAP) > 0); (MM.Info(STACK) > 0); MM.Info(HPOS) >= 0; MM.Info(VPOS) >= 0
""", 25),
    ("MM.INFO with a space before its keyword: text (fun_info's error)", """Dim Integer w
w = MM.Info( FONTWIDTH)
Print w
""", None),
    ("BLIT FLASH from an empty slot (cmd_blit's error)", """Mode 2
Blit Flash 8, N, 0, 0, 0, 0, 10, 10
""", None),
    ("BLIT FLASH, plain, READ/WRITE/CLOSE and FRAMEBUFFER", """Mode 2
CLS
Dim Integer k, ckx, cky, ck
For k = 0 To 15 : Box k * 8, 0, 8, 40, 0, Map(k), Map(k) : Next
Save Image "A:/rbbig4.bmp", 0, 0, 128, 40
Flash Load Image 4, "A:/rbbig4.bmp", O
CLS
For k = 1 To 30
  Blit Flash 4, N, k, 0, k, 50, 64, 20
  Blit 0, 50, 70, 50, 40, 10
Next
Blit Read #1, 0, 50, 32, 16
For k = 1 To 20 : Blit Write #1, 100 + k, 90 : Next
Blit Close #1
FrameBuffer Create
For k = 1 To 10 : Blit FrameBuffer N, F, 0, 50, 0, 0, 60, 20 : Blit FrameBuffer F, N, 0, 0, 10 + k, 75, 60, 12 : Next
FrameBuffer Close
zst$ = MM.Info(COMPILE)
For cky = 0 To 119 : For ckx = 0 To 159 : ck = ck + Pixel(ckx, cky) * (1 + ((ckx + 3 * cky) And 15)) : Next : Next
Mode 1
Print "CK"; ck
Print "STAT "; zst$
""", 30),
]

b = pc3.PC3(sys.argv[1])
b.attention()
was = b.cmd("PRINT MM.INFO(COMPILE)", 10).strip()
ok = True


def run(src):
    b.upload(src, 30)
    b.drain(0.1)
    out = pc3.ANSI.sub("", b.run(120)).replace("\r", "")
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
    print("%-4s %-52s text %-5d %s" % ("ok" if good else "BAD", name, text, " | ".join(outs["ON"][0])[-90:]))
    if not good:
        for mode in ("OFF", "ON", "SHADOW"):
            print("     %-6s %s  [%s]" % (mode, outs[mode][0], outs[mode][1]))
b.cmd("OPTION COMPILE " + ("OFF" if was.startswith("OFF") else "ON"), 10)
print("RBBIG4", "PASS" if ok else "FAIL")
b.close()
sys.exit(0 if ok else 1)
