# WVox: measurements of the reference (Phase 3)

Everything here was measured on the user's own licensed install, for private analysis. Raw renders, CSV tables and plots
are in `measurements/rvox/` and are **not** part of the public repository (git-ignored); this file only records what the
data showed. Reproduce with `python -I nulltest/measure_rvox.py` (needs `pedalboard`, `numpy`, `scipy`, `matplotlib`).

## Method

- **Host:** `pedalboard` 0.9.26 loading *RVox Mono* / *RVox Stereo* from `WaveShell1-VST3 17.1_x64.vst3` (`nulltest/refhost.py`), 512-sample buffers, 48 kHz unless noted, gate off (`-Inf`), Gain 0 unless noted.
- **Signals:** 1 kHz sine bursts (steady state read from the last 0.4 s), 12 kHz sine for sub-millisecond envelopes, step signals, square / noise, impulses.
- **Host defect found and handled:** about 12 % of renders came back with a block of silence or shifted by up to 0.8 s (the shell dropping a buffer). `refhost.render` detects silence where the input is not silent and renders again; after that 120 repeats of one setting agreed to the 0.01 dB printed. Without the re-render the first static sweep had random dips of up to 300 dB.
- **Latency compensation:** pedalboard removes the reported latency, so impulse offsets are 0 by construction; the number to compare is the *reported* latency.

## Results

| Item | Measured | Product page / manual |
|---|---|---|
| Latency (reported) | 64 samples at 44.1 / 48 kHz, 128 at 88.2 / 96 kHz, 256 at 176.4 / 192 kHz | the same figures |
| Parameter maps | Compression: raw 0…1 → −36…0 dB linear; Gain: the same; Gate: raw 0…1 → −80…0 dB linear, raw 0 shows `-Inf dB` | matches ParamXML ranges |
| Output at Compression 0, Gain 0 | exactly −0.0873 dB (a fixed ×0.99) at every level and frequency; no distortion | none stated |
| Gain control | pure output trim: output peak = (level with Gain 0) + Gain, e.g. 0 dBFS in → −12.09 dBFS out at −12; **no ceiling was found**: at Compression 0, +12 dBFS in gives +11.91 dBFS out for every Gain setting, and a 2.0 impulse train comes out at 1.98 | manual: Gain is the limiter ceiling |
| Static transfer (1 kHz sine, peak level x) | each Compression c gives out = x + G(x, c) with G(−80) = −c − 0.09 dB (full make-up) and G(0 dBFS) = −0.09 dB for every c; a very soft, wide knee in between; see `static_summary` in the data: effective slope at −10 dBFS 0.76 / 0.52 / 0.31 / 0.18 / 0.09 / 0.05 for c = −6 … −36 in steps of 6 | "threshold and ratio on one fader" |
| Level where gain is 3 dB below its low-level value | c − 6 dB (for c ≤ −18; −6 at c = −6, −16 at c = −12 at 2 dB resolution) | n/a |
| Knee | no sharp knee at any setting; slope falls smoothly from 1 to its 0 dBFS value over about 50 dB | "soft-knee curve" |
| Attack | gain moves **ahead of the input step**, 0.3 ms before it, and is complete within 0.3 ms (12 kHz, 0.167 ms envelope); a 1 ms tone burst already gets the full steady-state gain (+8.80 dB at −10 dBFS, c = −24) | not stated |
| Release | slow, exponential-like and program dependent: time to 63 % of the move 424 ms (2.9 dB move), 384 ms (5.6 dB), 290 ms (14.5 dB), 240 ms (24.7 dB); 90 % in 970 / 920 / 767 / 634 ms | not stated |
| Detector | follows RMS rather than peak: a square wave and a sine of equal RMS get gains of 15.8 and 16.0 dB, equal peak 13.9 vs 16.0 dB; noise of the same RMS gets 10.1 dB (it has a 13 dB crest factor), so the detector window is short | not stated |
| Stereo | linked; both channels get the gain of the louder one (L −40 dB with R −10 dB: +8.80 dB each; L alone: +22.71 dB); the Mono and Stereo components give the same gain on identical input | n/a |
| Frequency response | flat, ±0.0 dB, 20 Hz … 20 kHz at Compression 0; with Compression −24 the gain is 0.04 dB higher at 20 Hz at −50 dB and 0.28 dB higher at 20 Hz at −20 dB (the detector follows the waveform at very low frequency) | n/a |
| THD | below −147 dB for 1 kHz at −40 … 0 dBFS in all four settings tried (numerical noise floor, the gain is applied smoothly); no harmonic content | n/a |
| Parameter smoothing | Compression and Gain changes take effect within one 0.167 ms envelope window of the block boundary (no ramp); a Gate change glides in about 1.3 ms (10 % at 0.3 ms, 99 % at 1.3 ms) | n/a |
| Gate | downward expander, **hard knee, 2:1**: gain = (level − threshold) below the threshold, 0 dB above, exact to 0.01 dB at 0.5 dB resolution around the threshold (gate −40 dB: −2.09 dB at −42 dB, −0.59 dB at −40.5 dB); attack immediate; release after a 20 dB downward step with the gate at −30 dB: 50 % at 479 ms, 63 % at 531 ms, 90 % at 666 ms (the hidden GateRel default is 130 ms; the relation is not understood yet) | manual: soft knee |

## Not measured, and why

