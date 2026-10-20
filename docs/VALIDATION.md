# Validation

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
| Unit tests (`WBassXTests`, 28 checks, 2026-10-20: original path and its all-pass, Original Bass monitor pre-fader, Input trim stops at 0 dB, Audio = original + Harmonics × G, monitor pre-fader, Harmonics capped at +10.96 dB, Decay on the even harmonics, High-pass on the generated part, output clip, block-size invariance, channels independent, 96 kHz, processor and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBassX.vst3` (Windows, local, 2026-10-20) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Learned level control: C++ against the trained PyTorch model | −71 … −117 dB on most examples (−43 dB on a kick) |
| Sample rates: null at 96 kHz against 48 kHz (24 cases) | within 1.8 dB |
| Null test against the reference | [NULL_RESULTS_WBASSX.md](NULL_RESULTS_WBASSX.md): median −46.9 dB (whole output), −40.9 dB (generated part); not a null, see MEASUREMENTS.md "WBassX model" |

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
