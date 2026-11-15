// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Altess Production
//
// Vérification Node de la page Riddims dédiée (web/riddims.html) et de son
// export MIDI. Depuis la racine du dépôt :
//   node web/test_riddims_midi.mjs
//
//  1. Le module RIDDIM_MIDI de la page écrit des SMF (Standard MIDI File)
//     format 1 valides : en-têtes MThd/MTrk aux longueurs exactes, tempo du
//     livre (µs par noire), note-on/off aux bons tics (1 pas = PPQ/4),
//     batterie sur le canal 10 (carte GM), basse sur le canal 1 avec le
//     programme « Synth Bass 1 », fin de piste. Les 38 riddims × 2 fichiers
//     (riddim complet, basse seule) sont générés puis relus par un parseur
//     SMF écrit pour le test.
//  2. La page reste en phase avec le module de l'émulateur : le bloc synth
//     (synth:begin/end) et le CSS de scripts/riddims_page.js sont identiques
//     (à espace près) à ceux de web/index.html et de web/riddims.html — la
//     page dédiée ne peut pas diverger de l'émulateur sans échec du test.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const strip = (s) => s.replace(/\s+/g, "");

const HERE = fileURLToPath(new URL(".", import.meta.url));
const pageSrc = readFileSync(join(HERE, "riddims.html"), "utf8");
const moduleSrc = readFileSync(join(HERE, "..", "scripts", "riddims_page.js"), "utf8");
const indexSrc = readFileSync(join(HERE, "index.html"), "utf8");
const dataSrc = readFileSync(join(HERE, "riddims.js"), "utf8");

/* --- extraction du module MIDI de la page ------------------------------------ */
const m0 = pageSrc.indexOf("/* midi:begin"), m1 = pageSrc.indexOf("/* midi:end");
if (m0 < 0 || m1 < 0 || m1 <= m0) { console.error("module midi introuvable dans riddims.html"); process.exit(2); }
const midiSrc = pageSrc.slice(m0, m1);

/* --- VM : données réelles + module (aucun DOM nécessaire pour l'écriture) ----- */
const sandbox = { window: {} };
vm.runInNewContext(`${dataSrc}\n${midiSrc}`, sandbox, { timeout: 5000 });
const M = sandbox.window.RIDDIM_MIDI;
const RIDDB = sandbox.window.RIDDB;
if (!M || !RIDDB) { console.error("RIDDIM_MIDI ou RIDDB absent"); process.exit(2); }

/* --- parseur SMF (le générateur n'écrit pas de running status) ---------------- */
function parseSMF(bytes) {
  const ascii = (o, n) => String.fromCharCode(...bytes.slice(o, o + n));
  const rd32 = (o) => ((bytes[o] << 24) | (bytes[o + 1] << 16) | (bytes[o + 2] << 8) | bytes[o + 3]) >>> 0;
  const rd16 = (o) => (bytes[o] << 8) | bytes[o + 1];
  if (ascii(0, 4) !== "MThd") throw new Error("MThd attendu");
  const hlen = rd32(4), format = rd16(8), ntrk = rd16(10), div = rd16(12);
  const tracks = [];
  let o = 8 + hlen;
  for (let t = 0; t < ntrk; t++) {
    if (ascii(o, 4) !== "MTrk") throw new Error(`MTrk attendu (piste ${t})`);
    const len = rd32(o + 4), end = o + 8 + len;
    let p = o + 8, tick = 0;
    const evs = [];
    while (p < end) {
      let d = 0, b;
      do { b = bytes[p++]; d = (d << 7) | (b & 0x7F); } while (b & 0x80);
      tick += d;
      const st = bytes[p++];
      if (st === 0xFF) {
        const type = bytes[p++];
        let l = 0, lb;
        do { lb = bytes[p++]; l = (l << 7) | (lb & 0x7F); } while (lb & 0x80);
        evs.push({ tick, kind: "meta", type, data: bytes.slice(p, p + l) });
        p += l;
      } else if (st === 0xF0 || st === 0xF7) {
        let l = 0, lb;
        do { lb = bytes[p++]; l = (l << 7) | (lb & 0x7F); } while (lb & 0x80);
        p += l;
        evs.push({ tick, kind: "sysex" });
      } else if (st & 0x80) {
        const nb = (st & 0xF0) === 0xC0 || (st & 0xF0) === 0xD0 ? 1 : 2;
        const d1 = bytes[p++], d2 = nb === 2 ? bytes[p++] : 0;
        evs.push({ tick, kind: "ch", status: st, ch: st & 15, d1, d2 });
      } else throw new Error(`statut invalide ${st} (piste ${t})`);
    }
    if (p !== end) throw new Error(`longueur MTrk inexacte (piste ${t}, reste ${end - p} octets)`);
    tracks.push({ len, evs });
    o = end;
  }
  if (o !== bytes.length) throw new Error("octets en trop après la dernière piste");
  return { format, ntrk, div, tracks };
}
const arr = (u8) => Array.from(u8);
const eotOk = (tr) => tr.evs.length && tr.evs[tr.evs.length - 1].kind === "meta" && tr.evs[tr.evs.length - 1].type === 0x2F;

