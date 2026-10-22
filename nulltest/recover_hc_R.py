"""WComp Phase 4: recover H-Comp's internal gain reduction R[n] from a probe's exact gain trace by undoing the two
moving averages (38 and 27 samples) and the attack one-pole (falling only), then measure its release rate.

The reference's gain g = box27(box38(g1)), g1 the attack-smoothed 10^(-R/20). Each moving average is undone exactly by
its recursion; float32 rounding accumulates, so the recursion is re-anchored to 1 wherever the target is 0 and the
gain has been 1 for a while."""
import numpy as np

B1, B2 = 38, 27


def unbox(v, n):
    """x such that v = moving_average_n(x), with x = 1 before the start."""
    x = np.ones(len(v) + n)
    for i in range(len(v)):
        x[i + n] = x[i] + n * (v[i] - (v[i - 1] if i > 0 else 1.0))
    return x[n:]


def recover(g, att_c=0.0):
    v1 = unbox(g, B2)
    g1 = unbox(v1, B1)
    if att_c > 0:
        # undo the one-pole where it was falling: g1[n] = gt + c (g1[n-1] - gt)  ->  gt = (g1[n] - c g1[n-1]) / (1 - c)
        gt = g1.copy()
        fall = np.r_[False, g1[1:] < g1[:-1]]
        gt[1:][fall[1:]] = (g1[1:][fall[1:]] - att_c * g1[:-1][fall[1:]]) / (1 - att_c)
        g1 = gt
    return -20 * np.log10(np.clip(g1, 1e-6, None))
