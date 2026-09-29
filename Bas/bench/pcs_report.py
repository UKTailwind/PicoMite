"""pcs_report.py OUTPUT.txt PicoMite.elf [--top N] [--src FILE.bas] [--old: a recording before V7.0.00b1]

Turn the [PCS] lines that OPTION PROFILING ON, SAMPLE prints at END into a
time split by function and by interpreter bucket.  PCs are resolved against
the symbol table of the exact ELF that was flashed.
"""
import bisect, re, subprocess, sys, collections

NM = r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin\arm-none-eabi-nm.exe"

BUCKETS = [
    ("housekeeping", r"^(CheckAbort|routinechecks|check_interrupt|checkdetailinterrupts|ProcessWeb|ProcessTouch|time_us_64|timer_time_us_64|cyw43_.*|web_async_check_error|tud_.*|tuh_.*|hid_app_task|CursorRefresh|USBKeyboardInterrupt)$"),
    ("temp memory", r"^(GetTempMemory|GetTempStrMemory|GetTempMainMemory|GetSystemMemory|GetMemory|GetMemoryNull|FreeMemory|FreeMemorySafe|ClearTempMemory|ClearSpecificTempMemory|MBitsGet|MBitsSet|getheapstart|TryAllocAligned)$"),
    ("name lookup", r"^(findvar|FindSubFun|probe_local_slot|probe_global_slot|FindStructBase|ResolveStructMember|mystrncasecmp|str_equal|CompareNameToSubFunBase|hashlabels|findlabel|findline)$"),
    ("call machinery", r"^(DefinedSubFun|cmd_return|cmd_endfun|ClearVars|EnterLocalFrame|LeaveLocalFrame|cmd_dim|cmd_local|CheckIfTypeSpecified|cmd_subfun|CallExecuteProgram|cmd_gosub|cmd_exit.*|cmd_endsub)$"),
    ("arg parsing / text walk", r"^(makeargs|MakeCommaSeparatedArgs|getclosebracket|skipvar|skipexpression|checkstring|GetNextCommand|skipelement|getargaddress|tokentype|Mstrcpy|CtoM|MtoC|getCstring)$"),
    ("evaluator", r"^(evaluate|getvalue|doexpr|getnumber|getinteger|getint|getstring|op_.*|FloatToInt64|FloatToInt32|compare|fast_strtod|IntToStr|FloatToStr)$"),
    ("dispatch", r"^(ExecuteProgram|execute_one_command|commandtbl_decode)$"),
    ("statement handlers", r"^cmd_.*$"),
    ("built-in functions", r"^fun_.*$"),
    ("maths library", r"^(__wrap___aeabi_.*|__aeabi_.*|sin|cos|tan|atan|atan2|asin|acos|sqrt|pow|log|exp|floor|ceil|fmod|__ieee754_.*|__kernel_.*|__wrap_.*|double_.*|dcp_.*|sincos.*|__rem_pio2.*)$"),
    ("boot ROM maths", r"^\(boot ROM"),
    ("console / display I/O", r"^(ScrollLCD.*|DrawBitmap.*|ShowCursor|MMgetchar|CheckKeyboard|MMPrintString|MMputchar|putConsole|SerialConsolePutC|DisplayPutC|GUIPrintChar|DrawChar.*)$"),
    ("libc", r"^(memcpy|memset|memmove|strlen|strchr|strcpy|strcmp|strncmp|__aeabi_mem.*|__memcpy.*|__memset.*)$"),
]


def symbols(elf):
    out = subprocess.run([NM, "-n", "-S", "--defined-only", elf], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and p[2].lower() == "t":
            syms.append((int(p[0], 16), int(p[1], 16), p[3]))
        elif len(p) == 3 and p[1].lower() == "t":
            syms.append((int(p[0], 16), 0, p[2]))
    syms.sort()
    return syms


def main():
    text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    elf = sys.argv[2]
    top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 40
    m = re.search(r"\[PCS\] samples=(\d+) dropped=(\d+)", text)
    total = int(m.group(1)) if m else 0
    pcs = [(int(a, 16), int(n)) for a, n in re.findall(r"\[PCS\] ([0-9a-f]{8}) (\d+)", text)]
    lines = [(int(l), int(n)) for l, n in re.findall(r"\[PCSLINE\] (-?\d+) (\d+)", text)]
    syms = symbols(elf)
    addrs = [s[0] for s in syms]
    byfn = collections.Counter()
    for pc, n in pcs:
        i = bisect.bisect_right(addrs, pc) - 1
        name = "?"
        if pc < 0x10000000:
            name = "(boot ROM: float/double routines)"
        elif i >= 0:
            a, size, nm = syms[i]
            if size == 0 or pc < a + size:
                name = nm
            else:
                name = "?%s+" % nm
        byfn[name] += n
    listed = sum(n for _, n in pcs)
    bybucket = collections.Counter()
    for fn, n in byfn.items():
        base = fn.lstrip("?").rstrip("+")
        for b, rx in BUCKETS:
            if re.match(rx, base):
                bybucket[b] += n
                break
        else:
            bybucket["other"] += n
    print("samples %d, listed %d (%.1f%%)" % (total, listed, 100.0 * listed / max(total, 1)))
    print("\n== buckets (share of all samples)")
    for b, n in bybucket.most_common():
        print("  %-26s %6.1f%%  %d" % (b, 100.0 * n / max(total, 1), n))
    print("\n== top functions")
    for fn, n in byfn.most_common(top):
        print("  %-34s %6.1f%%  %d" % (fn, 100.0 * n / max(total, 1), n))
    if lines:
        src = None
        if "--src" in sys.argv:
            src = open(sys.argv[sys.argv.index("--src") + 1], "rb").read().decode("latin-1").splitlines()
        # [PCSLINE] is the editor's line number from V7.0.00b1; a recording from
        # earlier firmware is one less (--old)
        adj = 1 if "--old" in sys.argv else 0
        print("\n== busiest program lines")
        for l, n in lines[:20]:
            if l >= 0:
                l += adj
            text = src[l - 1].strip()[:90] if src and 1 <= l <= len(src) else ""
            print("  file line %-5d %6.1f%%  %s" % (l, 100.0 * n / max(total, 1), text))


if __name__ == "__main__":
    main()
