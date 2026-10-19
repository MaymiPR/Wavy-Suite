# WVox null test results

Residual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after the first 0.2 s. 'steady' = last second only. Lower is better; about -60 dB or less is a close match, 0 dB is no match.

| signal | comp / gate / gain (dB) | residual all (dB) | residual steady (dB) | peak error (dB re. peak) |
|---|---|---:|---:|---:|
| sine 1 kHz -40 dBFS | 0/off/0 | -196.9 | -196.9 | -199.9 |
| sine 1 kHz -40 dBFS | -12/off/0 | -121.5 | -120.3 | -85.9 |
| sine 1 kHz -40 dBFS | -24/off/0 | -116.3 | -112.7 | -73.9 |
| sine 1 kHz -40 dBFS | -36/off/0 | -112.0 | -107.7 | -69.1 |
| sine 1 kHz -40 dBFS | -24/-40/0 | -107.5 | -106.9 | -74.0 |
| sine 1 kHz -40 dBFS | -24/off/-6 | -116.1 | -112.6 | -73.9 |
| sine 1 kHz -40 dBFS | -12/-30/-3 | -107.1 | -107.0 | -86.0 |
| sine 1 kHz -20 dBFS | 0/off/0 | -216.9 | -216.9 | -219.9 |
| sine 1 kHz -20 dBFS | -12/off/0 | -114.8 | -110.4 | -71.3 |
| sine 1 kHz -20 dBFS | -24/off/0 | -110.2 | -107.2 | -70.1 |
| sine 1 kHz -20 dBFS | -36/off/0 | -103.6 | -102.0 | -66.9 |
| sine 1 kHz -20 dBFS | -24/-40/0 | -110.2 | -107.2 | -70.1 |
| sine 1 kHz -20 dBFS | -24/off/-6 | -110.1 | -107.2 | -70.1 |
| sine 1 kHz -20 dBFS | -12/-30/-3 | -114.8 | -110.4 | -71.3 |
| sine 1 kHz -6 dBFS | 0/off/0 | -230.9 | -230.9 | -233.9 |
| sine 1 kHz -6 dBFS | -12/off/0 | -112.8 | -108.3 | -70.0 |
| sine 1 kHz -6 dBFS | -24/off/0 | -101.8 | -101.3 | -70.6 |
| sine 1 kHz -6 dBFS | -36/off/0 | -113.4 | -109.0 | -70.9 |
| sine 1 kHz -6 dBFS | -24/-40/0 | -101.8 | -101.3 | -70.6 |
| sine 1 kHz -6 dBFS | -24/off/-6 | -101.7 | -101.2 | -70.6 |
| sine 1 kHz -6 dBFS | -12/-30/-3 | -112.8 | -108.3 | -70.0 |
| sine 100 Hz -20 | 0/off/0 | -216.9 | -216.9 | -219.9 |
| sine 100 Hz -20 | -12/off/0 | -51.7 | -52.1 | -41.8 |
| sine 100 Hz -20 | -24/off/0 | -47.8 | -48.9 | -34.7 |
| sine 100 Hz -20 | -36/off/0 | -51.1 | -63.7 | -28.1 |
| sine 100 Hz -20 | -24/-40/0 | -47.8 | -48.9 | -34.7 |
| sine 100 Hz -20 | -24/off/-6 | -47.8 | -48.9 | -34.7 |
| sine 100 Hz -20 | -12/-30/-3 | -51.7 | -52.1 | -41.8 |
| sine 8 kHz -20 | 0/off/0 | -216.9 | -216.9 | -218.7 |
| sine 8 kHz -20 | -12/off/0 | -109.4 | -109.4 | -72.4 |
| sine 8 kHz -20 | -24/off/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -36/off/0 | -101.8 | -101.8 | -61.3 |
| sine 8 kHz -20 | -24/-40/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -24/off/-6 | -102.6 | -102.6 | -64.3 |
| sine 8 kHz -20 | -12/-30/-3 | -109.3 | -109.3 | -72.4 |
| log sweep -20 | 0/off/0 | -216.9 | -216.9 | -219.9 |
| log sweep -20 | -12/off/0 | -58.7 | -94.5 | -43.1 |
| log sweep -20 | -24/off/0 | -53.1 | -84.4 | -33.5 |
| log sweep -20 | -36/off/0 | -50.1 | -82.0 | -25.1 |
| log sweep -20 | -24/-40/0 | -53.1 | -84.4 | -33.5 |
| log sweep -20 | -24/off/-6 | -53.1 | -84.4 | -33.5 |
| log sweep -20 | -12/-30/-3 | -58.7 | -94.5 | -43.1 |
| tone steps -50/-10/-30 | 0/off/0 | -222.5 | -206.9 | -229.9 |
| tone steps -50/-10/-30 | -12/off/0 | -62.4 | -45.6 | -39.7 |
| tone steps -50/-10/-30 | -24/off/0 | -36.9 | -25.2 | -28.3 |
| tone steps -50/-10/-30 | -36/off/0 | -22.9 | -14.4 | -18.1 |
| tone steps -50/-10/-30 | -24/-40/0 | -36.9 | -25.2 | -28.3 |
| tone steps -50/-10/-30 | -24/off/-6 | -36.9 | -25.2 | -28.3 |
| tone steps -50/-10/-30 | -12/-30/-3 | -62.4 | -45.6 | -39.7 |
| AM 5 Hz 3 kHz | 0/off/0 | -218.1 | -218.1 | -225.0 |
| AM 5 Hz 3 kHz | -12/off/0 | -49.4 | -48.2 | -37.6 |
| AM 5 Hz 3 kHz | -24/off/0 | -35.1 | -34.3 | -29.2 |
| AM 5 Hz 3 kHz | -36/off/0 | -19.3 | -18.5 | -14.6 |
| AM 5 Hz 3 kHz | -24/-40/0 | -35.1 | -34.3 | -29.2 |
| AM 5 Hz 3 kHz | -24/off/-6 | -35.1 | -34.3 | -29.2 |
| AM 5 Hz 3 kHz | -12/-30/-3 | -49.4 | -48.2 | -37.6 |
| white noise -20 rms | 0/off/0 | -219.9 | -219.9 | -233.7 |
| white noise -20 rms | -12/off/0 | -33.3 | -33.0 | -25.2 |
| white noise -20 rms | -24/off/0 | -27.6 | -25.2 | -19.8 |
| white noise -20 rms | -36/off/0 | -25.2 | -24.6 | -14.9 |
| white noise -20 rms | -24/-40/0 | -27.6 | -25.2 | -19.8 |
| white noise -20 rms | -24/off/-6 | -27.6 | -25.2 | -19.8 |
| white noise -20 rms | -12/-30/-3 | -33.3 | -33.0 | -25.2 |
| pink noise -20 rms | 0/off/0 | -220.0 | -220.3 | -232.3 |
| pink noise -20 rms | -12/off/0 | -35.3 | -35.7 | -26.4 |
| pink noise -20 rms | -24/off/0 | -26.1 | -25.1 | -18.4 |
| pink noise -20 rms | -36/off/0 | -22.6 | -23.3 | -16.5 |
| pink noise -20 rms | -24/-40/0 | -26.1 | -25.1 | -18.4 |
| pink noise -20 rms | -24/off/-6 | -26.1 | -25.1 | -18.4 |
| pink noise -20 rms | -12/-30/-3 | -35.3 | -35.7 | -26.4 |
| noise bursts | 0/off/0 | -220.3 | -220.8 | -243.9 |
| noise bursts | -12/off/0 | -44.8 | -44.2 | -32.9 |
| noise bursts | -24/off/0 | -33.1 | -32.3 | -21.5 |
| noise bursts | -36/off/0 | -25.6 | -24.9 | -15.4 |
| noise bursts | -24/-40/0 | -33.1 | -32.3 | -21.5 |
| noise bursts | -24/off/-6 | -33.1 | -32.3 | -21.5 |
| noise bursts | -12/-30/-3 | -44.8 | -44.2 | -32.9 |
| voice-like | 0/off/0 | -219.6 | -220.1 | -232.7 |
| voice-like | -12/off/0 | -52.2 | -51.2 | -43.2 |
| voice-like | -24/off/0 | -39.8 | -38.9 | -30.0 |
| voice-like | -36/off/0 | -29.5 | -29.0 | -18.8 |
| voice-like | -24/-40/0 | -39.8 | -38.9 | -30.0 |
| voice-like | -24/off/-6 | -39.8 | -38.9 | -30.0 |
| voice-like | -12/-30/-3 | -52.2 | -51.2 | -43.2 |
| sine 1 kHz -20 @44100 | -24/off/0 | -100.8 | -99.6 | -70.4 |
| sine 1 kHz -20 @44100 | -12/-40/-3 | -112.6 | -108.0 | -72.9 |
| tone steps @44100 | -24/off/0 | -35.4 | -24.3 | -29.0 |
| tone steps @44100 | -12/-40/-3 | -60.8 | -52.8 | -45.2 |
| white noise @44100 | -24/off/0 | -29.2 | -28.1 | -22.8 |
| white noise @44100 | -12/-40/-3 | -33.9 | -33.5 | -26.0 |
| sine 1 kHz -20 @96000 | -24/off/0 | -104.1 | -100.7 | -65.6 |
| sine 1 kHz -20 @96000 | -12/-40/-3 | -109.5 | -105.2 | -69.2 |
| tone steps @96000 | -24/off/0 | -35.4 | -24.3 | -29.0 |
| tone steps @96000 | -12/-40/-3 | -60.9 | -52.9 | -45.2 |
| white noise @96000 | -24/off/0 | -29.1 | -30.7 | -19.4 |
| white noise @96000 | -12/-40/-3 | -32.6 | -34.3 | -25.4 |
| sine 1 kHz -20 @192000 | -24/off/0 | -99.2 | -96.9 | -62.4 |
| sine 1 kHz -20 @192000 | -12/-40/-3 | -110.6 | -105.4 | -69.9 |
| tone steps @192000 | -24/off/0 | -35.4 | -24.3 | -29.0 |
| tone steps @192000 | -12/-40/-3 | -60.9 | -53.5 | -45.2 |
| white noise @192000 | -24/off/0 | -30.8 | -32.0 | -18.5 |
| white noise @192000 | -12/-40/-3 | -33.7 | -36.1 | -26.6 |
| stereo: L -10 / R -40 tones | -24/off/0 | -107.8 | -105.2 | -68.8 |
| stereo: L -10 / R -40 tones | -12/-40/-3 | -109.4 | -105.6 | -67.5 |
| stereo: noise L -20 / R -30 | -24/off/0 | -27.5 | -24.6 | -17.0 |
| stereo: noise L -20 / R -30 | -12/-40/-3 | -33.1 | -32.9 | -24.9 |
| stereo: alternating loud channel | -24/off/0 | -99.4 | -98.5 | -65.8 |
| stereo: alternating loud channel | -12/-40/-3 | -105.6 | -103.1 | -67.9 |

