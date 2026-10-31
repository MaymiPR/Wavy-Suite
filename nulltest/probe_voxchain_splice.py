"""Phase 4, the doubler's fourth pass: how the reference's pitch shifter (Wide, Spreader) picks its step at a jump.
Silent input but for a pair of clicks DELTA samples apart at sample U and a probe click 4000 samples after the jump the
voice makes at sample J (render samples, 1 s pre-roll); the probe's delay gives the step taken. The rule found (every
probe and every measured step on noise): the step is one of round (256 2^(k / 20)), k = 0 .. 19, the one where the
send's autocorrelation, kept with an exponential memory of about 1024 samples, is highest; on a tie the longest (495).

    python -I probe_voxchain_splice.py pair COLOUR SIDE J U DELTA [AMPLITUDE]   one probe: prints the probe's delay
    python -I probe_voxchain_splice.py rule NOISE.npz SIDE JUMP TAU              the rule's step at an output sample
                                                                                 (NOISE.npz: x, the input at 48 kHz)
"""
import os
import sys
import numpy as np
from scipy.signal import sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cv_ref as C
import fit_voxchain_deess as Fd
FS = 48000
CLEAR = dict(bass_color=0, treble_color=0, compress_color=0, reverb_color=0, delay_color=0, pitch_color=0)
RUNGS = np.round(256 * 2 ** (np.arange(20) / 20)).astype(int)


def probe_delay(w, side, at):
    """the strongest tap after the probe click (deconvolved from the send's click response)"""
    imp = np.zeros(4096); imp[0] = 0.5
    k0 = np.zeros(8192); k0[:4096] = sosfilt(Fd.fixed_sos(), imp)
    K = np.fft.rfft(k0); reg = 1e-6 * np.max(np.abs(K)) ** 2
    s = np.zeros(8192); s[:2500] = w[side, at:at + 2500]
    h = np.fft.irfft(np.fft.rfft(s) * np.conj(K) / (np.abs(K) ** 2 + reg), 8192)[:2450]
    i = int(np.argmax(np.abs(h[20:]))) + 20
    return (i * h[i] + (i + 1) * h[i + 1]) / (h[i] + h[i + 1])


def pair(colour, side, J, U, delta, amp=0.5):
    probe = J + 4000
    x = np.zeros(probe + 3000, np.float32); x[probe] = 0.5
    if delta > 0: x[U] += amp; x[U - delta] += amp
    y = C.render(x, FS, {C.NOISE: 0}, C.host(**dict(CLEAR, pitch_color=colour), pitch=10))
    y0 = C.render(x, FS, {C.NOISE: 0}, C.host(**CLEAR, pitch=10))
    print('probe delay %.2f samples' % probe_delay((y - y0).astype(float), side, probe))


def rule(x, n, tau=1024.0):
    """the step the rule picks at aligned output sample n (x: the input; the send is its fixed filter's output)"""
    y = np.concatenate([np.zeros(FS), sosfilt(Fd.fixed_sos(), np.asarray(x, float))])
    a = np.exp(-1 / tau)
    R = np.array([np.dot(y[s:n + 1] * y[:n + 1 - s], a ** (n - np.arange(s, n + 1))) for s in RUNGS])
    return int(RUNGS[19 - int(np.argmax(R[::-1]))])


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'pair': pair(int(a[1]), int(a[2]), int(a[3]), int(a[4]), int(a[5]), float(a[6]) if len(a) > 6 else 0.5)
    elif a[0] == 'rule': print(rule(np.load(a[1])['x'], int(a[3]), float(a[4]) if len(a) > 4 else 1024.0))
