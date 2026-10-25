# WDelay null test against H-Delay

`nulltest/nulltest_wdelay.py`: WDelay's DSP (`tools/WDelayRender`, the plug-in's own code) against the reference H-Delay,
rendered through `tools/RefRender`. Residual = 20 log10(rms(ours - reference) / rms(reference)) in dB, over 4 s of each
signal after a 3 s silent pre-roll. Columns: a drum loop, a bass line, a voice-like signal (left, and 0.8 of it on the
right). 48 kHz; Analog Off unless a case names a mode. The cases set Feedback at the input's start (see MEASUREMENTS.md
"WDelay model").

| case | drum loop | bass line | voice-like |
|---|---|---|---|
| default (Host 1/8D, Feedback 60) | -98.8 | -101.4 | -101.5 |
| ms 100, Feedback 0 | -101.1 | -104.3 | -104.4 |
| ms 250, Feedback 80 | -67.4 | -96.2 | -75.6 |
| Host 1/4 at 97 BPM, Mix 50 | -41.9 | -54.0 | -59.6 |
| ms 40, Feedback 150 | -85.9 | -86.2 | -84.4 |
| ms 120, HiPass 300, LoPass 4000 | -99.9 | -99.4 | -99.3 |
| ms 120, LoFi | -70.5 | -71.2 | -70.4 |
| Ping Pong, ms 150, Feedback 70 | -97.2 | -100.7 | -99.4 |
| Ping Pong right, Phase R | -98.2 | -100.5 | -100.7 |
| Depth 40, Rate 0.8 Hz, ms 30 | -4.7 | -14.4 | -16.4 |
| glide: ms 375 -> 120 in the input | -60.1 | -77.5 | -82.7 |
| Output -6, Mix 70, Analog 4 | -89.6 | -90.0 | -83.2 |
| Analog 1 (noise -103 dBFS) | -47.8 | -51.2 | -50.7 |
| Analog 2, the default (noise -86 dBFS) | -54.8 | -54.8 | -51.0 |
| Analog 3 (noise -106 dBFS) | -39.5 | -40.7 | -42.2 |
| Feedback 200, ms 60, LoPass 3000 | -94.0 | -87.3 | -91.6 |

Notes:
- The Analog modes 1-3 add random noise (the levels in the case names), which no null removes; mode 4 has none.
- Cases whose time was set less than a few time constants before the input are still gliding: in a glide's slow tail
  the reference and WDelay move to the next 1/32 phase of the interpolator on different samples ("ms 250, Feedback
  80", "Host 1/4 at 97 BPM"; both null at -100 dB once settled).
- LoFi: the interpolator's window is not quite Kaiser; LoFi's lower cutoff shows the difference.
- Modulation: the LFO's phase and the delay curve agree to a few samples, not to the sample.
- 44.1 kHz behaves as 48 kHz; at 96 kHz the passes round the feedback loop are not identified yet.