Median -53.1 dB, best -230.9 dB, worst -19.3 dB; 51 of 108 cases below -60 dB.

## Summary by signal

Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.

| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |
|---|---:|---:|---:|---:|
| sine 1 kHz -40 dBFS | -121.5 | -114.0 | -107.1 | -110.2 |
| sine 1 kHz -20 dBFS | -114.8 | -110.2 | -103.6 | -107.2 |
| sine 1 kHz -6 dBFS | -113.4 | -107.3 | -101.7 | -104.8 |
| sine 100 Hz -20 | -51.7 | -49.5 | -47.8 | -50.5 |
| sine 8 kHz -20 | -109.4 | -102.7 | -101.8 | -102.7 |
| log sweep -20 | -58.7 | -53.1 | -50.1 | -84.4 |
| tone steps -50/-10/-30 | -62.4 | -36.9 | -22.9 | -25.2 |
| AM 5 Hz 3 kHz | -49.4 | -35.1 | -19.3 | -34.3 |
| white noise -20 rms | -33.3 | -27.6 | -25.2 | -25.2 |
| pink noise -20 rms | -35.3 | -26.1 | -22.6 | -25.1 |
| noise bursts | -44.8 | -33.1 | -25.6 | -32.3 |
| voice-like | -52.2 | -39.8 | -29.5 | -38.9 |
| sine 1 kHz -20 @44100 | -112.6 | -106.7 | -100.8 | -103.8 |
| tone steps @44100 | -60.8 | -48.1 | -35.4 | -38.5 |
| white noise @44100 | -33.9 | -31.5 | -29.2 | -30.8 |
| sine 1 kHz -20 @96000 | -109.5 | -106.8 | -104.1 | -103.0 |
| tone steps @96000 | -60.9 | -48.1 | -35.4 | -38.6 |
| white noise @96000 | -32.6 | -30.9 | -29.1 | -32.5 |
| sine 1 kHz -20 @192000 | -110.6 | -104.9 | -99.2 | -101.2 |
| tone steps @192000 | -60.9 | -48.1 | -35.4 | -38.9 |
| white noise @192000 | -33.7 | -32.2 | -30.8 | -34.0 |
| stereo: L -10 / R -40 tones | -109.4 | -108.6 | -107.8 | -105.4 |
| stereo: noise L -20 / R -30 | -33.1 | -30.3 | -27.5 | -28.8 |
| stereo: alternating loud channel | -105.6 | -102.5 | -99.4 | -100.8 |

## How to read this

- **Steady tones match to numerical precision** (about -95 to -120 dB at 1 kHz and 8 kHz, at 44.1 to 192 kHz, mono and stereo). A null is claimed only for these stationary cases and for Compression 0 (a fixed gain).
- **Level changes are close but not a null** (medians over the settings): sweeps about -53 dB, a 100 Hz tone about -50 dB, voice-like material about -40 dB, steps about -37 dB (-48 dB at the other rates), amplitude modulation about -35 dB, noise bursts about -33 dB, white / pink noise about -26 to -32 dB; the worst case is -19 dB (AM at Compression -36). The detector, look-ahead and gate are identified exactly (see MEASUREMENTS.md, "Detector model"); the slow path is the fitted analytical path plus a small learned correction ("WVox learned slow path"), so on dynamic material expect a small difference in how the gain moves, not in the tone.
- The gate matches to 0.0004 dB rms on level steps; with gate and compression together the residual follows the compressor numbers.
- The learned correction was trained on separate random renders of the same kinds of material (voice, noise, tones, steps, AM, sweeps, drums), not on these signals; the analytical path was tuned on signals like these, so the numbers are not a fully independent test set.
