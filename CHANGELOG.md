# Changelog

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le
versionnage sémantique.

## [0.1.2] - 2026-11-15
### Corrigé
- **L'accompagnement (PLAY) restait muet** : `mt40_init()` appelait
  `mt40_synth_new()` et `mt40_drums_new()` sans garder les instances
  qu'ils retournent — sur le FM-1, ces constructeurs construisent en place
  une instance statique *interne* (pas de tas) : le taux d'échantillonnage du
  synthé et les gains des quatre voies de batterie restaient à zéro dans les
  struct locales `mt40_syn` / `mt40_drm`, et l'accompagnement jouait des
  motifs parfaits… dans le silence (les voix rendaient du NaN, la batterie
  un niveau nul). Les tests hôtes passaient parce qu'ils utilisent les
  pointeurs retournés. `mt40_init()` garde désormais les instances
  (`mt40_syn = mt40_synth_new(FS)`, `mt40_drm = mt40_drums_new(FS)`) et
  `mt40_block()` ne rend plus rien si elles manquent.
  Vérifié dans l'émulateur avec mesure audio réelle (analyseur Web Audio) :
  clavier 0.018 RMS, accompagnement ROCK 0.046 RMS, riddim Stalag chargé par
  la page Riddims (commande 0x52) 0.027 RMS — avant le correctif, PLAY
  mesurait 0.000 RMS depuis le premier paquet.

### Note
- Le paquet « v0.1.1 » publié ne contenait pas la banque riddim (binaire du
  build précédent, la commande 0x53 ne répondait pas) ; la v0.1.2 la
  embarque réellement (banque 44 : les six rythmes du MT-40 + 38 riddims du
  *Riddim Real Book*), réponse 0x53 = 44 vérifiée dans l'émulateur.

