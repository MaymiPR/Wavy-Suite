# WComp null test results

Residual = 20 log10(rms(WComp - reference) / rms(reference)), 48000 Hz, after the first 0.2 s. Analog Off unless stated; modes 1-3 add random noise, which no null can remove. Produced by `nulltest/nulltest_wcomp.py --out`.

| threshold | ratio | attack | release | punch | mix | output | limiter | analog | signal | residual (dB) |
|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | sine 1 kHz -40 dBFS | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | sine 1 kHz -20 dBFS | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | sine 1 kHz -6 dBFS | -62.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | sine 100 Hz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | sine 8 kHz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | log sweep -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | tone steps -50/-10/-30 | -57.2 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | AM 5 Hz 3 kHz | -67.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | white noise -20 rms | -26.7 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | pink noise -20 rms | -24.3 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | noise bursts | -27.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | voice-like | -62.3 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | drum loop | -20.7 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | Off | bass line | -58.6 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | sine 1 kHz -40 dBFS | -83.0 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | sine 1 kHz -20 dBFS | -51.9 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | sine 1 kHz -6 dBFS | -53.9 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | sine 100 Hz -20 | -26.7 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | sine 8 kHz -20 | -56.7 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | log sweep -20 | -27.0 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | tone steps -50/-10/-30 | -45.8 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | AM 5 Hz 3 kHz | -32.3 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | white noise -20 rms | -31.2 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | pink noise -20 rms | -31.8 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | noise bursts | -28.9 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | voice-like | -24.6 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | drum loop | -26.4 |
| -30 | 4 | 0.5 | 50 | 0 | 100 | -6 | Off | Off | bass line | -18.6 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | sine 1 kHz -40 dBFS | -99.2 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | sine 1 kHz -20 dBFS | -61.5 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | sine 1 kHz -6 dBFS | -55.7 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | sine 100 Hz -20 | -10.0 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | sine 8 kHz -20 | -67.8 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | log sweep -20 | -17.4 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | tone steps -50/-10/-30 | -44.3 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | AM 5 Hz 3 kHz | -16.0 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | white noise -20 rms | -16.2 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | pink noise -20 rms | -15.2 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | noise bursts | -32.4 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | voice-like | -9.6 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | drum loop | -15.7 |
| -24 | 10 | 20 | 300 | 3 | 100 | -6 | Off | Off | bass line | -13.9 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | sine 1 kHz -40 dBFS | -76.1 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | sine 1 kHz -20 dBFS | -78.4 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | sine 1 kHz -6 dBFS | -56.4 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | sine 100 Hz -20 | -51.8 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | sine 8 kHz -20 | -66.7 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | log sweep -20 | -49.1 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | tone steps -50/-10/-30 | -54.5 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | AM 5 Hz 3 kHz | -32.6 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | white noise -20 rms | -25.0 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | pink noise -20 rms | -23.9 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | noise bursts | -16.4 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | voice-like | -19.1 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | drum loop | -20.7 |
| -36 | 2 | 3 | 1000 | 1 | 100 | -12 | Off | Off | bass line | -21.7 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | sine 1 kHz -40 dBFS | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | sine 1 kHz -20 dBFS | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | sine 1 kHz -6 dBFS | -70.1 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | sine 100 Hz -20 | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | sine 8 kHz -20 | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | log sweep -20 | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | tone steps -50/-10/-30 | -63.2 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | AM 5 Hz 3 kHz | -90.3 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | white noise -20 rms | -65.0 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | pink noise -20 rms | -71.2 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | noise bursts | -29.4 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | voice-like | -73.1 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | drum loop | -40.4 |
| -12 | 50 | 1 | 30 | 10 | 100 | 0 | Off | Off | bass line | -71.6 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | sine 1 kHz -40 dBFS | -76.1 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | sine 1 kHz -20 dBFS | -73.6 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | sine 1 kHz -6 dBFS | -69.4 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | sine 100 Hz -20 | -20.6 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | sine 8 kHz -20 | -77.4 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | log sweep -20 | -36.2 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | tone steps -50/-10/-30 | -59.8 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | AM 5 Hz 3 kHz | -42.7 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | white noise -20 rms | -31.2 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | pink noise -20 rms | -34.5 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | noise bursts | -33.0 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | voice-like | -18.5 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | drum loop | -28.4 |
| -24 | 4 | 7 | 100 | 1 | 50 | 0 | Off | Off | bass line | -16.7 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | sine 1 kHz -40 dBFS | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | sine 1 kHz -20 dBFS | -84.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | sine 1 kHz -6 dBFS | -67.5 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | sine 100 Hz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | sine 8 kHz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | log sweep -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | tone steps -50/-10/-30 | -50.5 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | AM 5 Hz 3 kHz | -67.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | white noise -20 rms | -28.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | pink noise -20 rms | -27.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | noise bursts | -29.2 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | voice-like | -33.2 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | drum loop | -25.4 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | Off | Off | bass line | -34.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | sine 1 kHz -40 dBFS | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | sine 1 kHz -20 dBFS | -84.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | sine 1 kHz -6 dBFS | -67.5 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | sine 100 Hz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | sine 8 kHz -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | log sweep -20 | -84.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | tone steps -50/-10/-30 | -50.5 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | AM 5 Hz 3 kHz | -67.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | white noise -20 rms | -29.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | pink noise -20 rms | -26.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | noise bursts | -27.8 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | voice-like | -27.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | drum loop | -24.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 12 | On | Off | bass line | -31.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | sine 1 kHz -40 dBFS | -68.5 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | sine 1 kHz -20 dBFS | -74.5 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | sine 1 kHz -6 dBFS | -58.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | sine 100 Hz -20 | -16.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | sine 8 kHz -20 | -73.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | log sweep -20 | -31.6 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | tone steps -50/-10/-30 | -51.5 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | AM 5 Hz 3 kHz | -37.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | white noise -20 rms | -23.4 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | pink noise -20 rms | -27.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | noise bursts | -26.4 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | voice-like | -13.8 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | drum loop | -21.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 4 | bass line | -10.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | sine 1 kHz -40 dBFS | -38.3 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | sine 1 kHz -20 dBFS | -49.2 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | sine 1 kHz -6 dBFS | -49.0 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | sine 100 Hz -20 | -48.5 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | sine 8 kHz -20 | -57.8 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | log sweep -20 | -49.9 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | tone steps -50/-10/-30 | -49.1 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | AM 5 Hz 3 kHz | -54.7 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | white noise -20 rms | -26.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | pink noise -20 rms | -24.2 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | noise bursts | -27.4 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | voice-like | -48.4 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | drum loop | -20.6 |
| -18 | 3 | 7 | 100 | 1 | 100 | 0 | Off | 2 | bass line | -47.4 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | sine 1 kHz -40 dBFS | -60.0 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | sine 1 kHz -20 dBFS | -72.6 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | sine 1 kHz -6 dBFS | -58.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | sine 100 Hz -20 | -16.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | sine 8 kHz -20 | -67.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | log sweep -20 | -31.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | tone steps -50/-10/-30 | -51.5 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | AM 5 Hz 3 kHz | -37.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | white noise -20 rms | -23.4 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | pink noise -20 rms | -27.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | noise bursts | -26.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | voice-like | -13.8 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | drum loop | -21.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 1 | bass line | -10.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | sine 1 kHz -40 dBFS | -46.6 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | sine 1 kHz -20 dBFS | -28.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | sine 1 kHz -6 dBFS | -32.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | sine 100 Hz -20 | -16.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | sine 8 kHz -20 | -33.3 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | log sweep -20 | -28.9 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | tone steps -50/-10/-30 | -30.1 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | AM 5 Hz 3 kHz | -29.0 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | white noise -20 rms | -22.9 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | pink noise -20 rms | -26.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | noise bursts | -24.2 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | voice-like | -13.7 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | drum loop | -21.5 |
| -24 | 4 | 7 | 100 | 1 | 100 | 0 | Off | 3 | bass line | -10.8 |

median -37.2 dB, best -99.2, worst -9.6 (168 cases)
