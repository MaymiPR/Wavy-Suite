# WVox validation

| Check | Result |
|---|---|
| Unit tests (`WVoxTests`, 55 checks: latency, static law, gate, attack/release, finite output on extreme input, block-size invariance, stereo link, processor parameters, state round trip, Mono mode) | all pass (Windows, local) |
| pluginval 1.x, strictness level 10, `--skip-gui-tests`, on `WVox.vst3` (Windows, local, 2026-10-17) | SUCCESS, exit code 0; its optional "vst3 validator" step was skipped (no Steinberg validator path set) |
| Null test against the reference | see [NULL_RESULTS.md](NULL_RESULTS.md); steady tones null, level-changing material is close but not a null |
| CI (Windows / macOS / Linux, VST3 + CLAP + AU, unit tests, pluginval) | workflow written in `.github/workflows/ci.yml`; **has not run yet** (no remote repository) |
