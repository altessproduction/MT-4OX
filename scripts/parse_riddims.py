#!/usr/bin/env python3
# Parse "The Riddim Real Book (Tab).pdf" (Felix Rühling, Sibelius 6, 41 pages)
# -> riddims_draft.json : per riddim: meta + bass TAB notes (pitch from string/fret)
#    + rhythm hints (beam counts, rest glyphs) for later quantization.
import fitz, json, re, sys
from collections import defaultdict

PDF = "upload/The Riddim Real Book (Tab).pdf"
doc = fitz.open(PDF)

# ---------------------------------------------------------------- helpers
def cluster_hlines(page):
    """horizontal full-width lines grouped into staves (top->bottom)"""
    segs = []
    for d in page.get_drawings():
        for it in d["items"]:
            if it[0] == "l":
                p1, p2 = it[1], it[2]
                if abs(p1.y - p2.y) < 0.6 and abs(p1.x - p2.x) > 300:
                    segs.append((round((p1.y + p2.y) / 2, 2), min(p1.x, p2.x), max(p1.x, p2.x)))
    segs.sort()
    staves = []
    for y, x0, x1 in segs:
        if staves and y - staves[-1][-1][0] < 8:
            staves[-1].append((y, x0, x1))
        else:
            staves.append([(y, x0, x1)])
    return staves

def barlines(page, y_top, y_bot):
    """vertical lines crossing the band [y_top, y_bot] -> barline x positions"""
    xs = []
    for d in page.get_drawings():
        for it in d["items"]:
            if it[0] == "l":
                p1, p2 = it[1], it[2]
                if abs(p1.x - p2.x) < 0.8 and min(p1.y, p2.y) <= y_top + 2 and max(p1.y, p2.y) >= y_bot - 2:
                    xs.append(round((p1.x + p2.x) / 2, 1))
            elif it[0] == "re":  # filled rect (thick barline)
                r = it[1]
                if r.width < 6 and r.height > (y_bot - y_top) * 0.7:
                    xs.append(round((r.x0 + r.x1) / 2, 1))
    return sorted(set(xs))

def spans_in(page, y0, y1, fonts=None):
    """chars (x, yc, char, font, size) with center-y in band"""
    out = []
    d = page.get_text("rawdict")
    for b in d["blocks"]:
        if b.get("type") != 0:
            continue
        for l in b["lines"]:
            for s in l["spans"]:
                if fonts and not any(f in s["font"] for f in fonts):
                    continue
                for c in s["chars"]:
                    x0, cy0, x1, cy1 = c["bbox"]
                    yc = (cy0 + cy1) / 2
                    if y0 <= yc <= y1:
                        out.append((x0, yc, c["c"], s["font"], s["size"], cy0, cy1))
    out.sort()
    return out

TUNING = {  # string line index (top->bottom) -> MIDI open pitch (4-string bass TAB)
    0: 43,  # G2
    1: 38,  # D2
    2: 33,  # A1
    3: 28,  # E1
}

def nearest_string(yc, lines):
    best, bd = None, 1e9
    for i, ly in enumerate(lines):
        d = abs(yc - ly)
        if d < bd:
            best, bd = i, d
    return best, bd

