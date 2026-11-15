// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Altess Production
//
// Vérification Node de la page Riddims (bloc riddims:begin/end de index.html),
// sans navigateur ni matériel. Depuis la racine du dépôt :
//   node web/test_riddims.mjs
//
// L'écoute (« Écouter ») doit programmer la BASSE du riddim, pas seulement la
// batterie — régression : le destructuring `[, s, m, l]` décalait les champs de
// l'événement [pas, note, durée] et bassNote() n'était jamais appelée.
// Le harnais extrait l'IIFE `const synth = …` du bloc riddims, l'exécute dans un
// contexte VM avec un AudioContext factice, joue « Stalag » (données RIDDB
// réelles) sur plus de deux mesures et vérifie :
//   1. bassNote() est appelée,
//   2. les premiers appels correspondent aux événements de la mesure 0,
//   3. les hauteurs jouées sur 2 mesures = celles des données,
//   4. la batterie sonne toujours (starts d'oscillateurs).
// Puis il joue « Sleng Teng » (le classique ajouté en dernier) et vérifie :
//   5. sa basse en Ré : le galop du MIDI de référence — approche 37 puis la
//      fondamentale 38 RÉPÉTÉE en doubles-croches (C#-D-D-D ×3), quarte 43 /
//      tierce 42 en conclusion,
//   6. sa batterie vient de r.drm (kick aux pas 0, 4, 6, 8, 12 — pas le style
//      digital).

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };

const HERE = fileURLToPath(new URL(".", import.meta.url));
const html = readFileSync(join(HERE, "index.html"), "utf8");

/* --- extraction du bloc riddims --------------------------------------------- */
const b0 = html.indexOf("<!-- riddims:begin");
const b1 = html.indexOf("<!-- riddims:end");
if (b0 < 0 || b1 < 0) { console.error("bloc riddims introuvable dans index.html"); process.exit(2); }
const block = html.slice(b0, b1);
const s0 = block.indexOf("const synth = (() => {");
const s1 = block.indexOf("function toggleListen");
if (s0 < 0 || s1 < 0 || s1 <= s0) { console.error("IIFE synth introuvable"); process.exit(2); }
let synthSrc = block.slice(s0, s1);

/* --- instrumentation : bassNote et kick journalisent leurs appels -------------- */
const BASS_MARK = "function bassNote(t, midi, dur) {";
if (!synthSrc.includes(BASS_MARK)) { console.error("bassNote introuvable"); process.exit(2); }
synthSrc = synthSrc.replace(BASS_MARK, BASS_MARK + " __bassCalls.push({t, midi, dur});");
const KICK_MARK = "function kick(t) {";
if (!synthSrc.includes(KICK_MARK)) { console.error("kick introuvable"); process.exit(2); }
synthSrc = synthSrc.replace(KICK_MARK, KICK_MARK + " __kickCalls.push({t});");

/* --- données réelles (web/riddims.js → window.RIDDB) ------------------------- */
const dataSrc = readFileSync(join(HERE, "riddims.js"), "utf8");

/* --- AudioContext factice ----------------------------------------------------- */
class Param {
  constructor(v = 1) { this.value = v; }
  setValueAtTime() {} linearRampToValueAtTime() {} exponentialRampToValueAtTime() {}
}
class FNode {
  constructor(ac, kind) { this.ctx = ac; this._kind = kind; this.gain = new Param(); this.frequency = new Param(); this.Q = new Param(); }
  connect(n) { return n; }
  start(t) { this.ctx._starts.push({ t: t ?? 0, kind: this._kind, type: this.type }); }
  stop() {}
}
class MockAC {
  constructor() { this.currentTime = 0; this.sampleRate = 48000; this.state = "running"; this.destination = {}; this._starts = []; MockAC.all.push(this); }
  resume() { this.state = "running"; return Promise.resolve(); }
  createGain() { return new FNode(this, "gain"); }
  createBiquadFilter() { return new FNode(this, "filter"); }
  createOscillator() { const n = new FNode(this, "osc"); n.type = "sine"; return n; }
  createBufferSource() { const n = new FNode(this, "src"); n.buffer = null; return n; }
  createBuffer(ch, len) { return { getChannelData: () => new Float32Array(len) }; }
}
MockAC.all = [];

/* --- VM ------------------------------------------------------------------------ */
const timers = [];
const sandbox = {
  window: { AudioContext: MockAC },
  document: { querySelectorAll: () => [] },
  setInterval: (cb) => { timers.push(cb); return timers.length; },
  clearInterval: () => {},
  Math, Promise,
  __bassCalls: [],
  __kickCalls: [],
};
const synth = vm.runInNewContext(`
  "use strict";
  ${dataSrc}
  const D = window.RIDDB;
  let playingId = null;
  ${synthSrc}
  synth;
`, sandbox, { timeout: 5000 });

/* --- lecture : Stalag (2 mesures, 15 événements de basse) ------------------------ */
const rid = sandbox.window.RIDDB.list.find((r) => r.id === "stalag");
if (!rid) { console.error("stalag absent des données RIDDB"); process.exit(2); }
const evBar0 = rid.bars[0];
const expected0 = evBar0.map(([, m]) => m);
const allNotes = rid.bars.flatMap((b) => b.map(([, m]) => m));

