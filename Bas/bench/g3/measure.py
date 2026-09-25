"""Approximate MMBasic tokenise() and measure what a symbol-indexed format would change.

Read-only analysis. Emulates core/MMBasic.c tokenise() closely enough for byte accounting:
line header (T_NEWLINE + skip byte), spaces kept, strings, ' comments and REM, ':' -> 0,
numbers as text, command at statement start (longest match), function/operator tokens
elsewhere (first match, table order), labels, names; trailing blanks trimmed; per line
terminator 0; program terminator.
"""
import re, sys, os, collections, math

ROOT = r"D:/Dropbox/PicoMite/PicoMite"
src = open(os.path.join(ROOT, "AllCommands.h"), encoding="latin-1").read()

def table(start, end):
    a = src.index(start); b = src.index(end, a)
    return re.findall(r'\{\s*\(unsigned char \*\)"((?:[^"\\]|\\.)*)"\s*,\s*([^,]*),', src[a:b])

cmds = [(n.replace('\\\\', '\\'), t) for n, t in table("#ifdef INCLUDE_COMMAND_TABLE", "#endif /* INCLUDE_COMMAND_TABLE */") if n]
toks = [(n.replace('\\\\', '\\'), t) for n, t in table("#ifdef INCLUDE_TOKEN_TABLE", "#endif /* INCLUDE_TOKEN_TABLE */") if n]

def isnamestart(c): return c.isalpha() and c.isascii() or c == '_'
def isnamechar(c): return (c.isalnum() and c.isascii()) or c in '_.'
def isnameend(c): return (c.isalnum() and c.isascii()) or c in '_.$!%'

def match_cmd(s, p):
    best = None; bestlen = 0
    for i, (name, typ) in enumerate(cmds):
        tp = 0; q = p
        while tp < len(name) and q < len(s) and s[q].upper() == name[tp].upper():
            if name[tp] == ' ':
                while q < len(s) and s[q] == ' ': q += 1
            else:
                q += 1
            tp += 1
            if tp < len(name) and name[tp] == '(':
                pass
            if tp > 0 and name[tp-1] == '(':
                while q < len(s) and s[q] == ' ': q += 1
        if tp == len(name):
            nxt = s[q] if q < len(s) else '\0'
            if isnamechar(nxt) and name[-1] != '(' and 'T_FUN' not in typ:
                continue
            if name[-1] != '(' and isnamechar(nxt):
                continue
            if len(name) > bestlen:
                best, bestlen = (i, q), len(name)
    return best

def match_tok(s, p):
    for i, (name, typ) in enumerate(toks):
        tp = 0; q = p
        while tp < len(name) and q < len(s) and s[q].upper() == name[tp].upper():
            tp += 1; q += 1
            if tp < len(name) and name[tp] == '(':
                pass
            if name[tp-1] == '(':
                while q < len(s) and s[q] == ' ': q += 1
        if tp == len(name):
            nxt = s[q] if q < len(s) else '\0'
            if (not isnameend(name[-1])) or (not isnamechar(nxt)):
                return i, q
    return None

class Stats:
    def __init__(s):
        s.c = collections.Counter()
        s.names = collections.Counter()      # canonical (upper) -> uses
        s.spell = collections.defaultdict(set)
        s.lines = 0
        s.stmts = 0
        s.indent_saved = 0
        s.trailing_comments = 0
        s.for_do_select = 0