- **Gain-reduction meter ballistics and the Energy / Output meter values:** the VST3 exposes only Bypass, Compression, Gate, Gain (plus generic MIDI controllers), no meter parameters, so the meters cannot be read through a host. The audio gain trajectory above is the closest proxy. Reading the meters would need screen capture of the editor.
- **Hidden GateRel:** not exposed by the VST3, so its effect cannot be separated from the fixed behaviour.
- **Limiter behaviour with the compressor engaged on peaky material:** only steady tones were driven through the compressor and limiter together; transients with compression active are untested.

## Differences from the product documentation

1. The manual calls the Gain control a limiter ceiling; measured, it is an exact post-trim and nothing limits at Compression 0.
2. The manual and product page call the gate knee soft; measured, it is a hard-knee 2:1 expander on the peak level of the signal.

## What this means for Phase 4 (implementation)

Look-ahead of about 1.3 ms (the reported 64 samples at 48 kHz), instant attack on the look-ahead, a slow program-dependent exponential release, an RMS-style detector with a short window, one linked gain for both channels, a smooth static law G(x, c) tabulated from `static_gain.csv`, a fixed ×0.99, and a hard-knee 2:1 gate. The static law has no simple closed form yet; the first implementation will interpolate the measured table.

---

## Detector model (Phase 4 identification)

Phase 4 needed more than the Phase 3 numbers, so further experiments (`nulltest/ident_*.py`, `fit_*.py`, `tune_wvox.py`) looked at what the reference reads as "level" and how its gain moves. Findings, in the order they were established:

1. **Closed-form static law.** On a dense grid (Compression 0 ... -36 dB in 1 dB steps, 1 kHz sine from -100 to +12 dBFS in 1 dB steps, 4181 cells) the gain of a steady sine of peak amplitude x is
   `gain = 0.99 * M / (1 + (M - 1) * x)` with `M = 10^(-Compression / 20)`.
   Median error 0.00002 dB, 99th percentile 0.0005 dB; 13 of 4181 cells differ by more than 0.05 dB (mostly host dropouts and -100 dB tones). So the compressor is a gain that falls with the detected amplitude along a hyperbola, no separate threshold, ratio or knee.
2. **The level reading is a four-phase true-peak detector, identified to 0.0006.** Isolated patterns of 1 - 8 samples in silence (`ident_kernel.py`) are read as the largest of |x| and the outputs of three parallel first-order all-passes `(a + z^-1) / (1 + a z^-1)` with `a = 0.1607, 0.3569, 0.6308` (fractional delays of about 3/4, 1/2 and 1/4 sample; the fit error over 190 patterns is 0.0006 in amplitude, against 0.17 for the sample peak and 0.05 for a fitted interpolation kernel). This is why steady sines of any frequency and phase read as their true peak, why square, saw and pulse signals read up to 1.5 times their sample peak, and why a single-sample impulse reads exactly 1.0.
3. **Slow path (fitted).** A burst after silence gets its full reduction at once, but the reduction left after the burst grows with its length (2 ms at -10 dB leaves 0.07 dB, 100 ms 3.2 dB, 400 ms 14.6 dB of 15.0 dB), and level steps on a DC carrier (`ident_slow_dc.py`) show a level-dependent onset: a step from -40 to -10 dB starts to charge after 5 ms, to -20 dB after 8 ms, to -30 dB after about 25 ms. WVox models this as a slow level path in dB: the peak over a 16 ms window, a one-pole stage (rise 35 ms, fall 0.75 ms), and a second stage with a 0.05 dB dead band (rise 68 ms, fall 488 ms); the applied gain is the smaller of the fast and the slow gain. The constants come from the DC steps (error 0.56 dB) refined on the whole test set (`tune_wvox.py`); this is the one part that is fitted rather than identified exactly.
4. **Gate (identified).** Hard-knee 2:1 below the threshold, on a level that rises at once and falls toward the live level as a one-pole in linear amplitude with 130 ms (the default of the hidden GateRel parameter); the gate gain passes through the same two moving averages as the look-ahead, so it opens just ahead of a rise. On DC level steps across the threshold (`ident_gate.py`) WVox matches to 0.0004 dB rms (worst 0.09 dB). The gate and the compressor detect the input in parallel and their gains multiply (steady tones below the gate threshold, Compression -12 and -36: within 0.01 dB).
5. **Look-ahead (identified).** The instant gain of each sample is held as a minimum over 65 samples and smoothed by two moving averages of 39 and 26 samples in the linear gain domain, with 64 samples of audio delay (`ident_ramp.py`, `fit_ramp2.py`: error 0.05 dB around single-sample and 24-sample events). The lengths scale with the latency at 88.2 - 192 kHz, and the residuals do not depend on the sample rate.
6. **Stereo.** One gain for both channels, driven by the louder one; checked with unequal channels and alternating loud channels.

### What is still not understood

- **Noise and speech-like material.** The reference reads Gaussian noise as about 3.5 - 3.8 times its RMS (a peak-like level over a window of several milliseconds), and the best window length for noise (about 10 ms at -20 dB, 4 ms at -30 dB) disagrees with the one that suits tone steps (about 1 - 3 ms). Three smoothing structures were fitted (smoothing the gain, the amplitude level, the dB level) and all stop at about the same score, so the missing piece is not which domain is smoothed; it is probably how the live level is formed from the signal over a few milliseconds, or an extra stage that has not been found.
- The exact time constants: the fitted values are the best fit over the signals in `tune_wvox.py`, not measurements.
- The unexposed meters and the hidden GateRel parameter.
