"""Add per-command/option/function PDF outline bookmarks to the exported
PicoMite_User_Manual.pdf, nested under each "Detailed Listing" heading -
reproducing the outline older manuals had.

Word's "Save as PDF" only bookmarks heading-STYLED paragraphs; the detailed
listings are plain table rows, so Word never emits them. This script adds them
after export by reading the authoritative name list from the .docx tables and
locating each name in the PDF's left (name) column.

Usage:
    python docs/add_manual_bookmarks.py --validate      # report coverage only
    python docs/add_manual_bookmarks.py                 # write bookmarks (backs up)
    python docs/add_manual_bookmarks.py in.pdf out.pdf
"""
import sys, re, os, shutil
sys.stdout.reconfigure(encoding="utf-8")
from docx import Document
from docx.oxml.ns import qn
import pypdf
from pypdf.generic import Fit

DOCX = "PicoMite_User_Manual.docx"
PDF = "PicoMite_User_Manual.pdf"

# docx table index -> (PDF parent section title, header rows to skip, use_full)
# use_full: take the name from the whole cell (names whose text is split across
# runs, e.g. MM.HEIGHT). Options keep run0 only (run1+ are CAPS value lists).
SECTIONS = [
    (14, "Predefined Read Only Variables", 0, True),
    (15, "Options", 1, False),
    (16, "Commands", 0, True),
    (17, "Functions", 0, True),
    (18, "Obsolete Commands and Functions", 0, True),
]
NAME_X_MAX = 198          # name column: fragments left of this (desc col ~205)
KEEP_TRAIL = set("([,")   # caps-prefix before one of these still counts as a name
TOP_PAD = 16              # raise the scroll target ~one line above the baseline


def runtext(r):
    return "".join(t.text or "" for t in r.findall(qn("w:t")))


def first_cell_text(tc):
    """(first run text, whole first non-empty paragraph text) of a name cell."""
    for p in tc.findall(qn("w:p")):
        runs = [x for x in (runtext(r) for r in p.findall(qn("w:r"))) if x != ""]
        if runs and "".join(runs).strip():
            return runs[0], "".join(runs)
    return None, None


def clean_name(tc, use_full=True):
    r0, full = first_cell_text(tc)
    if r0 is None:
        return None
    s = ((full if use_full else r0) or full).replace("\xa0", " ").strip()
    out = []
    for tok in s.split():
        m = re.match(r"[A-Z][A-Z0-9.$]*", tok)
        if m and m.end() == len(tok):
            out.append(tok)                       # whole all-caps token
        elif m and tok[m.end()] in KEEP_TRAIL:
            out.append(m.group(0)); break         # NAME( ... -> keep NAME, stop
        else:
            break                                 # lowercase / symbol -> stop
    name = " ".join(out).strip()
    if name:
        return name
    # no all-caps prefix: mixed-case keyword (e.g. TMC22xx) -> first token;
    # symbol commands (', *file, ?, /*) -> keep the descriptive text verbatim
    return s.split()[0] if s[:1].isalnum() else s


def docx_names():
    d = Document(DOCX)
    sections = []
    for ti, parent, skip, use_full in SECTIONS:
        trs = d.tables[ti]._tbl.findall(qn("w:tr"))
        names = []
        seen = set()
        for ri, tr in enumerate(trs):
            if ri < skip:
                continue
            tc = tr.find(qn("w:tc"))
            if tc is not None:
                nm = clean_name(tc, use_full)
                # Variant forms get a row each (e.g. PRINT, PRINT @, PRINT #)
                # but clean to the same name, which would emit duplicate
                # bookmarks (PRINT PRINT PRINT). Keep only the first.
                if nm and nm not in seen:
                    names.append(nm)
                    seen.add(nm)
        sections.append((parent, names))
    return sections


def page_name_lines(reader):
    """For each page -> ordered list of (top_y, text) for left-column lines."""
    pages = []
    for page in reader.pages:
        frags = []
        def visit(text, cm, tm, fd, fs, frags=frags):
            if text.strip():
                frags.append((round(tm[5], 1), round(tm[4], 1), text))
        page.extract_text(visitor_text=visit)
        # group fragments into lines by y, keep only those starting in name column
        lines = {}
        for y, x, t in frags:
            lines.setdefault(y, []).append((x, t))
        out = []
        for y, parts in lines.items():
            parts.sort()
            # keep ONLY name-column fragments; drop the description column
            namep = [p for p in parts if p[0] <= NAME_X_MAX]
            if namep:
                txt = "".join(p[1] for p in namep).replace("\xa0", " ").strip()
                if txt:
                    out.append((y, namep[0][0], txt))
        out.sort(key=lambda r: -r[0])             # top to bottom
        pages.append(out)
    return pages


def norm(s):
    return re.sub(r"\s+", " ", s.replace("\xa0", " ")).strip().lower()


def ns(s):
    """space-insensitive key: PDF extraction often drops inter-run spaces
    (e.g. 'OPTION ANGLERADIANS'), so compare with all whitespace removed."""
    return re.sub(r"\s+", "", norm(s))


