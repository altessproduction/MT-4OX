"use strict";
/* ============================================================= ==================
   Page Riddims — Altess Production, GPL-3.0.
   Les 38 riddims du « The Riddim Real Book (Tab) », pour l'émulateur MT-40X :
   - Écouter : un petit synthé Web Audio de la page (batterie + basse du riddim),
   - Séquenceur : la basse et la batterie sont écrites dans les pistes 3 et 4 du
     FM-1 émulé, par le protocole éditeur (SysEx dans l'USB de la puce émulée),
   - Motif de basse : le MT-40X prend la ligne comme motif d'accompagnement
     (transposée par l'accord tenu au clavier, comme le mini-clavier basse du
     MT-40) — firmware MT-40X avec banque riddim, sinon repli séquenceur.
   Les données sont dans window.RIDDB (générées par scripts/riddims_site.py).
   ============================================================================ */
(() => {
  if (window.RIDDIMS_UI) return;
  const D = window.RIDDB;
  if (!D) { console.warn("riddims.js absent"); return; }

  /* ---------------------------------------------------------------- constantes */
  const HDR = [0x7D, 0x46, 0x4C];               // SysEx : F0 7D 46 4C <cmd> ... F7
  const ED_INFO = 1, ED_GET = 2, ED_SET = 3, ED_TRACK = 27, ED_TRACK_STEP = 30, ED_TRACK_PARAM = 31;
  const RIDB_INFO = 0x51, RIDB_SET = 0x50;      // commandes de la banque riddim du MT-40X
  const P_SLEN = 29, P_SDIV = 28, P_SGATE = 32; // paramètres de piste (vérifiés sur le firmware : LEN 16, DIV 2 = 1/16, GATE 64)
  const G_BPM = 0, G_ENGSEL = 20;               // paramètres globaux
  const ENG_DRUM = 10;                          // NENGINES : 0 ANALOG … 10 DRUM (protocole)
  const TRK_BASS = 2, TRK_DRUM = 3;             // pistes 3 et 4 (index 0..3)
  const LANES = { k: 0, s: 1, c: 2, hc: 3, ho: 4, t: 5, rm: 6, b: 7 };  // grille DRUM v5
  const N_RSTYLE = 6;                           // les 6 rythmes du MT-40 précèdent la banque
  const ST_NOTE = 0, ST_TIE = 1, ST_REST = 2;

  /* ------------------------------------------------------------------- style */
  const css = `
  .rid-btn{border:1px solid rgba(128,128,128,.35);background:rgba(127,127,127,.08);color:inherit;
    border-radius:999px;padding:.35rem .8rem;font:600 12.5px/1.2 inherit;cursor:pointer;white-space:nowrap}
  .rid-btn:hover{border-color:var(--accent,#2b7865)}
  .rid-btn.on{background:var(--accent,#2b7865);color:#fff;border-color:transparent}
  #rid-open{display:inline-flex;align-items:center;gap:.4rem}
  .rid-sheet{position:fixed;inset:0;z-index:9000;display:flex;align-items:stretch;justify-content:center;
    background:rgba(0,0,0,.45);backdrop-filter:blur(3px)}
  .rid-sheet[hidden]{display:none}
  .rid-panel{background:var(--bg,#f5f5f7);color:var(--ink,#1d1d1f);width:min(880px,100%);margin:0;
    border-radius:18px 18px 0 0;box-shadow:0 -12px 48px rgba(0,0,0,.35);display:flex;flex-direction:column;
    max-height:100%;align-self:flex-end}
  @media(min-height:700px){.rid-panel{align-self:center;height:min(86vh,720px);border-radius:18px}}
  .rid-head{display:flex;align-items:baseline;gap:.7rem;padding:1.1rem 1.3rem .6rem;flex-wrap:wrap}
  .rid-head h2{margin:0;font:800 1.35rem/1 inherit;letter-spacing:-.01em}
  .rid-head small{color:var(--muted,#6e6e73)}
  .rid-close{margin-left:auto;border:0;background:none;color:var(--muted,#6e6e73);font-size:1.5rem;cursor:pointer;line-height:1}
  .rid-tools{display:flex;gap:.45rem;padding:.4rem 1.3rem .6rem;flex-wrap:wrap;align-items:center}
  .rid-search{flex:1;min-width:9rem;border:1px solid rgba(128,128,128,.35);border-radius:999px;
    padding:.35rem .8rem;font:inherit;background:rgba(127,127,127,.06);color:inherit}
  .rid-status{padding:0 1.3rem .5rem;color:var(--muted,#6e6e73);font-size:.85rem;min-height:1.2em}
  .rid-status.err{color:#c0392b;font-weight:600}
  .rid-list{overflow:auto;padding:.2rem 1.3rem 1.4rem;display:grid;gap:.55rem;
    grid-template-columns:repeat(auto-fill,minmax(258px,1fr))}
  .rid-card{border:1px solid rgba(128,128,128,.28);border-radius:14px;padding:.75rem .85rem .7rem;
    display:flex;flex-direction:column;gap:.3rem;background:rgba(127,127,127,.045)}
  .rid-card.play{border-color:var(--accent,#2b7865);box-shadow:0 0 0 1px var(--accent,#2b7865)}
  .rid-name{font-weight:700;font-size:1.02rem}
  .rid-aka{color:var(--muted,#6e6e73);font-size:.8rem}
  .rid-meta{display:flex;gap:.35rem;flex-wrap:wrap;margin-top:.15rem}
  .rid-chip{font:600 10.5px/1 inherit;letter-spacing:.02em;text-transform:uppercase;color:var(--accent,#2b7865);
    border:1px solid currentColor;border-radius:999px;padding:.22rem .55rem;opacity:.9}
  .rid-chip.dim{color:var(--muted,#6e6e73)}
  .rid-chords{font-size:.85rem;color:var(--muted,#6e6e73)}
  .rid-acts{display:flex;gap:.4rem;margin-top:.45rem;flex-wrap:wrap}
  .rid-foot{padding:.5rem 1.3rem 1rem;color:var(--muted,#6e6e73);font-size:.8rem;border-top:1px solid rgba(128,128,128,.2)}
  .rid-foot b{color:inherit}
  `;
  const style = document.createElement("style");
  style.textContent = css;
  document.head.appendChild(style);

  /* --------------------------------------------------------------------- DOM */
  const openBtn = document.createElement("button");
  openBtn.id = "rid-open"; openBtn.type = "button"; openBtn.className = "rid-btn";
  openBtn.innerHTML = "&#9835; Riddims";
  const sheet = document.createElement("div");
  sheet.className = "rid-sheet"; sheet.hidden = true;
  sheet.innerHTML = `
    <div class="rid-panel" role="dialog" aria-label="Riddims">
      <div class="rid-head">
        <h2>Riddims</h2><small>les ${D.list.length} du « ${D.src} »</small>
        <button class="rid-close" type="button" aria-label="Fermer">&#215;</button>
      </div>
      <div class="rid-tools">
        <input class="rid-search" type="search" placeholder="Nom, label, accord…" aria-label="Rechercher un riddim">
        <span class="rid-filters" style="display:contents"></span>
      </div>
      <div class="rid-status" role="status"></div>
      <div class="rid-list"></div>
      <div class="rid-foot">
        <b>Séquenceur</b> : la basse va sur la piste 3 et la batterie sur la piste 4 (moteur DRUM) du FM-1 émulé ;
        appuie ensuite sur SEQ puis PLAY. <b>Motif de basse</b> : le MT-40X joue la ligne comme accompagnement,
        transposée par l'accord tenu au clavier (firmware avec banque riddim). <b>Écouter</b> : maquette de la page,
        batterie et basse synthétisées ici même.
      </div>
    </div>`;
  document.body.append(openBtn, sheet);
  const fwPicker = document.getElementById("fw-picker");   // le bouton à côté du sélecteur de firmware
  if (fwPicker && fwPicker.parentNode) fwPicker.parentNode.insertBefore(openBtn, fwPicker.nextSibling);

  const listEl = sheet.querySelector(".rid-list"), statusEl = sheet.querySelector(".rid-status");
  const searchEl = sheet.querySelector(".rid-search"), filtersEl = sheet.querySelector(".rid-filters");
  let filter = "tous", playingId = null;

  function say(msg, err) {
    statusEl.textContent = msg || "";
    statusEl.classList.toggle("err", !!err);
  }
  function close() { sheet.hidden = true; synth.stop(); }
  openBtn.addEventListener("click", () => { sheet.hidden = !sheet.hidden; if (!sheet.hidden) say(""); });
  sheet.querySelector(".rid-close").addEventListener("click", close);
  sheet.addEventListener("click", (e) => { if (e.target === sheet) close(); });
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !sheet.hidden) close(); });

  /* --------------------------------------------------------------- carte d'un riddim */
  const fmtBpm = (r) => r.bpm[0] === r.bpm[1] ? `${r.bpm[0]} BPM` : `${r.bpm[0]}–${r.bpm[1]} BPM`;
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
        <button class="rid-btn rid-motif" type="button">Motif de basse</button>
        <button class="rid-btn rid-seq" type="button">Séquenceur</button>
      </div>`;
    el.querySelector(".rid-name").textContent = r.n;
    if (r.a) el.querySelector(".rid-aka").textContent = r.a;
    if (r.ch) el.querySelector(".rid-chords").textContent = "Accords : " + r.ch;
    el.querySelector(".rid-listen").addEventListener("click", () => toggleListen(r, el));
    el.querySelector(".rid-motif").addEventListener("click", () => loadMotif(r));
    el.querySelector(".rid-seq").addEventListener("click", () => loadSeq(r, true));
    return el;
  }
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
  render();

  /* ------------------------------------------------- écoute : le synthé de la page */
  const synth = (() => {
    let ac = null, out = null, noise = null;
    let run = null;                                       // {rid, t0, step, next, timer}
    const stepDur = (bpm) => 60 / bpm / 4;                // une double-croche
    function ctx() {
      if (!ac) {
        ac = new (window.AudioContext || window.webkitAudioContext)();
        out = ac.createGain(); out.gain.value = 0.85; out.connect(ac.destination);
        const n = ac.createBuffer(1, ac.sampleRate, ac.sampleRate);
        const d = n.getChannelData(0);
        for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
        noise = n;
      }
      if (ac.state === "suspended") ac.resume();
      return ac;
    }
    const src = (t) => { const s = ac.createBufferSource(); s.buffer = noise; s.start(t); return s; };
    const env = (t, a, peak, dur) => {
      const g = ac.createGain();
      g.gain.setValueAtTime(0, t);
      g.gain.linearRampToValueAtTime(peak, t + a);
      g.gain.exponentialRampToValueAtTime(0.001, t + dur);
      g.connect(out); return g;
    };
    function kick(t) {
      const o = ac.createOscillator(); o.type = "sine";
      o.frequency.setValueAtTime(150, t);
      o.frequency.exponentialRampToValueAtTime(45, t + 0.09);
      o.connect(env(t, 0.002, 1.0, 0.30)); o.start(t); o.stop(t + 0.32);
      const c = src(t), f = ac.createBiquadFilter(); f.type = "highpass"; f.frequency.value = 1200;
      c.connect(f); f.connect(env(t, 0.001, 0.25, 0.03)); c.stop(t + 0.04);
    }
    function snare(t) {
      const s = src(t), f = ac.createBiquadFilter(); f.type = "bandpass"; f.frequency.value = 1900; f.Q.value = 0.8;
      s.connect(f); f.connect(env(t, 0.002, 0.55, 0.19)); s.stop(t + 0.2);
      const o = ac.createOscillator(); o.type = "triangle"; o.frequency.value = 185;
      o.connect(env(t, 0.001, 0.4, 0.11)); o.start(t); o.stop(t + 0.12);
    }
    function hat(t, open) {
      const s = src(t), f = ac.createBiquadFilter(); f.type = "highpass"; f.frequency.value = 7200;
      const dur = open ? 0.28 : 0.055;
      s.connect(f); f.connect(env(t, 0.001, open ? 0.22 : 0.18, dur)); s.stop(t + dur + 0.02);
    }
    function rim(t) {
      const s = src(t), f = ac.createBiquadFilter(); f.type = "bandpass"; f.frequency.value = 3600; f.Q.value = 6;
      s.connect(f); f.connect(env(t, 0.001, 0.35, 0.045)); s.stop(t + 0.06);
      const o = ac.createOscillator(); o.type = "square"; o.frequency.value = 1050;
      o.connect(env(t, 0.001, 0.12, 0.03)); o.start(t); o.stop(t + 0.04);
    }
    let bassOsc = null;
    function bassNote(t, midi, dur) {
      if (bassOsc) { try { bassOsc.stop(t); } catch { /* déjà arrêté */ } bassOsc = null; }
      const o = ac.createOscillator(); o.type = "sawtooth";
      o.frequency.value = 440 * Math.pow(2, (midi - 69) / 12);
      const f = ac.createBiquadFilter(); f.type = "lowpass"; f.Q.value = 7;
      f.frequency.setValueAtTime(240, t);
      f.frequency.exponentialRampToValueAtTime(900, t + 0.03);
      f.frequency.exponentialRampToValueAtTime(260, t + Math.min(dur, 0.4));
      const g = ac.createGain();
      g.gain.setValueAtTime(0, t);
      g.gain.linearRampToValueAtTime(0.5, t + 0.006);
      g.gain.setValueAtTime(0.5, t + Math.max(0.01, dur - 0.05));
      g.gain.linearRampToValueAtTime(0, t + dur);
      o.connect(f); f.connect(g); g.connect(out);
      o.start(t); o.stop(t + dur + 0.02);
      bassOsc = o;
    }
    function tick() {
      const r = run; if (!r) return;
      const sd = stepDur(r.bpm);
      while (r.next < ac.currentTime + 0.18) {
        const bar = Math.floor(r.step / 16) % r.bars.length, st = r.step % 16;
        const dr = D.drums[r.st];
        for (const lane of ["k", "s", "hc", "ho", "rm"]) {
          if ((dr[lane][bar] || []).includes(st)) {
            const t = Math.max(r.next, ac.currentTime);
            if (lane === "k") kick(t); else if (lane === "s") snare(t);
            else if (lane === "hc") hat(t, false); else if (lane === "ho") hat(t, true); else rim(t);
          }
        }
        for (const [, s, m, l] of r.bars[bar]) {
          if (s === st) bassNote(Math.max(r.next, ac.currentTime), m, l * sd * 0.92);
        }
        r.next += sd; r.step = (r.step + 1) % (r.bars.length * 16);
      }
    }
    function start(r, el) {
      ctx();
      stop();
      run = { rid: r.id, bpm: r.bpm[0] === r.bpm[1] ? r.bpm[0] : Math.round((r.bpm[0] + r.bpm[1]) / 2),
        bars: r.bars, step: 0, next: ac.currentTime + 0.06 };
      if (el) { el.classList.add("play"); playingId = r.id; }
      run.timer = setInterval(tick, 30);
    }
    function stop() {
      if (run) { clearInterval(run.timer); run = null; }
      if (bassOsc) { try { bassOsc.stop(); } catch { /* rien */ } bassOsc = null; }
      document.querySelectorAll(".rid-card.play").forEach((x) => x.classList.remove("play"));
      playingId = null;
    }
    return { start, stop, get playingId() { return playingId; } };
  })();
  function toggleListen(r, el) {
    if (synth.playingId === r.id) { synth.stop(); say("Écoute arrêtée."); }
    else { synth.start(r, el); say(`« ${r.n} » en écoute — ${fmtBpm(r)}.`); }
  }

  /* --------------------------------------- client du protocole éditeur (SysEx USB) */
  /* fm1-emu : les messages USB vont et viennent en octets MIDI bruts (un message
   * complet à la fois). Les paquets USB-MIDI 4 octets restent gérés en réception,
   * au cas où un autre cœur les enverrait ainsi. */
  const link = (() => {
    let waiters = [];                                     // {cmd, resolve, reject, timer}
    let rx = null, f = null;
    function frame(bytes) {                               // une trame complète F0 … F7
      if (bytes.length < 6 || bytes[0] !== 0xF0) return;
      if (bytes[1] !== HDR[0] || bytes[2] !== HDR[1] || bytes[3] !== HDR[2]) return;
      const cmd = bytes[4];
      if ([23, 24, 26, 32].includes(cmd)) return;         // les poussées ne sont pas des réponses
      const w = waiters.find((x) => x.cmd === cmd);
      if (w) {
        clearTimeout(w.timer);
        waiters = waiters.filter((x) => x !== w);
        w.resolve(bytes.slice(5, bytes.length - 1));
      }
    }
    function onUsb(bytes) {
      if (bytes[0] === 0xF0) { frame(bytes); return; }    // le cœur fm1-emu : un message complet
      for (let i = 0; i + 3 < bytes.length; i += 4) {     // un autre cœur : paquets USB-MIDI (CIN, 3 octets)
        const cin = bytes[i] & 15;
        const nb = cin >= 4 && cin <= 7 ? (cin === 4 || cin === 7 ? 3 : cin === 6 ? 2 : 1) : 0;
        for (let k = 0; k < nb; k++) {
          const b = bytes[i + 1 + k];
          if (b === 0xF0) f = [0xF0];
          else if (f) {
            if (b !== 0xF7) f.push(b);
            else { const t = f; f = null; frame(t); }
          }
        }
      }
    }
    function send(emu, cmd, args, ms = 2500) {
      const bytes = [0xF0, ...HDR, cmd, ...args, 0xF7];
      return new Promise((resolve, reject) => {
        const w = { cmd, resolve, reject, timer: setTimeout(() => {
          waiters = waiters.filter((x) => x !== w);
          reject(new Error("pas de réponse (cmd " + cmd + ")"));
        }, ms) };
        waiters.push(w);
        emu.usbSend(bytes);
      });
    }
    function attach(emu) {
      if (rx && rx.emu === emu) return;
      if (rx) { const prev = rx.prev; emu.onUsb = prev || null; }
      const prev = emu.onUsb || null;
      emu.onUsb = (b) => { if (prev) prev(b); onUsb(b); };
      rx = { emu, prev };
    }
    return { send, attach };
  })();

  /* ------------------------------------------------------------- accès au FM-1 émulé */
  const engine = () => window.__engine ? window.__engine() : null;
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  async function host() {
    let emu = engine();
    if (!emu || emu.kind !== "emu" || !emu.running) {
      if (!(await ensureMt40x())) return null;
      emu = engine();
      if (!emu || emu.kind !== "emu" || !emu.running) return null;
    }
    if (emu.usb !== 2) { emu.usbPlug(); for (let i = 0; i < 24 && emu.usb !== 2; i++) await wait(150); }
    link.attach(emu);
    return emu;
  }
  const v14 = (v) => [(v + 8192) & 127, ((v + 8192) >> 7) & 127];
  const rv = (lo, hi) => (lo | (hi << 7)) - 8192;
  async function emuInfo(emu) {                            // INFO : nengines, pcount, gcount, nstep, pe0
    const a = await link.send(emu, ED_INFO, []);
    let i = a.indexOf(0);                                   // la chaîne "FELUCCA x.y" se termine par 0
    return { nengines: a[i + 1], pcount: a[i + 2], gcount: a[i + 3], nstep: a[i + 4], pe0: a[i + 5], ver: String.fromCharCode(...a.slice(0, i)) };
  }
  const trackSel = (emu, t) => link.send(emu, ED_TRACK, t === undefined ? [] : [t]);
  const getParam = (emu, t, id) => link.send(emu, ED_TRACK_PARAM, [t, id]).then((a) => rv(a[2], a[3]));
  async function setParam(emu, t, id, v) {                // écrit puis relit : les ids comptent
    await link.send(emu, ED_TRACK_PARAM, [t, id, ...v14(v)]);
    const back = await getParam(emu, t, id);
    if (back !== v) throw new Error("paramètre " + id + " : relu " + back + " au lieu de " + v);
  }
  async function setGlobal(emu, id, v) {
    await link.send(emu, ED_SET, [1, id, ...v14(v)]);
    const back = await link.send(emu, ED_GET, [1, id]);   // [scope, id, v14]
    const got = rv(back[2], back[3]);
    if (got !== v) throw new Error("global " + id + " : relu " + got + " au lieu de " + v);
  }

  function stepBytes(st) {                                 // pas au format v5 : n, 4 notes, time, flags, vel, hit, acc, ext
    const n = Math.min(4, st.notes ? st.notes.length : 0);
    const b = [n];
    for (let i = 0; i < 4; i++) b.push(n > i ? (st.notes[i] & 127) : 0);
    b.push(st.tie ? ST_TIE : st.rest ? ST_REST : ST_NOTE, st.flags || 0, st.vel || 0);
    const hit = st.hit || 0, acc = st.acc || 0;
    b.push(hit & 127, acc & 127, (hit >> 7) | ((acc >> 7) << 1));
    return b;                                              // 11 octets : les grille et notes ensemble
  }
  const trackStep = (emu, t, idx, st) => link.send(emu, ED_TRACK_STEP, [t, idx, ...stepBytes(st)]);

  /* --------------------------------------------------- écriture d'un riddim dans le FM-1 */
  function bassSteps(r) {                                  // tous les pas du motif : note, tenue ou silence
    const nb = r.bars.length, out = [];
    for (let idx = 0; idx < nb * 16; idx++) {
      const bar = Math.floor(idx / 16), st = idx % 16;
      const note = r.bars[bar].find((s) => s[0] === st);
      const tail = r.bars[bar].find((s) => st > s[0] && st < s[0] + s[2]);
      if (note) out.push({ idx, notes: [note[1]], vel: st % 8 === 0 ? 112 : 96 });
      else if (tail) out.push({ idx, tie: true, notes: [] });
      else out.push({ idx, rest: true, notes: [] });
    }
    return out;
  }
  function drumSteps(r) {
    const nb = r.bars.length, dr = D.drums[r.st], out = [];
    for (let idx = 0; idx < nb * 16; idx++) {
      const bar = Math.floor(idx / 16), st = idx % 16;
      let hit = 0, acc = 0;
      for (const lane of ["k", "s", "hc", "ho", "rm"]) {
        if ((dr[lane][bar] || []).includes(st)) {
          const bit = 1 << LANES[lane];
          hit |= bit;
          if (st % 8 === 0) acc |= bit;                   // temps forts accentués
        }
      }
      out.push({ idx, notes: [], vel: 0, hit, acc });
    }
    return out;
  }
  async function writeTrack(emu, t, steps) {
    for (const st of steps) await trackStep(emu, t, st.idx, st);
  }
  const bpmOf = (r) => r.bpm[0] === r.bpm[1] ? r.bpm[0] : Math.round((r.bpm[0] + r.bpm[1]) / 2);

  async function loadSeq(r, withDrums) {
    synth.stop();
    const emu = await host();
    if (!emu) { say("Le firmware MT-40X ne démarre pas — ouvre le sélecteur de firmware et lance MT-40X.", true); return; }
    say("Écriture dans le FM-1 émulé…");
    try {
      const info = await emuInfo(emu);
      if (info.nstep < r.bars.length * 16) throw new Error("patterns trop courts côté firmware");
      const cur = await trackSel(emu);                   // [sel, ntrk, …] : la piste sélectionnée
      const sel = cur ? cur[0] : 0;
      await setParam(emu, TRK_BASS, P_SLEN, r.bars.length * 16);
      await setParam(emu, TRK_BASS, P_SDIV, 2);          // DIV 1/16
      await setParam(emu, TRK_BASS, P_SGATE, 55);
      await writeTrack(emu, TRK_BASS, bassSteps(r));
      if (withDrums) {
        await trackSel(emu, TRK_DRUM);
        await link.send(emu, ED_SET, [1, G_ENGSEL, ...v14(ENG_DRUM)]);  // la piste 4 passe au moteur DRUM
        const tmix = await trackSel(emu);                // [sel, ntrk, par piste : engine, preset, level(2), mute, armed]
        const eng = tmix[2 + TRK_DRUM * 6];
        if (eng !== ENG_DRUM) throw new Error("moteur DRUM non appliqué (piste 4 : moteur " + eng + ")");
        await setParam(emu, TRK_DRUM, P_SLEN, r.bars.length * 16);
        await setParam(emu, TRK_DRUM, P_SDIV, 2);
        await writeTrack(emu, TRK_DRUM, drumSteps(r));
      }
      await setGlobal(emu, G_BPM, bpmOf(r));
      await trackSel(emu, sel);                          // la piste du clavier retrouve sa place
      const back = await link.send(emu, ED_TRACK_STEP, [TRK_BASS, 0]);  // vérification : relecture du pas 0
      const okBack = back && back.length >= 17 && (back[7] === ST_NOTE || back[7] === ST_TIE || back[7] === ST_REST);
      say(`« ${r.n} » chargé ${okBack ? "" : "(vérification incertaine) "}: basse piste 3` +
        (withDrums ? ", batterie piste 4" : "") + `, ${bpmOf(r)} BPM. Appuie sur SEQ puis PLAY.`);
    } catch (err) {
      say("Échec : " + err.message + " — branche l'USB (bouton MIDI) ou réessaie.", true);
    }
  }

  async function loadMotif(r) {
    synth.stop();
    const emu = await host();
    if (!emu) { say("Le firmware MT-40X ne démarre pas — ouvre le sélecteur de firmware et lance MT-40X.", true); return; }
    const idx = N_RSTYLE + D.list.indexOf(r);              // les 6 rythmes du MT-40, puis la banque
    try {
      const a = await link.send(emu, RIDB_INFO, [], 700);  // la banque riddim du firmware ?
      const count = a[0];
      if (idx >= count) throw new Error("banque du firmware plus courte que la liste");
      await link.send(emu, RIDB_SET, [idx]);
      say(`« ${r.n} » en motif de basse du MT-40X (${count - N_RSTYLE} riddims en banque). ` +
        "Tiens un accord au clavier : la ligne transpose comme sur le MT-40. PLAY lance le tout.");
    } catch (err) {
      say("Ce firmware n'a pas la banque riddim — la basse va au séquenceur (piste 3), sans transposition d'accord.",
        false);
      await loadSeq(r, false);
    }
  }

  /* ------------------------------------------- si le firmware MT-40X ne tourne pas encore */
  async function ensureMt40x() {
    if (synth.playingId) synth.stop();
    const emu = engine();
    if (emu && emu.kind === "emu" && emu.running) return true;
    if (!window.__useEngine) return false;
    try {
      const cat = await (await fetch("firmwares/catalogue.json")).json();
      const entry = (cat.firmwares || []).find((f) => f.id === "mt40x");
      if (entry) { say("Démarrage du firmware MT-40X…"); return !!(await window.__useEngine(entry)); }
    } catch { /* pas de catalogue : l'utilisateur choisira lui-même */ }
    return false;
  }

  window.RIDDIMS_UI = { open: () => { sheet.hidden = false; }, synth, link, loadSeq, loadMotif, ensureMt40x };
})();
