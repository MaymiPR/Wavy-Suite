"""Generates the factory preset tables of WBass and WVox (plugins/<P>/<P>Presets.h).

Input: build/ref_presets_RBass.json and build/ref_presets_RVox.json, written by nulltest/read_ref_presets.py from the
reference presets on the user's own install (setting values only). The preset names below are our own: no artist
names, no Waves product names, no other trademarks; each one describes what the setting is for, so it stays
recognisable. Groups: "Essentials" (the reference's factory set) and "Studio" (its artist set).

    python tools/make_presets.py
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

# reference file (relative path) -> (group, our name)
BASS_NAMES = {
    r"Factory\Bassic Setup.xps": ("Essentials", "Basic Bass"),
    r"Factory\Big Bottoms.xps": ("Essentials", "Big Bottom End"),
    r"Factory\Bombasstic.xps": ("Essentials", "Bass Bomb"),
    r"Factory\Bottoms Up.xps": ("Essentials", "Bottom Lift"),
    r"Factory\Fuzz Bass.xps": ("Essentials", "Fuzzy Bass"),
    r"Factory\Nose Job.xps": ("Essentials", "Upper Bass Lift"),
    r"Factory\Small Speaker Prep.xps": ("Essentials", "Small Speaker Ready"),
    r"Factory\Under Drive.xps": ("Essentials", "Low Drive"),
    r"Artists\Ben Mink\Low Boost Elec. Fiddle.xps": ("Studio", "Electric Fiddle Low Lift"),
    r"Artists\Dave Aron\Synth Bass RBass.xps": ("Studio", "Synth Bass Weight"),
    r"Artists\Dave Darlington\Acappella Bass Voice.xps": ("Studio", "Vocal Bass Line"),
    r"Artists\Dave Darlington\Kick Drum Seriousness.xps": ("Studio", "Serious Kick"),
    r"Artists\Dave Pensado\Bass.xps": ("Studio", "Bass Glue"),
    r"Artists\Dave Pensado\Burg Bass.xps": ("Studio", "Burly Bass"),
    r"Artists\Dave Pensado\Kik Lo.xps": ("Studio", "Kick Low End"),
    r"Artists\Dave Pensado\Live Bass.xps": ("Studio", "Live Bass Body"),
    r"Artists\Dave Pensado\RBass WS.xps": ("Studio", "Bass Workstation"),
    r"Artists\Gil Norton\808 Sub.xps": ("Studio", "Boom Sub"),
    r"Artists\Gil Norton\Bass GTR Rock.xps": ("Studio", "Rock Bass Guitar"),
    r"Artists\Gil Norton\Bass GTR Sub.xps": ("Studio", "Bass Guitar Sub"),
    r"Artists\JBeatzz\Jbeatzz 808.xps": ("Studio", "Trap Boom"),
    r"Artists\Lu Diaz\Boost That Sub.xps": ("Studio", "Sub Push"),
    r"Artists\Luizinho Mazzei\808 Fight.xps": ("Studio", "Boom Fight"),
    r"Artists\MarioSo DeJesus\NelFlow SubWoofer Bass.xps": ("Studio", "Subwoofer Bass"),
    r"Artists\MarioSo DeJesus\Nesty Tone Bass Pop.xps": ("Studio", "Pop Bass Tone"),
    r"Artists\Mark Ralph\Live Bass Love.xps": ("Studio", "Live Bass Warmth"),
    r"Artists\Mark Ralph\Prophet's Octave.xps": ("Studio", "Poly Synth Octave"),
    r"Artists\Michael Brauer\808.xps": ("Studio", "Boom Low"),
    r"Artists\Michael Brauer\Bass.xps": ("Studio", "Bass Bloom"),
    r"Artists\Michael Brauer\Sub Bass Send.xps": ("Studio", "Sub Send"),
    r"Artists\Michael Brauer\Toms.xps": ("Studio", "Tom Depth"),
    r"Artists\Mike Hedges\Drumsareus.xps": ("Studio", "Drum Bus Weight"),
    r"Artists\Ott\1950s Snare.xps": ("Studio", "Vintage Snare"),
    r"Artists\Ott\Closer Percussion.xps": ("Studio", "Close Percussion"),
    r"Artists\Ott\Laptop Tablas.xps": ("Studio", "Small Tablas"),
    r"Artists\Ott\Smaller Than It Sounds.xps": ("Studio", "Bigger Than It Is"),
    r"Artists\Ott\Soft Tape Breakbeat.xps": ("Studio", "Tape Breakbeat"),
    r"Artists\Ross Hogarth\Bass 1.xps": ("Studio", "Bass Shape 1"),
    r"Artists\Ross Hogarth\Bass 2.xps": ("Studio", "Bass Shape 2"),
    r"Artists\Ross Hogarth\Bass 3.xps": ("Studio", "Bass Shape 3"),
    r"Artists\Scott Gershin\Knock Knock.xps": ("Studio", "Door Knock"),
    r"Artists\Scott Gershin\Lo Lo Thump For Guns.xps": ("Studio", "Gunshot Low Thump"),
    r"Artists\Scott Gershin\Mid Range Gun Boost.xps": ("Studio", "Gunshot Mid Boost"),
    r"Artists\Scott Gershin\Music Lo End Tone.xps": ("Studio", "Score Low End"),
    r"Artists\Todd Parker\Parker Bass Thump.xps": ("Studio", "Bass Thump"),
}
VOX_NAMES = {
    r"Factory\25db Comp No Gate.xps": ("Essentials", "Heavy Comp No Gate"),
    r"Factory\Ball Game Sports Cast.xps": ("Essentials", "Stadium Announcer"),
    r"Factory\Hissy Lecture Intelligizer.xps": ("Essentials", "Noisy Lecture Cleanup"),
    r"Factory\Out Door News Cast.xps": ("Essentials", "Outdoor Reporter"),
    r"Artists\Chris Baseford\BG Vocal.xps": ("Studio", "Background Vocal"),
    r"Artists\Chris Baseford\Lead Vocal.xps": ("Studio", "Lead Vocal Control"),
    r"Artists\Dave Darlington\Easy Vocal.xps": ("Studio", "Gentle Lead"),
    r"Artists\Dave Darlington\In Yo Face Vocal.xps": ("Studio", "Up Front Vocal"),
    r"Artists\Dave Pensado\Good Vocal.xps": ("Studio", "Solid Vocal"),
    r"Artists\Dave Pensado\Start.xps": ("Studio", "Starting Point"),
    r"Artists\Eddie Kramer\Female Vocal\Gentle Compression.xps": ("Studio", "Soft Vocal Squeeze"),
    r"Artists\Eddie Kramer\Metal Drums\Metal Snare Gate.xps": ("Studio", "Hard Snare Gate"),
    r"Artists\Greg Wells\Vocal Control.xps": ("Studio", "Steady Vocal"),
    r"Artists\Lu Diaz\Vocal Presence.xps": ("Studio", "Vocal Forward"),
    r"Artists\Mark Ralph\Gasping For Air.xps": ("Studio", "Breath Control"),
    r"Artists\Mark Ralph\Vocal Tracking.xps": ("Studio", "Tracking Vocal"),
    r"Artists\Michael Brauer\Backing Vocal.xps": ("Studio", "Backing Stack"),
    r"Artists\Michael Brauer\Lead Vox.xps": ("Studio", "Lead Up Front"),
    r"Artists\Miles Comaskey\BGV Stack.xps": ("Studio", "BGV Pile"),
    r"Artists\RENEGADE EL REY\Less Is More.xps": ("Studio", "Light Touch"),
    r"Artists\Sterling Winfield\Dyn Mic Vox Squeeze.xps": ("Studio", "Dynamic Mic Squeeze"),
}


def num(v, lo, hi, nd=2):
    x = float(v)
    if abs(x) < 1e-9: x = 0.0
    return round(min(hi, max(lo, x)), nd)


def bass_values(rw):
    # reference order: Input, Freq, Response, Ratio, Clip, Intensity, HiPass (1 = Original Bass out), Gain, Decay,
    # 4 meters, SmoothTc, Tc, XFadeTc, Smooth, (Skin)
    v = [("inGain", num(rw[0], -30, 0, 1)), ("freq", num(rw[1], 32, 256, 0)), ("response", num(rw[2], 0, 30, 1)),
         ("ratio", num(rw[3], 0.25, 1)), ("intensity", num(rw[5], -24, 24, 1)),
         ("origBass", 0.0 if float(rw[6]) > 0.5 else 1.0), ("gain", num(rw[7], -30, 0, 1)), ("decay", num(rw[8], 0, 1, 1))]
    for i, pid, lo, hi in ((13, "smoothTc", 0, 1000), (14, "tc", 0, 2000), (15, "xfadeTc", 0, 5000)):
        if i < len(rw) and rw[i] != '*': v.append((pid, num(rw[i], lo, hi, 1)))
    if len(rw) > 16 and rw[16] != '*': v.append(("smooth", 1.0 if float(rw[16]) > 0.5 else 0.0))
    return v


def vox_values(rw):
    # reference order: Compression, Gate, GateRel, 4 meters, Gain
    return [("comp", num(rw[0], -36, 0, 1)), ("gate", num(rw[1], -80, 0, 1)), ("gateRel", num(rw[2], 10, 250, 1)),
            ("gain", num(rw[7], -36, 0, 1))]


def write(product, src, names, conv):
    data = json.load(open(os.path.join(ROOT, "build", src)))
    by = {d["file"]: d for d in data}
    missing = [f for f in by if f not in names and "reset" not in f.lower()]
    assert not missing, missing
    rows = []
    for f, (group, name) in names.items():
        vals = conv(by[f]["realworld"])
        rows.append('    { "%s", "%s", { %s } },' % (group, name, ", ".join('{ "%s", %rf }' % (kv[0], float(kv[1])) for kv in vals)))
    rows.sort(key=lambda r: (0 if '"Essentials"' in r else 1, r))
    out = os.path.join(ROOT, "plugins", product, product + "Presets.h")
    open(out, "w", newline="\n").write(
        "#pragma once\n// Factory presets of %s, generated by tools/make_presets.py (do not edit by hand).\n"
        "// Setting values carried over from the reference's presets; names are Wavy's own.\n"
        '#include "WavyPresets.h"\n\nnamespace %s\n{\ninline const std::vector<wavy::FactoryPreset>& factoryPresets()\n{\n'
        "    static const std::vector<wavy::FactoryPreset> list {\n%s\n    };\n    return list;\n}\n}\n"
        % (product, product.lower(), "\n".join(rows)))
    print(out, len(rows), "presets")


write("WBass", "ref_presets_RBass.json", BASS_NAMES, bass_values)
write("WVox", "ref_presets_RVox.json", VOX_NAMES, vox_values)
