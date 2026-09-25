"""symab.py PORT FILE.bas [OUTDIR] - A/B one program: saved as text (OPTION SYMBOLS OFF) and
with symbols (OPTION SYMBOLS ON).  Uploads each way with AUTOSAVE, captures LIST ALL and the
RUN output, and reports whether the listings and the outputs are identical."""
import sys, os, re, time, difflib
sys.path.insert(0, r"D:/Dropbox/PicoMite/PicoMite/Bas/elite_tools")
import pc3

port, path = sys.argv[1], sys.argv[2]
outdir = sys.argv[3] if len(sys.argv) > 3 else "."
src = open(path, encoding="latin-1").read()
name = os.path.splitext(os.path.basename(path))[0]
b = pc3.PC3(port)
b.attention()


def listing():
    b.drain(0.05)
    b.send_line("LIST ALL")
    out = b.wait_prompt(120)
    lines = out.split("\n")
    # drop the echoed command and the prompt
    if lines and "LIST ALL" in lines[0]:
        lines = lines[1:]
    while lines and lines[-1].strip() in ("", ">"):
        lines.pop()
    return "\n".join(l.rstrip() for l in lines)


res = {}
for mode in ("OFF", "ON"):
    b.cmd("OPTION SYMBOLS " + mode)
    saved, n = b.upload(src, timeout=120)
    lst = listing()
    run = b.run(600)
    run = "\n".join(l.rstrip() for l in run.split("\n")[1:])
    mem = b.cmd("MEMORY")
    res[mode] = (saved, lst, run, mem)
    open(os.path.join(outdir, "%s_%s_list.txt" % (name, mode)), "w", encoding="utf-8").write(lst)
    open(os.path.join(outdir, "%s_%s_run.txt" % (name, mode)), "w", encoding="utf-8").write(run)
    print("%s: saved %s bytes, %d lines sent" % (mode, saved, n))
b.cmd("OPTION SYMBOLS ON")
b.close()

for what, i in (("LIST", 1), ("RUN", 2)):
    a, c = res["OFF"][i], res["ON"][i]
    if a == c:
        print(what, "identical (%d lines)" % len(a.split("\n")))
    else:
        print(what, "DIFFERS:")
        for l in list(difflib.unified_diff(a.split("\n"), c.split("\n"), "text", "symbols", lineterm="", n=1))[:60]:
            print("  " + l)
print("--- RUN output with symbols (tail):")
print(res["ON"][2][-2500:])
prog = [l for l in res["ON"][3].split("\n") if "Program" in l or "program" in l][:3]
print("MEMORY text:   ", [l.strip() for l in res["OFF"][3].split("\n") if "%" in l][:2])
print("MEMORY symbols:", [l.strip() for l in res["ON"][3].split("\n") if "%" in l][:2])
