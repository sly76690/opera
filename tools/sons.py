# OPERA - writes the factory sounds (Presets/Sounds), embedded in the plugin and installed with it.
# Each sound only lists what differs from the defaults: Opera fills in the rest when it loads it.
# Every sound is programmed from scratch on Opera; no patch of any other instrument is copied.
# Usage:  python3 tools/sons.py   (the order of the BANK / PATCH keys is in Source/FactoryBank.h)
import os, glob
from xml.sax.saxutils import quoteattr

def op(n, ratio=1, fine=0, det=0, level=0, vel=.3, ks=0, t=(.003, .4, 1.5, .35), l=(1, .75, .6, 0)):
    """One operator: ratio .5 or 1-31 (stored as the RATIO stop), fine, detune in cents, level, velocity,
    key scaling, the four envelope times (s) and levels."""
    p = "op%d" % n
    d = {p + "coarse": 0 if ratio == .5 else ratio, p + "fine": fine, p + "detune": det, p + "level": level, p + "vel": vel, p + "keyscale": ks}
    for k in range(4): d[p + "t%d" % (k + 1)] = t[k]; d[p + "l%d" % (k + 1)] = l[k]
    return d

def sound(algorithm, *ops, **rest):
    d = {"algorithm": algorithm - 1}
    for o in ops: d.update(o)
    d.update(rest); return d

