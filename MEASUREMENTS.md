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
3. **The gain follows the level through two stages.** A burst of tone after silence gets its full steady-state reduction at once (the instant gain), but the reduction that remains after the burst grows with the burst's length (a 2 ms burst at -10 dB leaves 0.07 dB of reduction, 50 ms 1.4 dB, 100 ms 3.2 dB, 200 ms 8.3 dB, 400 ms 14.6 dB of 15.0 dB), and the reduction stays at its full depth for about 20 ms after a long tone ends and then recovers as an exponential in the *linear gain* (a release time constant of about 440 - 490 ms; in dB it looks program dependent because of that). The identified structure: the applied gain is the smaller of the instant gain and a copy smoothed by two cascaded one-pole stages, each with its own attack (gain falling) and release (gain rising) time. Fitted to the burst data alone the stages are 93 / 15 ms and 83 / 439 ms (attack / release), error 0.5 dB; fitted to the whole set of test signals (`tune_wvox.py`) the first stage collapses to a few milliseconds and the second to 184 / 488 ms.
4. **Gate.** The expander uses the same construction on its gain (hard-knee 2:1 as measured in Phase 3) in front of the compressor, which sees the gated signal; around the threshold the combined gain is within 0.3 dB of the reference.
5. **Look-ahead.** The gain is applied 16 samples (0.33 ms) ahead of a level step, ramping in, and the audio is delayed by the reported 64 / 128 / 256 samples. The behaviour does not depend on the sample rate (44.1 - 192 kHz give the same residuals).
6. **Stereo.** One gain for both channels, driven by the louder one; checked with unequal channels and alternating loud channels.

### What is still not understood

- **Noise and speech-like material.** The reference reads Gaussian noise as about 3.5 - 3.8 times its RMS (a peak-like level over a window of several milliseconds), and the best window length for noise (about 10 ms at -20 dB, 4 ms at -30 dB) disagrees with the one that suits tone steps (about 1 - 3 ms). Three smoothing structures were fitted (smoothing the gain, the amplitude level, the dB level) and all stop at about the same score, so the missing piece is not which domain is smoothed; it is probably how the live level is formed from the signal over a few milliseconds, or an extra stage that has not been found.
- The exact time constants: the fitted values are the best fit over the signals in `tune_wvox.py`, not measurements.
- The unexposed meters and the hidden GateRel parameter.
