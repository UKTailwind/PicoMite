"""Threads (branch threads): the CMM2's interpreter names in core/Thread.c and core/Thread.h as the PicoMite's.

Run once on the files as copied from CMM2V1.5 78d4172 (e9282f2): python -I tools/threads_port_names.py
Only names are changed here, by whole word; what the machine demands besides is in the commits after this one.
"""
import re, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = [  # CMM2 -> PicoMite (the PicoMite's interpreter globals carry g_)
    ("forstack_main", "g_forstack_main"), ("forstack", "g_forstack"), ("forindex", "g_forindex"),
    ("dostack_main", "g_dostack_main"), ("dostack", "g_dostack"), ("doindex", "g_doindex"),
    ("hashlist_main", "g_hashlist_main"), ("hashlist", "g_hashlist"), ("hashlistpointer", "g_hashlistpointer"),
    ("localtop", "g_localtop"), ("LocalIndex", "g_LocalIndex"),
    ("Localvarcnt", "g_Localvarcnt"), ("Globalvarcnt", "g_Globalvarcnt"), ("varcnt", "g_varcnt"),
    ("StrTmp_main", "g_StrTmp_main"), ("StrTmpLocalIndex_main", "g_StrTmpLocalIndex_main"),
    ("StrTmpLocalIndex", "g_StrTmpLocalIndex"), ("StrTmpIndex", "g_StrTmpIndex"), ("StrTmp", "g_StrTmp"),
    ("TempMemoryIsChanged", "g_TempMemoryIsChanged"), ("FunReturnArrayCount", "g_FunReturnArrayCount"),
    ("VarIndex", "g_VarIndex"),
]
for f in ("core/Thread.c", "core/Thread.h"):
    p = os.path.join(ROOT, f)
    t = open(p, encoding="latin-1", newline="").read()
    total = 0
    for a, b in NAMES:
        t, n = re.subn(r"(?<![A-Za-z0-9_])" + a + r"(?![A-Za-z0-9_])", b, t)
        total += n
    t, n = re.subn(r"\bGetuSec\(\)", "time_us_64()", t)
    total += n
    open(p, "w", encoding="latin-1", newline="").write(t)
    print(f, total, "names")
