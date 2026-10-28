# WVox Rider: the factory presets against the reference's own

Each of the reference's 13 presets loaded into its state (its stored values, hidden ones included), against
WVox Rider's preset of the same settings (Wavy names), both Mono, on held-out vocal parts of the corpus
(`nulltest/vocal_corpus.py`); the output residual after the first 0.1 s, dB (lower is better). Made by
`nulltest/nulltest_wvoxrider_presets.py`. The Music Sensitivity a preset stores has no effect on either
side here (no side-chain).

| preset | part 24 | part 27 | part 33 |
|---|---|---|---|
| Full Reset | -64.2 | -59.3 | -62.3 |
| Gentle Ride | -68.6 | -67.9 | -70.2 |
| Steady Lead | -70.7 | -71.7 | -81.2 |
| Quick Ride | -55.3 | -70.7 | -77.8 |
| Low Ride | -60.9 | -58.5 | -52.6 |
| High Ride | -45.7 | -55.4 | -52.2 |
| Big Swings | -70.7 | -65.7 | -70.9 |
| Loud Mix | -42.5 | -65.2 | -49.1 |
| Loud Lead | -41.6 | -70.7 | -78.4 |
| Auto Fader | -50.4 | -55.4 | -50.2 |
| Level Tamer | -54.7 | -64.2 | -62.2 |
| Ceiling Ride | -37.9 | -39.6 | -60.0 |
| Open Up | -59.8 | -37.9 | -61.4 |

Median -60.9 dB, worst -37.9 dB over 39 renders.
