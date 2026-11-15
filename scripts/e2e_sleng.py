#!/usr/bin/env python3
"""E2E Chromium des pages Riddims avec le classique Sleng Teng en dernier :
   - riddims.html : 39 cartes, « Écouter » du Sleng Teng sonne réellement
     (mesure Web Audio), « Riddim MIDI » télécharge sleng-teng-riddim-82bpm.mid,
     « Basse MIDI » télécharge sleng-teng-basse-82bpm.mid, zéro erreur de page ;
   - index.html (émulateur) : la feuille Riddims liste 39 riddims, le Sleng Teng
     y est en dernier, son écoute sonne, l'en-tête annonce le livre + le classique.
"""
import asyncio, json, sys
from pathlib import Path
from playwright.async_api import async_playwright

WEB = Path("/home/z/my-project/build-fw/MT-4OX/web")
PORT = 8908
BASE = f"http://127.0.0.1:{PORT}/"

TAP = """
window.__peak = 0;
/* interception : le synthé des pages crée un AudioContext à la première écoute ;
   on déroute tout ce qui se branche sur destination vers un mesureur muet. */
const _AC = window.AudioContext;
window.AudioContext = function (...a) {
  const ac = new _AC(...a);
  window.__ridAC = ac;
  try {
    const sp = ac.createScriptProcessor(1024, 2, 2);
    sp.onaudioprocess = (e) => {
      for (let c = 0; c < 2; c++) {
        const d = e.inputBuffer.getChannelData(c);
        for (let i = 0; i < d.length; i++) { const v = Math.abs(d[i]); if (v > window.__peak) window.__peak = v; }
      }
    };
    const g = ac.createGain(); g.gain.value = 0; g.connect(ac.destination);
    sp.connect(g);                       // le mesureur est muet : gain 0
    const orig = AudioNode.prototype.connect;
    AudioNode.prototype.connect = function (target, ...rest) {
      if (target === ac.destination && this !== g && this !== sp) { try { orig.call(this, sp); } catch (e) {} }
      return orig.call(this, target, ...rest);
    };
  } catch (e) {}
  return ac;
};
window.AudioContext.prototype = _AC.prototype;
"""

DL = """/* téléchargement : l'attribut download du <a> créé par la page */"""


