#!/usr/bin/env python3
# Génère les données de la page Riddims du site web (web/index.html, bloc RIDDB).
# Source : scripts/riddims_final.json (38 riddims du Riddim Real Book, extraits du PDF),
# puis le classique Sleng Teng (le preset « Rock » du MT-40, transcription vérifiée du
# firmware MT-40X, injectée en dernier — voir SLENG_TENG ci-dessous).
#   - basse : notes MIDI absolues telles qu'extraites (rythme + hauteurs fidèles au livre)
#   - batterie : pattern par style (one drop, rockers, steppers, rocksteady, digital, dancehall, reggae)
#   - métadonnées du livre : label, année, akas, accords d'origine, tempo
import json, sys
from pathlib import Path

SRC = Path("/home/z/my-project/scripts/riddims_final.json")
SRC_TITLE = "The Riddim Real Book (Tab)"
OUT = Path("/home/z/my-project/build-fw/MT-4OX/web/riddims.js")

# métadonnées du livre (extraites du PDF : libellés, années, akas, accords d'origine)
BOOK = {
    "african-beat":     dict(lb="Studio One", y=1968,   a="", ch="B · F#"),
    "all-we-need":      dict(lb="Keith Hudson", y=1972, a="", ch="Am · D · G · D"),
    "answer":           dict(lb="Studio One", y=1967,   a="a.k.a. Never Let You Go", ch="Bb · Eb"),
    "artibella":        dict(lb="Phill Pratt", y=0,     a="", ch="Dm · Am · Dm"),
    "baby-i-love-you":  dict(lb="", y=0,                a="", ch="G#m · C#m"),
    "ballistic":        dict(lb="", y=0,                a="", ch="Am · Bm"),
    "bam-bam":          dict(lb="", y=0,                a="riddim Stalag", ch="D"),
    "beat-down-babylon":dict(lb="", y=0,                a="", ch="Am · G · Am · C"),
    "better-must-come": dict(lb="Bunny Lee", y=0,       a="", ch="A · D"),
    "billie-jean":      dict(lb="", y=0,                a="d'après E. Morricone", ch="Gm · C"),
    "created-by-father":dict(lb="Studio One", y=1970,   a="", ch="Dm · Am · Dm"),
    "cuss-cuss":        dict(lb="Harry J", y=1968,      a="", ch="Cm · Bb"),
    "death-in-arena":   dict(lb="Studio One", y=1968,   a="d'après P. Purdie", ch="Bm · D"),
    "dirty-harry":      dict(lb="Glen Brown", y=1972,   a="", ch="Am · G"),
    "drifter":          dict(lb="Harry Mudie", y=1969,  a="", ch=""),
    "duck":             dict(lb="Jammy's", y=1987,      a="", ch="A · Em"),
    "expo-train":       dict(lb="Treasure Isle", y=1967,a="a.k.a. The Last Train", ch="G · Em"),
    "far-east":         dict(lb="", y=0,                a="", ch="C · Dm"),
    "fattie-fattie":    dict(lb="Studio One", y=1967,   a="", ch="Em · F#m · G · A"),
    "get-in-groove":    dict(lb="Studio One", y=0,      a="", ch="C · Dm · Em · Dm"),
    "head-to-toe":      dict(lb="Jammy's", y=1986,      a="", ch="Bb · Cm"),
    "movie-star":       dict(lb="Studio One", y=1970,   a="", ch="Gm · Cm"),
    "my-conversation":  dict(lb="Bunny Lee", y=0,       a="", ch="F# · G#m"),
    "my-jamaican-girl": dict(lb="Beverleys", y=1970,    a="", ch="Eb · Gm"),
    "pick-up-pieces":   dict(lb="Studio One", y=1967,   a="", ch="Am · Bm"),
    "pressure-slide":   dict(lb="Studio One", y=1967,   a="", ch="Bb · Cm"),
    "pretty-looks":     dict(lb="Studio One", y=1968,   a="", ch="Am · Bm"),
    "punaany":          dict(lb="Jammy's", y=1987,      a="", ch="G · Dm7"),
    "queen-majesty":    dict(lb="Treasure Isle", y=1967,a="d'après C. Mayfield", ch="Dm7 · G"),
    "real-rock":        dict(lb="Studio One", y=1968,   a="", ch="C (Bb)"),
    "stalag":           dict(lb="", y=0,                a="", ch="Bb · Ab"),
    "stars":            dict(lb="Studio One", y=0,      a="", ch="A · C#m · D · E"),
    "swing-easy":       dict(lb="Studio One", y=1967,   a="", ch="Ebm · Abm"),
    "take-a-ride":      dict(lb="Studio One", y=0,      a="a.k.a. Truth & Rights", ch="C#m · E · G#"),
    "tempo":            dict(lb="", y=0,                a="", ch="Bm · G"),
    "world-a-music":    dict(lb="Taxi", y=1983,         a="", ch="Eb · Bb"),
    "you-dont-care":    dict(lb="Treasure Isle", y=1967,a="a.k.a. Barbwire", ch="Eb · Bb"),
    "youth-man":        dict(lb="Glen Brown", y=0,      a="a.k.a. Wicked Can't Run Away", ch="Bbm · Fm"),
}

