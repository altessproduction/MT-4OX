# Changelog

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le
versionnage sémantique.

## [Unreleased]

### Ajouté
- **L'émulateur web** : le site (<https://altessproduction.github.io/MT-4OX/>)
  embarque l'émulateur FM-1+VA de Ninjathune-Human avec MT-40X préchargé — le
  firmware y tourne tel quel sur la puce émulée par fm1-emu (Joakim Eriksson,
  MIT), sans installation ; « Ouvrir un .fwsc… » charge n'importe quel paquet.
  Les fichiers sont dans `web/` (les seuls changements faits à l'émulateur sont
  listés dans `web/EMULATOR_CHANGES.md`), et l'action GitHub `Site`
  (`.github/workflows/pages.yml`) publie le dossier.

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
