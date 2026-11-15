# Changelog

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le
versionnage sémantique.

## [Non publié]

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