/* --- 1. fichier complet de Stalag : structure attendue ------------------------- */
const rid = RIDDB.list.find((r) => r.id === "stalag");
if (!rid) { console.error("stalag absent des données RIDDB"); process.exit(2); }
const STEP = M.STEP;
const full = parseSMF(arr(M.riddimMidi(rid)));

ok(full.format === 1 && full.div === M.PPQ && full.ntrk === 3,
  `midi: SMF format 1, ${M.PPQ} PPQ, 3 pistes`);
ok(full.tracks.every(eotOk), "midi: fin de piste (FF 2F) en fin de chaque piste");

const tempoEv = full.tracks[0].evs.find((e) => e.kind === "meta" && e.type === 0x51);
const usq = (tempoEv.data[0] << 16) | (tempoEv.data[1] << 8) | tempoEv.data[2];
ok(usq === Math.round(60000000 / M.bpmOf(rid)),
  `midi: tempo ${M.bpmOf(rid)} BPM (${usq} µs/noire)`);
const tsEv = full.tracks[0].evs.find((e) => e.kind === "meta" && e.type === 0x58);
ok(tsEv && tsEv.data.join(",") === "4,2,24,8", "midi: signature 4/4");
ok(!full.tracks[0].evs.some((e) => e.kind === "ch"), "midi: la piste méta n'a pas d'événement de canal");

/* basse : les événements [pas, note, durée] des 2 mesures, au tic attendu */
const expBass = [];
for (let b = 0; b < rid.bars.length; b++) for (const [s, m, l] of rid.bars[b]) expBass.push({ tick: (b * 16 + s) * STEP, m, len: l * STEP, s });
const bassEvs = full.tracks[2].evs.filter((e) => e.kind === "ch" && e.ch === M.CH_BASS);
const ons = bassEvs.filter((e) => (e.status & 0xF0) === 0x90 && e.d2 > 0);
const offs = bassEvs.filter((e) => (e.status & 0xF0) === 0x80);
ok(ons.length === expBass.length,
  `midi: basse — ${ons.length} notes pour ${expBass.length} événements du livre`);
ok(ons.every((e, i) => e.tick === expBass[i].tick && e.d1 === expBass[i].m),
  `midi: basse — tics et hauteurs de la mesure 0 : ${expBass.slice(0, 6).map((e) => e.m).join(",")}…`);
ok(offs.every((o) => expBass.some((e) => e.tick + e.len === o.tick && e.m === o.d1)) &&
   offs.length === ons.length,
  "midi: basse — chaque note s'arrête à pas × durée (l × PPQ/4)");
ok(ons.every((e, i) => e.d2 === (expBass[i].s % 8 === 0 ? 112 : 96)),
  "midi: basse — vélocités temps forts/ordinaires (112/96)");
const progEv = full.tracks[2].evs.find((e) => e.kind === "ch" && (e.status & 0xF0) === 0xC0);
ok(progEv && progEv.d1 === 38 && progEv.tick === 0, "midi: basse — programme GM 38 « Synth Bass 1 » au tic 0");

