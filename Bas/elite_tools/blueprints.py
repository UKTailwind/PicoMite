"""Elite ship blueprints -> MMBasic Draw3D meshes.

Reads the BBC Micro cassette Elite source (elite-source.asm, beebasm format),
extracts every SHIP_* blueprint (header stats, VERTEX / EDGE / FACE macros),
rebuilds face polygons from the edge list and writes:

  Bas/elite/data/ships.bas    DATA blocks, one label per ship, for RESTORE/READ
  Bas/elite/data/ships.json   the same data for other tools

and prints a report, including a cross-check against the meshes in Bas/3ddemo.bas.

How the polygons are built (this is the part Draw3D cares about):
  * An edge whose two face ids differ is a boundary edge of both faces.  The
    boundary edges of a face are chained into one closed loop.
  * An edge whose two face ids are the same face is surface detail (engine
    recesses, cockpit lines).  Detail edges of one face are grouped into
    connected components; a closed component becomes an extra polygon that
    inherits the host face's normal, so it is culled with its host.  Open
    components are reported (Draw3D cannot draw a bare line as a face).
  * Draw3D derives a face normal from its first three vertices as
    N = (v1->v2) x (v1->v0) and draws the face when dot(v0 - camera, N) < 0.
    Each loop is oriented so that N agrees with Elite's stored normal, and
    rotated so its first three vertices are not collinear.

Usage:  python blueprints.py [path/to/elite-source.asm]
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ASM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "cache", "elite-source.asm")
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "elite", "data"))
DEMO = os.path.normpath(os.path.join(HERE, "..", "3ddemo.bas"))

# XX21 order: type number -> blueprint label, display name
TYPES = [
    (1, "SIDEWINDER", "Sidewinder"),
    (2, "VIPER", "Viper"),
    (3, "MAMBA", "Mamba"),
    (4, "PYTHON", "Python"),
    (5, "COBRA_MK_3", "Cobra Mk III"),
    (6, "THARGOID", "Thargoid"),
    (7, "COBRA_MK_3", "Cobra Mk III"),   # trader, same blueprint
    (8, "CORIOLIS", "Coriolis"),
    (9, "MISSILE", "Missile"),
    (10, "ASTEROID", "Asteroid"),
    (11, "CANISTER", "Canister"),
    (12, "THARGON", "Thargon"),
    (13, "ESCAPE_POD", "Escape pod"),
]
# 3ddemo.bas section names -> blueprint label (Asp is disc-only, not compared)
DEMO_MAP = {"Viper": "VIPER", "Thargoid": "THARGOID", "Pod": "ESCAPE_POD", "Asteroid": "ASTEROID",
            "Canister": "CANISTER", "Cobra": "COBRA_MK_3", "Mamba": "MAMBA", "Missile": "MISSILE",
            "Python": "PYTHON", "Sidewinder": "SIDEWINDER", "Station": "CORIOLIS"}


# ----------------------------------------------------------------- parsing
def parse_asm(path):
    text = open(path, encoding="latin-1").read().splitlines()
    ships = {}
    label_re = re.compile(r"^\.SHIP_([A-Z0-9_]+?)(_VERTICES|_EDGES|_FACES)?\s*$")
    cur, section = None, None
    for ln in text:
        m = label_re.match(ln)
        if m:
            name, part = m.group(1), m.group(2)
            if part is None:
                cur = ships.setdefault(name, {"name": name, "hdr": [], "verts": [], "edges": [], "faces": [], "edges_from": name})
                section = "hdr"
            else:
                cur = ships[name]
                section = part[1:].lower()
            continue
        if cur is None:
            continue
        s = ln.strip()
        if section == "hdr":
            if s.startswith("EQUB") or s.startswith("EQUW"):
                body = s.split("\\")[0][4:].strip()
                if "LO(" in body or "HI(" in body:
                    mm = re.search(r"SHIP_([A-Z0-9_]+)_EDGES", body)
                    if mm and mm.group(1) != cur["name"]:
                        cur["edges_from"] = mm.group(1)
                    continue
                if body.startswith("%"):
                    val = int(body[1:], 2)
                else:
                    val = int(eval(body))          # e.g. "95 * 95"
                cur["hdr"].append(val)
        elif section == "vertices" and s.startswith("VERTEX"):
            nums = [int(x) for x in re.findall(r"-?\d+", s.split("\\")[0])]
            cur["verts"].append(nums[:3])
        elif section == "edges" and s.startswith("EDGE"):
            nums = [int(x) for x in re.findall(r"-?\d+", s.split("\\")[0])]
            cur["edges"].append(tuple(nums[:4]))     # v1, v2, face1, face2
        elif section == "faces" and s.startswith("FACE"):
            nums = [int(x) for x in re.findall(r"-?\d+", s.split("\\")[0])]
            cur["faces"].append(nums[:3])            # normal x, y, z
    for sh in ships.values():
        if sh["edges_from"] != sh["name"]:
            sh["edges"] = list(ships[sh["edges_from"]]["edges"])
        h = sh["hdr"]
        # canisters, area, heap, gun*4, explosion, nv*6, ne, bounty, nf*4, vis, energy, speed, scale, laser byte
        sh["stats"] = {
            "canisters": h[0], "area": h[1], "heap": h[2], "gun_vertex": h[3] // 4,
            "explosion": h[4], "nv": h[5] // 6, "ne": h[6], "bounty10": h[7], "nf": h[8] // 4,
            "visdist": h[9], "energy": h[10], "speed": h[11], "normal_scale": h[12],
            "laser": h[13] >> 3, "missiles": h[13] & 7,
        }
        assert sh["stats"]["nv"] == len(sh["verts"]), (sh["name"], sh["stats"]["nv"], len(sh["verts"]))
        assert sh["stats"]["nf"] == len(sh["faces"]), (sh["name"], sh["stats"]["nf"], len(sh["faces"]))
        assert sh["stats"]["ne"] == len(sh["edges"]), (sh["name"], sh["stats"]["ne"], len(sh["edges"]))
    return ships


# ----------------------------------------------------------------- geometry
def sub(a, b): return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def draw3d_normal(P, loop):
    v0, v1, v2 = P[loop[0]], P[loop[1]], P[loop[2]]
    return cross(sub(v2, v1), sub(v0, v1))


def chain_loops(edge_list):
    """Chain undirected edges (a,b) into loops / open chains. Returns list of (vertex_list, closed)."""
    adj = {}
    for a, b in edge_list:
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    unused = set((min(a, b), max(a, b)) for a, b in edge_list)
    out = []
    while unused:
        # prefer to start at a vertex of odd degree (open chain end), else any
        starts = [v for v in adj if sum(1 for w in adj[v] if (min(v, w), max(v, w)) in unused) == 1]
        if starts:
            start = starts[0]
        else:
            start = next(iter(unused))[0]
        path = [start]
        cur = start
        while True:
            nxt = None
            for w in adj[cur]:
                key = (min(cur, w), max(cur, w))
                if key in unused:
                    nxt = w
                    unused.discard(key)
                    break
            if nxt is None:
                break
            if nxt == start:
                out.append((path, True))
                path = None
                break
            path.append(nxt)
            cur = nxt
        if path is not None:
            out.append((path, False))
    return out


def orient(P, loop, normal):
    """Rotate the loop so its first three vertices are non-collinear, then flip if needed."""
    n = len(loop)
    for k in range(n):
        cand = loop[k:] + loop[:k]
        N = draw3d_normal(P, cand)
        if dot(N, N) > 0:
            loop = cand
            break
    else:
        return loop, "degenerate"
    N = draw3d_normal(P, loop)
    d = dot(N, normal)
    if d < 0:
        loop = [loop[0]] + loop[1:][::-1]
        N = draw3d_normal(P, loop)
        d = dot(N, normal)
    return loop, ("ok" if d > 0 else "perpendicular")


def prune_dangling(edge_list):
    """Repeatedly strip edges that end at a degree-1 vertex.  What is left is the
    face's closed boundary; what was stripped are bare lines (fins, spikes)."""
    edges = list(edge_list)
    lines = []
    while True:
        deg = {}
        for a, b in edges:
            deg[a] = deg.get(a, 0) + 1
            deg[b] = deg.get(b, 0) + 1
        dangling = [e for e in edges if deg[e[0]] == 1 or deg[e[1]] == 1]
        if not dangling:
            return edges, lines
        for e in dangling:
            edges.remove(e)
            lines.append(e)


