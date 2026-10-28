# WBass Rider: null test against the reference

Residuals of WBass Rider's output against the reference's, over the loud parts, in dB (lower is better). The plug-in
runs through its own processor offline (`WBassRiderRender`; the built VST3 gives the same samples); the reference
through its VST3 at 48 kHz unless stated. How the model was found: MEASUREMENTS.md, "WBass Rider: Phase 3" and
"Phase 4" (nine passes).

## The fixed cases (`nulltest/nulltest_wbassrider.py`)

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -41.5 | 0.074 | 0.074 |
| steps 1 kHz | -48.6 | 0.029 | 0.030 |
| bass line | -30.2 | 0.073 | 0.160 |
| bass line, Target -24 | -30.2 | 0.103 | 0.200 |
| bass line, Sensitivity 80 | -28.1 | 0.196 | 0.622 |
| bass line, Artifacts 20 | -30.3 | 0.103 | 0.185 |
| synthetic part 6 (ghost notes, gaps) | -34.5 | 0.097 | 0.388 |
| synthetic part 7, Fast | -18.8 | 0.123 | 4.430 |
| synthetic part 8, range -6 .. +6 | -34.5 | 0.050 | 0.347 |

## The bass line at every Sensitivity x Response

| Sensitivity | 0 | 25 | 50 | 75 | 100 |
|---|---|---|---|---|---|
| Slow | -30.2 | -30.6 | -30.2 | -28.0 | -25.3 |
| Fast | -30.3 | -30.5 | -29.3 | -28.0 | -26.2 |

Artifacts on the bass line: 0 / 30 / 50 -30.3; 68.2 -24.8; 100 -37.6 (Slow).

## A corpus of real bass parts (`nulltest/br_corpus_eval.py`)

36 twelve-second parts from FL Studio's own bass samples (acoustic, 80s pick, Ibanez, slap, Temptated, a picked
5-string, a pluck) and three synthetic voices (808 sub, filtered saw, slow swells): rhythms, accents, ghost notes,
rests, staccato, legato and ringing notes. 24 parts were used in the fit, 12 are held out. Median residual:

| setting | held-out parts | fitted parts |
|---|---|---|
| defaults | -24.7 | -23.6 |
| Sensitivity 0 | -24.4 | -23.9 |
| Sensitivity 75 | -21.6 | -21.4 |
| Sensitivity 100 | -22.9 | -18.3 |
| Fast | -24.0 | -19.6 |
| Fast, Sensitivity 25 | -19.6 | -19.4 |
| Fast, Sensitivity 100 | -23.2 | -18.5 |
| Artifacts 15 | -24.7 | -23.6 |
| Artifacts 85 | -21.2 | -20.5 |
| Target -24 | -23.2 | -24.1 |

The worst parts are -10 .. -17 dB. Nearly all of the remaining error is a note ridden by one side and not the
other (then the two hold 1 .. 4 dB apart until the next note), not a different gain law.

## Sample rates, components, stereo (`nulltest/validate_wbassrider.py`)

| case | residual (dB) |
|---|---|
| bass line at 44.1 kHz | -30.8 |
| bass line at 96 kHz | -30.4 |
| Live Mono (5 ms look-ahead), bass line | -26.2 |
| Live Mono, Fast | -25.7 |
| Stereo, two different bass parts on L / R (one gain, from the mid) | -27.0 |
| Live Stereo, the same | -26.8 |

## Factory presets

The reference's 17, set on both: median -24.8 dB, see [NULL_RESULTS_WBASSRIDER_PRESETS.md](NULL_RESULTS_WBASSRIDER_PRESETS.md).
