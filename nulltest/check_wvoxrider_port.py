"""The C++ ride (WVox Rider's processor, run by WVoxRiderRender) against the fitting model (vrmodel.c with the values in
plugins/WVoxRider/WVoxRiderFit.h, as tools/vr_export.py wrote them): the gains should agree to float precision.

    python -I check_wvoxrider_port.py
"""
import sys, os, re, subprocess, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import vr_ref as Q
import vr_eval as E
import vrmodel as M

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get('WVOXRIDER_RENDER', os.path.join(HERE, '..', 'build', 'plugins', 'WVoxRider', 'WVoxRiderRender_artefacts', 'Release', 'WVoxRiderRender.exe'))
FIT_H = os.path.join(HERE, '..', 'plugins', 'WVoxRider', 'WVoxRiderFit.h')
CPP = dict(c='c', bias='bias', followerAttackSlow='fatt_s', followerAttackFast='fatt_f', followerRelease='frel', followerHold='fhold',
           followerPower='fpow', vadAttack='vatt', vadRelease='vrel', vk='vk', rideAttackSlow='ratt_s',
           rideAttackFast='ratt_f', rideRelease='rrel', ridePower='rpow', parkTime='park', parkHoldSlow='phold_s',
           parkHoldFast='phold_f', spillOffset='spill', initialV='v0')


def header_params():
    t = open(FIT_H, encoding='utf-8').read()
    p = {CPP[m.group(1)]: float(m.group(2)) for m in re.finditer(r'double (\w+) = ([-0-9.e+]+);', t)}
    return p


def render_cpp(x, st):
    d = tempfile.mkdtemp()
    fi, fo = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
    np.asarray(x, np.float32).tofile(fi)
    args = [EXE, fi, fo, '1', '48000', 'target=%g' % st['target'], 'rangeMin=%g' % st['rmin'], 'rangeMax=%g' % st['rmax'],
            'vocalSensitivity=%g' % st['vs'], 'spill=%g' % st['spill'], 'attack=%g' % st['fast'], 'mode=%d' % (3 if st['live'] else 1),
            'idle=%g' % st['idle']]
    subprocess.run(args, check=True)
    return np.fromfile(fo, np.float32)


def main():
    p = header_params()
    worst = 0.0
    for key in ('step-18-30', 'gap0.60-30', 'stop-24', 'syll0', 'syll2'):
        for s in ('default', 'fast', 'range-3+9', 'fast12', 'live-30'):
            x, _ = Q.load('probes', key, s)
            st = E.model_settings(s)
            gm = M.ride(x, st, p).astype(np.float64)
            y = render_cpp(x, st).astype(np.float64)
            ok = np.abs(x) > 1e-3
            gc = 20 * np.log10(np.abs(y[ok]) / np.abs(x[ok]))
            d = np.abs(gc - gm[ok]).max()
            worst = max(worst, d)
            print('%-12s %-10s max |C++ - model| %.4f dB' % (key, s, d))
    print('worst %.4f dB' % worst)


if __name__ == '__main__':
    main()