def tokenise_program(text, st):
    in_csub = False
    multi = False
    for raw in text.splitlines():
        line = ''.join(ch if ' ' <= ch < '\x7f' else ' ' for ch in raw.replace('\t', '  '))
        st.lines += 1
        st.c['hdr'] += 2
        s = line
        p = 0
        # leading indentation
        ind = len(s) - len(s.lstrip(' '))
        if s.strip() == '':
            st.c['term'] += 1
            continue
        if ind >= 3:
            st.indent_saved += ind - 2
        # line number
        m = re.match(r'\s*(\d+)', s)
        if m and not re.match(r'\s*[0-9A-Fa-f]{8}', s):
            n = int(m.group(1))
            if 0 < n <= 63999:
                st.c['linenbr'] += 3
            p = m.end()
        stripped = s.strip().upper()
        if in_csub:
            st.c['csub'] += len(s.rstrip()) - 0
            st.c['term'] += 1
            if stripped.startswith('END CSUB') or stripped.startswith('END DEFINEFONT'):
                in_csub = False
            continue
        if stripped.startswith('CSUB ') or stripped.startswith('DEFINEFONT'):
            in_csub = True
        firstnonwhite = True
        labelvalid = True
        stmt_raw = False  # DATA / REM: keep raw
        out_len = 0
        seen_code = False
        while p < len(s):
            ch = s[p]
            if ch == ' ':
                st.c['space'] += 1; p += 1; continue
            if ch == '"':
                q = s.find('"', p + 1)
                q = len(s) if q < 0 else q + 1
                st.c['string'] += q - p + (0 if s[q-1:q] == '"' and q - p > 1 else 1)
                p = q; firstnonwhite = False; continue
            if ch == "'":
                st.c['comment'] += len(s.rstrip()) - p
                if seen_code:
                    st.trailing_comments += 1
                    st.c['comment_trailing'] += len(s.rstrip()) - p
                p = len(s); continue
            if ch == ':':
                st.c['sep'] += 1; p += 1; firstnonwhite = True; stmt_raw = False; continue
            if ch.isdigit() or ch == '.' and p + 1 < len(s) and s[p+1].isdigit():
                q = p
                while q < len(s) and (s[q].isdigit() or s[q] in '.Ee'):
                    if s[q] in 'Ee' and q + 1 < len(s) and s[q+1] in '+-':
                        q += 1
                    q += 1
                st.c['number'] += q - p; p = q; firstnonwhite = False; seen_code = True; continue
            if ch == '&' and p + 1 < len(s) and s[p+1].upper() in 'HOB':
                q = p + 2
                while q < len(s) and s[q].isalnum(): q += 1
                st.c['number'] += q - p; p = q; firstnonwhite = False; seen_code = True; continue
            if firstnonwhite:
                if ch == '?':
                    st.c['cmdtok'] += 2; st.stmts += 1; p += 1
                    if p < len(s) and s[p] == ' ': p += 1
                    firstnonwhite = False; labelvalid = False; seen_code = True; continue
                m = match_cmd(s, p)
                if m:
                    i, q = m
                    name = cmds[i][0]
                    st.c['cmdtok'] += 2; st.stmts += 1; seen_code = True
                    p = q
                    if p < len(s) and s[p] == ' ' and s[p-1].isalpha(): p += 1
                    firstnonwhite = False; labelvalid = False
                    up = name.upper()
                    if up in ('FOR', 'DO', 'SELECT CASE'):
                        st.for_do_select += 1
                    if up == 'REM':
                        st.c['comment'] += len(s.rstrip()) - p; p = len(s)
                    if up == 'DATA':
                        stmt_raw = True
                    continue
                if labelvalid and isnamestart(ch):
                    q = p + 1
                    while q < len(s) and isnamechar(s[q]): q += 1
                    if q < len(s) and s[q] == ':':
                        nm = s[p:q]
                        st.c['label'] += 2 + len(nm)
                        st.names[nm.upper()] += 1; st.spell[nm.upper()].add(nm)
                        st.c['name_label_def'] += len(nm)
                        p = q + 1; labelvalid = False; continue
            else:
                m = match_tok(s, p)
                if m:
                    i, q = m
                    st.c['tok'] += 1; p = q
                    nm = toks[i][0].upper()
                    firstnonwhite = nm in ('THEN', 'ELSE')
                    continue
            if isnamestart(ch):
                q = p
                while q < len(s) and isnamechar(s[q]): q += 1
                nm = s[p:q]
                if firstnonwhite:
                    r = q
                    while r < len(s) and s[r] in '$%!': r += 1
                    if r < len(s) and s[r] == '(':
                        d = 0
                        while r < len(s):
                            if s[r] == '(': d += 1
                            elif s[r] == ')':
                                d -= 1
                                if d == 0: r += 1; break
                            r += 1
                    while r < len(s) and s[r] == ' ': r += 1
                    if r < len(s) and s[r] == '=':
                        st.c['cmdtok'] += 2  # implied LET
                    st.stmts += 1
                if stmt_raw:
                    st.c['data_word'] += len(nm)
                else:
                    st.c['name'] += len(nm)
                    st.names[nm.upper()] += 1
                    st.spell[nm.upper()].add(nm)
                p = q; firstnonwhite = False; labelvalid = False; seen_code = True
                continue
            st.c['punct'] += 1; p += 1; firstnonwhite = False; labelvalid = False; seen_code = True
        # trailing spaces trimmed: approximate by subtracting trailing spaces of the line
        st.c['space'] -= len(s) - len(s.rstrip(' ')) if s.rstrip(' ') != s else 0
        st.c['term'] += 1
    st.c['term'] += 1

