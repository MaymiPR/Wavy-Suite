"""Renders the built WDelay VST3 through tools/RefRender (the same host as the reference), for nulls of the plug-in
itself. Settings by RefRender's arguments (WDelay's parameter names and texts)."""
import os, subprocess, tempfile
import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RR = os.path.join(ROOT, 'build', 'RefRender_artefacts', 'Release', 'RefRender.exe')
VST = os.path.abspath(os.path.join(ROOT, 'build', 'plugins', 'WDelay', 'WDelay_artefacts', 'Release', 'VST3', 'WDelay.vst3'))


def render(x, fs=48000, args=(), preroll=3.0, bpm=120.0):
    x = np.atleast_2d(np.asarray(x, np.float32))
    if x.shape[0] == 1: x = np.concatenate([x, x])
    d = tempfile.mkdtemp(prefix='wdv_'); fi = os.path.join(d, 'i.f32'); fo = os.path.join(d, 'o.f32')
    x.T.astype(np.float32).tofile(fi)
    r = subprocess.run([RR, VST, 'WDelay', fi, fo, 'fs=%d' % fs, 'bpm=%g' % bpm, 'preroll=%g' % preroll] + list(args), capture_output=True, text=True)
    if not os.path.exists(fo): raise RuntimeError(r.stdout + r.stderr)
    return np.fromfile(fo, np.float32).reshape(-1, 2).T
