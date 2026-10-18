# WVox null test results

Residual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after the first 0.2 s. 'steady' = last second only. Lower is better; about -60 dB or less is a close match, 0 dB is no match.

| signal | comp / gate / gain (dB) | residual all (dB) | residual steady (dB) | peak error (dB re. peak) |
|---|---|---:|---:|---:|
| sine 1 kHz -40 dBFS | 0/off/0 | -196.9 | -196.9 | -199.9 |
| sine 1 kHz -40 dBFS | -12/off/0 | -124.9 | -124.8 | -12.3 |
| sine 1 kHz -40 dBFS | -24/off/0 | -131.0 | -130.9 | -8.1 |
| sine 1 kHz -40 dBFS | -36/off/0 | -131.7 | -131.7 | -6.3 |
| sine 1 kHz -40 dBFS | -24/-40/0 | -64.2 | -64.1 | -1.2 |
| sine 1 kHz -40 dBFS | -24/off/-6 | -131.2 | -131.2 | -8.1 |
| sine 1 kHz -40 dBFS | -12/-30/-3 | -33.6 | -33.6 | -1.2 |
| sine 1 kHz -20 dBFS | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 1 kHz -20 dBFS | -12/off/0 | -114.7 | -114.7 | -13.7 |
| sine 1 kHz -20 dBFS | -24/off/0 | -118.2 | -118.2 | -10.2 |
| sine 1 kHz -20 dBFS | -36/off/0 | -112.4 | -112.4 | -9.2 |
| sine 1 kHz -20 dBFS | -24/-40/0 | -122.8 | -122.8 | -2.5 |
| sine 1 kHz -20 dBFS | -24/off/-6 | -118.7 | -118.7 | -10.2 |
| sine 1 kHz -20 dBFS | -12/-30/-3 | -107.3 | -107.3 | -1.6 |
| sine 1 kHz -6 dBFS | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 1 kHz -6 dBFS | -12/off/0 | -112.0 | -112.0 | -20.3 |
| sine 1 kHz -6 dBFS | -24/off/0 | -106.7 | -106.7 | -17.9 |
| sine 1 kHz -6 dBFS | -36/off/0 | -114.3 | -114.3 | -17.4 |
| sine 1 kHz -6 dBFS | -24/-40/0 | -106.7 | -106.7 | -4.0 |
| sine 1 kHz -6 dBFS | -24/off/-6 | -106.5 | -106.5 | -17.9 |
| sine 1 kHz -6 dBFS | -12/-30/-3 | -106.8 | -106.8 | -2.3 |
| sine 100 Hz -20 | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 100 Hz -20 | -12/off/0 | -39.7 | -41.4 | -31.1 |
| sine 100 Hz -20 | -24/off/0 | -39.1 | -52.9 | -20.5 |
| sine 100 Hz -20 | -36/off/0 | -34.7 | -63.7 | -14.5 |
| sine 100 Hz -20 | -24/-40/0 | -39.1 | -52.9 | -16.0 |
| sine 100 Hz -20 | -24/off/-6 | -39.1 | -52.9 | -20.5 |
| sine 100 Hz -20 | -12/-30/-3 | -39.7 | -41.4 | -16.0 |
| sine 8 kHz -20 | 0/off/0 | -110.6 | -110.6 | -110.6 |
| sine 8 kHz -20 | -12/off/0 | -86.9 | -82.9 | -6.3 |
| sine 8 kHz -20 | -24/off/0 | -78.7 | -74.7 | -3.8 |
| sine 8 kHz -20 | -36/off/0 | -75.7 | -71.6 | -3.3 |
| sine 8 kHz -20 | -24/-40/0 | -78.8 | -74.7 | -0.5 |
| sine 8 kHz -20 | -24/off/-6 | -78.7 | -74.7 | -3.8 |
| sine 8 kHz -20 | -12/-30/-3 | -86.7 | -82.9 | -0.2 |
| log sweep -20 | 0/off/0 | -110.5 | -110.5 | -110.4 |
| log sweep -20 | -12/off/0 | -45.1 | -82.4 | -33.3 |
| log sweep -20 | -24/off/0 | -40.1 | -81.6 | -22.9 |
| log sweep -20 | -36/off/0 | -36.4 | -77.2 | -16.4 |
| log sweep -20 | -24/-40/0 | -40.1 | -81.8 | -20.0 |
| log sweep -20 | -24/off/-6 | -40.1 | -81.6 | -22.9 |
| log sweep -20 | -12/-30/-3 | -45.1 | -82.3 | -19.1 |
| tone steps -50/-10/-30 | 0/off/0 | -110.5 | -110.5 | -110.6 |
| tone steps -50/-10/-30 | -12/off/0 | -42.6 | -25.8 | -32.9 |
| tone steps -50/-10/-30 | -24/off/0 | -43.2 | -31.5 | -27.4 |
| tone steps -50/-10/-30 | -36/off/0 | -29.2 | -20.8 | -18.2 |
| tone steps -50/-10/-30 | -24/-40/0 | -43.2 | -31.5 | -27.4 |
| tone steps -50/-10/-30 | -24/off/-6 | -43.2 | -31.5 | -27.4 |
| tone steps -50/-10/-30 | -12/-30/-3 | -39.4 | -25.7 | -7.1 |
| AM 5 Hz 3 kHz | 0/off/0 | -110.5 | -110.5 | -110.4 |
| AM 5 Hz 3 kHz | -12/off/0 | -32.3 | -32.6 | -11.8 |
| AM 5 Hz 3 kHz | -24/off/0 | -23.9 | -25.1 | -6.9 |
| AM 5 Hz 3 kHz | -36/off/0 | -20.1 | -22.8 | -5.0 |
| AM 5 Hz 3 kHz | -24/-40/0 | -23.9 | -25.1 | -2.1 |
| AM 5 Hz 3 kHz | -24/off/-6 | -23.9 | -25.1 | -6.9 |
| AM 5 Hz 3 kHz | -12/-30/-3 | -32.3 | -32.6 | -3.8 |
| white noise -20 rms | 0/off/0 | -110.5 | -110.5 | -110.6 |
| white noise -20 rms | -12/off/0 | -27.2 | -27.2 | -14.7 |
| white noise -20 rms | -24/off/0 | -21.6 | -20.7 | -9.6 |
| white noise -20 rms | -36/off/0 | -17.9 | -16.6 | -8.0 |
| white noise -20 rms | -24/-40/0 | -21.6 | -20.7 | -5.0 |
| white noise -20 rms | -24/off/-6 | -21.6 | -20.7 | -9.6 |
| white noise -20 rms | -12/-30/-3 | -27.2 | -27.2 | -6.6 |
| pink noise -20 rms | 0/off/0 | -110.5 | -110.5 | -110.6 |
| pink noise -20 rms | -12/off/0 | -23.1 | -23.1 | -14.6 |
| pink noise -20 rms | -24/off/0 | -22.2 | -24.5 | -10.5 |
| pink noise -20 rms | -36/off/0 | -19.2 | -22.0 | -8.0 |
| pink noise -20 rms | -24/-40/0 | -22.2 | -24.5 | -10.1 |
| pink noise -20 rms | -24/off/-6 | -22.2 | -24.5 | -10.5 |
| pink noise -20 rms | -12/-30/-3 | -23.1 | -23.1 | -8.1 |
| noise bursts | 0/off/0 | -110.5 | -110.5 | -110.6 |
| noise bursts | -12/off/0 | -21.6 | -21.8 | -13.8 |
| noise bursts | -24/off/0 | -17.1 | -17.3 | -11.2 |
| noise bursts | -36/off/0 | -12.1 | -12.4 | -8.0 |
| noise bursts | -24/-40/0 | -17.1 | -17.3 | -11.2 |
| noise bursts | -24/off/-6 | -17.1 | -17.3 | -11.2 |
| noise bursts | -12/-30/-3 | -21.7 | -22.0 | -2.8 |
| voice-like | 0/off/0 | -110.5 | -110.5 | -110.5 |
| voice-like | -12/off/0 | -24.0 | -24.0 | -20.4 |
| voice-like | -24/off/0 | -17.7 | -17.7 | -15.8 |
| voice-like | -36/off/0 | -14.5 | -14.6 | -7.8 |
| voice-like | -24/-40/0 | -17.7 | -17.7 | -15.8 |
| voice-like | -24/off/-6 | -17.7 | -17.7 | -15.8 |
| voice-like | -12/-30/-3 | -23.8 | -23.9 | -20.4 |