SLIVER = 0.5   # units; the extra vertex sits this far off the line, invisible at 320 px


def sliver(P, a, b, normal):
    """A bare line a-b as a sliver triangle [a, b, m], m just off the line, so Draw3D
    strokes it as a line and culls it with (approximately) the host face's normal."""
    d = sub(P[b], P[a])
    u = cross(d, normal)                       # perpendicular to the line, closest to the face plane
    if dot(u, u) == 0:
        u = cross(d, [1, 0, 0]) if abs(d[0]) < abs(d[1]) else cross(d, [0, 1, 0])
    L = dot(u, u) ** 0.5
    m = [round(P[b][k] + SLIVER * u[k] / L, 3) for k in range(3)]
    P.append(m)
    return [a, b, len(P) - 1]


def build(sh):
    P = sh["verts"]
    nv0 = len(P)
    nf = len(sh["faces"])
    polys = []       # (vertex list, host face, kind)
    notes = []
    for f in range(nf):
        boundary = [(a, b) for a, b, f1, f2 in sh["edges"] if f1 != f2 and f in (f1, f2)]
        detail = [(a, b) for a, b, f1, f2 in sh["edges"] if f1 == f2 == f]
        loop_edges, lines = prune_dangling(boundary)
        loops = chain_loops(loop_edges) if loop_edges else []
        closed = [l for l, c in loops if c]
        if len(closed) != 1 or any(not c for l, c in loops):
            notes.append("face %d: boundary makes %d loops (%s)" % (f, len(loops), ["closed" if c else "open %d" % len(l) for l, c in loops]))
        for l, c in loops:
            if c and len(l) >= 3:
                l2, st = orient(P, l, sh["faces"][f])
                if st != "ok":
                    notes.append("face %d boundary: %s" % (f, st))
                polys.append((l2, f, "boundary"))
        for l, c in chain_loops(detail) if detail else []:
            if c and len(l) >= 3:
                l2, st = orient(P, l, sh["faces"][f])
                if st != "ok":
                    notes.append("face %d detail: %s" % (f, st))
                polys.append((l2, f, "detail"))
            else:
                for k in range(len(l) - 1):
                    lines.append((l[k], l[k + 1]))
        for a, b in lines:
            tri = sliver(P, a, b, sh["faces"][f])
            tri, st = orient(P, tri, sh["faces"][f])
            if st != "ok":
                notes.append("face %d line %d-%d: %s" % (f, a, b, st))
            polys.append((tri, f, "line"))
    sh["polys"] = polys
    sh["notes"] = notes
    sh["nv0"] = nv0
    return sh


