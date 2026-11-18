# OPERA — synthétiseur FM à 6 opérateurs (dossier VSTFM)

Synthé FM polyphonique 16 voix dans l'esprit des grands FM à six opérateurs des années 80, avec son propre design,
ses propres algorithmes et ses propres sons. Projet issu d'Oxide (même base JUCE, même panneau, mêmes effets, même installeur).

## Règles

- Principes publics uniquement : aucune façade, aucun nom ni son d'usine d'un instrument existant.
- La table des 16 algorithmes est la nôtre.

## Le son

- 6 opérateurs : RATIO (0.5, 1 à 31), FINE, DETUNE, LEVEL, VEL, KEY SC., enveloppe 4 temps / 4 niveaux (en décibels).
- 16 algorithmes dessinés en direct, FEEDBACK sur l'opérateur 6.
- LFO (vitesse, retard, forme, hauteur, amplitude, molette), TRANSPOSE, TUNE, BEND, KEY RATE.
- Chorus, delay, reverb.

## Construire (macOS)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
TMPDIR="$PWD/.local-cache" cmake --build build --config Release --parallel 4
tools/paquet.sh
```
