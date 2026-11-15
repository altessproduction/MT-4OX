# Soumission à The Firmware Hub — MT-40X, identifiant FM-1_23456

Ce document prépare la soumission sur <https://thefirmwarehub.com/submit/>.
Il récapitule ce que le Hub exige et où chaque pièce se trouve.

## 1. Ce que le Hub exige, et où c'est

| Exigence du Hub | Chez MT-40X |
|---|---|
| Licence ouverte, sources publics | GPL-3.0-only (`LICENSE`), sources dans ce dépôt, vendoriés MIT/MPL-2.0 documentés dans `LICENSING.md` |
| Un identifiant `FM-1_` unique | **FM-1_23456** — cuit dans le paquet `.fwsc` (champ produit), vérifier qu'il est libre sur le Hub avant l'envoi |
| Une release GitHub avec le `.fwsc` en pièce jointe | Release **v0.1.0-alpha** : `MT-40X-v0.1.0-FM-1_23456.fwsc` |
| Les sources à l'étiquette de la release | L'étiquette `v0.1.0-alpha` pointe sur ce dépôt complet |
| Le loader de mise à jour dans le paquet | Le `.fwsc` contient `flash.bin` + `ota.bin` (le loader Felucca) — c'est le format `fm1pkg_make.py` |
| `fmhub.json` à la racine | [`fmhub.json`](../fmhub.json) (`what`, `author`, `kinds`, `image`, `color`) |
| Image carrée 240..1024 px, <= 300 Ko | [`docs/card.png`](card.png) (512 x 512) |
| Testé sur un vrai FM-1 | **À faire par vous** — voir la liste ci-dessous |

## 2. Avant d'envoyer : le test sur matériel (obligatoire)

Le Hub refuse les « ça compile » — il faut avoir installé et quitté le firmware
sur un vrai FM-1 :

1. Compiler le paquet (voir le README, section *Compiler et installer*).
2. Installer sur le FM-1 (installateur web ou `fm1_install.py`).
3. Vérifier : PLAY lance l'accompagnement, un accord une-touche transpose la
   basse, FILL (couche FX, 5e touche noire) fait le fill, VAR et le changement
   de rythme répondent, le clavier joue toujours les moteurs Felucca.
4. **Quitter le firmware** : réinstaller Felucca par-dessus — l'appareil doit
   revenir à un firmware précédent sans manipulations spéciales.
5. Noter d'où vers où vous avez installé (le formulaire le demande).

## 3. Le formulaire, pré-rempli

| Champ du formulaire | Réponse |
|---|---|
| Repository | `https://github.com/altessproduction/MT-4OX` |
| Name of the firmware | `MT-40X` |
| Your name | `Altess Production` |
| Your address (privé) | *votre adresse* |
| Installed on a real FM-1, leaving works | *cocher après le test ci-dessus* |
| How you tested it | *ex. : « Installed v0.1.0-alpha from Felucca 1.8.2 over USB (Web MIDI installer), verified the accompaniment, chords, FILL and rhythm switching, then reinstalled Felucca 1.8.2 to leave. Same package verified to boot in the online browser emulator: https://altessproduction.github.io/MT-4OX/ »* |
| Anything Sven should know | « Built on Felucca 1.8 (GPL-3.0, engines MIT, BSP MPL-2.0, vendorised). The ROCK pattern is the Casio MT-40 preset by Hiroko Okuda, source of the Sleng Teng riddim (Wayne Smith / Noel Davey / King Jammy, 1985) - credited in code and README. Drums: synth fallback (no samples shipped yet); PCM lanes can take the free MusicRadar SampleRadar MT-40 pack. Package identity FM-1_23456, contains the Felucca update loader (ota.bin). » |

## 4. Tester aussi dans le navigateur (sans matériel)

Le paquet `.fwsc` tourne tel quel dans l'émulateur en ligne du dépôt
(<https://altessproduction.github.io/MT-4OX/>) : la page démarre sur MT-40X, la
puce AC791N est émulée par fm1-emu (Joakim Eriksson, MIT), et « Ouvrir un
.fwsc… » charge n'importe quel paquet local. C'est le test de boot et
d'interface sans FM-1 — il ne remplace pas le test sur matériel exigé au-dessus.

## 5. Après l'envoi

- Le Hub vérifie le dépôt et la release (site de test d'abord, puis public).
- Chaque release suivante de ce dépôt, avec son `.fwsc` attaché, apparaît
  automatiquement sur la carte du firmware sous ~3 heures.