async def main():
    import http.server, socketserver, threading, functools
    socketserver.TCPServer.allow_reuse_address = True
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(WEB))
    httpd = socketserver.TCPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    fails = []
    ok = lambda cond, what: print(f"{what:<66} {'ok' if cond else 'FAIL'}") or (None if cond else fails.append(what))

    async with async_playwright() as p:
        browser = await p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required", "--enable-unsafe-swiftshader"])
        ctx = await browser.new_context()
        await ctx.add_init_script(TAP)
        page = await ctx.new_page()
        errs = []
        page.on("pageerror", lambda e: errs.append(str(e)[:200]))
        # les « Failed to load resource » (404 réseau) sont couverts par bad404 ci-dessous
        page.on("console", lambda m: errs.append(m.text[:120]) if m.type == "error" and
                "Failed to load resource" not in m.text else None)
        bad404 = []
        # favicon.ico : requête automatique du navigateur ; natif/manifest.json :
        # fetch hérité de la page FM-1+VA d'origine (r.ok testé, catch -> {}) — 404 bénin
        page.on("response", lambda r: bad404.append(r.url) if r.status == 404 and
                not r.url.endswith("favicon.ico") and not r.url.endswith("natif/manifest.json") else None)

        # ---------------------------------------------------------- riddims.html
        await page.goto(BASE + "riddims.html", wait_until="load", timeout=60000)
        await page.wait_for_timeout(600)
        cards = await page.locator(".rid-card").count()
        ok(cards == 39, f"riddims.html: 39 cartes ({cards})")
        sub = await page.locator(".hd .sub").inner_text()
        ok("Sleng Teng" in sub and "The Riddim Real Book" in sub, f"riddims.html: l'en-tête ({sub[:72]}…)")
        last = page.locator(".rid-card").last
        name = await last.locator(".rid-name").inner_text()
        ok(name == "Sleng Teng", f"riddims.html: la dernière carte = « {name} »")
        chips = await last.locator(".rid-chip").all_inner_texts()
        ok(any("82 BPM" in c for c in chips) and any("jammy's" in c.lower() for c in chips),
           f"riddims.html: chips de la dernière carte ({chips})")
        chords = await last.locator(".rid-chords").inner_text()
        ok("D" in chords, f"riddims.html: accords ({chords})")

        await page.evaluate("window.__peak = 0")
        await last.locator(".rid-listen").click()
        await page.wait_for_timeout(2600)
        peak = await page.evaluate("window.__peak")
        ok(peak > 0.01, f"riddims.html: « Écouter » Sleng Teng sonne (pic {peak:.3f})")
        status = await page.locator(".rid-status").inner_text()
        ok("Sleng Teng" in status, f"riddims.html: le statut dit qui joue ({status[:60]}…)")
        await page.evaluate("window.__peak = 0")

        # téléchargements MIDI
        async with page.expect_download() as dl1:
            await last.locator(".rid-dl-full").click()
        d1 = await dl1.value
        p1 = Path("/tmp/render") / d1.suggested_filename
        await d1.save_as(p1)
        ok(d1.suggested_filename == "sleng-teng-riddim-82bpm.mid",
           f"riddims.html: « Riddim MIDI » -> {d1.suggested_filename}")
        async with page.expect_download() as dl2:
            await last.locator(".rid-dl-bass").click()
        d2 = await dl2.value
        ok(d2.suggested_filename == "sleng-teng-basse-82bpm.mid",
           f"riddims.html: « Basse MIDI » -> {d2.suggested_filename}")
        b1 = p1.read_bytes()
        ok(b1[:4] == b"MThd", "riddims.html: le fichier téléchargé est un SMF (MThd)")
        ok(len(b1) > 200, f"riddims.html: fichier non vide ({len(b1)} o)")

        # filtre par style : le Sleng Teng est chez les Digital
        await page.locator(".rid-filters .rid-btn", has_text="Digital").click()
        dig = await page.locator(".rid-card").count()
        names = await page.locator(".rid-card .rid-name").all_inner_texts()
        ok("Sleng Teng" in names and dig == 4, f"riddims.html: filtre Digital -> {names}")
        await page.locator(".rid-filters .rid-btn", has_text="Tous").click()

        # ---------------------------------------------------------- index.html
        await page.goto(BASE + "index.html", wait_until="load", timeout=60000)
        await page.wait_for_timeout(1200)
        ok(await page.locator("#rid-open").count() == 1, "index.html: le bouton Riddims est là")
        await page.locator("#rid-open").click()
        await page.wait_for_timeout(500)
        cards = await page.locator(".rid-sheet .rid-card").count()
        ok(cards == 39, f"index.html: la feuille liste 39 riddims ({cards})")
        head = await page.locator(".rid-head small").inner_text()
        ok("The Riddim Real Book" in head and "Sleng Teng" in head, f"index.html: l'en-tête ({head[:70]}…)")
        last = page.locator(".rid-sheet .rid-card").last
        name = await last.locator(".rid-name").inner_text()
        ok(name == "Sleng Teng", f"index.html: le dernier de la liste = « {name} »")
        await page.evaluate("window.__peak = 0")
        await last.locator(".rid-listen").click()
        await page.wait_for_timeout(2600)
        peak = await page.evaluate("window.__peak")
        ok(peak > 0.01, f"index.html: « Écouter » Sleng Teng sonne (pic {peak:.3f})")
        # recherche
        await page.fill(".rid-sheet .rid-search", "sleng")
        await page.wait_for_timeout(400)
        found = await page.locator(".rid-sheet .rid-card").count()
        ok(found == 1, f"index.html: la recherche « sleng » trouve le classique ({found})")
        foot = await page.locator(".rid-foot").inner_text()
        ok("riddims.html" in (await page.locator(".rid-foot a").first.get_attribute("href") or ""),
           "index.html: le pied renvoie vers la page dédiée")

        ok(not errs and not bad404, f"aucune erreur de page (errs={errs[:1]}, 404={bad404[:1]})")
        await page.screenshot(path="/tmp/render/riddims_page.png", full_page=False)
        await browser.close()
    httpd.shutdown()
    print("\nRÉSULTAT :", "E2E Sleng Teng OK" if not fails else f"ÉCHEC : {fails}")
    sys.exit(1 if fails else 0)

asyncio.run(main())
