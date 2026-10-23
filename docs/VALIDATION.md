# Validation

## WComp

| Check | Result |
|---|---|
| Unit tests (`WCompTests`, 609 checks, 2026-10-22: the 26 factory presets load and leave Mix alone; typed values on Threshold read as negative; latency at 44.1 … 192 kHz, idle identity, static curve and makeup, block-size invariance, stereo link, limiter Off / On ceiling, Mix 0, tempo sync and the long-note fallback, Analog Off identity, Analog noise levels and independent channels, Analog 4 crossover offset, processor: side-chain ignored, state round trip, Mode Mono) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WComp.vst3` (Windows, local, 2026-10-22, after the release refit) | SUCCESS, exit code 0 |
| The built VST3 (hosted by `tools/RefRender`) against the offline renderer, 5 signals × 5 settings, sync BPM / Host / no host tempo (`nulltest/check_wcomp_plugin.py`) | identical (−85 … −230 dB; the larger values from Release 100.25 against 100) |
| Analog stage: C++ against the fitted model | the difference is the added noise alone (−82.9 … −109.3 dBFS) |
| Null test against the reference | [NULL_RESULTS_WCOMP.md](NULL_RESULTS_WCOMP.md): median −39.6 dB over 168 cases (release law refitted by CMA-ES, 2026-10-22; before −37.2); defaults (Analog Off) voice-like −62.5, bass −58.5, drums −31.7, noise −24; not a null on noise and deep slow settings, see MEASUREMENTS.md "WComp model" |
| Factory presets against the reference (each preset on voice-like, drum loop, bass line, pink noise; Analog Off and the preset's mode) | [NULL_RESULTS_WCOMP_PRESETS.md](NULL_RESULTS_WCOMP_PRESETS.md): median −30.4 dB (Analog Off); weakest on deep, slow presets |
| VU needle against the reference | not measured (the reference's editor does not paint while the desktop is locked) |

## WVox

| Check | Result |
|---|---|
| Unit tests (`WVoxTests`, 332 checks: latency, static law, gate, gate and compressor together, attack / release, finite output on extreme input, block-size invariance, stereo link, processor parameters, state round trip, Mono mode, factory presets, learned slow path leaves steady tones and Compression 0 exactly as without it) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WVox.vst3` (Windows, local, 2026-10-18) | SUCCESS, exit code 0; its optional "vst3 validator" step was skipped (no Steinberg validator path set) |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Meters against the reference's editor (`tools/MeterCapture`, bass line + kicks, Compression −12 dB): bar height difference | Energy, Output, Gain reduction within 2.0 / 2.3 / 2.0 px on 90 % of frames |
| Null test against the reference | [NULL_RESULTS.md](NULL_RESULTS.md) (signal set) and [NULL_GRID.md](NULL_GRID.md) (672 settings) |
| Learned slow path: C++ against the trained PyTorch model on the null-test signals | −70 dB or lower |
| Learned slow path at 44.1 / 96 kHz (5 signals × 3 settings) | same gains as at 48 kHz; steady tones unchanged |
| CI on version tags (or started by hand): build, unit tests and pluginval on Windows / macOS / Linux, auval on macOS, one installer per OS | `.github/workflows/release.yml` |

## WBassX

| Check | Result |
|---|---|
| Unit tests (`WBassXTests`, 67 checks, 2026-10-20: the identified structure against the reference's own measurements (generated part at rest, 4 settings, 48 / 44.1 / 96 kHz, within 0.15 dB; Decay feedback h2 / h3 within 0.3 / 0.6 dB), original path and its all-pass, Original Bass monitor pre-fader, Input trim stops at 0 dB, Audio = original + Harmonics × G, monitor pre-fader, Harmonics capped at +10.96 dB, Decay on the even harmonics, High-pass on the generated part, output clip, block-size invariance, channels independent, 96 kHz, processor and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBassX.vst3` (Windows, local, 2026-10-20) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Factory presets (22): values from the reference's presets converted by the reference itself (16; the other 6 by the same scales, which reproduce all 16 exactly); Input untouched on preset load | unit tests pass |
| Value entry against the reference (MeterCapture `dbl` / `click` scripts: double-click and type on a knob, a fader and Freq; single click on the value; Ctrl- and Alt-click) | same parameter values after every step; entry boxes match to the pixel |
| Output meters against the reference's editor (MeterCapture, −6 / −20 dBFS bursts) | bar height within 1 px on 88 % of frames, 3 px on 93 %; steady levels and the silence dot exact |
| Learned level control: C++ against the trained PyTorch model | −71 … −117 dB on most examples (−43 dB on a kick) |
| Sample rates: null at 96 kHz against 48 kHz (24 cases) | within 1.8 dB |
| Null test against the reference | [NULL_RESULTS_WBASSX.md](NULL_RESULTS_WBASSX.md): median −48.5 dB (whole output), −42.7 dB (generated part); random-settings grid (480 cases) −35.2 / −29.2 dB; not a null, see MEASUREMENTS.md "WBassX model" |

## WBass

| Check | Result |
|---|---|
| Unit tests (`WBassTests`, 29 checks: original path In / Out, latency 0, Intensity exact on the generated part, low-level generated gain, Decay switches the harmonic product, output clip, Gain trim, block-size invariance, silence, stereo, processor parameters and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBass.vst3` (Windows, local, 2026-10-18) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Meters against the reference's editor (`tools/MeterCapture`, bass line + kicks): bar height difference | within 3.5 px (0.7 dB) on 90 % of frames (Original Bass); output and harmonics within 8 px, the rest being the generated level |
| Learned gain block: C++ against the trained PyTorch model on held-out renders | −74 … −91 dB |
| Sample rates: null at 44.1 / 96 kHz against 48 kHz (5 signals) | within 0.2 dB |
| Null test against the reference | [NULL_RESULTS_WBASS.md](NULL_RESULTS_WBASS.md): median −36.8 dB (whole output), −31.5 dB (generated part); not a null, see MEASUREMENTS.md "WBass model" |
