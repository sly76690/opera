# OPERA - balances the factory sounds: reads the loudness the smoke tests measured for each sound (its loudest 50 ms,
# a four-note chord at velocity 0.8) and sets its LEVEL so that all of them reach the same mark.
# Usage: python3 tools/niveaux.py run.log   then   python3 tools/sons.py
import json, os, re, sys
TARGET = -16.0   # dBFS, the loudest 50 ms of the chord
here = os.path.dirname(os.path.abspath(__file__)); path = os.path.join(here, "levels.json")
levels = json.load(open(path)) if os.path.exists(path) else {}
for line in open(sys.argv[1]):
    m = re.match(r"\s+(.+?)\s+rms .* loud (-?[\d.]+) dB", line)
    if m:
        name, loud = m.group(1), float(m.group(2))
        levels[name] = round(max(-18., min(12., levels.get(name, 0.) + TARGET - loud)), 1)
json.dump(dict(sorted(levels.items())), open(path, "w"), indent=1)
print(len(levels), "levels")
