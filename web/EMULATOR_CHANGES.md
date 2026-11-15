# Changements apportés à l'émulateur FM-1+VA

Cette page reprend l'émulateur FM-1+VA de [Ninjathune-Human](https://github.com/Ninjathune-Human/Fm1)
(`engine.js`, les assets et le cœur d'émulation sous MIT ; `index.html` et `backup.js` sous GPL-3.0-only),
avec le cœur [fm1-emu](https://github.com/joakimeriksson/fm1-emu) de Joakim Eriksson (MIT).
MT-40X l'utilise tel quel, avec les seules différences ci-dessous (fichier `web/index.html`) :

1. Les titres : `<title>MT-40X · émulateur FM-1</title>`, le nom d'app `MT-40X`,
   l'en-tête `MT-40X` (le `X` garde la couleur du `+VA`) et la mention
   « Émulateur FM-1 · clone Casio MT-40 ».
2. `restoreLast()` : à la première visite (aucun firmware gardé dans le
   navigateur), MT-40X démarre d'emblée au lieu de FM-1+VA. Les visites
   suivantes gardent le comportement d'origine : le dernier firmware choisi.
3. Le volet « Crédits et licences » cite MT-40X (Altess Production, GPL-3.0)
   et renvoie à ce fichier.

Tout le reste — le panneau, le clavier, le Duo, les sauvegardes, le MIDI —
est le code d'origine, sans modification.
