# Validation

## WVox

| Check | Result |
|---|---|
| Unit tests (`WVoxTests`, 61 checks: latency, static law, gate, gate and compressor together, attack / release, finite output on extreme input, block-size invariance, stereo link, processor parameters, state round trip, Mono mode) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WVox.vst3` (Windows, local, 2026-10-17) | SUCCESS, exit code 0; its optional "vst3 validator" step was skipped (no Steinberg validator path set) |
| Null test against the reference | [NULL_RESULTS.md](NULL_RESULTS.md) (signal set) and [NULL_GRID.md](NULL_GRID.md) (672 settings) |
| CI on version tags (or started by hand): build, unit tests and pluginval on Windows / macOS / Linux, auval on macOS, one installer per OS | `.github/workflows/release.yml` |

## WBass

| Check | Result |
|---|---|
| Unit tests (`WBassTests`, 29 checks: original path In / Out, latency 0, Intensity exact on the generated part, low-level generated gain, Decay switches the harmonic product, output clip, Gain trim, block-size invariance, silence, stereo, processor parameters and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBass.vst3` (Windows, local, 2026-10-18) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Null test against the reference | [NULL_RESULTS_WBASS.md](NULL_RESULTS_WBASS.md): median −28.9 dB (whole output), −23.2 dB (generated part); not a null, see MEASUREMENTS.md "WBass model" |