NAMES_FR = {
    "reggae": "Reggae", "onedrop": "One drop", "rockers": "Rockers", "steppers": "Steppers",
    "rocksteady": "Rocksteady", "digital": "Digital", "dancehall": "Dancehall",
}

# batteries par style : 16 pas par mesure (double-croches), voies du DRUM engine du FM-1
# k = kick, s = snare, hc = hat fermée, ho = hat ouverte, rm = rim
DRUMS = {
    # one drop : la grosse caisse et le rim ensemble sur le temps 3, rien ailleurs
    "onedrop":    dict(k=[[8], [8]], s=[[], []], hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]],
                       ho=[[], [14]], rm=[[8], [8]]),
    # reggae (famille one drop, un poil plus de pompe)
    "reggae":     dict(k=[[8, 11], [8, 11]], s=[[], []], hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]],
                       ho=[[], [14]], rm=[[8], [8]]),
    # rockers : la grosse caisse roule (1, 2et, 3, 4et)
    "rockers":    dict(k=[[0, 6, 8, 14], [0, 6, 8, 14]], s=[[], []],
                       hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]], ho=[[], [14]], rm=[[8], [8]]),
    # steppers : grosse caisse à chaque temps
    "steppers":   dict(k=[[0, 4, 8, 12], [0, 4, 8, 12]], s=[[], []],
                       hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]], ho=[[6], [6, 14]], rm=[[8], [8]]),
    # rocksteady : posé, sparse
    "rocksteady": dict(k=[[0, 8], [0, 8]], s=[[], []], hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]],
                       ho=[[], []], rm=[[8], [8]]),
    # digital (famille Sleng Teng) : pompe programmatrice, caisse claire 2 et 4
    "digital":    dict(k=[[0, 6, 10], [0, 6, 10]], s=[[4, 12], [4, 12]],
                       hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]], ho=[[], [14]], rm=[[], []]),
    # dancehall
    "dancehall":  dict(k=[[0, 6, 10], [0, 6, 10]], s=[[4, 12], [4, 12]],
                       hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]], ho=[[], [14]], rm=[[], []]),
}

# Le classique Sleng Teng, en dernier dans la liste : le preset « Rock » du MT-40
# (Hiroko Okuda, 1981) qui a lancé le dancehall numérique — Wayne Smith, « Under Mi
# Sleng Teng », Jammy's Records, 1985, 82 BPM. Transcription du firmware MT-40X
# (mt40_patterns.c, pat_ROCK, « verified » : deux tablatures recoupées, dossier
# technique § 3 et annexe A), racine de référence Ré (D2 = MIDI 38) :
# approche -1 puis fondamentale ×3, trois cellules, paire quarte (+5) / tierce (+4).
# Batterie : celle du preset (kick 1 et 3, snare 2 et 4, hats en croches, hat ouverte
# sur le dernier 1/16) — pas la batterie générique « digital » : DRM_OVERRIDE la
# transporte dans l'entrée. « fw » : côté firmware, « Motif de basse » charge le
# preset ROCK (index 0 de la table des motifs) — le riddim d'origine lui-même.
SLENG_BASS_D2 = 38
SLENG_BARS = [
    # mesures 1 et 2 (pas, MIDI, durée en pas) : offsets du pat_ROCK + 38
    [[0, SLENG_BASS_D2 - 1, 2], [2, SLENG_BASS_D2, 3], [5, SLENG_BASS_D2, 3],
     [8, SLENG_BASS_D2, 2], [10, SLENG_BASS_D2 - 1, 2], [12, SLENG_BASS_D2, 3],
     [15, SLENG_BASS_D2, 3]],
    [[2, SLENG_BASS_D2, 2], [4, SLENG_BASS_D2 - 1, 2], [6, SLENG_BASS_D2, 3],
     [9, SLENG_BASS_D2, 3], [12, SLENG_BASS_D2, 2], [14, SLENG_BASS_D2 + 5, 1],
     [15, SLENG_BASS_D2 + 4, 1]],
]
DRM_OVERRIDE = {
    "sleng-teng": dict(k=[[0, 8], [0, 8]], s=[[4, 12], [4, 12]],
                       hc=[[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 4, 6, 8, 10, 12, 14]],
                       ho=[[15], [15]], rm=[[], []]),
}
FW_ALIAS = {"sleng-teng": 0}          # le preset ROCK de la table du firmware