SOUNDS = {
    "EPIANO - Tine Glow": sound(1,
        op(1, level=.85, t=(.002, 1.2, 4, .4), l=(1, .55, 0, 0)),
        op(2, ratio=14, level=.42, vel=.8, ks=.4, t=(.001, .25, 1.5, .3), l=(1, .15, 0, 0)),
        op(3, det=3, level=.75, t=(.002, 1.5, 5, .45), l=(1, .5, 0, 0)),
        op(4, level=.5, vel=.7, t=(.002, .9, 3, .4), l=(1, .35, 0, 0)),
        op(5, det=-3, level=.55, t=(.002, 2, 5, .5), l=(1, .5, 0, 0)),
        op(6, level=.35, vel=.5, t=(.002, 1, 3, .4), l=(1, .3, 0, 0)),
        feedback=.25, chorus=.45),
    "BELL - Glass Tower": sound(16,
        op(1, level=.8, vel=.4, t=(.002, 4, 8, 2), l=(1, .3, 0, 0)),
        op(2, ratio=3, fine=.5, level=.55, t=(.002, 3, 6, 2), l=(1, .2, 0, 0)),
        op(3, ratio=7, level=.25, t=(.001, .6, 2, 1), l=(1, .1, 0, 0)),
        op(4, ratio=2, det=4, level=.6, t=(.002, 5, 8, 2), l=(1, .3, 0, 0)),
        op(5, ratio=4, level=.5, t=(.002, 3, 6, 2), l=(1, .2, 0, 0)),
        op(6, ratio=5, fine=.4, level=.45, t=(.001, 2, 5, 2), l=(1, .2, 0, 0)),
        reverbMix=.25, chorus=.2),
    "BASS - Rubber FM": sound(4,
        op(1, level=.9, vel=.3, t=(.002, .6, 1.5, .15), l=(1, .7, .4, 0)),
        op(2, level=.62, vel=.6, t=(.001, .25, .8, .1), l=(1, .2, 0, 0)),
        op(3, ratio=.5, level=.7, t=(.002, 1, 2, .15), l=(1, .6, .4, 0)),
        op(4, level=.4, t=(.001, .3, 1, .1), l=(1, .2, 0, 0)),
        op(5, level=.3, t=(.001, .3, 1, .1), l=(1, .2, 0, 0)),
        op(6, level=.2, t=(.001, .3, 1, .1), l=(1, .2, 0, 0)),
        feedback=.5, chorus=0, reverbMix=.04),
    "BRASS - Gold Section": sound(2,
        op(1, level=.85, t=(.06, .4, 2, .3), l=(1, .85, .8, 0)),
        op(2, level=.6, t=(.12, .5, 2, .3), l=(1, .7, .6, 0)),
        op(3, level=.4, t=(.15, .6, 2, .3), l=(1, .6, .5, 0)),
        op(4, det=4, level=.8, t=(.06, .4, 2, .3), l=(1, .85, .8, 0)),
        op(5, level=.55, t=(.12, .5, 2, .3), l=(1, .7, .6, 0)),
        op(6, level=.45, t=(.15, .5, 2, .3), l=(1, .6, .5, 0)),
        feedback=.6, lfoPitch=.1, lfoDelay=.6, chorus=.3),
    "ORGAN - Drawbars": sound(14,
        op(1, ratio=.5, level=.8, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        op(2, ratio=1, level=.85, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        op(3, ratio=2, level=.7, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        op(4, ratio=3, level=.5, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        op(5, ratio=4, level=.45, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        op(6, ratio=6, level=.35, vel=0, t=(.005, .1, .1, .05), l=(1, 1, 1, 0)),
        chorus=.5, reverbMix=.1),
    "KEYS - Clav Snap": sound(5,
        op(1, level=.85, vel=.5, t=(.001, .5, 1.5, .1), l=(1, .3, 0, 0)),
        op(2, ratio=3, level=.6, vel=.7, t=(.001, .15, .6, .1), l=(1, .3, 0, 0)),
        op(3, ratio=5, level=.4, t=(.001, .08, .4, .1), l=(1, .1, 0, 0)),
        op(4, ratio=2, level=.5, t=(.001, .4, 1.2, .1), l=(1, .3, 0, 0)),
        op(5, level=.5, t=(.001, .2, .8, .1), l=(1, .2, 0, 0)),
        op(6, level=.3, t=(.001, .2, .8, .1), l=(1, .2, 0, 0)),
        feedback=.4, chorus=.2, reverbMix=.06),
    "MALLET - Marimba": sound(1,
        op(1, level=.9, vel=.4, t=(.001, .6, 1.2, .3), l=(1, .2, 0, 0)),
        op(2, ratio=4, level=.5, vel=.7, t=(.001, .08, .3, .1), l=(1, 0, 0, 0)),
        op(3, ratio=4, level=.3, t=(.001, .3, .8, .2), l=(1, .1, 0, 0)),
        op(4, ratio=10, level=.3, t=(.001, .04, .2, .1), l=(1, 0, 0, 0)),
        chorus=0, reverbMix=.14),
    "PAD - Crystal Air": sound(15,
        op(1, level=.6, vel=.1, t=(1.2, 2, 3, 2.5), l=(1, .9, .85, 0)),
        op(2, ratio=2, det=5, level=.45, vel=.1, t=(1.5, 2, 3, 2.5), l=(1, .9, .8, 0)),
        op(3, det=-4, level=.55, vel=.1, t=(1.4, 2, 3, 2.5), l=(1, .9, .85, 0)),
        op(4, ratio=3, level=.35, t=(2, 3, 4, 2.5), l=(1, .7, .6, 0)),
        op(5, det=7, level=.5, vel=.1, t=(1.6, 2, 3, 2.5), l=(1, .9, .85, 0)),
        op(6, ratio=5, level=.3, t=(2.5, 3, 4, 2.5), l=(1, .6, .5, 0)),
        chorus=.6, reverbMix=.3, lfoPitch=.05),
}

folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets", "Sounds")
os.makedirs(folder, exist_ok=True)
for old in glob.glob(os.path.join(folder, "*.orbitpreset")):   # sounds no longer in the list are removed; the others are rewritten
    if os.path.basename(old)[:-len(".orbitpreset")] not in SOUNDS:
        os.remove(old)
for name, settings in SOUNDS.items():
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<OrbitState stateVersion="1" program="0" modified="0">']
    lines += ['  <PARAM id=%s value="%s"/>' % (quoteattr(key), repr(float(value))) for key, value in settings.items()]
    lines.append('</OrbitState>')
    with open(os.path.join(folder, name + ".orbitpreset"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
print(len(SOUNDS), "sons")