/* batterie : carte GM sur le canal 10, mesure par mesure comme à l'écoute */
const dr = RIDDB.drums[rid.st];
const expDrum = [];
for (let b = 0; b < rid.bars.length; b++) {
  for (const [lane, n] of Object.entries(M.DRUM_NOTE)) for (const s of dr[lane][b] || []) expDrum.push({ tick: (b * 16 + s) * STEP, n, s });
}
const drumEvs = full.tracks[1].evs.filter((e) => e.kind === "ch" && e.ch === M.CH_DRUMS);
const dOns = drumEvs.filter((e) => (e.status & 0xF0) === 0x90 && e.d2 > 0);
ok(dOns.length === expDrum.length,
  `midi: batterie — ${dOns.length} frappes pour ${expDrum.length} du style`);
ok(expDrum.every((e) => dOns.some((o) => o.tick === e.tick && o.d1 === e.n)),
  `midi: batterie — carte GM ${Object.entries(M.DRUM_NOTE).map(([k, n]) => k + "→" + n).join(", ")}`);
ok(!full.tracks[1].evs.some((e) => e.kind === "ch" && e.ch !== M.CH_DRUMS),
  "midi: batterie — rien hors canal 10");
ok(!full.tracks[2].evs.some((e) => e.kind === "ch" && e.ch !== M.CH_BASS),
  "midi: basse — rien hors canal 1");

/* --- 2. basse seule : deux pistes, aucune frappe ------------------------------- */
const only = parseSMF(arr(M.bassMidi(rid)));
ok(only.ntrk === 2, "midi: basse seule — 2 pistes (méta + basse)");
ok(!only.tracks.flatMap((t) => t.evs).some((e) => e.kind === "ch" && e.ch === M.CH_DRUMS),
  "midi: basse seule — aucune note de batterie");
const onlyOns = only.tracks[1].evs.filter((e) => e.kind === "ch" && (e.status & 0xF0) === 0x90 && e.d2 > 0);
ok(onlyOns.length === expBass.length, "midi: basse seule — la même ligne de basse");

/* --- 3. les 38 riddims : tout se génère et se relit ----------------------------- */
let nOk = 0;
for (const r of RIDDB.list) {
  try {
    const f = parseSMF(arr(M.riddimMidi(r)));
    const b = parseSMF(arr(M.bassMidi(r)));
    const bassNotes = f.tracks[2].evs.filter((e) => e.kind === "ch" && (e.status & 0xF0) === 0x90 && e.d2 > 0).length;
    const drumNotes = f.tracks[1].evs.filter((e) => e.kind === "ch" && (e.status & 0xF0) === 0x90 && e.d2 > 0).length;
    const tEv = f.tracks[0].evs.find((e) => e.kind === "meta" && e.type === 0x51);
    const u = (tEv.data[0] << 16) | (tEv.data[1] << 8) | tEv.data[2];
    const bpm = Math.round(60000000 / u);
    if (f.ntrk === 3 && b.ntrk === 2 && f.tracks.every(eotOk) && b.tracks.every(eotOk) &&
        bassNotes >= 1 && drumNotes >= 1 && bpm >= 40 && bpm <= 200 &&
        bassNotes === b.tracks[1].evs.filter((e) => e.kind === "ch" && (e.status & 0xF0) === 0x90 && e.d2 > 0).length) nOk++;
  } catch (err) { console.error(`  ${r.id} : ${err.message}`); }
}
ok(nOk === RIDDB.list.length, `midi: les ${RIDDB.list.length} riddims × 2 fichiers se génèrent et se relent (${nOk})`);
ok(M.fname(rid, "riddim") === `stalag-riddim-${M.bpmOf(rid)}bpm.mid` &&
   M.fname(rid, "basse") === `stalag-basse-${M.bpmOf(rid)}bpm.mid`,
  `midi: noms de fichiers (${M.fname(rid, "riddim")})`);

/* --- 3 bis. Sleng Teng : le classique, sa batterie propre (drm) et 82 BPM ------- */
const sleng = RIDDB.list.find((r) => r.id === "sleng-teng");
if (!sleng) { console.error("sleng-teng absent des données RIDDB"); process.exit(2); }
const sf = parseSMF(arr(M.riddimMidi(sleng)));
const sOns = (n) => sf.tracks[1].evs.filter((e) => e.kind === "ch" && e.ch === M.CH_DRUMS &&
  (e.status & 0xF0) === 0x90 && e.d2 > 0 && e.d1 === n).map((e) => e.tick).sort((a, b) => a - b);