def locate(sections, reader, parents):
    """Return {parent: [(name, page_idx, top_y), ...]} and list of misses.

    Names are matched in document order against a flat stream of name-column
    lines. A name may wrap across consecutive lines (e.g. OPTION DEFAULT /
    COLOURS), so we accumulate up to 4 lines while they remain a prefix of the
    wanted name; the bookmark points at the first line of the match.
    """
    pages = page_name_lines(reader)
    # flat stream of (page_idx, top_y, space-insensitive text), reading order
    stream = [(pi, y, ns(txt)) for pi, rows in enumerate(pages) for (y, x, txt) in rows]

    located = {}
    misses = []
    pos = 0
    for parent, names in sections:
        start_pg = parents.get(parent)
        if start_pg is None:
            misses += [(parent, n, "no parent") for n in names]
            continue
        while pos < len(stream) and stream[pos][0] < start_pg:
            pos += 1
        found = []
        for nm in names:
            want = ns(nm)
            hit = None
            j = pos
            while j < len(stream):
                pi, y, nt = stream[j]
                if nt and nt.startswith(want):
                    hit = (pi, y, j + 1)
                    break
                # wrapped name: this line is a strict prefix -> accumulate lines
                if nt and want.startswith(nt):
                    acc, k = nt, j + 1
                    while k < len(stream) and len(acc) < len(want):
                        acc += stream[k][2]
                        k += 1
                        if acc.startswith(want):
                            hit = (pi, y, k)
                            break
                        if not want.startswith(acc[: len(want)]):
                            break
                    if hit:
                        break
                j += 1
            if hit:
                found.append((nm, hit[0], hit[1]))
                pos = hit[2]                       # advance only past a real match
            else:
                misses.append((parent, nm, "not found"))   # keep pos; scan on
        located[parent] = found
    return located, misses


def section_parents(reader):
    """page index of each section's 'Detailed Listing' child."""
    wanted = {p for _, p, _, _ in SECTIONS}
    res = {}

    def rec(items, parent):
        i = 0
        while i < len(items):
            it = items[i]
            if isinstance(it, list):
                i += 1
                continue
            title = str(it.title)
            child = items[i + 1] if i + 1 < len(items) and isinstance(items[i + 1], list) else None
            if title == "Detailed Listing" and parent in wanted:
                res[parent] = reader.get_destination_page_number(it)
            if child:
                rec(child, title)
            i += 1

    rec(reader.outline, "ROOT")
    return res


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    validate = "--validate" in sys.argv
    inp = args[0] if len(args) > 0 else PDF
    outp = args[1] if len(args) > 1 else PDF

    reader = pypdf.PdfReader(inp)
    sections = docx_names()
    parents = section_parents(reader)
    print("section parent pages:", parents)
    located, misses = locate(sections, reader, parents)

    for parent, names in sections:
        print(f"{parent}: {len(located.get(parent, []))}/{len(names)} located")
    if misses:
        print(f"\n{len(misses)} MISSES:")
        for p, n, why in misses[:60]:
            print(f"  [{p}] {n!r} ({why})")

    if validate:
        return

    # rebuild outline: copy existing headings, inject leaves under Detailed Listing
    writer = pypdf.PdfWriter()
    writer.append(reader, import_outline=False)

    def page_top(it):
        pg = reader.get_destination_page_number(it)
        top = getattr(it, "top", None)
        return pg, (float(top) if top is not None else None)

    def rebuild(items, wparent, parent_title):
        i = 0
        while i < len(items):
            it = items[i]
            if isinstance(it, list):
                i += 1
                continue
            title = str(it.title)
            child = items[i + 1] if i + 1 < len(items) and isinstance(items[i + 1], list) else None
            pg, top = page_top(it)
            fit = Fit.xyz(top=top) if top is not None else Fit.fit()
            # is_open=False -> the PDF opens collapsed; reader drills down
            node = writer.add_outline_item(title, pg, parent=wparent, fit=fit, is_open=False)
            # Inject the located leaves under each "Detailed Listing" heading.
            # Doing this regardless of whether the source node already has
            # children makes the script idempotent: re-running on an
            # already-bookmarked PDF REPLACES the old leaves (we skip recursing
            # into them below) rather than refusing or duplicating them.
            inject = title == "Detailed Listing" and parent_title in located
            if inject:
                for nm, lpi, ly in located[parent_title]:
                    # ly is the text baseline; aim the view a line higher so the
                    # target row isn't clipped off the top of the window.
                    page_h = float(reader.pages[lpi].mediabox.top)
                    dst = min(ly + TOP_PAD, page_h)
                    writer.add_outline_item(nm, lpi, parent=node,
                                            fit=Fit.xyz(top=dst), is_open=False)
            elif child:
                rebuild(child, node, title)
            i += 1

    rebuild(reader.outline, None, "ROOT")

    # write spec-correct /Count: every parent collapsed (negative = closed).
    # |Count| = direct children (revealed on first expand; they stay closed).
    from pypdf.generic import NameObject, NumberObject

    def fix_counts(node):
        child = node.get("/First")
        n = 0
        while child is not None:
            c = child.get_object()
            fix_counts(c)
            n += 1
            child = c.get("/Next")
        if n:
            node[NameObject("/Count")] = NumberObject(-n)
        return n

    outlines = writer._root_object["/Outlines"]
    top = fix_counts(outlines)
    outlines[NameObject("/Count")] = NumberObject(top)   # root stays open

    if outp == inp and not os.path.exists(inp + ".bak_nobm"):
        shutil.copy2(inp, inp + ".bak_nobm")
        print("backup ->", inp + ".bak_nobm")
    with open(outp, "wb") as f:
        writer.write(f)
    total = sum(len(v) for v in located.values())
    print(f"\nDONE - wrote {outp} with {total} leaf bookmarks added")


if __name__ == "__main__":
    main()
