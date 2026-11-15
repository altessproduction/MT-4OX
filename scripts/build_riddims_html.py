#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Altess Production
"""Génère web/riddims.html : la page Riddims DÉDIÉE (hors émulateur).

  python3 scripts/build_riddims_html.py

La page reprend À L'IDENTIQUE :
  - le synthé d'écoute (bloc entre /* synth:begin */ et /* synth:end */),
  - le CSS .rid-* (template `const css = …`),
du module scripts/riddims_page.js — la même source canonique que la page
Riddims de l'émulateur (scripts/patch_riddims_page.py). Elle ajoute :
  - l'export MIDI par riddim : « Riddim MIDI » (batterie canal 10 en carte
    General MIDI + basse canal 1, programme « Synth Bass 1 ») et
    « Basse MIDI » (la seule ligne de basse),
  - Standard MIDI File format 1, 480 PPQ, tempo du livre (milieu de la
    fourchette), une passe des mesures — importable dans n'importe quel DAW.
Le fichier généré est committé ; web/test_riddims_midi.mjs vérifie que la
page reste en phase avec le module (blocs synth et CSS identiques) et relit
les fichiers MIDI produits.
"""
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
CANON = Path("/home/z/my-project/scripts/riddims_page.js")   # source canonique (comme patch_riddims_page.py)
REPO = HERE / "riddims_page.js"                              # copie du dépôt (repli)
OUT = HERE.parent / "web" / "riddims.html"

