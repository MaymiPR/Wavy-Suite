# WVox null test results

Residual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after the first 0.2 s. 'steady' = last second only. Lower is better; about -60 dB or less is a close match, 0 dB is no match.

| signal | comp / gate / gain (dB) | residual all (dB) | residual steady (dB) | peak error (dB re. peak) |
|---|---|---:|---:|---:|
| sine 1 kHz -40 dBFS | 0/off/0 | -196.9 | -196.9 | -199.9 |
| sine 1 kHz -40 dBFS | -12/off/0 | -121.7 | -120.5 | -86.2 |
| sine 1 kHz -40 dBFS | -24/off/0 | -114.4 | -110.5 | -72.1 |
| sine 1 kHz -40 dBFS | -36/off/0 | -105.3 | -100.9 | -61.9 |
| sine 1 kHz -40 dBFS | -24/-40/0 | -107.2 | -106.2 | -72.2 |
| sine 1 kHz -40 dBFS | -24/off/-6 | -114.3 | -110.5 | -72.1 |
| sine 1 kHz -40 dBFS | -12/-30/-3 | -33.9 | -33.9 | -33.9 |
| sine 1 kHz -20 dBFS | 0/off/0 | -216.9 | -216.9 | -219.9 |
| sine 1 kHz -20 dBFS | -12/off/0 | -110.2 | -105.8 | -66.8 |
| sine 1 kHz -20 dBFS | -24/off/0 | -101.0 | -96.7 | -57.4 |
| sine 1 kHz -20 dBFS | -36/off/0 | -97.0 | -93.0 | -54.0 |
| sine 1 kHz -20 dBFS | -24/-40/0 | -101.0 | -96.7 | -57.4 |
| sine 1 kHz -20 dBFS | -24/off/-6 | -101.0 | -96.7 | -57.4 |
| sine 1 kHz -20 dBFS | -12/-30/-3 | -110.2 | -105.8 | -66.8 |
| sine 1 kHz -6 dBFS | 0/off/0 | -230.9 | -230.9 | -233.9 |
| sine 1 kHz -6 dBFS | -12/off/0 | -101.5 | -97.1 | -57.7 |
| sine 1 kHz -6 dBFS | -24/off/0 | -96.5 | -92.9 | -54.4 |
| sine 1 kHz -6 dBFS | -36/off/0 | -97.0 | -92.5 | -53.5 |
| sine 1 kHz -6 dBFS | -24/-40/0 | -96.5 | -92.9 | -54.4 |
| sine 1 kHz -6 dBFS | -24/off/-6 | -96.5 | -92.9 | -54.4 |
| sine 1 kHz -6 dBFS | -12/-30/-3 | -101.5 | -97.1 | -57.7 |
| sine 100 Hz -20 | 0/off/0 | -216.9 | -216.9 | -219.9 |
| sine 100 Hz -20 | -12/off/0 | -40.1 | -41.9 | -33.1 |
| sine 100 Hz -20 | -24/off/0 | -39.6 | -53.1 | -23.4 |
| sine 100 Hz -20 | -36/off/0 | -36.2 | -49.4 | -18.8 |
| sine 100 Hz -20 | -24/-40/0 | -39.6 | -53.1 | -23.4 |
| sine 100 Hz -20 | -24/off/-6 | -39.6 | -53.1 | -23.4 |
| sine 100 Hz -20 | -12/-30/-3 | -40.1 | -41.9 | -33.1 |
| sine 8 kHz -20 | 0/off/0 | -216.9 | -216.9 | -218.7 |
| sine 8 kHz -20 | -12/off/0 | -109.8 | -109.8 | -72.4 |
| sine 8 kHz -20 | -24/off/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -36/off/0 | -101.8 | -101.8 | -61.3 |
| sine 8 kHz -20 | -24/-40/0 | -102.7 | -102.7 | -64.4 |
| sine 8 kHz -20 | -24/off/-6 | -102.6 | -102.6 | -64.3 |
| sine 8 kHz -20 | -12/-30/-3 | -109.7 | -109.7 | -72.4 |
| log sweep -20 | 0/off/0 | -216.9 | -216.9 | -219.9 |
| log sweep -20 | -12/off/0 | -44.8 | -90.8 | -33.0 |
| log sweep -20 | -24/off/0 | -38.8 | -77.3 | -23.4 |
| log sweep -20 | -36/off/0 | -34.9 | -74.1 | -18.2 |
| log sweep -20 | -24/-40/0 | -38.8 | -77.3 | -23.4 |
| log sweep -20 | -24/off/-6 | -38.8 | -77.3 | -23.4 |
| log sweep -20 | -12/-30/-3 | -44.8 | -90.8 | -33.0 |
| tone steps -50/-10/-30 | 0/off/0 | -222.5 | -206.9 | -229.9 |
| tone steps -50/-10/-30 | -12/off/0 | -42.6 | -25.8 | -40.2 |
| tone steps -50/-10/-30 | -24/off/0 | -43.6 | -31.9 | -28.1 |
| tone steps -50/-10/-30 | -36/off/0 | -29.2 | -20.9 | -16.3 |
| tone steps -50/-10/-30 | -24/-40/0 | -43.7 | -31.9 | -38.0 |
| tone steps -50/-10/-30 | -24/off/-6 | -43.6 | -31.9 | -28.1 |
| tone steps -50/-10/-30 | -12/-30/-3 | -42.6 | -25.8 | -40.2 |
| AM 5 Hz 3 kHz | 0/off/0 | -218.1 | -218.1 | -225.0 |
| AM 5 Hz 3 kHz | -12/off/0 | -44.8 | -51.1 | -32.7 |
| AM 5 Hz 3 kHz | -24/off/0 | -24.8 | -24.7 | -17.1 |
| AM 5 Hz 3 kHz | -36/off/0 | -17.2 | -17.0 | -9.2 |
| AM 5 Hz 3 kHz | -24/-40/0 | -24.8 | -24.7 | -17.1 |
| AM 5 Hz 3 kHz | -24/off/-6 | -24.8 | -24.7 | -17.1 |
| AM 5 Hz 3 kHz | -12/-30/-3 | -44.8 | -51.1 | -32.7 |
| white noise -20 rms | 0/off/0 | -219.9 | -219.9 | -233.7 |
| white noise -20 rms | -12/off/0 | -28.4 | -28.3 | -19.8 |
| white noise -20 rms | -24/off/0 | -21.5 | -19.7 | -12.7 |
| white noise -20 rms | -36/off/0 | -17.8 | -15.8 | -11.1 |
| white noise -20 rms | -24/-40/0 | -21.5 | -19.7 | -12.7 |
| white noise -20 rms | -24/off/-6 | -21.5 | -19.7 | -12.7 |
| white noise -20 rms | -12/-30/-3 | -28.4 | -28.3 | -19.8 |
| pink noise -20 rms | 0/off/0 | -220.0 | -220.3 | -232.3 |
| pink noise -20 rms | -12/off/0 | -26.1 | -26.5 | -20.1 |
| pink noise -20 rms | -24/off/0 | -20.2 | -21.1 | -11.3 |
| pink noise -20 rms | -36/off/0 | -16.1 | -17.0 | -9.7 |
| pink noise -20 rms | -24/-40/0 | -20.2 | -21.1 | -11.3 |
| pink noise -20 rms | -24/off/-6 | -20.2 | -21.1 | -11.3 |
| pink noise -20 rms | -12/-30/-3 | -26.1 | -26.5 | -20.1 |
| noise bursts | 0/off/0 | -220.3 | -220.8 | -243.9 |
| noise bursts | -12/off/0 | -30.9 | -30.4 | -13.4 |
| noise bursts | -24/off/0 | -27.2 | -26.5 | -10.5 |
| noise bursts | -36/off/0 | -18.3 | -19.0 | -9.8 |
| noise bursts | -24/-40/0 | -27.2 | -26.5 | -10.5 |
| noise bursts | -24/off/-6 | -27.2 | -26.5 | -10.5 |
| noise bursts | -12/-30/-3 | -30.9 | -30.4 | -13.4 |
| voice-like | 0/off/0 | -219.6 | -220.1 | -232.7 |
| voice-like | -12/off/0 | -27.3 | -27.2 | -19.8 |
| voice-like | -24/off/0 | -20.6 | -20.4 | -14.9 |
| voice-like | -36/off/0 | -17.1 | -16.6 | -12.7 |
| voice-like | -24/-40/0 | -20.6 | -20.4 | -14.9 |
| voice-like | -24/off/-6 | -20.6 | -20.4 | -14.9 |
| voice-like | -12/-30/-3 | -27.3 | -27.2 | -19.8 |
| sine 1 kHz -20 @44100 | -24/off/0 | -97.9 | -94.0 | -59.2 |
| sine 1 kHz -20 @44100 | -12/-40/-3 | -108.7 | -103.5 | -68.3 |
| tone steps @44100 | -24/off/0 | -36.6 | -22.8 | -27.4 |
| tone steps @44100 | -12/-40/-3 | -44.8 | -26.4 | -41.9 |
| white noise @44100 | -24/off/0 | -23.0 | -23.3 | -14.8 |
| white noise @44100 | -12/-40/-3 | -27.5 | -27.6 | -21.1 |
| sine 1 kHz -20 @96000 | -24/off/0 | -100.1 | -95.3 | -59.4 |
| sine 1 kHz -20 @96000 | -12/-40/-3 | -108.7 | -104.1 | -68.8 |
| tone steps @96000 | -24/off/0 | -36.6 | -22.7 | -28.1 |
| tone steps @96000 | -12/-40/-3 | -44.8 | -26.4 | -41.9 |
| white noise @96000 | -24/off/0 | -22.5 | -23.0 | -11.4 |
| white noise @96000 | -12/-40/-3 | -28.7 | -29.9 | -16.6 |
| sine 1 kHz -20 @192000 | -24/off/0 | -98.3 | -95.1 | -60.5 |
| sine 1 kHz -20 @192000 | -12/-40/-3 | -110.6 | -105.5 | -69.9 |
| tone steps @192000 | -24/off/0 | -36.6 | -22.8 | -28.1 |
| tone steps @192000 | -12/-40/-3 | -44.8 | -26.4 | -41.9 |
| white noise @192000 | -24/off/0 | -23.5 | -24.5 | -15.0 |
| white noise @192000 | -12/-40/-3 | -30.6 | -32.7 | -20.5 |
| stereo: L -10 / R -40 tones | -24/off/0 | -98.2 | -93.9 | -54.7 |
| stereo: L -10 / R -40 tones | -12/-40/-3 | -103.2 | -98.9 | -59.5 |
| stereo: noise L -20 / R -30 | -24/off/0 | -21.0 | -19.0 | -12.1 |
| stereo: noise L -20 / R -30 | -12/-40/-3 | -27.8 | -29.3 | -18.1 |
| stereo: alternating loud channel | -24/off/0 | -94.1 | -90.4 | -53.2 |
| stereo: alternating loud channel | -12/-40/-3 | -100.9 | -97.0 | -59.6 |