def parse_page(pi):
    page = doc[pi]
    res = {"page": pi + 1}
    words = page.get_text("words")
    # ---- title: words on the biggest font size line containing 'Riddim'
    cand = [w for w in words if w[4] == "Riddim" and w[1] < 120]
    if cand:
        ry = cand[0][1]
        line = sorted([w for w in words if abs(w[1] - ry) < 12 and w[0] < 470], key=lambda w: w[0])
        res["title"] = " ".join(w[4] for w in line)
    # ---- label (right top: Studio One (1968) etc.)
    lab = sorted([w for w in words if w[1] < 100 and w[0] > 470], key=lambda w: w[0])
    if lab:
        res["label"] = " ".join(w[4] for w in lab)
    # ---- aka
    ak = [w for w in words if w[4] in ("a.k.a", "aka")]
    if ak:
        ay = ak[0][3]
        res["aka"] = " ".join(w[4] for w in sorted([w for w in words if w[1] < ay + 6 and w[3] > ay - 30], key=lambda w: w[0]))
    # ---- tempo  q = 70-78
    m = re.search(r"=\s*([\d.\-]+)", page.get_text())
    qs = [w for w in words if re.fullmatch(r"[\d.\-]+", w[4]) and w[1] < 110 and w[0] < 200]
    if qs:
        res["tempo"] = qs[0][4]
    staves = cluster_hlines(page)
    # ---- classify staves
    kinds = []
    for st in staves:
        ys = [y for y, _, _ in st]
        n = len(ys)
        kind = "tab4" if n == 4 else ("tab6" if n == 6 else ("std" if n == 5 else "?"))
        kinds.append({"kind": kind, "y0": min(ys), "y1": max(ys), "lines": ys, "x0": min(a for _, a, _ in st), "x1": max(b for _, _, b in st)})
    res["staves"] = kinds
    # ---- instrument labels (RepriseScript)
    labels = spans_in(page, 0, 842, fonts=("RepriseScript",))
    lab_txt = defaultdict(list)
    for x, yc, ch, f, sz, *_ in labels:
        if ch.strip():
            lab_txt[round(yc / 14)].append((x, ch))
    res["labels"] = {f"{k*14}": "".join(ch for _, ch in sorted(v)) for k, v in sorted(lab_txt.items())}
    # ---- chords (RepriseChords)
    chords = spans_in(page, 0, 842, fonts=("RepriseChords",))
    ch_txt = defaultdict(list)
    for x, yc, ch, f, sz, *_ in chords:
        ch_txt[round(yc / 16)].append((x, ch))
    res["chords_raw"] = [{"y": k * 16, "text": "".join(ch for _, ch in sorted(v, key=lambda t: t[0])),
                          "xs": [x for x, _ in sorted(v, key=lambda t: t[0])]}
                         for k, v in sorted(ch_txt.items())]
    # ---- text notes (Helvetica/Arial non-bold small) = remarks like "Intro", "TAB Version"
    notes = [w for w in words if w[4].strip() and w[1] > 40]
    res["remarks"] = []
    for w in words:
        t = w[4]
        if re.fullmatch(r"[A-Za-z'()/ -]{3,}", t) and t not in ("Bass", "Guitar", "Riddim") and w[1] > 40:
            res["remarks"].append({"y": round(w[1], 1), "x": round(w[0], 1), "t": t})
    res["remarks"] = res["remarks"][:40]
    return res

# ---- deep TAB extraction for a given 4-line staff band
def tab_notes(page, staff, extra_below=14):
    y0, y1 = staff["y0"], staff["y1"]
    lines = sorted(staff["lines"])
    # digits: Arial-BoldMT ~8.4pt, centers inside [y0-3, y1+3]
    digs = [c for c in spans_in(page, y0 - 4, y1 + 4) if c[2].isdigit() and "Bold" in c[3]]
    notes = []
    for x, yc, ch, f, sz, cy0, cy1 in digs:
        si, dist = nearest_string(yc, lines)
        if dist > 5.5:
            continue  # not a fret digit (rhythm row below etc.)
        notes.append({"x": round(x, 1), "str": si, "fret": int(ch), "yc": round(yc, 2)})
    # merge adjacent digits of the same note (two-digit frets share x and string)
    merged = []
    for n in notes:
        if merged and abs(n["x"] - merged[-1]["x"]) < 7 and n["str"] == merged[-1]["str"]:
            merged[-1]["fret"] = merged[-1]["fret"] * 10 + n["fret"]
        else:
            merged.append(dict(n))
    return merged

def beams_between(page, y_top, y_bot):
    """short horizontal line pairs (beam outlines) with center y in [y_top,y_bot] -> segments"""
    segs = []
    for d in page.get_drawings():
        for it in d["items"]:
            if it[0] == "l":
                p1, p2 = it[1], it[2]
                if abs(p1.y - p2.y) < 0.6:
                    y = (p1.y + p2.y) / 2
                    x0, x1 = sorted((p1.x, p2.x))
                    if y_top <= y <= y_bot and 6 < (x1 - x0) < 300:
                        segs.append((round(y, 2), round(x0, 1), round(x1, 1)))
    segs.sort()
    # pair segments: same x-range overlapping, close y -> one beam line
    lines = []
    for y, x0, x1 in segs:
        placed = False
        for L in lines:
            if abs(L["y"] - y) < 3 and not (x1 < L["x0"] - 2 or x0 > L["x1"] + 2):
                L["y"] = (L["y"] + y) / 2
                L["x0"] = min(L["x0"], x0); L["x1"] = max(L["x1"], x1)
                placed = True
                break
        if not placed:
            lines.append({"y": y, "x0": x0, "x1": x1})
    return lines

if __name__ == "__main__":
    pages = range(1, 41)
    out = []
    for pi in pages:
        out.append(parse_page(pi))
    json.dump(out, open("scripts/riddims_map.json", "w"), ensure_ascii=False, indent=1)
    for r in out:
        print(f"p{r['page']:02d} {r.get('title','?'):45s} tempo={r.get('tempo','?'):8s} label={r.get('label','')[:30]}")