def report(path, text=None):
    if text is None:
        text = open(path, encoding='latin-1').read()
    st = Stats()
    tokenise_program(text, st)
    c = st.c
    total = sum(v for k, v in c.items() if k not in ('comment_trailing', 'name_label_def'))
    N = len(st.names)
    uses = sum(st.names.values())
    namebytes = c['name'] + c['name_label_def']
    ranked = sorted(st.names.items(), key=lambda kv: -kv[1])
    # encoding: first 882 names (14 prefixes x 63) -> 2 bytes, rest -> 3 bytes
    ref_bytes = 0
    for i, (nm, u) in enumerate(ranked):
        ref_bytes += u * (2 if i < 882 else 3)
    dict_text = sum(len(n) for n in st.names)
    # flash name block: per name len+1 (len) +1 flags +4 hash +3 static +2 offset +2 sorted perm
    block = sum(len(n) + 13 for n in st.names) + 16
    multi_spell = sum(1 for n in st.spell if len(st.spell[n]) > 1)
    extra_spell = sum(sum(len(x) + 1 for x in st.spell[n]) - len(n) - 1 for n in st.spell if len(st.spell[n]) > 1)
    new_total = total - namebytes + ref_bytes + block
    new_total_ind = new_total - st.indent_saved + st.trailing_comments
    print(f"== {os.path.relpath(path, ROOT) if path else 'text'}  src={len(text)} B  lines={st.lines}  stmts~{st.stmts}")
    print(f"   tokenised(est) = {total} B : " + ", ".join(f"{k}={v}" for k, v in sorted(c.items(), key=lambda kv: -kv[1])))
    print(f"   names: distinct={N} uses={uses} name bytes={namebytes} ({100*namebytes/total:.1f}% of image) avg len/use={namebytes/max(uses,1):.2f} avg distinct len={dict_text/max(N,1):.2f}")
    print(f"   names with >1 spelling (case)={multi_spell}, extra bytes to keep exact case={extra_spell}")
    print(f"   refs bytes={ref_bytes}  name block(flash)={block}  => image {total} -> {new_total} ({100*(new_total-total)/total:+.1f}%)")
    print(f"   + indentation runs>=3 as 2 bytes (saves {st.indent_saved}) + trailing-comment separators (+{st.trailing_comments}) => {new_total_ind} ({100*(new_total_ind-total)/total:+.1f}%)")
    print(f"   comments={c['comment']} B ({100*c['comment']/total:.1f}%), trailing comments={st.trailing_comments} ({c['comment_trailing']} B), spaces={c['space']} B ({100*c['space']/total:.1f}%), numbers={c['number']} B")
    print(f"   save-time dictionary RAM ~ {dict_text + 6*N} B (text + 6 B/name); FOR/DO/SELECT stmts={st.for_do_select}")
    print(f"   RAM per-name binding (2 B/name) = {2*N} B; 'in library' bitmap = {(N+7)//8} B")
    top = ", ".join(f"{n}:{u}" for n, u in ranked[:12])
    print(f"   top names: {top}")
    return total, new_total, N