Median -43.6 dB, best -230.9 dB, worst -16.1 dB; 45 of 108 cases below -60 dB.

## Summary by signal

Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.

| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |
|---|---:|---:|---:|---:|
| sine 1 kHz -40 dBFS | -121.7 | -110.8 | -33.9 | -108.3 |
| sine 1 kHz -20 dBFS | -110.2 | -101.0 | -97.0 | -96.7 |
| sine 1 kHz -6 dBFS | -101.5 | -96.8 | -96.5 | -92.9 |
| sine 100 Hz -20 | -40.1 | -39.6 | -36.2 | -51.2 |
| sine 8 kHz -20 | -109.8 | -102.7 | -101.8 | -102.7 |
| log sweep -20 | -44.8 | -38.8 | -34.9 | -77.3 |
| tone steps -50/-10/-30 | -43.7 | -43.1 | -29.2 | -28.9 |
| AM 5 Hz 3 kHz | -44.8 | -24.8 | -17.2 | -24.7 |
| white noise -20 rms | -28.4 | -21.5 | -17.8 | -19.7 |
| pink noise -20 rms | -26.1 | -20.2 | -16.1 | -21.1 |
| noise bursts | -30.9 | -27.2 | -18.3 | -26.5 |
| voice-like | -27.3 | -20.6 | -17.1 | -20.4 |
| sine 1 kHz -20 @44100 | -108.7 | -103.3 | -97.9 | -98.8 |
| tone steps @44100 | -44.8 | -40.7 | -36.6 | -24.6 |
| white noise @44100 | -27.5 | -25.2 | -23.0 | -25.5 |
| sine 1 kHz -20 @96000 | -108.7 | -104.4 | -100.1 | -99.7 |
| tone steps @96000 | -44.8 | -40.7 | -36.6 | -24.5 |
| white noise @96000 | -28.7 | -25.6 | -22.5 | -26.4 |
| sine 1 kHz -20 @192000 | -110.6 | -104.4 | -98.3 | -100.3 |
| tone steps @192000 | -44.8 | -40.7 | -36.6 | -24.6 |
| white noise @192000 | -30.6 | -27.1 | -23.5 | -28.6 |
| stereo: L -10 / R -40 tones | -103.2 | -100.7 | -98.2 | -96.4 |
| stereo: noise L -20 / R -30 | -27.8 | -24.4 | -21.0 | -24.1 |
| stereo: alternating loud channel | -100.9 | -97.5 | -94.1 | -93.7 |

## How to read this

- **Steady tones match to numerical precision** (about -95 to -120 dB at 1 kHz and 8 kHz, at 44.1 to 192 kHz, mono and stereo). A null is claimed only for these stationary cases and for Compression 0 (a fixed gain).
- **Level changes are close but not a null**: steps and sweeps about -35 to -45 dB, a 100 Hz tone about -40 dB, amplitude modulation about -25 dB, noise bursts about -27 dB, white / pink noise about -20 dB, voice-like material about -21 dB. The detector, look-ahead and gate are identified exactly (see MEASUREMENTS.md, "Detector model"); the slow path of the compressor is a fitted model, so on dynamic material expect a small difference in how the gain moves, not in the tone.
- The gate matches to 0.0004 dB rms on level steps; with gate and compression together the residual follows the compressor numbers.
- WVox was tuned on the same kinds of signals (steps, bursts, AM, noise, voice-like, sweeps); these numbers are therefore not an independent test set.
