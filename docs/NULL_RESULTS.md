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
| sine 100 Hz -20 | -12/off/0 | -49.8 | -49.8 | -40.6 |
| sine 100 Hz -20 | -24/off/0 | -29.8 | -31.6 | -22.1 |
| sine 100 Hz -20 | -36/off/0 | -24.1 | -27.6 | -17.3 |
| sine 100 Hz -20 | -24/-40/0 | -29.8 | -31.6 | -22.1 |
| sine 100 Hz -20 | -24/off/-6 | -29.8 | -31.6 | -22.1 |
| sine 100 Hz -20 | -12/-30/-3 | -49.8 | -49.8 | -40.6 |
| sine 8 kHz -20 | 0/off/0 | -216.9 | -216.9 | -218.7 |
| sine 8 kHz -20 | -12/off/0 | -109.4 | -109.4 | -72.4 |
| sine 8 kHz -20 | -24/off/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -36/off/0 | -101.8 | -101.8 | -61.3 |
| sine 8 kHz -20 | -24/-40/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -24/off/-6 | -102.6 | -102.6 | -64.3 |
| sine 8 kHz -20 | -12/-30/-3 | -109.3 | -109.3 | -72.4 |
| log sweep -20 | 0/off/0 | -216.9 | -216.9 | -219.9 |
| log sweep -20 | -12/off/0 | -57.7 | -94.5 | -42.9 |
| log sweep -20 | -24/off/0 | -35.7 | -82.8 | -22.6 |
| log sweep -20 | -36/off/0 | -29.0 | -79.5 | -15.8 |
| log sweep -20 | -24/-40/0 | -35.7 | -82.8 | -22.6 |
| log sweep -20 | -24/off/-6 | -35.7 | -82.8 | -22.6 |
| log sweep -20 | -12/-30/-3 | -57.7 | -94.5 | -42.9 |
| tone steps -50/-10/-30 | 0/off/0 | -222.5 | -206.9 | -229.9 |
| tone steps -50/-10/-30 | -12/off/0 | -64.6 | -47.7 | -54.1 |
| tone steps -50/-10/-30 | -24/off/0 | -36.9 | -25.1 | -29.3 |
| tone steps -50/-10/-30 | -36/off/0 | -22.9 | -14.4 | -18.1 |
| tone steps -50/-10/-30 | -24/-40/0 | -36.9 | -25.1 | -29.3 |
| tone steps -50/-10/-30 | -24/off/-6 | -36.9 | -25.1 | -29.3 |
| tone steps -50/-10/-30 | -12/-30/-3 | -64.5 | -47.7 | -54.1 |
| AM 5 Hz 3 kHz | 0/off/0 | -218.1 | -218.1 | -225.0 |
| AM 5 Hz 3 kHz | -12/off/0 | -59.4 | -58.2 | -48.8 |
| AM 5 Hz 3 kHz | -24/off/0 | -23.3 | -22.5 | -16.1 |
| AM 5 Hz 3 kHz | -36/off/0 | -13.0 | -12.8 | -7.4 |
| AM 5 Hz 3 kHz | -24/-40/0 | -23.3 | -22.5 | -16.1 |
| AM 5 Hz 3 kHz | -24/off/-6 | -23.3 | -22.5 | -16.1 |
| AM 5 Hz 3 kHz | -12/-30/-3 | -59.4 | -58.2 | -48.8 |
| white noise -20 rms | 0/off/0 | -219.9 | -219.9 | -233.7 |
| white noise -20 rms | -12/off/0 | -25.0 | -24.4 | -18.2 |
| white noise -20 rms | -24/off/0 | -13.2 | -12.6 | -9.2 |
| white noise -20 rms | -36/off/0 | -10.1 | -9.5 | -6.8 |
| white noise -20 rms | -24/-40/0 | -13.2 | -12.6 | -9.2 |
| white noise -20 rms | -24/off/-6 | -13.2 | -12.6 | -9.2 |
| white noise -20 rms | -12/-30/-3 | -25.0 | -24.4 | -18.2 |
| pink noise -20 rms | 0/off/0 | -220.0 | -220.3 | -232.3 |
| pink noise -20 rms | -12/off/0 | -27.9 | -27.1 | -19.4 |
| pink noise -20 rms | -24/off/0 | -13.4 | -13.7 | -9.0 |
| pink noise -20 rms | -36/off/0 | -9.3 | -10.1 | -6.1 |
| pink noise -20 rms | -24/-40/0 | -13.4 | -13.7 | -9.0 |
| pink noise -20 rms | -24/off/-6 | -13.4 | -13.7 | -9.0 |
| pink noise -20 rms | -12/-30/-3 | -27.9 | -27.1 | -19.4 |
| noise bursts | 0/off/0 | -220.3 | -220.8 | -243.9 |
| noise bursts | -12/off/0 | -43.1 | -42.7 | -29.9 |
| noise bursts | -24/off/0 | -27.8 | -27.2 | -18.0 |
| noise bursts | -36/off/0 | -18.3 | -17.5 | -10.4 |
| noise bursts | -24/-40/0 | -27.8 | -27.2 | -18.0 |
| noise bursts | -24/off/-6 | -27.8 | -27.2 | -18.0 |
| noise bursts | -12/-30/-3 | -43.1 | -42.7 | -29.9 |
| voice-like | 0/off/0 | -219.6 | -220.1 | -232.7 |
| voice-like | -12/off/0 | -51.5 | -50.4 | -42.0 |
| voice-like | -24/off/0 | -30.7 | -29.5 | -20.9 |
| voice-like | -36/off/0 | -20.6 | -19.3 | -12.7 |
| voice-like | -24/-40/0 | -30.7 | -29.5 | -20.9 |
| voice-like | -24/off/-6 | -30.7 | -29.5 | -20.9 |
| voice-like | -12/-30/-3 | -51.5 | -50.4 | -42.0 |
| sine 1 kHz -20 @44100 | -24/off/0 | -100.8 | -99.6 | -70.4 |
| sine 1 kHz -20 @44100 | -12/-40/-3 | -112.6 | -108.0 | -72.9 |
| tone steps @44100 | -24/off/0 | -35.2 | -24.3 | -29.0 |
| tone steps @44100 | -12/-40/-3 | -61.6 | -52.8 | -45.1 |
| white noise @44100 | -24/off/0 | -13.4 | -13.4 | -9.3 |
| white noise @44100 | -12/-40/-3 | -25.1 | -24.9 | -18.4 |
| sine 1 kHz -20 @96000 | -24/off/0 | -104.1 | -100.7 | -65.6 |
| sine 1 kHz -20 @96000 | -12/-40/-3 | -109.5 | -105.2 | -69.2 |
| tone steps @96000 | -24/off/0 | -35.2 | -24.3 | -29.0 |
| tone steps @96000 | -12/-40/-3 | -61.6 | -52.9 | -45.1 |
| white noise @96000 | -24/off/0 | -13.7 | -14.6 | -9.9 |
| white noise @96000 | -12/-40/-3 | -24.9 | -25.1 | -16.9 |
| sine 1 kHz -20 @192000 | -24/off/0 | -99.2 | -96.9 | -62.4 |
| sine 1 kHz -20 @192000 | -12/-40/-3 | -110.6 | -105.4 | -69.9 |
| tone steps @192000 | -24/off/0 | -35.2 | -24.3 | -29.0 |
| tone steps @192000 | -12/-40/-3 | -61.7 | -53.5 | -45.0 |
| white noise @192000 | -24/off/0 | -14.5 | -15.8 | -10.8 |
| white noise @192000 | -12/-40/-3 | -24.7 | -25.5 | -17.5 |
| stereo: L -10 / R -40 tones | -24/off/0 | -107.8 | -105.2 | -68.8 |
| stereo: L -10 / R -40 tones | -12/-40/-3 | -109.4 | -105.6 | -67.5 |
| stereo: noise L -20 / R -30 | -24/off/0 | -13.1 | -12.8 | -9.6 |
| stereo: noise L -20 / R -30 | -12/-40/-3 | -25.1 | -25.0 | -17.6 |
| stereo: alternating loud channel | -24/off/0 | -99.4 | -98.4 | -65.8 |
| stereo: alternating loud channel | -12/-40/-3 | -105.6 | -103.1 | -67.9 |

