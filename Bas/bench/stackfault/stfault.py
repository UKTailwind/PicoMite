"""stfault.py PORT ROUNDS [--variants a,b,...] [--elf PicoMite.elf] [--out FILE.json]

Stage 1 of the PC3 stack-top fault diagnosis ("Found on the way" in
docs/Interpreter_RouteB_Design.html): the goldens' pioout.bas faults about
30% of the time just after it ends, at the prompt, with the top of core 0's
stack corrupted.  This runs pioout.bas split into its parts, and controls,
round-robin, and counts faults per part.

Each run is the cycle that reproduced it: LOAD another program, LOAD the
test program, RUN, then wait at the prompt and poke it (an expression is
printed, which goes through IntToStr, where the faults were seen) while
watching every byte for "*** FAULT".  A fault's whole report is kept, and PC
and LR are decoded with addr2line when --elf is given (use the ELF of the
firmware on the board).  After a fault the board is waited for until it
answers again.

The variant programs are written to A:/st by TFTP on the first run.
OPTION COMPILE is left as it is: the fault was seen with it OFF.
"""
import sys, os, time, json, re, subprocess
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "elite_tools"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "exile_tools"))
import pc3, tftp

HOST = "192.168.1.245"
VARIANTS = {
    # the whole of pioout.bas
    "full": """Dim Integer c(11)
Dim Integer i
For i = 0 To 11 : c(i) = RGB(255, 0, 0) : Next i
WS2812 B, gp7, 12, c()
WS2812 O, gp7, 12, c()
WS2812 S, gp7, 12, c()
WS2812 W, gp7, 12, c()
WS2812 B, gp7, 1, RGB(0, 255, 0)
Dim Integer d(9)
For i = 0 To 9 : d(i) = 250 : Next i
BITSTREAM gp2, 10, d()
BITSTREAM gp2, 10, d(), 1
Dim Float e(9)
For i = 0 To 9 : e(i) = 100.5 : Next i
BITSTREAM gp2, 10, e()
Print "st done"
""",
    # WS2812 alone: interrupts off for ~0.4 ms a call
    "ws2812": """Dim Integer c(11)
Dim Integer i
For i = 0 To 11 : c(i) = RGB(255, 0, 0) : Next i
WS2812 B, gp7, 12, c()
WS2812 O, gp7, 12, c()
WS2812 S, gp7, 12, c()
WS2812 W, gp7, 12, c()
WS2812 B, gp7, 1, RGB(0, 255, 0)
Print "st done"
""",
    # BITSTREAM from an Integer array, push-pull and open-collector: ~2.5 ms off each
    "bs_int": """Dim Integer d(9)
Dim Integer i
For i = 0 To 9 : d(i) = 250 : Next i
BITSTREAM gp2, 10, d()
BITSTREAM gp2, 10, d(), 1
Print "st done"
""",
    # BITSTREAM from a Float array
    "bs_float": """Dim Float e(9)
Dim Integer i
For i = 0 To 9 : e(i) = 100.5 : Next i
BITSTREAM gp2, 10, e()
Print "st done"
""",
    # control: the same pins driven by hand, no timed output, interrupts never off
    "pins_only": """SetPin gp7, DOUT
SetPin gp2, DOUT
Dim Integer i
For i = 1 To 20 : Pin(gp7) = i And 1 : Pin(gp2) = i And 1 : Next i
Print "st done"
""",
    # control: about as long as "full", nothing touched
    "pause_only": """Dim Integer i
For i = 1 To 10 : Pause 1 : Next i
Print "st done"
""",
}
OTHERS = ["A:/g/arrsize.bas", "A:/g/circle.bas"]  # (the goldens corpus, kept on A:)


def decode(elf, addr):
    if not elf:
        return ""
    try:
        out = subprocess.run(["arm-none-eabi-addr2line", "-f", "-C", "-e", elf, addr], capture_output=True, text=True).stdout.split()
        return "%s %s" % (out[0], os.path.basename(out[1])) if len(out) >= 2 else ""
    except Exception:
        return ""


def main():
    port, rounds = sys.argv[1], int(sys.argv[2])
    names = list(VARIANTS)
    if "--variants" in sys.argv:
        names = sys.argv[sys.argv.index("--variants") + 1].split(",")
    elf = sys.argv[sys.argv.index("--elf") + 1] if "--elf" in sys.argv else None
    outf = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "stfault.json"

    b = pc3.PC3(port)
    b.attention()
    b.cmd('Mkdir "A:/st"', 10)
    for n in names:
        tftp.send(HOST, "A:/st/%s.bas" % n, VARIANTS[n].encode())
    print("sent", names)
    info = pc3.ANSI.sub("", b.cmd("? MM.DEVICE$; MM.VER; MM.INFO(CPUSPEED)", 10)).replace("\r", "").strip()
    print("board:", info)

    results = {n: {"runs": 0, "faults": 0, "reports": []} for n in names}
    for r in range(rounds):
        for n in names:
            b.cmd('LOAD "%s"' % OTHERS[r % len(OTHERS)], 30)
            b.cmd('LOAD "A:/st/%s.bas"' % n, 30)
            b.drain(0.1)
            out = pc3.ANSI.sub("", b.run(60))
            # at the prompt: wait, and make it print numbers (IntToStr)
            time.sleep(1.0)
            b.send_line("? 12345; 6.5; MM.INFO(FREE SPACE)")
            out += pc3.ANSI.sub("", b.drain(2.0))
            results[n]["runs"] += 1
            if "FAULT" in out:
                results[n]["faults"] += 1
                lines = [l.strip() for l in out.replace("\r", "").split("\n") if "FAULT" in l]
                dec = []
                for l in lines:
                    for key in ("PC", "LR"):
                        m = re.search(key + r"=([0-9A-Fa-f]{8})", l)
                        if m:
                            dec.append("%s %s %s" % (key, m.group(1), decode(elf, "0x" + m.group(1))))
                results[n]["reports"].append({"round": r, "lines": lines, "decoded": dec})
                print("round %d %-10s FAULT %s" % (r, n, " | ".join(lines)[:200]))
                for d in dec:
                    print("      ", d)
                b.close()
                time.sleep(15)
                b = pc3.PC3(port)
                b.attention()
            else:
                ok = "st done" in out
                print("round %d %-10s %s" % (r, n, "ok" if ok else "?? " + out.strip()[-80:]))
        json.dump({"board": info, "results": results}, open(outf, "w"), indent=1)
    print("\n== faults per part")
    for n in names:
        print("  %-10s %d of %d" % (n, results[n]["faults"], results[n]["runs"]))
    b.close()


if __name__ == "__main__":
    main()