# ----------------------------------------------------------------- 3ddemo cross-check
def parse_demo(path):
    if not os.path.exists(path):
        return {}
    lines = open(path, encoding="latin-1").read().splitlines()
    ships, cur, nums = {}, None, []
    for ln in lines:
        m = re.match(r"^'(\w+) Data", ln)
        if m:
            if cur:
                ships[cur] = nums
            cur, nums = m.group(1), []
            continue
        if ln.startswith("'End Data"):
            if cur:
                ships[cur] = nums
            cur = None
            continue
        if cur and ln.strip().lower().startswith("data"):
            nums += [int(x) for x in re.findall(r"-?\d+", ln.split("'")[0][4:])]
    out = {}
    for name, n in ships.items():
        nv, nf, fc, scale = n[:4]
        i = 4
        verts = [n[i + 3 * k:i + 3 * k + 3] for k in range(nv)]
        i += 3 * nv
        counts = n[i:i + nf]
        i += nf
        faces, polys = n[i:], []
        j = 0
        for c in counts:
            polys.append(faces[j:j + c])
            j += c
        out[name] = {"verts": verts, "polys": polys}
    return out


def edge_set(polys, nv0=None):
    """Undirected edges drawn by a polygon list; edges touching sliver vertices (>= nv0) are ignored."""
    s = set()
    for p in polys:
        for k in range(len(p)):
            a, b = p[k], p[(k + 1) % len(p)]
            if nv0 is not None and (a >= nv0 or b >= nv0):
                continue
            s.add((min(a, b), max(a, b)))
    return s


