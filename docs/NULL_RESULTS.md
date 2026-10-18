# WVox null test results

Residual = 20 log10(rms(WVox - reference) / rms(reference)), 48 kHz, after the first 0.2 s. 'steady' = last second only. Lower is better; about -60 dB or less is a close match, 0 dB is no match.

| signal | comp / gate / gain (dB) | residual all (dB) | residual steady (dB) | peak error (dB re. peak) |
|---|---|---:|---:|---:|
| sine 1 kHz -40 dBFS | 0/off/0 | -196.9 | -196.9 | -199.9 |
| sine 1 kHz -40 dBFS | -12/off/0 | -123.4 | -123.3 | -8.2 |
| sine 1 kHz -40 dBFS | -24/off/0 | -130.7 | -130.6 | -4.6 |
| sine 1 kHz -40 dBFS | -36/off/0 | -131.7 | -131.7 | -3.2 |
| sine 1 kHz -40 dBFS | -24/-40/0 | -64.2 | -64.1 | -0.2 |
| sine 1 kHz -40 dBFS | -24/off/-6 | -128.0 | -128.0 | -4.6 |
| sine 1 kHz -40 dBFS | -12/-30/-3 | -33.6 | -33.6 | -0.2 |
| sine 1 kHz -20 dBFS | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 1 kHz -20 dBFS | -12/off/0 | -114.7 | -114.7 | -9.4 |
| sine 1 kHz -20 dBFS | -24/off/0 | -118.2 | -118.2 | -6.4 |
| sine 1 kHz -20 dBFS | -36/off/0 | -111.6 | -111.6 | -5.5 |
| sine 1 kHz -20 dBFS | -24/-40/0 | -108.2 | -108.2 | -0.2 |
| sine 1 kHz -20 dBFS | -24/off/-6 | -118.7 | -118.7 | -6.4 |
| sine 1 kHz -20 dBFS | -12/-30/-3 | -120.1 | -120.1 | -0.2 |
| sine 1 kHz -6 dBFS | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 1 kHz -6 dBFS | -12/off/0 | -112.0 | -112.0 | -15.8 |
| sine 1 kHz -6 dBFS | -24/off/0 | -106.2 | -106.2 | -13.5 |
| sine 1 kHz -6 dBFS | -36/off/0 | -114.9 | -114.9 | -13.0 |
| sine 1 kHz -6 dBFS | -24/-40/0 | -106.1 | -106.1 | -0.3 |
| sine 1 kHz -6 dBFS | -24/off/-6 | -106.1 | -106.1 | -13.5 |
| sine 1 kHz -6 dBFS | -12/-30/-3 | -111.5 | -111.5 | -0.2 |
| sine 100 Hz -20 | 0/off/0 | -110.5 | -110.5 | -110.4 |
| sine 100 Hz -20 | -12/off/0 | -39.4 | -41.7 | -26.6 |
| sine 100 Hz -20 | -24/off/0 | -36.8 | -57.0 | -20.2 |
| sine 100 Hz -20 | -36/off/0 | -31.6 | -49.2 | -14.2 |
| sine 100 Hz -20 | -24/-40/0 | -36.8 | -56.9 | -13.3 |
| sine 100 Hz -20 | -24/off/-6 | -36.8 | -57.0 | -20.2 |
| sine 100 Hz -20 | -12/-30/-3 | -39.4 | -41.7 | -14.5 |
| sine 8 kHz -20 | 0/off/0 | -110.6 | -110.6 | -110.6 |
| sine 8 kHz -20 | -12/off/0 | -105.1 | -105.1 | -4.3 |
| sine 8 kHz -20 | -24/off/0 | -99.2 | -99.2 | -2.4 |
| sine 8 kHz -20 | -36/off/0 | -98.5 | -98.5 | -2.0 |
| sine 8 kHz -20 | -24/-40/0 | -100.1 | -100.1 | -0.3 |
| sine 8 kHz -20 | -24/off/-6 | -99.2 | -99.2 | -2.4 |
| sine 8 kHz -20 | -12/-30/-3 | -106.5 | -106.5 | -0.1 |
| log sweep -20 | 0/off/0 | -110.5 | -110.5 | -110.4 |
| log sweep -20 | -12/off/0 | -42.0 | -90.2 | -27.9 |
| log sweep -20 | -24/off/0 | -35.2 | -86.9 | -18.0 |
| log sweep -20 | -36/off/0 | -32.0 | -89.1 | -12.9 |
| log sweep -20 | -24/-40/0 | -35.2 | -91.1 | -18.0 |
| log sweep -20 | -24/off/-6 | -35.2 | -86.9 | -18.0 |
| log sweep -20 | -12/-30/-3 | -42.0 | -90.2 | -20.0 |
| tone steps -50/-10/-30 | 0/off/0 | -110.5 | -110.5 | -110.6 |
| tone steps -50/-10/-30 | -12/off/0 | -65.7 | -53.7 | -30.6 |
| tone steps -50/-10/-30 | -24/off/0 | -39.0 | -27.3 | -24.8 |
| tone steps -50/-10/-30 | -36/off/0 | -24.0 | -15.6 | -17.7 |
| tone steps -50/-10/-30 | -24/-40/0 | -38.4 | -27.3 | -14.8 |
| tone steps -50/-10/-30 | -24/off/-6 | -39.0 | -27.3 | -24.8 |
| tone steps -50/-10/-30 | -12/-30/-3 | -34.9 | -53.3 | -3.3 |
| AM 5 Hz 3 kHz | 0/off/0 | -110.5 | -110.5 | -110.4 |
| AM 5 Hz 3 kHz | -12/off/0 | -38.2 | -38.9 | -9.1 |
| AM 5 Hz 3 kHz | -24/off/0 | -26.6 | -25.8 | -4.7 |
| AM 5 Hz 3 kHz | -36/off/0 | -15.7 | -15.1 | -2.9 |
| AM 5 Hz 3 kHz | -24/-40/0 | -26.6 | -25.8 | -1.8 |
| AM 5 Hz 3 kHz | -24/off/-6 | -26.6 | -25.8 | -4.7 |
| AM 5 Hz 3 kHz | -12/-30/-3 | -31.6 | -31.9 | -3.7 |
| white noise -20 rms | 0/off/0 | -110.5 | -110.5 | -110.6 |
| white noise -20 rms | -12/off/0 | -31.2 | -31.9 | -12.6 |
| white noise -20 rms | -24/off/0 | -18.5 | -17.9 | -7.8 |
| white noise -20 rms | -36/off/0 | -14.5 | -14.1 | -6.3 |
| white noise -20 rms | -24/-40/0 | -18.5 | -17.9 | -7.8 |
| white noise -20 rms | -24/off/-6 | -18.5 | -17.9 | -7.8 |
| white noise -20 rms | -12/-30/-3 | -31.2 | -31.9 | -9.2 |
| pink noise -20 rms | 0/off/0 | -110.5 | -110.5 | -110.6 |
| pink noise -20 rms | -12/off/0 | -28.3 | -29.4 | -12.6 |
| pink noise -20 rms | -24/off/0 | -18.6 | -19.6 | -8.8 |
| pink noise -20 rms | -36/off/0 | -13.5 | -15.0 | -7.2 |
| pink noise -20 rms | -24/-40/0 | -18.6 | -19.6 | -8.8 |
| pink noise -20 rms | -24/off/-6 | -18.6 | -19.6 | -8.8 |
| pink noise -20 rms | -12/-30/-3 | -28.3 | -29.4 | -12.6 |
| noise bursts | 0/off/0 | -110.5 | -110.5 | -110.6 |
| noise bursts | -12/off/0 | -19.1 | -18.8 | -10.9 |
| noise bursts | -24/off/0 | -17.6 | -17.6 | -7.8 |
| noise bursts | -36/off/0 | -17.2 | -17.0 | -7.1 |
| noise bursts | -24/-40/0 | -17.1 | -17.3 | -1.5 |
| noise bursts | -24/off/-6 | -17.6 | -17.6 | -7.8 |
| noise bursts | -12/-30/-3 | -17.9 | -18.0 | -1.8 |
| voice-like | 0/off/0 | -110.5 | -110.5 | -110.5 |
| voice-like | -12/off/0 | -19.1 | -19.1 | -17.1 |
| voice-like | -24/off/0 | -13.8 | -13.9 | -10.9 |
| voice-like | -36/off/0 | -12.4 | -12.6 | -8.8 |
| voice-like | -24/-40/0 | -13.8 | -13.9 | -10.9 |
| voice-like | -24/off/-6 | -13.8 | -13.9 | -10.9 |
| voice-like | -12/-30/-3 | -18.9 | -18.9 | -17.1 |
| sine 1 kHz -20 @44100 | -24/off/0 | -111.3 | -111.9 | -5.9 |
| sine 1 kHz -20 @44100 | -12/-40/-3 | -116.7 | -117.5 | -0.1 |
| tone steps @44100 | -24/off/0 | -41.8 | -31.4 | -25.4 |
| tone steps @44100 | -12/-40/-3 | -38.6 | -38.6 | -9.3 |
| white noise @44100 | -24/off/0 | -18.4 | -19.3 | -7.1 |
| white noise @44100 | -12/-40/-3 | -30.0 | -32.3 | -11.9 |
| sine 1 kHz -20 @96000 | -24/off/0 | -123.9 | -123.9 | -6.0 |
| sine 1 kHz -20 @96000 | -12/-40/-3 | -138.0 | -137.8 | -0.2 |
| tone steps @96000 | -24/off/0 | -41.9 | -31.5 | -25.5 |
| tone steps @96000 | -12/-40/-3 | -38.7 | -38.3 | -9.4 |
| white noise @96000 | -24/off/0 | -18.4 | -20.8 | -10.6 |
| white noise @96000 | -12/-40/-3 | -30.5 | -33.1 | -14.0 |
| sine 1 kHz -20 @192000 | -24/off/0 | -103.7 | -103.7 | -5.8 |
| sine 1 kHz -20 @192000 | -12/-40/-3 | -116.0 | -116.0 | -0.2 |
| tone steps @192000 | -24/off/0 | -41.9 | -31.4 | -25.9 |
| tone steps @192000 | -12/-40/-3 | -38.7 | -38.3 | -9.4 |
| white noise @192000 | -24/off/0 | -19.3 | -21.9 | -4.7 |
| white noise @192000 | -12/-40/-3 | -32.1 | -35.5 | -8.2 |
| stereo: L -10 / R -40 tones | -24/off/0 | -109.7 | -109.7 | -10.0 |
| stereo: L -10 / R -40 tones | -12/-40/-3 | -118.1 | -118.1 | -0.2 |
| stereo: noise L -20 / R -30 | -24/off/0 | -18.2 | -17.6 | -8.7 |
| stereo: noise L -20 / R -30 | -12/-40/-3 | -30.8 | -32.6 | -12.9 |
| stereo: alternating loud channel | -24/off/0 | -110.3 | -110.4 | -7.7 |
| stereo: alternating loud channel | -12/-40/-3 | -112.0 | -111.8 | -0.2 |

