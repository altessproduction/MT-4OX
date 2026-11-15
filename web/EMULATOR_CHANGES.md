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
4. **La page Riddims** (bloc `riddims:begin` … `riddims:end` en fin de fichier,
   Altess Production, GPL-3.0) : un bouton « ♫ Riddims » à côté du sélecteur de
   firmware ouvre un volet avec les 38 riddims du « The Riddim Real Book (Tab) »
   (données dans le bloc `window.RIDDB`, générées par `scripts/riddims_site.py`
   depuis le PDF). Pour chaque riddim :
   - **Écouter** : une maquette Web Audio de la page (batterie et basse
     synthétisées, au tempo du livre) ;
   - **Séquenceur** : la basse est écrite dans la piste 3 et la batterie dans la
     piste 4 (moteur DRUM, grille v5) du FM-1 émulé, par le protocole éditeur
     (SysEx `F0 7D 46 4C …` dans l'USB de la puce) : LEN 16 ou 32 pas, DIV 1/16,
     tempo de la fourchette du livre ; puis SEQ + PLAY sur le panneau ;
   - **Motif de basse** : si le firmware embarque la banque riddim (commandes
     éditeur `0x52`/`0x53`), le MT-40X joue la ligne comme motif
     d'accompagnement transposé par l'accord tenu au clavier ; sans banque, la
     basse va au séquenceur (piste 3) et la page le dit.

Tout le reste — le panneau, le clavier, le Duo, les sauvegardes, le MIDI —
est le code d'origine, sans modification.