if __name__ == '__main__':
    for f in sys.argv[1:]:
        report(f)

def refined(path):
    text = open(path, encoding='latin-1').read()
    st = Stats(); tokenise_program(text, st)
    c = st.c
    total = sum(v for k, v in c.items() if k not in ('comment_trailing', 'name_label_def'))
    ranked = sorted(st.names.items(), key=lambda kv: -kv[1])
    sym = [(n, u) for n, u in ranked if u >= 2]
    raw1 = [(n, u) for n, u in ranked if u < 2]
    ref = 0
    for i, (n, u) in enumerate(sym):
        ref += u * (2 if i < 882 else 3)
    symbytes = sum(len(n) * u for n, u in sym)
    block = sum(len(n) + 6 for n, u in sym) + 2 * len(sym) + 8   # entry(len+flags+hash16+chars)+offset
    new = total - symbytes + ref + block
    new_ind = new - st.indent_saved + st.trailing_comments
    usesym = sum(u for n, u in sym)
    print(f"{os.path.relpath(path, ROOT):40s} img~{total:7d}  sym names={len(sym):5d} (uses {usesym:6d}) raw-once={len(raw1):4d}  "
          f"-> {new:7d} ({100*(new-total)/total:+5.1f}%)  +indent/comment -> {new_ind:7d} ({100*(new_ind-total)/total:+5.1f}%)  "
          f"savetime dict ~{sum(len(n)+6 for n,u in ranked)} B  2B-bind RAM={2*len(sym)} B")

if __name__ == '__main__' and os.environ.get('REFINED'):
    print('--- refined: names used >=2 times symbolised; entry = len+6 B + 2 B offset; 16-bit hash; no sorted index')
    for f in sys.argv[1:]:
        refined(f)

def final(path, two=481):
    text = open(path, encoding='latin-1').read()
    st = Stats(); tokenise_program(text, st)
    c = st.c
    total = sum(v for k, v in c.items() if k not in ('comment_trailing', 'name_label_def'))
    ranked = sorted(st.names.items(), key=lambda kv: -kv[1])
    sym = [(n, u) for n, u in ranked if u >= 2]
    ref = sum(u * (2 if i < two else 3) for i, (n, u) in enumerate(sym))
    symbytes = sum(len(n) * u for n, u in sym)
    N = len(sym)
    hidx = 2 * (1 << max(1, math.ceil(math.log2(max(2, int(1.5 * N))))))
    block = sum(len(n) + 6 for n, u in sym) + 2 * N + 16 + hidx
    new = total - symbytes + ref + block
    new_ind = new - st.indent_saved
    leading = 0
    print(f"{os.path.basename(path):22s} img~{total/1024:6.1f}K names={100*(c['name']+c['name_label_def'])/total:4.1f}% N={N:5d} block={block/1024:4.1f}K(hidx {hidx}) refs={ref/1024:5.1f}K "
          f"-> {new/1024:6.1f}K ({100*(new-total)/total:+5.1f}%)  +indent -> {new_ind/1024:6.1f}K ({100*(new_ind-total)/total:+5.1f}%)  "
          f"dict~{(sum(len(n)+6 for n,u in ranked))/1024:4.1f}K tier1={2*N}B comments={100*c['comment']/total:4.1f}% trailingC={st.trailing_comments}")

if __name__ == '__main__' and os.environ.get('FINAL'):
    for f in sys.argv[1:]:
        final(f)