# ----------------------------------------------------------------- output
def write_outputs(ships):
    os.makedirs(OUT_DIR, exist_ok=True)
    bas = ["' ships.bas - Elite cassette ship meshes for Draw3D, generated by elite_tools/blueprints.py",
           "' Do not edit: regenerate from the source.  One label per blueprint; RESTORE it, then READ:",
           "'   name$, nv, nf, nfv, nf0, nv0, canisters, area, bounty10, visdist, energy, speed, laser, missiles, gun, explosion",
           "'   nv * (x,y,z)   nf * facecount   nf * hostface   nf0 * stored normal (x,y,z)   nfv * vertex index",
           "'   nf0 / nv0 = the blueprint's own face / vertex counts (polygons beyond nf0 are detail or sliver lines,",
           "'   vertices beyond nv0 are the sliver vertices that turn bare lines into drawable triangles)",
           "' Coordinates are blueprint units (same units as ship positions).  Faces beyond the",
           "' blueprint's face count are surface-detail polygons that share their host face's normal.",
           ""]
    js = {}
    for name in sorted(ships, key=lambda n: [t[0] for t in TYPES if t[1] == n][0]):
        sh = ships[name]
        st = sh["stats"]
        polys = sh["polys"]
        nfv = sum(len(p[0]) for p in polys)
        disp = [t[2] for t in TYPES if t[1] == name][0]
        bas.append("dat_%s:" % name.lower())
        bas.append('DATA "%s", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d' % (
            disp, len(sh["verts"]), len(polys), nfv, len(sh["faces"]), sh["nv0"], st["canisters"], st["area"], st["bounty10"],
            st["visdist"], st["energy"], st["speed"], st["laser"], st["missiles"], st["gun_vertex"], st["explosion"]))
        vs = sh["verts"]
        for k in range(0, len(vs), 6):
            bas.append("DATA " + ", ".join("%g,%g,%g" % tuple(v) for v in vs[k:k + 6]))
        bas.append("DATA " + ",".join(str(len(p[0])) for p in polys))
        bas.append("DATA " + ",".join(str(p[1]) for p in polys))
        ns = sh["faces"]
        for k in range(0, len(ns), 6):
            bas.append("DATA " + ", ".join("%d,%d,%d" % tuple(n) for n in ns[k:k + 6]))
        for p in polys:
            bas.append("DATA " + ",".join(str(i) for i in p[0]))
        bas.append("")
        js[name] = {"display": disp, "types": [t[0] for t in TYPES if t[1] == name], "stats": st,
                    "vertices": vs, "normals": sh["faces"],
                    "polygons": [{"vertices": p[0], "host": p[1], "kind": p[2]} for p in polys]}
    open(os.path.join(OUT_DIR, "ships.bas"), "w", newline="\n").write("\n".join(bas))
    json.dump(js, open(os.path.join(OUT_DIR, "ships.json"), "w"), indent=1)
    return len("\n".join(bas))


def main():
    ships = parse_asm(ASM)
    for sh in ships.values():
        build(sh)
    demo = parse_demo(DEMO)
    print("%-12s %3s %3s %3s | %5s %6s %6s | %s" % ("ship", "nv", "ne", "nf", "polys", "detail", "maxfv", "notes / demo cross-check"))
    for t, name, disp in TYPES:
        if t == 7:
            continue
        sh = ships[name]
        polys = sh["polys"]
        det = sum(1 for p in polys if p[2] == "detail")
        lin = sum(1 for p in polys if p[2] == "line")
        maxfv = max(len(p[0]) for p in polys)
        msgs = list(sh["notes"])
        if lin:
            msgs.append("%d bare lines as slivers (+%d vertices)" % (lin, len(sh["verts"]) - sh["nv0"]))
        dname = [k for k, v in DEMO_MAP.items() if v == name]
        if dname and dname[0] in demo:
            d = demo[dname[0]]
            same_v = d["verts"] == sh["verts"][:sh["nv0"]]
            e_ours = edge_set([p[0] for p in polys], sh["nv0"])
            e_demo = edge_set(d["polys"])
            msgs.append("demo: verts %s, edges ours %d demo %d, missing-in-demo %d, extra-in-demo %d" % (
                "same" if same_v else "DIFFER", len(e_ours), len(e_demo), len(e_ours - e_demo), len(e_demo - e_ours)))
        else:
            msgs.append("not in 3ddemo")
        print("%-12s %3d %3d %3d | %5d %6d %6d | %s" % (disp, len(sh["verts"]), len(sh["edges"]), len(sh["faces"]),
                                                       len(polys), det, maxfv, "; ".join(msgs)))
    n = write_outputs(ships)
    print("\nwrote %s (%d bytes) and ships.json" % (os.path.join(OUT_DIR, "ships.bas"), n))


if __name__ == "__main__":
    main()