Median -57.7 dB, best -230.9 dB, worst -9.3 dB; 51 of 108 cases below -60 dB.

## Summary by signal

Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.

| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |
|---|---:|---:|---:|---:|
| sine 1 kHz -40 dBFS | -121.5 | -114.0 | -107.1 | -110.2 |
| sine 1 kHz -20 dBFS | -114.8 | -110.2 | -103.6 | -107.2 |
| sine 1 kHz -6 dBFS | -113.4 | -107.3 | -101.7 | -104.8 |
| sine 100 Hz -20 | -49.8 | -29.8 | -24.1 | -31.6 |
| sine 8 kHz -20 | -109.4 | -102.7 | -101.8 | -102.7 |
| log sweep -20 | -57.7 | -35.7 | -29.0 | -82.8 |
| tone steps -50/-10/-30 | -64.6 | -36.9 | -22.9 | -25.1 |
| AM 5 Hz 3 kHz | -59.4 | -23.3 | -13.0 | -22.5 |
| white noise -20 rms | -25.0 | -13.2 | -10.1 | -12.6 |
| pink noise -20 rms | -27.9 | -13.4 | -9.3 | -13.7 |
| noise bursts | -43.1 | -27.8 | -18.3 | -27.2 |
| voice-like | -51.5 | -30.7 | -20.6 | -29.5 |
| sine 1 kHz -20 @44100 | -112.6 | -106.7 | -100.8 | -103.8 |
| tone steps @44100 | -61.6 | -48.4 | -35.2 | -38.5 |
| white noise @44100 | -25.1 | -19.2 | -13.4 | -19.1 |
| sine 1 kHz -20 @96000 | -109.5 | -106.8 | -104.1 | -103.0 |
| tone steps @96000 | -61.6 | -48.4 | -35.2 | -38.6 |
| white noise @96000 | -24.9 | -19.3 | -13.7 | -19.9 |
| sine 1 kHz -20 @192000 | -110.6 | -104.9 | -99.2 | -101.2 |
| tone steps @192000 | -61.7 | -48.5 | -35.2 | -38.9 |
| white noise @192000 | -24.7 | -19.6 | -14.5 | -20.6 |
| stereo: L -10 / R -40 tones | -109.4 | -108.6 | -107.8 | -105.4 |
| stereo: noise L -20 / R -30 | -25.1 | -19.1 | -13.1 | -18.9 |
| stereo: alternating loud channel | -105.6 | -102.5 | -99.4 | -100.8 |

## How to read this

- **Steady tones match to numerical precision** (about -95 to -120 dB at 1 kHz and 8 kHz, at 44.1 to 192 kHz, mono and stereo). A null is claimed only for these stationary cases and for Compression 0 (a fixed gain).
- **Level changes are close but not a null**: steps and sweeps about -35 to -45 dB, a 100 Hz tone about -40 dB, amplitude modulation about -25 dB, noise bursts about -27 dB, white / pink noise about -20 dB, voice-like material about -21 dB. The detector, look-ahead and gate are identified exactly (see MEASUREMENTS.md, "Detector model"); the slow path of the compressor is a fitted model, so on dynamic material expect a small difference in how the gain moves, not in the tone.
- The gate matches to 0.0004 dB rms on level steps; with gate and compression together the residual follows the compressor numbers.
- WVox was tuned on the same kinds of signals (steps, bursts, AM, noise, voice-like, sweeps); these numbers are therefore not an independent test set.