ok(M.bpmOf(sleng) === 82, "midi sleng: tempo 82 BPM (Under Mi Sleng Teng)");
ok(M.fname(sleng, "riddim") === `sleng-teng-riddim-82bpm.mid`, "midi sleng: nom de fichier");
const tickSet = (arr) => [...new Set(arr.map((t) => `${t}`))].sort((a, b) => a - b).join(",");
ok(sOns(36).length === 4 && tickSet(sOns(36)) === tickSet([0, 8, 16, 24].map((s) => s * M.STEP)),
  `midi sleng: kick GM 36 aux pas 0, 8, 16, 24 (${sOns(36)})`);
ok(sOns(38).length === 4 && tickSet(sOns(38)) === tickSet([4, 12, 20, 28].map((s) => s * M.STEP)),
  "midi sleng: caisse claire GM 38 aux pas 4, 12, 20, 28");
ok(sOns(42).length === 16 && tickSet(sOns(42)) === tickSet([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30].map((s) => s * M.STEP)),
  "midi sleng: charley fermé GM 42 en croches sur les deux mesures");
ok(sOns(46).length === 2 && tickSet(sOns(46)) === tickSet([15, 31].map((s) => s * M.STEP)),
  "midi sleng: charley ouvert GM 46 sur les pas 15 et 31");
ok(!sf.tracks[1].evs.some((e) => e.kind === "ch" && e.ch === M.CH_DRUMS && e.d1 === 37),
  "midi sleng: aucun rim (le preset ROCK du MT-40 n'en a pas)");
const sBassOns = sf.tracks[2].evs.filter((e) => e.kind === "ch" && e.ch === M.CH_BASS &&
  (e.status & 0xF0) === 0x90 && e.d2 > 0);
ok(sBassOns.length === sleng.bars.flat().length,
  `midi sleng: ${sBassOns.length} notes de basse (les ${sleng.bars.flat().length} événements du preset)`);
ok(sBassOns.map((e) => e.d1).join(",") === "37,38,38,38,37,38,38,38,37,38,38,38,43,42",
  "midi sleng: la ligne en Ré — approche 37, fondamentale 38, quarte 43, tierce 42");
ok(sBassOns[0].tick === 0 && sBassOns[7].tick === 18 * M.STEP && sBassOns[7].d1 === 38,
  "midi sleng: la mesure 2 (première note au pas 2) suit la mesure 1");

/* --- 4. la page dédiée reste en phase avec le module de l'émulateur ------------- */
const synthBlock = (src) => {
  const a = src.indexOf("/* synth:begin"), b = src.indexOf("/* synth:end");
  if (a < 0 || b < 0 || b <= a) throw new Error("bloc synth introuvable");
  return src.slice(a, b);
};
try {
  ok(strip(synthBlock(pageSrc)) === strip(synthBlock(moduleSrc)),
    "sync: synth riddims.html = scripts/riddims_page.js");
  const b0 = indexSrc.indexOf("<!-- riddims:begin"), b1 = indexSrc.indexOf("<!-- riddims:end");
  ok(strip(synthBlock(indexSrc.slice(b0, b1))) === strip(synthBlock(moduleSrc)),
    "sync: synth index.html = scripts/riddims_page.js");
  const css = moduleSrc.match(/const css = `([\s\S]*?)`;/)[1];
  ok(strip(pageSrc).includes(strip(css)), "sync: CSS riddims.html = scripts/riddims_page.js");
} catch (err) { ok(false, `sync: ${err.message}`); }
ok(indexSrc.includes('<a href="riddims.html">page Riddims dédiée</a>'),
  "sync: l'émulateur renvoie vers la page dédiée");
ok(!pageSrc.includes("window.__engine") && !pageSrc.includes("fw-picker"),
  "page: aucun accès au moteur de l'émulateur (page autonome)");

console.log(failed === 0
  ? "\nRÉSULTAT : l'export MIDI est valide et la page dédiée est en phase avec l'émulateur."
  : "\nRÉSULTAT : ÉCHEC — voir les lignes FAIL.");
process.exit(failed === 0 ? 0 : 1);