Median -39.0 dB, best -196.9 dB, worst -12.4 dB; 46 of 108 cases below -60 dB.

## Summary by signal

Compression settings -12, -24, -36 dB (with and without gate / gain); residual over everything after 0.2 s, and over the last second only.

| signal | best (dB) | median (dB) | worst (dB) | median, last second (dB) |
|---|---:|---:|---:|---:|
| sine 1 kHz -40 dBFS | -131.7 | -125.7 | -33.6 | -125.7 |
| sine 1 kHz -20 dBFS | -120.1 | -116.5 | -108.2 | -116.5 |
| sine 1 kHz -6 dBFS | -114.9 | -108.8 | -106.1 | -108.8 |
| sine 100 Hz -20 | -39.4 | -36.8 | -31.6 | -53.0 |
| sine 8 kHz -20 | -106.5 | -99.7 | -98.5 | -99.7 |
| log sweep -20 | -42.0 | -35.2 | -32.0 | -89.7 |
| tone steps -50/-10/-30 | -65.7 | -38.7 | -24.0 | -27.3 |
| AM 5 Hz 3 kHz | -38.2 | -26.6 | -15.7 | -25.8 |
| white noise -20 rms | -31.2 | -18.5 | -14.5 | -17.9 |
| pink noise -20 rms | -28.3 | -18.6 | -13.5 | -19.6 |
| noise bursts | -19.1 | -17.6 | -17.1 | -17.6 |
| voice-like | -19.1 | -13.8 | -12.4 | -13.9 |
| sine 1 kHz -20 @44100 | -116.7 | -114.0 | -111.3 | -114.7 |
| tone steps @44100 | -41.8 | -40.2 | -38.6 | -35.0 |
| white noise @44100 | -30.0 | -24.2 | -18.4 | -25.8 |
| sine 1 kHz -20 @96000 | -138.0 | -130.9 | -123.9 | -130.9 |
| tone steps @96000 | -41.9 | -40.3 | -38.7 | -34.9 |
| white noise @96000 | -30.5 | -24.4 | -18.4 | -27.0 |
| sine 1 kHz -20 @192000 | -116.0 | -109.8 | -103.7 | -109.8 |
| tone steps @192000 | -41.9 | -40.3 | -38.7 | -34.8 |
| white noise @192000 | -32.1 | -25.7 | -19.3 | -28.7 |
| stereo: L -10 / R -40 tones | -118.1 | -113.9 | -109.7 | -113.9 |
| stereo: noise L -20 / R -30 | -30.8 | -24.5 | -18.2 | -25.1 |
| stereo: alternating loud channel | -112.0 | -111.2 | -110.3 | -111.1 |

## How to read this

- **Steady tones at 1 kHz and 8 kHz match to numerical precision** (-75 dB and below; mostly -105 to -130 dB): the static law, the look-ahead, the true-peak level reading and the fixed x0.99 are right. A perfect null is claimed only for these stationary cases, and for Compression 0 (which is a fixed gain).
- **Everything with level changes is only close, not a null**: steps and sweeps -30 to -45 dB, 100 Hz tone about -39 dB, amplitude modulation about -24 dB, noise and noise bursts about -17 to -22 dB, voice-like material about -18 dB. The reference's detector and release behaviour is only partly understood (see MEASUREMENTS.md, "Detector model"); WVox approximates it with a fitted model, so on dynamic programme material expect an audible difference in how the gain moves, not in the tone.
- The gate is a hard-knee 2:1 expander on the same detector; with a gate and compression together the gain is within about 0.3 dB of the reference around the threshold (the -40 dB tone with a -40 dB gate nulls to only -64 dB).
- WVox was tuned on the same kind of signals (steps, bursts, AM, noise, voice-like, sweeps); the numbers above are therefore not an independent test set.