Median -40.1 dB, best -196.9 dB, worst -12.1 dB; 35 of 84 cases below -60 dB.

## Summary by signal

Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.

| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |
|---|---:|---:|---:|---:|
| sine 1 kHz -40 dBFS | -131.7 | -128.0 | -33.6 | -127.8 |
| sine 1 kHz -20 dBFS | -122.8 | -116.5 | -107.3 | -116.5 |
| sine 1 kHz -6 dBFS | -114.3 | -106.8 | -106.5 | -106.8 |
| sine 100 Hz -20 | -39.7 | -39.1 | -34.7 | -52.9 |
| sine 8 kHz -20 | -86.9 | -78.8 | -75.7 | -74.7 |
| log sweep -20 | -45.1 | -40.1 | -36.4 | -81.7 |
| tone steps -50/-10/-30 | -43.2 | -42.9 | -29.2 | -28.6 |
| AM 5 Hz 3 kHz | -32.3 | -23.9 | -20.1 | -25.1 |
| white noise -20 rms | -27.2 | -21.6 | -17.9 | -20.7 |
| pink noise -20 rms | -23.1 | -22.2 | -19.2 | -23.8 |
| noise bursts | -21.7 | -17.1 | -12.1 | -17.3 |
| voice-like | -24.0 | -17.7 | -14.5 | -17.7 |

## How to read this

- **Steady tones at 1 kHz and 8 kHz match to numerical precision** (-75 dB and below; mostly -105 to -130 dB): the static law, the look-ahead, the true-peak level reading and the fixed x0.99 are right. A perfect null is claimed only for these stationary cases, and for Compression 0 (which is a fixed gain).
- **Everything with level changes is only close, not a null**: steps and sweeps -30 to -45 dB, 100 Hz tone about -39 dB, amplitude modulation about -24 dB, noise and noise bursts about -17 to -22 dB, voice-like material about -18 dB. The reference's detector and release behaviour is only partly understood (see MEASUREMENTS.md, "Detector model"); WVox approximates it with a fitted model, so on dynamic programme material expect an audible difference in how the gain moves, not in the tone.
- The gate is a hard-knee 2:1 expander on the same detector; with a gate and compression together the gain is within about 0.3 dB of the reference around the threshold (the -40 dB tone with a -40 dB gate nulls to only -64 dB).
- WVox was tuned on the same kind of signals (steps, bursts, AM, noise, voice-like, sweeps); the numbers above are therefore not an independent test set.
