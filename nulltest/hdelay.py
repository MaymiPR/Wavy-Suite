"""Renders the reference H-Delay offline (tools/RefRender, a JUCE host with a tempo) for the Phase 3 measurements.

Settings by short names, in real units or choice texts; they are applied after a first block of a silent pre-roll
(H-Delay shows but does not process values set before its first block). Choices are set by their normalised value
(the shell does not parse its own choice texts). changes: [(seconds into the input, name, value)], sample exact.

    import hdelay; y = hdelay.render(x, 48000, sync="ms", delay=100, feedback=0, analog="Off")
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import refhost as R

NAMES = {"Stereo": "H-Delay Stereo@1849387175", "Mono": "H-Delay Mono@1847911073", "Mono/Stereo": "H-Delay Mono/Stereo@1850617249"}
NOTES = ["1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T",
         "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1B", "2B"]
CHOICE = {  # short name: (VST3 name, the choices in order)
    "sync": ("Sync", ["BPM", "Host", "ms"]),
    "analog": ("Analog", ["Off", "Mod1", "Mod2", "Mod3", "Mod4"]),
    "lofi": ("LoFi", ["HiFi", "LoFi"]),
    "pingpong": ("PingPong", ["Stereo", "PingPong"]),
    "phase_l": ("Phase L", ["+", "-"]),
    "phase_r": ("Phase R", ["+", "-"]),
    "start": ("Ping Pong Start Direction", ["Left", "Right"]),
    "note": ("Delay BPM", NOTES),
    "note_r": ("Delay BPM R", NOTES),
}
TEXT = {"delay": "Delay Sec", "delay_r": "Delay Sec R", "feedback": "Feedback", "mix": "Mix", "output": "Output",
        "rate": "Rate", "depth": "Depth", "hipass": "HiPass", "lopass": "LoPass", "bpm": "BPM"}


def arg(key, value):
    if key in CHOICE:
        name, ch = CHOICE[key]
        i = ch.index(value) if isinstance(value, str) else int(value)
        return "%s:=%.6f" % (name, i / (len(ch) - 1))
    return "%s=%s" % (TEXT[key], value)


def render(x, fs=48000, comp="Stereo", preroll=1.0, host_bpm=120.0, changes=(), block=512, log=False, **settings):
    """x: (n,) or (2, n); returns (2, n)"""
    extra = ["preroll=%g" % preroll] + [arg(k, v) for k, v in settings.items()]
    extra += ["at=%.9f:%s" % (t, arg(k, v)) for t, k, v in changes]
    y, out = R.render_ref(x, fs, name=NAMES[comp], bpm=host_bpm, block=block, extra=extra)
    if log: print(out)
    if "no parameter" in out: raise ValueError(out)
    return y