def sleng_teng_item():
    return {
        "id": "sleng-teng", "n": "Sleng Teng",
        "a": "a.k.a. Under Mi Sleng Teng",
        "lb": "Jammy's", "y": 1985,
        "st": "digital", "bpm": [82, 82],
        "ch": "D", "p": 0, "bars": [list(b) for b in SLENG_BARS],
        "drm": DRM_OVERRIDE["sleng-teng"], "fw": FW_ALIAS["sleng-teng"],
    }


def clean_bars(raw_bars):
    """normalise les mesures : 1 ou 2 mesures de 16 pas, notes [pas, midi, longueur].
    - une "mesure" dont les pas vont jusqu'à 31 : en fait 2 mesures (le parseur les a fusionnées)
    - un pas >= 16 d'une vraie mesure : note d'appel (len 1) -> pas 15 si libre, sinon perdue ;
      note tenue (len >= 2) -> repliée au pas équivalent de la mesure suivante (repli en tête)."""
    bars = []
    for bar in raw_bars:
        steps = [[int(s["step"]), int(s["midi"]), int(s["len"])] for s in bar["steps"]]
        if steps and max(s[0] for s in steps) >= 16 and len(raw_bars) == 1:
            bars.append([s for s in steps if s[0] < 16])
            bars.append([[s[0] - 16, s[1], s[2]] for s in steps if s[0] >= 16])
        else:
            bars.append(steps)
    out = []
    for bi, bar in enumerate(bars):
        keep, rest = [], []
        for s in bar:
            (keep if s[0] < 16 else rest).append(s)
        for s in rest:
            if s[2] <= 1:                                   # appel : une double-croche plus tôt
                if not any(t[0] == 15 for t in keep):
                    keep.append([15, s[1], 1])
            else:                                           # tenue : la mesure suivante (repli)
                tj = (bi + 1) % len(bars)
                if not any(t[0] == s[0] - 16 for t in bars[tj]):
                    bars[tj].append([s[0] - 16, s[1], s[2]])
        keep.sort()
        out.append(keep)
    # la 2e mesure identique à la 1re : un motif d'une seule mesure suffit
    if len(out) == 2 and out[0] == out[1]:
        out = [out[0]]
    return out

def main():
    d = json.loads(SRC.read_text(encoding="utf-8"))
    out = []
    for r in d:
        b = BOOK[r["id"]]
        bars = clean_bars(r["bars"])
        lo, hi = r["bpm"]
        item = {
            "id": r["id"], "n": r["name"], "a": b["a"], "lb": b["lb"], "y": b["y"],
            "st": r["style"], "bpm": [int(lo), int(hi)],
            "ch": b["ch"], "p": r["page"], "bars": bars,
        }
        if r["id"] in DRM_OVERRIDE:              # batterie propre à un riddim (Sleng Teng)
            item["drm"] = DRM_OVERRIDE[r["id"]]
        if r["id"] in FW_ALIAS:                  # motif déjà présent comme preset du firmware
            item["fw"] = FW_ALIAS[r["id"]]
        out.append(item)
    out.append(sleng_teng_item())                # le classique, en dernier
    # validation
    for r in out:
        assert 1 <= len(r["bars"]) <= 2, r["id"]
        for bar in r["bars"]:
            assert bar and all(0 <= s[0] <= 15 and 24 <= s[1] <= 60 and 1 <= s[2] <= 8 for s in bar), (r["id"], bar)
        if "drm" in r:
            for lane in ("k", "s", "hc", "ho", "rm"):
                assert (len(r["drm"][lane]) == len(r["bars"]) and
                        all(0 <= s <= 15 for st in r["drm"][lane] for s in st)), (r["id"], lane)
    head = f"les {len(d)} du « {SRC_TITLE} » et le classique Sleng Teng (Jammy's, 1985)"
    data = {
        "gen": "2026-11-16", "src": SRC_TITLE, "head": head,
        "styles": NAMES_FR, "drums": DRUMS, "list": out,
    }
    js = "/* Données de la page Riddims — générée par scripts/riddims_site.py : les riddims\n"
    js += "   du Riddim Real Book plus le classique Sleng Teng (preset Rock du MT-40).\n"
    js += "   Basse : notes telles qu'extraites (rythme + hauteurs). Batterie : pattern par style.\n"
    js += "   Ne pas éditer à la main : régénérer. */\n\"use strict\";\nwindow.RIDDB = "
    js += json.dumps(data, ensure_ascii=False, separators=(",", ":")) + ";\n"
    OUT.write_text(js, encoding="utf-8")
    print(f"OK {OUT} ({len(js)} octets, {len(out)} riddims)")

if __name__ == "__main__":
    main()