synth.start(rid, null);
const ac = MockAC.all[0];
for (let i = 0; i < 80; i++) { ac.currentTime += 0.1; timers[0] && timers[0](); }
synth.stop();

/* --- verdict --------------------------------------------------------------------- */
const bassCalls = sandbox.__bassCalls;
ok(bassCalls.length > 0, `riddims: bassNote appelée (${bassCalls.length} fois sur ~46 pas)`);
ok(bassCalls.slice(0, expected0.length).map((c) => c.midi).join(",") === expected0.join(","),
  `riddims: mesure 0 = [${expected0.join(",")}] dans l'ordre des pas`);
const played = new Set(bassCalls.map((c) => c.midi));
ok(allNotes.every((n) => played.has(n)), "riddims: toutes les hauteurs des 2 mesures sonnent");
ok(bassCalls.every((c) => c.midi >= 20 && c.midi <= 60), "riddims: hauteurs plausibles pour une basse");
const oscStarts = ac._starts.filter((s) => s.kind === "osc").length;
ok(oscStarts >= 6, `riddims: la batterie sonne toujours (${oscStarts} starts oscillateurs)`);
if (bassCalls.length) {
  const sd = 60 / Math.round((rid.bpm[0] + rid.bpm[1]) / 2) / 4;
  ok(Math.abs(bassCalls[0].dur - evBar0[0][2] * sd * 0.92) < 1e-9, "riddims: durée = pas × pasDur × 0.92");
} else {
  ok(false, "riddims: durée du premier appel (aucun appel de basse)");
}

/* --- lecture : Sleng Teng (le classique, en dernier ; basse en Ré, batterie drm) */
const sleng = sandbox.window.RIDDB.list.find((r) => r.id === "sleng-teng");
if (!sleng) { console.error("sleng-teng absent des données RIDDB"); process.exit(2); }
ok(sandbox.window.RIDDB.list[sandbox.window.RIDDB.list.length - 1].id === "sleng-teng",
  "sleng: en dernier dans la liste (39 riddims)");
ok(sleng.fw === 0, "sleng: « Motif de basse » charge le preset ROCK (fw 0)");
ok(sleng.bpm[0] === 82 && sleng.bpm[1] === 82, "sleng: 82 BPM (le tempo du morceau de Wayne Smith)");
const expSleng0 = sleng.bars[0].map(([, m]) => m);
ok(sleng.bars.length === 1 && expSleng0.length === 14 &&
   expSleng0.join(",") === "37,38,38,38,37,38,38,38,37,38,38,38,43,42",
  `sleng: une mesure de 14 notes = [${expSleng0.join(",")}] (C#-D-D-D ×3 puis F#/F)`);
const stepsSleng = sleng.bars[0].map(([s]) => s);
ok(stepsSleng.join(",") === "0,1,2,3,4,5,6,7,8,9,10,11,12,14",
  "sleng: le galop en doubles-croches staccato (pas 0-3, 4-7, 8-11, puis 12 et 14)");
const lastSleng = sleng.bars[0].slice(-2).map(([, m]) => m);
ok(lastSleng.join(",") === "43,42", "sleng: conclusion quarte (43) / tierce (42) aux pas 12 et 14");

sandbox.__bassCalls = []; sandbox.__kickCalls = [];
synth.start(sleng, null);
const sdSleng = 60 / 82 / 4;
for (let i = 0; i < 80; i++) { ac.currentTime += 0.1; timers[0] && timers[0](); }
synth.stop();

const slengBass = sandbox.__bassCalls.map((c) => c.midi);
ok(slengBass.slice(0, expSleng0.length).join(",") === expSleng0.join(","),
  "sleng: la basse jouée = la transcription vérifiée du preset ROCK");
const kicks = sandbox.__kickCalls.map((c) => c.t);
ok(kicks.length >= 10, `sleng: kick joué (${kicks.length} frappes sur ~43 pas)`);
/* le kick du preset : pas 0, 4, 6, 8, 12 — écarts de 2 ou 4 pas entre frappes
   consécutives (la double frappe 6→8 est la signature du groove). La première
   frappe peut être calée sur le temps courant du mock (Math.max du
   planificateur) : on part de la deuxième, qui est sur les pas exacts du run. */
const gaps = kicks.slice(2).map((t, i) => (t - kicks[i + 1]) / sdSleng);
const near = (g, n) => Math.abs(g - n) < 1e-9;
ok(gaps.length > 0 && gaps.every((g) => near(g, 2) || near(g, 4)),
  "sleng: batterie drm — kick aux pas 0, 4, 6, 8, 12 (écarts 2 ou 4 pas), pas le style digital");
ok(gaps.some((g) => near(g, 2)), "sleng: la double frappe 6→8 du kick est bien là");

ok(html.includes("et le classique Sleng Teng"),
  "sleng: l'en-tête de la page annonce le livre et le classique");

console.log(failed === 0 ? "\nRÉSULTAT : la basse EST entendue dans l'écoute." : "\nRÉSULTAT : ÉCHEC — la basse ne joue pas.");
process.exit(failed === 0 ? 0 : 1);
