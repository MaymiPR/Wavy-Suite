"""Phase 4: the reverb's impulse responses, packed for the plug-in (the user chose convolution, 2026-10-28). The reference's
reverb is linear and exactly time-invariant at every colour (an impulse fed later gives a bit-identical response), true
stereo (L and R inputs give uncorrelated responses) and designed per sample rate (one rate's response resampled is
unrelated to another's), so each rate keeps its own four paths (L->L, L->R, R->L, R->R) per colour, as captured by
cv_ref at -26 dBFS with every other colour clear (git-ignored reference/wvoxchain/meas/rvb_ir_<rate>_c<colour>_in<side>.npy:
the wet part, the output less the output with Reverb clear, per unit input, at fader 0). Each is cut where the decay of
the four together has fallen 65 dB, faded over the last 30 ms, and stored as 16-bit floats.

Writes plugins/WVoxChain/ir/reverb_<rate>.bin: per colour 1..3: uint32 length, then length x 4 float16 (LL, LR, RL, RR
interleaved per sample).

    python -I pack_voxchain_reverb.py
"""
import os
import struct
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MEAS = os.path.join(HERE, '..', 'reference', 'wvoxchain', 'meas')
OUT = os.path.join(HERE, '..', 'plugins', 'WVoxChain', 'ir')
RATES = (44100, 48000, 88200, 96000)


def main():
    os.makedirs(OUT, exist_ok=True)
    total = 0
    for fs in RATES:
        blob = b''
        for c in (1, 2, 3):
            a = np.load(os.path.join(MEAS, 'rvb_ir_%d_c%d_in0.npy' % (fs, c))).astype(float)    # from L: (L out, R out)
            b = np.load(os.path.join(MEAS, 'rvb_ir_%d_c%d_in1.npy' % (fs, c))).astype(float)    # from R
            P = np.stack([a[0], a[1], b[0], b[1]])                                               # LL, LR, RL, RR
            sch = np.cumsum((P ** 2).sum(0)[::-1])[::-1]; sch /= sch[0]
            n = int(np.argmax(sch < 10 ** (-65 / 10)))
            fade = int(0.03 * fs)
            P = P[:, :n].copy(); P[:, -fade:] *= np.linspace(1, 0, fade)
            q = P.astype(np.float16)
            err = 10 * np.log10(np.sum((q.astype(float) - P) ** 2) / np.sum(P ** 2))
            print('%6d Hz colour %d: %.2f s, float16 error %.1f dB' % (fs, c, n / fs, err))
            blob += struct.pack('<I', n) + q.T.astype('<f2').tobytes()
        path = os.path.join(OUT, 'reverb_%d.bin' % fs)
        open(path, 'wb').write(blob)
        total += len(blob)
        print('  %s: %.1f MB' % (os.path.basename(path), len(blob) / 1e6))
    print('total %.1f MB' % (total / 1e6))


if __name__ == '__main__':
    main()