## [Non publié]
### Ajouté
- **Le classique Sleng Teng en dernier sur les deux pages Riddims** (la feuille
  de l'émulateur et [`web/riddims.html`](web/riddims.html)) : le preset
  « Rock » du MT-40 lui-même — Wayne Smith, « Under Mi Sleng Teng », Jammy's
  Records, 1985, 82 BPM — en 39ᵉ entrée, avec la transcription vérifiée du
  firmware MT-40X (`pat_ROCK`, deux tablatures recoupées, dossier technique § 3
  et annexe A) : basse en Ré (approche chromatique, fondamentale répétée, paire
  quarte/tierce en conclusion) et batterie du preset d'origine (kick 1 et 3,
  snare 2 et 4, hats en croches, hat ouverte au dernier 1/16) — transportée par
  un champ `drm` propre au riddim, au lieu de la batterie générique du style
  « digital ». « Motif de basse » sur ce riddim charge le preset ROCK de la
  machine (alias `fw` 0, le riddim d'origine), et les exports MIDI sortent
  `sleng-teng-riddim-82bpm.mid` / `sleng-teng-basse-82bpm.mid`. Les en-têtes
  des deux pages disent maintenant « les 38 du « The Riddim Real Book (Tab) »
  et le classique Sleng Teng (Jammy's, 1985) » ; le filtre Digital, la
  recherche « sleng » et l'a.k.a. « Under Mi Sleng Teng » le trouvent.
  Tests : `web/test_riddims.mjs` (basse jouée = transcription du preset, kicks
  aux pas 0 et 8 via `drm`) et `web/test_riddims_midi.mjs` (export 82 BPM,
  carte GM aux pas du preset, 14 notes de basse, a.k.a., mesure 2), plus l'E2E
  Chromium `scripts/e2e_sleng.py` (39 cartes, écoutes réelles mesurées,
  téléchargements, zéro erreur page).
- **Page Riddims dédiée avec export MIDI** (`web/riddims.html`, hors
  émulateur, en ligne sur /MT-4OX/riddims.html) : la même écoute que la page
  Riddims de l'émulateur — le synthé Web Audio et le CSS sont copiés du même
  module `scripts/riddims_page.js` par `scripts/build_riddims_html.py`, copie
  vérifiée par test — plus deux téléchargements par riddim : « Riddim MIDI »
  (batterie sur le canal 10 en carte General MIDI — grosse caisse 36, caisse
  claire 38, charley fermé 42, ouvert 46, rim 37 — et basse sur le canal 1,
  programme GM « Synth Bass 1 ») et « Basse MIDI » (la seule ligne de basse,
  telle que tablée dans le livre). Standard MIDI File format 1, 480 PPQ, un
  pas = PPQ/4 tics, tempo du livre (milieu de la fourchette), signature 4/4,
  une passe des mesures du riddim ; vélocités temps forts/ordinaires comme au
  séquenceur. S'importe tel quel dans n'importe quel DAW. Le pied de la page
  Riddims de l'émulateur renvoie vers la page dédiée.
  `web/test_riddims_midi.mjs` (`node web/test_riddims_midi.mjs`) relit les
  38 × 2 fichiers générés avec un parseur SMF (en-têtes, longueurs de chunks,
  tempo, tics et hauteurs des notes, carte GM, fin de piste) et vérifie que
  la page dédiée reste en phase avec le module de l'émulateur (blocs synth et
  CSS identiques) ; fichiers Stalag contrôlés aussi par libmagic (SMF format
  1, 3 et 2 pistes) et par un essai Chromium réel (38 cartes, écoute,
  téléchargements, zéro erreur page).

### Corrigé
- **L'émulateur web rendait le silence chez certains utilisateurs** alors que le
  même paquet v0.1.2 sonnait dans les tests : la page imposait
  `sampleRate: 44100` à son contexte audio, et sur une configuration où le
  navigateur crée le contexte mais ne rend rien sur une fréquence imposée
  (le cas observé sous Firefox), tout le firmware émulé — comme FM-1+VA —
  restait muet, tandis que la page Riddims, elle, sonnait (son propre contexte
  n'impose rien). La chaîne audio est désormais **éprouvée et réparée** :
  sonde du graphe au démarrage (note inaudible à -60 dB) avec repli à la
  fréquence de l'appareil si le contexte imposé ne rend rien ; contexte repris
  à tout geste (pointerdown/touche/toucher, en plus du retour d'onglet) ;
  garde-fou qui reconstruit la chaîne sur un contexte neuf si la machine rend
  (touche tenue ou PLAY vert) sans qu'aucun échantillon ne sorte pendant 1,2 s
  (une fois par session, flash sauvegardée avant). Vérifié par intégration
  (Chromium) : note audible avant incident, muiterie simulée, garde-fou
  déclenché, note audible après réparation (RMS 0.021), zéro erreur page ;
  état exposé par `window.__audioState`.
- **La page Riddims : l'« Écouter » ne jouait que la batterie** — le
  destructuring de l'ordonnanceur de l'écoute sautait le premier champ des
  événements `[pas, note MIDI, durée]` : `[, s, m, l]` mettait la note dans
  `s`, comparée au pas courant (jamais égal, MIDI 28–51 contre 0–15), donc
  `bassNote()` n'était jamais appelée — batterie seule, comme constaté à
  l'oreille. Le destructuring est désormais `[s, m, l]` (le chemin
  « Séquenceur », lui, utilisait déjà `s[0]` et écrivait bien la basse dans
  la piste 3). Régression couverte par `web/test_riddims.mjs`
  (`node web/test_riddims.mjs`) : bassNote appelée, mesure 0 de Stalag dans
  l'ordre des pas, hauteurs des 2 mesures présentes, batterie intacte.
- **Le silence au démarrage du paquet émulé** : le BSP vendu (felucca-bsp
  récent, contrat « checked start » de Kerem Kilic) refuse de démarrer l'ALNK0
  quand le PLL audio n'a pas les valeurs stock exactes — et la chaîne de boot
  de l'émulateur web (fm1-emu) laisse ce PLL éteint, là où le m-bootloader le
  démarre sur le vrai matériel. `audio_init()` ignorait le refus : le
  firmware tournait (écran, boutons) sans jamais rendre une trame. Le
  correctif (`firmware/bsp/boards/fm1/fm1_audio.h`, `fm1_audio_pll_ensure`)
  redémarre le PLL0 avec les mots stock **seulement s'il ne tourne pas** —
  no-op sur le matériel réel, l'intention du contrôle (ne pas retuner un PLL
  partagé) est préservée. Vérifié dans l'émulateur : boot, note de piano et
  accompagnement PLAY (ROCK/Sleng Teng) audibles, LED PLAY allumée.


### Ajouté
- **L'émulateur web** (`web/`) : la page embarque l'émulateur FM-1 de
  [Ninjathune-Human](https://github.com/Ninjathune-Human/Fm1) (page GPL-3.0,
  moteur `engine.js` MIT, cœur [fm1-emu](https://github.com/joakimeriksson/fm1-emu)
  de Joakim Eriksson, MIT) et démarre sur MT-40X : le firmware y tourne tel
  quel sur la puce émulée, sans installation ; « Ouvrir un .fwsc… » charge
  n'importe quel paquet. Le paquet `.fwsc` est préchargé dans
  `web/firmwares/` (catalogue et fiche à côté), les seules retouches à la
  page d'origine sont listées dans `web/EMULATOR_CHANGES.md`, et l'action
  GitHub `Site` (`.github/workflows/pages.yml`) publie le dossier sur
  <https://altessproduction.github.io/MT-4OX/> (lien `play` du `fmhub.json`).
- L'affiche rétro MT-40X en entête du `README.md`
  (`assets/affiche_mt40x.png`).

### Modifié
- La carte du Firmware Hub est désormais l'affiche rétro elle-même :
  `docs/card.jpg` (1024 x 1024, JPEG 266 Ko — dans les limites du Hub :
  carré 240..1024 px, <= 300 Ko) ; `fmhub.json` pointe dessus et le champ
  `what` est ramené sous les 300 caractères exigés.

## [0.1.0-alpha] - 2026-11-15

### Ajouté
- Le moteur d'accompagnement MT-40 (`firmware/src/mt40/`) :
  - horloge 1/16 en Q20.12, swing MPC alterné (0.5 droit à 0.75 poussé),
  - synchro start pré-enroulée (le pas 0 tombe dans le bloc du PLAY),
  - **FILL** : prend les batteries jusqu'au prochain temps fort,
  - **variation A/B**, sélection de rythme à chaud,
  - transposition de la basse par la fondamentale de l'accord joué
    (chord.c `chord_last`, registre calé autour de D2),
  - nappes d'accord rejouées sur les steps du pattern (forme tirée du masque
    `chord_last`).
- Les 6 rythmes MT-40 (ROCK/Sleng Teng vérifié sur deux tablatures, 5
  brouillons éditables) et 22 emplacements de timbres « stairwave ».
- 9 voix pulse PolyBLEP (float32, sans libm : table de demi-tons, coefficient
  de passe-bas approximé) + 4 pistes de batterie (PCM ou synthèse de secours).
- L'intégration Felucca : un appel dans `mix_block`, deux accesseurs dans
  `seq.c`, le hook des touches noires libres de la couche FX
  (FILL / VAR / rythme suivant / précédent).
- Le test hôte (`mt40_host_test.c`) : ligne de basse Sleng Teng vérifiée,
  comptes de frappes, FILL, rendu audio borné - **ALL CHECKS PASSED**.
- `fmhub.json` (fiche The Firmware Hub, identifiant FM-1_23456) et la carte
  `docs/card.png`.

### Notes
- Le clavier continue de jouer les moteurs Felucca ; MT-40X ajoute
  l'accompagnement sans prendre les pistes.
- Les patterns non-ROCK sont des brouillons : calage à l'oreille prévu en P2
  (voir le dossier technique, chapitre 9 et la feuille de route).
