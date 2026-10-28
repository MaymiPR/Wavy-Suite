# WVox Rider: null test against the reference

Residuals of WVox Rider's output against the reference Vocal Rider's, after the first 0.1 s, in dB (lower is better),
and the error of the applied gain where the input is loud enough to read it. The plug-in runs through its own
processor offline (`WVoxRiderRender`); the reference through its VST3 (WaveShell1-VST3 17.1, a fresh instance per
case). The vocal parts are held-out parts of the corpus (`nulltest/vocal_corpus.py` 24 .. 35: FL Studio's own vocal
phrases strung into 20 s parts with wandering levels, gaps and room noise); they were not used to identify the model.
How the model was found: MEASUREMENTS.md, "WVox Rider: Phase 3" and "Phase 4".

## The fixed cases (`nulltest/nulltest_wvoxrider.py`, 2026-10-28)

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steps 1 kHz | -74.0 | 0.001 | 0.003 |
| syllable train | -50.8 | 0.025 | 0.033 |
| vocal part 24 | -64.2 | 0.002 | 0.009 |
| vocal part 25, Fast | -56.9 | 0.002 | 0.088 |
| vocal part 26, range -12 .. +12 | -62.0 | 0.003 | 0.008 |
| vocal part 27, Target -30 | -63.0 | 0.003 | 0.009 |
| vocal part 28, Vocal Sensitivity +6 | -65.6 | 0.002 | 0.007 |
| vocal part 29, range -3 .. +9, Idle 0.67 | -48.3 | 0.004 | 0.105 |
| vocal part 30, Live, Spill -30 | -60.4 | 0.003 | 0.017 |
| vocal part 31, Live Fast, Spill -36 | -46.6 | 0.004 | 0.101 |
| vocal part 32, stereo (R 4 dB lower) | -60.3 | 0.003 | 0.009 |
| vocal part 33, 44.1 kHz | -63.5 | 0.003 | 0.008 |
| vocal part 34, 96 kHz, Fast | -53.2 | 0.005 | 0.037 |

## The model over all cached renders (`nulltest/vr_fit.py report`)

The same model (`nulltest/vrmodel.c`, identical to the plug-in's C++ to 0.0000 dB) against the 620 reference renders
of `nulltest/vr_ref.py`: 26 probes (steps, gaps, stops, tones, syllable trains) and the 36 corpus parts, each at ten
settings. Median null in dB per setting:

| set | all | default | Fast | VS +6 | VS -6 | Target -30 | Target -12 | ±12 | -3 .. +9 | Fast ±12 | Live, Spill -30 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| probes | -70.5 | -76.1 | -56.7 | -76.0 | -71.9 | -74.9 | -76.4 | -69.3 | -74.7 | -55.0 | -72.7 |
| corpus 0 .. 23 | -60.8 | -61.1 | -49.5 | -62.0 | -62.8 | -64.7 | -64.2 | -59.5 | -60.5 | -52.3 | -63.6 |
| corpus 24 .. 35 (held out) | -62.3 | -64.2 | -50.3 | -63.4 | -64.2 | -64.9 | -68.7 | -60.2 | -61.6 | -54.9 | -63.8 |

The worst case over all 620 is -40 dB (a training part at Fast). The residual left is mostly at word onsets at
Attack Fast, where the gain drops in about 10 ms and the two differ by a fraction of a dB for a few milliseconds.

## Not covered

- The **Music side-chain**: the reference used it in no host available offline (its Music knob stays disabled, as
  in Phase 3), so Music Sensitivity is not modelled and the plug-in rides without the music. To be tested in FL.
- Automation Read and Write, and grabbing the Rider fader, are host interactions; the unit tests check them, not
  this null test.