TEMPLATE = r'''<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="description" content="Les riddims du Riddim Real Book et le classique Sleng Teng pour le MT-40X : écoute Web Audio (batterie et basse) et export MIDI — riddim complet ou ligne de basse — à importer dans n'importe quel DAW.">
<meta name="theme-color" content="#2b7865">
<title>Riddims — MT-40X</title>
<link rel="icon" href="assets/icone.png">
<style>
/* Même palette que l'émulateur (clair et sombre). */
:root{--bg:#f5f5f7;--surface:#ffffff;--ink:#1d1d1f;--muted:#6e6e73;--rule:rgba(128,128,128,.28);--accent:#2b7865}
@media (prefers-color-scheme: dark){:root{--bg:#000000;--surface:#1c1c1e;--ink:#f5f5f7;--muted:#a1a1a6;--rule:#2c2c2e;--accent:#9fd4c4}}
*{box-sizing:border-box}
body{margin:0;font:14px/1.55 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--ink)}
.hd{border-bottom:1px solid var(--rule);background:var(--surface)}
.hd-in{max-width:1080px;margin:0 auto;padding:1.1rem 1.3rem;display:flex;gap:1rem;align-items:center;flex-wrap:wrap}
.hd h1{margin:0;font:800 1.5rem/1.1 inherit;letter-spacing:-.01em}
.hd .sub{margin:.25rem 0 0;color:var(--muted);font-size:.9rem}
.hd .rid-btn{margin-left:auto;text-decoration:none}
.wrap{max-width:1080px;margin:0 auto;padding:1.1rem 1.3rem 2rem}
.wrap .rid-tools{padding:0 0 .6rem}
.wrap .rid-status{padding:.2rem 0 .8rem}
.ft{max-width:1080px;margin:0 auto;padding:1rem 1.3rem 2.5rem;color:var(--muted);font-size:.85rem;border-top:1px solid var(--rule)}
.ft b{color:inherit}
a{color:var(--accent)}
/*{{CSS}}*/
</style>
</head>
<body>
<header class="hd">
  <div class="hd-in">
    <div>
      <h1>&#9835; Riddims</h1>
      <p class="sub"><span id="rid-count">Les riddims</span> — écoute et export MIDI pour ton DAW.</p>
    </div>
    <a class="rid-btn" href="index.html">&#9654; Ouvrir l'émulateur MT-40X</a>
  </div>
</header>
<main class="wrap">
  <div class="rid-tools">
    <input class="rid-search" type="search" placeholder="Nom, label, accord…" aria-label="Rechercher un riddim">
    <span class="rid-filters" style="display:contents"></span>
  </div>
  <div class="rid-status" role="status"></div>
  <div class="rid-list"></div>
</main>
<footer class="ft">
  <b>Export MIDI</b> — Standard MIDI File format 1, 480 PPQ, tempo du riddim (le milieu de la fourchette du
  livre), une passe des mesures du riddim. La piste batterie joue sur le canal 10, carte General MIDI : grosse caisse 36, caisse
  claire 38, charley fermé 42, charley ouvert 46, rim 37 (side stick). La piste basse joue sur le canal 1,
  programme GM « Synth Bass 1 », la ligne de basse du riddim, telle quelle. Ces fichiers s'importent tels quels
  dans n'importe quel DAW (Ableton Live, FL Studio, Reaper, Logic, GarageBand, Bitwig, Cubase…). L'écoute
  ci-dessus est la même maquette Web Audio que la page Riddims de l'<a href="index.html">émulateur MT-40X</a>.
</footer>
<noscript><div style="max-width:1080px;margin:1rem auto;padding:0 1.3rem">Cette page a besoin de JavaScript pour l'écoute et la génération des fichiers MIDI.</div></noscript>
<script src="riddims.js"></script>
<script>
"use strict";
(() => {
  const D = window.RIDDB;
  const statusEl = document.querySelector(".rid-status");
  const say = (msg, err) => { statusEl.textContent = msg || ""; statusEl.classList.toggle("err", !!err); };
  if (!D) { say("riddims.js introuvable à côté de riddims.html.", true); return; }
  let filter = "tous", playingId = null;

  /* ------------------------------------------------------------ export MIDI */
  /* midi:begin (Altess Production, GPL-3.0)
     Écriture de fichiers MIDI (SMF format 1) sans dépendance, pour le DAW :
     piste 0 méta (nom, tempo, 4/4), piste batterie canal 10 (carte GM),
     piste basse canal 1 (programme « Synth Bass 1 »). La grille RIDDB est
     en doubles-croches : 16 pas par mesure, 1 pas = PPQ/4 tics. */
  window.RIDDIM_MIDI = (() => {
    const PPQ = 480;                  // résolution du fichier : 480 tics par noire
    const STEP = PPQ / 4;             // 16 pas par mesure 4/4 : la double-croche
    const CH_BASS = 0, CH_DRUMS = 9;  // canal 1 (basse) et canal 10 (batterie GM), indexés à 0 dans les octets
    const PROG_BASS = 38;             // GM n° 39 « Synth Bass 1 » (le message compte à partir de 0)
    const DRUM_NOTE = { k: 36, s: 38, hc: 42, hcg: 42, ho: 46, rm: 37 };  // GM : grosse caisse, caisse claire, charley fermé, charley fantôme (doux), charley ouvert, rim (side stick)

    const bpmOf = (r) => r.bpm[0] === r.bpm[1] ? r.bpm[0] : Math.round((r.bpm[0] + r.bpm[1]) / 2);
    const vlq = (n) => { const b = [n & 127]; for (n >>= 7; n > 0; n >>= 7) b.unshift((n & 127) | 128); return b; };
    const be16 = (n) => [(n >> 8) & 255, n & 255];
    const be24 = (n) => [(n >> 16) & 255, (n >> 8) & 255, n & 255];
    const be32 = (n) => [(n >>> 24) & 255, (n >> 16) & 255, (n >> 8) & 255, n & 255];
    const utf8 = (s) => {              // les méta-textes en UTF-8 (les noms ont des accents)
      const out = [];
      for (const ch of s) {
        const c = ch.codePointAt(0);
        if (c < 0x80) out.push(c);
        else if (c < 0x800) out.push(0xC0 | (c >> 6), 0x80 | (c & 63));
        else if (c < 0x10000) out.push(0xE0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
        else out.push(0xF0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
      }
      return out;
    };
    const meta = (type, data) => [0xFF, type, ...vlq(data.length), ...data];
    const name = (s) => meta(0x03, utf8(s));
    const chunk = (id, data) => [...Array.from(id, (c) => c.charCodeAt(0)), ...be32(data.length), ...data];

    /* events : {tick, bytes, ord} — ord départage les événements d'un même tic :
       méta et programme avant les notes, note-off avant le note-on suivant. */
    function track(events) {
      const evs = events.slice().sort((a, b) => (a.tick - b.tick) || (a.ord || 0) - (b.ord || 0));
      const data = [];
      let last = 0;
      for (const ev of evs) { data.push(...vlq(ev.tick - last), ...ev.bytes); last = ev.tick; }
      data.push(...vlq(0), ...meta(0x2F, []));          // fin de piste
      return chunk("MTrk", data);
    }

    function bassEvents(r) {                             // événements [pas, note MIDI, durée en pas]
      const out = [];
      for (let b = 0; b < r.bars.length; b++) {
        for (const [s, m, l] of r.bars[b]) {
          const tick = (b * 16 + s) * STEP, len = Math.max(1, l) * STEP;
          out.push({ tick, ord: 1, bytes: [0x90 | CH_BASS, m & 127, s % 8 === 0 ? 112 : 96] });
          out.push({ tick: tick + len, ord: 0, bytes: [0x80 | CH_BASS, m & 127, 0] });
        }
      }
      return out;
    }

    function drumEvents(r) {                             // la batterie du riddim (propre, p. ex. Sleng Teng, sinon celle du style), mesure par mesure comme à l'écoute
      const dr = r.drm || window.RIDDB.drums[r.st], out = [];
      for (let b = 0; b < r.bars.length; b++) {
        for (const [lane, n] of Object.entries(DRUM_NOTE)) {
          for (const s of ((dr[lane] && dr[lane][b]) || [])) {
            const tick = (b * 16 + s) * STEP;
            const vel = lane === "hcg" ? 36 : (s % 8 === 0 ? 110 : 92);   // hcg : hats fantômes doux (Sleng Teng)
            out.push({ tick, ord: 1, bytes: [0x90 | CH_DRUMS, n, vel] });
            out.push({ tick: tick + STEP, ord: 0, bytes: [0x80 | CH_DRUMS, n, 0] });
          }
        }
      }
      return out;
    }

    function conductor(r) {
      const usq = Math.round(60000000 / bpmOf(r));       // µs par noire
      return track([
        { tick: 0, ord: -2, bytes: name(`MT-40X ${r.n}`) },
        { tick: 0, ord: -1, bytes: meta(0x51, [...be24(usq)]) },
        { tick: 0, ord: -1, bytes: meta(0x58, [4, 2, 24, 8]) },   // 4/4
      ]);
    }

    const bassTrack = (r) => track([
      { tick: 0, ord: -2, bytes: name("Basse") },
      { tick: 0, ord: -1, bytes: [0xC0 | CH_BASS, PROG_BASS] },
      ...bassEvents(r),
    ]);

    const smf = (tracks) => new Uint8Array([
      ...chunk("MThd", [...be16(1), ...be16(tracks.length), ...be16(PPQ)]),
      ...tracks.flat(),
    ]);

    const riddimMidi = (r) => smf([
      conductor(r),
      track([{ tick: 0, ord: -2, bytes: name("Batterie") }, ...drumEvents(r)]),
      bassTrack(r),
    ]);
    const bassMidi = (r) => smf([conductor(r), bassTrack(r)]);

    function download(bytes, filename) {
      const url = URL.createObjectURL(new Blob([bytes], { type: "audio/midi" }));
      const a = Object.assign(document.createElement("a"), { href: url, download: filename });
      document.body.appendChild(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 4000);
    }

    const fname = (r, kind) => `${r.id}-${kind}-${bpmOf(r)}bpm.mid`;
    return { PPQ, STEP, CH_BASS, CH_DRUMS, DRUM_NOTE, bpmOf, riddimMidi, bassMidi, download, fname };
  })();
  /* midi:end */

  /* ------------------------------------- écoute : le synthé de l'émulateur */
  /* Copié tel quel de scripts/riddims_page.js (bloc synth:begin/end) par
     scripts/build_riddims_html.py — même source que la page Riddims de
     l'émulateur ; web/test_riddims_midi.mjs vérifie la copie. */
  /* synth:begin */
/*{{SYNTH}}*/
  /* synth:end */

  /* --------------------------------------------------------------- cartes */
  const fmtBpm = (r) => r.bpm[0] === r.bpm[1] ? `${r.bpm[0]} BPM` : `${r.bpm[0]}–${r.bpm[1]} BPM`;
  function toggleListen(r, el) {
    if (synth.playingId === r.id) { synth.stop(); say("Écoute arrêtée."); }
    else { synth.start(r, el); say(`« ${r.n} » en écoute — ${fmtBpm(r)}. Les boutons MIDI téléchargent les fichiers.`); }
  }
  function downloadMidi(r, kind) {
    const M = window.RIDDIM_MIDI;
    M.download(kind === "basse" ? M.bassMidi(r) : M.riddimMidi(r), M.fname(r, kind));
    say(`« ${r.n} » téléchargé (${kind === "basse" ? "ligne de basse" : "riddim complet, batterie + basse"}) : ` +
      `${M.fname(r, kind)} — ${M.bpmOf(r)} BPM, à importer dans ton DAW.`);
  }
  function card(r) {
    const el = document.createElement("div");
    el.className = "rid-card"; el.dataset.id = r.id;
    const chips = [`<span class="rid-chip">${D.styles[r.st] || r.st}</span>`];
    if (r.lb) chips.push(`<span class="rid-chip dim">${r.lb}${r.y ? " · " + r.y : ""}</span>`);
    if (r.p) chips.push(`<span class="rid-chip dim">p. ${r.p}</span>`);
    el.innerHTML = `
      <div><span class="rid-name"></span>${r.a ? ` <span class="rid-aka"></span>` : ""}</div>
      <div class="rid-meta">${chips.join("")}<span class="rid-chip dim">${fmtBpm(r)}</span></div>
      ${r.ch ? `<div class="rid-chords"></div>` : ""}
      <div class="rid-acts">
        <button class="rid-btn rid-listen" type="button">&#9654; Écouter</button>
        <button class="rid-btn rid-dl-full" type="button">&#10547; Riddim MIDI</button>
        <button class="rid-btn rid-dl-bass" type="button">&#10547; Basse MIDI</button>
      </div>`;
    el.querySelector(".rid-name").textContent = r.n;
    if (r.a) el.querySelector(".rid-aka").textContent = r.a;
    if (r.ch) el.querySelector(".rid-chords").textContent = "Accords : " + r.ch;
    el.querySelector(".rid-listen").addEventListener("click", () => toggleListen(r, el));
    el.querySelector(".rid-dl-full").addEventListener("click", () => downloadMidi(r, "riddim"));
    el.querySelector(".rid-dl-bass").addEventListener("click", () => downloadMidi(r, "basse"));
    return el;
  }
  const listEl = document.querySelector(".rid-list");
  const searchEl = document.querySelector(".rid-search"), filtersEl = document.querySelector(".rid-filters");
  function render() {
    const q = searchEl.value.trim().toLowerCase();
    listEl.replaceChildren(...D.list
      .filter((r) => filter === "tous" || r.st === filter)
      .filter((r) => !q || (r.n + " " + r.a + " " + r.lb + " " + r.ch).toLowerCase().includes(q))
      .map(card));
  }
  const mkChip = (key, label) => {
    const b = document.createElement("button");
    b.type = "button"; b.className = "rid-btn" + (key === filter ? " on" : "");
    b.textContent = label;
    b.addEventListener("click", () => { filter = key; filtersEl.querySelectorAll(".rid-btn").forEach((x) => x.classList.remove("on")); b.classList.add("on"); render(); });
    return b;
  };
  filtersEl.replaceChildren(mkChip("tous", "Tous"),
    ...Object.keys(D.styles).map((k) => mkChip(k, D.styles[k])));
  searchEl.addEventListener("input", render);
  document.getElementById("rid-count").textContent = D.head
    ? D.head.charAt(0).toUpperCase() + D.head.slice(1)
    : "Les " + D.list.length + " riddims du « " + D.src + " »";
  render();
  say(`${D.list.length} riddims — « Écouter » joue batterie et basse ici même ; les boutons MIDI téléchargent des fichiers pour ton DAW.`);
})();
</script>
</body>
</html>
'''


def main():
    src = CANON if CANON.exists() else REPO
    page = src.read_text(encoding="utf-8")
    css = re.search(r"const css = `(.*?)`;", page, re.S).group(1).strip()
    synth = re.search(r"/\* synth:begin \*/(.*?)/\* synth:end \*/", page, re.S).group(1).strip()
    html = TEMPLATE.replace("/*{{CSS}}*/", css).replace("/*{{SYNTH}}*/", synth)
    if "{{" in html:
        print("ERREUR : placeholder non remplacé"); raise SystemExit(1)
    OUT.write_text(html, encoding="utf-8")
    print(f"web/riddims.html écrit ({len(html)} caractères) — synthé et CSS copiés de {src}")


if __name__ == "__main__":
    main()
