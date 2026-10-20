# WVox: measurements of the reference (Phase 3)

Everything here was measured on the user's own licensed install. The data (JSON, CSV tables, plots) is in
`measurements/rvox/`; large raw arrays (`.npz`) are left out and regenerate with the scripts. Reproduce with `python -I nulltest/measure_rvox.py` (needs `pedalboard`, `numpy`, `scipy`, `matplotlib`).

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

### WVox learned slow path (Phase 4, second round)

The reference's own gain can be read sample by sample (gate off: output / delayed input / fixed gain). Against it, the
identified fast path is an upper bound (the reference gain is above it on 0.0 - 0.1 % of the samples), and where the
two differ the reference compresses more than the fitted slow path. On dense noise at heavy compression it holds an
almost constant gain 2 - 3 dB below even the fast ratio (the gain of the larger peaks over a long window); on a
100 Hz tone it holds the fast ratio's value at the waveform peak with 0.02 dB ripple, where the fitted path sits
0.5 dB too high. A plain peak hold of the fast ratio fits the low tone but loses 10 - 20 dB elsewhere, so the slow path
is left to a small learned block, as for WBass:

- **Structure.** slow = max(fitted slow ratio x exp(c), floor); gain = min(fast, slow). c comes from a GRU (48 units)
  stepped every 1/12000 s (4 samples at 48 kHz, a phase accumulator at other rates) on: ln of the step's peak level,
  smallest fast ratio and last fitted slow ratio, ln M, running minima of the fast ratio over 1 - 20 ms and running
  maxima of the level over 10 - 300 ms. c is applied to the following samples, moving linearly between the last two
  finished steps (causal).
- **Two guards keep the identified cases exact.** The floor is the static-law ratio at the loudest level of the last
  second, so a steady signal can never be compressed below its fast ratio; and while the fast ratio stays flat for
  20 ms (a steady tone) the correction fades out over 20 ms. Without them the network cost 10 - 60 dB on steady
  sines and level steps that the fitted path already nulled.
- **Training.** 3600 random 2 s renders of RVox Mono (voice, speech-like noise, white / pink noise, tones 25 Hz -
  10 kHz, steps, AM, sweeps, drums, mixes; peak -60 ... 0 dBFS; Compression 0 ... -36 dB, gate off) with the WVox
  internals dumped by `WVoxRender dump=1` (`nulltest/render_trainset_wvox.py`); supervised on the reference's
  sample-wise gain first, then on the null residual itself, with the compression settings balanced
  (`nulltest/train_wvox.py`; exported by `tools/export_wvoxnet.py` to `plugins/WVox/WVoxNet.h`). Training straight on
  the null residual did not move (the slow ratio gets no gradient while it is above the fast ratio).
- **Result** (48 kHz unless noted; [docs/NULL_RESULTS.md](docs/NULL_RESULTS.md), [docs/NULL_GRID.md](docs/NULL_GRID.md)):
  worst case of the 672-case grid -10.1 -> -22.8 dB (median -71.4 -> -70.2 dB, 396 cases below -60 dB either way);
  null-test worst -9.3 -> -19.3 dB, 44 of 117 rows better by more than 1 dB, 4 worse. White / pink noise at
  Compression -24 / -36: -13 / -10 -> -27 / -24 dB (the same at 44.1 - 192 kHz and in stereo); voice-like -31 / -21 ->
  -40 / -30 dB; 100 Hz tone -30 / -24 -> -48 / -51 dB; sweeps -36 / -29 -> -49 / -48 dB. Worse: AM at Compression -12
  (-59 -> -49 dB) and level steps at light compression (grid median at -6 dB: -105 -> -88 dB). Steady tones unchanged.
- **Checks.** C++ against the trained model on the null-test signals: -70 dB or lower. At 44.1 and 96 kHz the gains
  are the same as at 48 kHz. Cost: about 3.5 % of one core for linked stereo (twice that in dual mono).
- This is a learned model of the reference's slow path, not an identification of it.

---

# WBass: measurements of the reference (Phase 3)

Measured on the user's own install; data and plots are in `measurements/rbass/`.
Reproduce with `python -I nulltest/measure_rbass.py`.

## Method

- **Host:** pedalboard loading *RBass Stereo* / *RBass Mono* from the WaveShell (`nulltest/refhost.py`, `render_rbass`), 512-sample buffers, 48 kHz.
- **Parameters the VST3 does not expose** (Original Bass In/Out, Response, Ratio, Decay, ...) are written into the plugin's own state: the IComponent chunk is a JUCE base64 block holding the Waves preset XML with the 18-value parameter array (`nulltest/wavesstate.py`). The shell ignores some otherwise valid states depending on the payload size, so every write is padded and verified by reading it back. Freq, Intensity and Gain are set as normal parameters. The Input parameter (index 0) could not be set this way (the shell kept 0), so it is not measured.
- **Signals:** sine tones (component amplitudes and phases by synchronous detection over whole periods), level steps, DC-free sweeps of the parameters.

## Results

| Item | Measured |
|---|---|
| Latency | 0 samples at 44.1, 48, 96 and 192 kHz (reported and measured), matching the product page |
| Gain | a pure output trim, exact to 0.01 dB (0 / −6 / −12 / −30 dB) |
| Above about 2 kHz | untouched (0.00 dB at 10 kHz); 0.17 dB at 1 kHz with Freq 160 |
| Original Bass **out** | the input below Freq is removed by a steep high-pass at Freq: with Freq 80 it is −24.5 dB at 20 Hz, −10 dB at 30 Hz, −2.2 dB at 40 Hz (Intensity 0; −34.8 / −25.9 / −22.3 dB with Intensity −24, i.e. about 24 dB/octave below Freq once the generated signal is turned down) |
| Original Bass **in** | the low end passes; a small dip just below Freq (−3 dB at 30 Hz with Freq 80) where the generated band and the original overlap |
| Generated band | a resonant boost just above Freq added to the signal: +13.4 dB at 82 Hz (Freq 80, Intensity 0, −30 dBFS tone), +12.2 dB at 168 Hz (Freq 160); with Intensity −24 it shrinks to +1.6 dB, so it is the generated path, not a fixed EQ |
| Intensity | close to a plain gain on the generated path: for a 50 Hz tone the generated fundamental is −23.2 / −1.0 / +12.5 dB re input at Intensity −12 / 0 / +12 |
| Harmonic content | mainly 2nd and 3rd harmonics, 4th and 5th 20–30 dB lower; for −20 dBFS tones with Freq 80 the 2nd harmonic is −14.8 dB re input at 50 Hz and −29.6 dB at 120 Hz; the generated path also carries a strong component at the input frequency itself (e.g. +8.4 dB at 80 Hz); content falls away above about 2 × Freq |
| Level dependence | strongly non-linear: the generated harmonics grow about 2 dB per dB at low level (−99.8 dB at −60 dBFS, −76.7 at −48, −55.8 at −36) and level off towards 1 dB per dB at high level (−25.6 at −12, −14.9 at 0 dBFS); the generated fundamental is compressed (−52.8 dB at −60 dBFS but −18.0 dB at −12 and −19.0 at 0 dBFS), so a level control acts on the generated signal |
| Dynamics | the generated part follows a level step within about 50–100 ms (attack) and after a step down overshoots briefly (the generated signal decays more slowly than the input), settling within about 100 ms |
| Hidden Ratio | sets how strongly the generated level follows the input: steady gain −24.4 / −6.6 / −5.1 dB for Ratio 0.25 / 0.63 / 1.0 (−10 dBFS step) |
| Hidden Decay | small effect (−7.4 vs −6.6 dB for 0 / 1); reads back only 0 or 1 |
| Hidden Response | no measurable effect on these tests (all within 0.001 dB for 0 / 5 / 15 / 30 ms) |
| Clipping | the output never exceeds 0 dBFS: inputs of 0 to +6 dBFS and Intensity +24 all peak at 0.0 dBFS (the "clip-free" stage of the product page) |
| Stereo | the generated bass is mono, from the sum of both channels: with signal only on the left the right output carries the generated part at −29 dB; a loud left channel also lowers the generated level of a quiet right channel (−74.6 → −92.7 dB 2nd harmonic), so the level control is shared |
| Smoothing | an Out Gain change glides in over about 25 ms (50 % at 14 ms, 90 % at 25 ms) |

## What this means for Phase 4

WBass needs: a high-pass at Freq for Original Bass Out (about 4th order); a band around Freq feeding a harmonic generator that makes a fundamental plus mostly 2nd and 3rd harmonics; a level control (ratio set by the hidden Ratio, 0.63 by default) with tens-of-milliseconds attack and release on the generated signal; Intensity as the gain of that path; the generated signal mono and added to both channels; a 0 dBFS safety clipper or limiter at the output; and the Out Gain trim. The exact filter shapes, the generator's transfer curve and phase, and the level detector are the identification work of Phase 4.

## WBass model (Phase 4 identification)

Scripts: `nulltest/ident_rbass_linear.py`, `ident_rbass_nl.py`, `ident_rbass_sweep.py`, `ident_rbass_k2.py`,
`wbass_fitdata.py`, `wbass_model.py` (Python model), data in `measurements/rbass/` (`linear_paths.json`, `k2_grid6.json`,
`levels_decay0.json`, `cs_vs_f0.json`, `am70.json`, `beat70.json`, ...). "F" is the Freq setting.

**Identified exactly** (residual given):

| Part | Finding |
|---|---|
| Original path | Original Bass Out: 3rd-order Butterworth high-pass at F; In: LP3(F) + HP3(F), i.e. a 2nd-order all-pass. Within −60 … −75 dB of a bilinear HP3 on the renders (a small extra low-frequency gain difference, below −57 dB, is not explained yet) |
| Generated path, low level | −C0 · LP3(F/3) · HP3(F) · HP3(F), C0 ≈ 290–300 (≈ +49.5 dB): log-magnitude error 0.0044 rms over F = 40…256 Hz |
| Harmonic product | a 2nd-order term (u · b) followed by HP3(F/3) and HP3(F), with u = LP3(F/3) HP3(F) x and b = LP3(F) x. Found from 190 two-tone kernels on a grid where many pairs share the sum frequency (free post-filter, free factors: 0.04 dB / 0.05° rms, then the closed form: 0.085 dB / 0.3° rms) |
| Hidden **Decay** | the gain of that product: Decay 0 removes the even harmonics completely (2nd harmonic −166 dB) and leaves the rest unchanged; it reads back only 0 or 1 |
| Intensity | an exact gain on the generated path; it is used to split the generated part off the reference in all null tests |
| Output stage | a hard clip to the 16-bit range, −1 … 32767/32768, no knee and no time constant (loud 1 kHz tones, where nothing is generated: the gain drops instantly on a +26 dB step and returns instantly) |
| Hidden **Ratio** | 1.0 and 0.63 give identical output; below about 0.5 the compression gets stronger (0.25: −5.95 dB at 0 dBFS). WBass keeps the parameter (presets use 0.63) but does not use it yet |

**Gain control of the generated path (second round of probing).** Measured directly, then fitted:
- *Where the gain acts:* solving for a free gain trajectory under each placement hypothesis (`nulltest/extract_cs.py`)
  and fitting complete models show it acts mostly after the first HP3(F), with about 5 % of it (in dB) ahead of it
  (`alpha`); a gain fully ahead of the high-passes gives the right 3rd harmonic on low notes but the wrong one near F,
  and the reverse when fully after.
- *Dynamics:* the gain trajectory through level steps of a 70 Hz tone (ratio of Hilbert envelopes, `dyn_env70.json`)
  shows a second-order, asymmetric response: about 10 ms of delay, then the gain settles within 30–40 ms after a rise
  and more slowly after a fall, without overshoot. Modelled as two attack / release followers in cascade.
- *Ripple:* the recovered gain over one period is a sawtooth locked to the zero crossings of LP3(F/3) x (it jumps at
  each zero crossing and slides down through the half-cycle); the first follower reproduces it by falling almost at
  once and rising slowly.
- *Level band:* a resonant low-pass near F (LP2 at 1.0 F with Q 1.35, times LP1 at 10 F): compression is strongest
  for notes just below F and lighter above it.
- *Freq dependence:* fitting the four Freq settings 40 / 80 / 160 / 256 together, the follower times scale as
  (80/F)^0.34 and (80/F)^0.19.
- *Generated-signal limiter ("clip-free"):* Intensity is an exact gain on the generated part (−147 dB) until it peaks
  near 0.7; above that a limiter on the generated signal alone holds it. Measured on the reference's own unlimited
  signal (its Intensity −24 render scaled up, which never reaches the limiter) against its limited one: the gain
  drops at once to what each sample needs to stay under 0.703 and recovers as a one-pole in the gain domain with
  28.4 ms (median −42 dB, kicks −32 … −43 dB; a level-domain release gave only −22 dB).
- *Two level detectors (pinning test, `nulltest/render_pin.py`):* a low carrier (15–40 Hz) plus a strong tone at
  157–220 Hz, which the wide level detector sees but the l band does not. The pinning tone lowers the overall gain
  as expected, but the carrier's 3rd harmonic hardly changes (20 Hz at −26 dB: +4.5 → +1.7 dB re h1 with the pinning
  tone at −3 dBFS), where a single detector predicts a collapse (to −8 dB). So a second gain stage reads l itself:
  CS = C0 (1 + E/E0)^-p (1 + EL/El0)^-pL, EL an attack / release follower on |l|. Its ripple makes the 3rd harmonic
  of low notes, locked to the zero crossings of l as measured earlier.
- *The l-driven stage alone (`nulltest/render_pinned.py`, `fit_lstage.py`):* with the wide detector pinned by a
  −3 dBFS tone near 200 Hz, the gain over one period of l is a sawtooth that jumps up at each zero crossing and
  slides down through the half-cycle, deeper for lower notes (±50 % at 10 Hz). Fitted alone it is a follower with a
  slower attack (14 ms) than release (3.7 ms) on |l|; h1 then matches to ±1 dB and h3 to −8 … +4 dB, except the
  lowest notes (10–12 Hz, 6–10 dB weak). In the combined fit the waveform data keep this stage small, so its shape
  is not settled.
- *Kicks at high Freq:* the reference lets the first generated peak of a kick through about 3 dB higher at Freq
  160 / 256 (its gain reacts later there); a plain delay of the level makes everything worse, and Freq-dependent
  attack times (fitted: slower at high Freq) help only slightly.
Fitted on time-domain renders across the four Freq settings, on transients (kicks, bass notes and a riff at Freq
80 / 160 / 256, each against the reference's unlimited generated signal, `nulltest/render_transients.py`) and on the
steady harmonic and pinning sets together (`nulltest/fit_combo.py`). A fit to the harmonic sets alone reproduces
the pinning behaviour well but fails on everything else (null median −12.7 dB); a fit to the waveforms alone keeps
the l stage small. Still not understood: the exact detectors, the 3rd harmonic of 40 Hz notes (4–10 dB weak when
pinned) and of the lowest notes (10–12 Hz), and kicks at Freq 160 / 256 (−11 / −7 dB on the unlimited generated
signal). This is a fitted model of the gain control, not an identified one.

**Learned gain block (current).** Since no analytical detector reproduced everything at once, the plug-in now keeps the
identified structure exact (filters, gain ahead of / after the first high-pass, harmonic product, limiter, clip) and
learns only the level-to-gain block from data: a GRU with 32 units reads |x|, |b|, |l|, |u| (compressed with asinh)
and ln(Freq/80) and gives ln g_pre and ln g_post, sample by sample. Magnitudes only, so the gain is symmetric and
Decay 0 leaves no even harmonics, as in the reference. Trained with PyTorch on 900 reference renders (tones, steps,
AM, two-tones, noise, kicks, bass lines, pinned pairs, sweeps; Freq 32 … 256; Decay 0 / 1) against the exact
generated part (`nulltest/render_trainset.py`, `nulltest/train_gain.py`, exported by `tools/export_gainnet.py`
into `plugins/WBass/WBassGainNet.h`); validated on 80 held-out renders: median −37.7 dB (the fitted analytical
model −25.3 dB). In the plug-in the network runs on its own 48 kHz clock (the rate it was trained at) at any host
rate: the null at 44.1 and 96 kHz is within 0.2 dB of 48 kHz. The C++ matches the trained model to −74 … −91 dB.
Cost: about 6 % of one core for real-time stereo. This is a learned model of the reference's gain control, not an
identification of its algorithm.

**Null test** ([docs/NULL_RESULTS_WBASS.md](docs/NULL_RESULTS_WBASS.md), 12 signals × 8 settings, generated part split
off the reference with a second Intensity render): median residual −36.8 dB on the whole output and −31.5 dB on the
generated part alone (fitted analytical model: −28.9 / −23.2 dB; first round −22.6 / −18.0 dB). By setting, the
generated part reaches −34 … −36 dB at Freq 32 / 40, −30 … −31 dB at Freq 80, −26 dB at Freq 160 and −17 dB at Freq
256 (whose worst cases are very low notes with a generated part 38 dB under the output). Weakest signals: kick-like
transients (−22 dB) and a 30 Hz tone (−22 dB). This is not a null.

### Meter ballistics (WBass / RBass)

The reference's meters cannot be read through the plug-in interface, so they were filmed:
`tools/MeterCapture` hosts the plug-in, plays a test signal through it in real time, records its output, and grabs the
editor about 40 times a second. `nulltest/meter_frames.py` turns the frames into bar heights; the numbers are in
[measurements/rbass/meters](measurements/rbass/meters) (the screenshots themselves stay out of the repository).
Signals: tone bursts and level steps at 25 / 50 / 300 / 1000 Hz, 5 / 20 / 100 ms bursts (`nulltest/meter_probe.py`),
and a bass line and kicks.

- **Law** (`nulltest/fit_meter.py`, fitted on the output meter against the recorded audio): each frame takes the
  highest peak of the last ~17 ms; the bar jumps up to it at once and otherwise moves toward it as a one-pole on
  **linear amplitude**, time constant **0.325 s**. In silence that is a straight 26.7 dB/s fall (the same on all
  three meters, from any level); with the signal still playing, a drop to a lower level settles gently (from −3.3 to
  −8.7 dB takes about a second), and an onset peak above a steady tone eases back over ~1 s. Residual of the fit:
  1.1 px RMS (0.2 dB) over the whole probe.
- **Scale**: 5.17 px per dB, 0 dB at the top of a 155 px track (30 dB), on all three meters.
- **Readouts** hold the highest peak until clicked.
- **Not matched**: the Original Bass bar sometimes drops at once to a lower level after a long sustained tone instead
  of easing down (e.g. a 2 s tone stopping). WBass eases down there too. On music (bass line, kicks) the bars agree
  with the reference within 3.5 px (0.7 dB) on 90 % of the frames.
- WBass implements this in `Source/common/WavyMeter.h` (the audio thread only collects peaks; the editor applies the
  law at 60 frames per second with the real time between frames, so the movement does not depend on the host's
  buffer size).

### Meter ballistics (WVox / RVox)

Filmed the same way (`tools/MeterCapture`, the same probe and bass-line signals, Compression −12 dB); bar heights in
[measurements/rvox/meters](measurements/rvox/meters).

- **Output** (36 dB over the 155 px track, 4.3 px/dB): the same law as RBass, one-pole on linear amplitude with
  0.325 s (26.7 dB/s in silence); fits the bass-line capture to 6 px RMS, most frames within 1–2 px.
- **Energy** (input level, 80 dB over the track, 1.97 px/dB): two such stages in a row, 0.341 s then 0.094 s
  (`nulltest/fit_meter_energy.py`, 2.2 px RMS); the second stage makes the fall start gently.
- **Gain reduction**: the compressor's reduction itself with no meter smoothing (a 5 ms burst shows its full
  reduction for one frame and is gone in the next). WVox shows the deepest reduction since the last frame.
- **Not matched**: after a steady test tone that stops (or steps down), the reference output bar sometimes snaps down
  at once instead of easing (the same quirk as the RBass Original Bass meter); no rule tried in
  `nulltest/fit_meter_vox.py` (falling line with several hold rules, updates only on change) explains it. It does not
  show on the bass line.
- WVox against the reference on the bass line: Energy, Output and Gain reduction within 2.0, 2.3 and 2.0 px on 90 % of
  the frames.

### Control feel (WBassX / MaxxBass)

Measured by dragging the real editor with `tools/MeterCapture drag=x,y,dx,dy[,mod,steps]` (synthetic mouse moves,
parameters read back, cursor visibility checked mid-drag):

- The cursor is hidden while a fader, knob or the Freq slider is dragged (WBassX puts it back on the control).
- Faders and the Freq slider follow the mouse 1:1 (a fader's 165 px travel, the slider's 264 px).
- Knobs: about 500 px for the full range when the mouse moves 5 px or more per move, about 1000 px when it moves
  slowly (2 px or less per move); Ctrl about 3000 px. Shift does nothing special.
- WBassX gives the same values as the reference on every test drag (e.g. Ratio 1.00 -> 1.12 -> 1.18, Decay -15.0 ->
  -16.2 -> -16.8, Input -4.8 vs -4.9 dB, Freq 80 -> 119 Hz).

### Selection and groups (WBassX / MaxxBass)

Measured with the MeterCapture drag script (screenshots on hover, press, drag, release, mouse away; parameters
read back after each step):

- A click marks a control until another one is clicked; hovering shows nothing. Marks: knobs a ring at radius 20.5
  (yellow (241, 227, 7), brightest at 3 and 9 o'clock, gone at the top and bottom), the Freq thumb the same ring at
  6 px, faders a 13 x 2 px bar under the thumb ((85, 81, 10)) plus a glow of the line's own colour over the thumb, the
  Filter box a 2 px outline 2.5 px outside its border ((85, 82, 24) at the ends, fading toward the middle of the top
  and bottom), the Monitor column a 1 px line under the buttons (pure yellow in the middle, cosine fade).
- A lasso dragged on the background (1 px pure white rectangle) selects every control it touches, live; Freq is not
  part of groups. The first in the control order (Input, Original Bass, Ratio, Harmonics, Decay, Response, Monitor,
  Filter) is the primary (yellow), the others members (blue: knob ring (7, 168, 229), fader bar (9, 60, 80), Filter
  outline (22, 66, 84)). Clicking a member makes it the primary and keeps the group; clicking the background clears.
- Dragging a member moves the group by equal steps on the reference's 0..1 parameter scale (Original Bass +0.093,
  Input +0.089, Harmonics +0.090 for a 15 px drag), each member clamped at its own ends (Input stops at 0 dB, the top
  2/3 of its scale); choice members step one setting per mouse move. WBassX gives the same values on the same drags
  (e.g. Ratio 1.12 -> 1.25 -> 1.00, Decay -16.2 -> -15.0 -> -17.4, Filter 12 -> 24 -> 6 dB/oct).

# WBassX: measurements of the reference (Phase 3)

Measured on the user's own install (*MaxxBass Stereo* / *MaxxBass Mono* from the WaveShell, pedalboard, 512-sample
buffers, 48 kHz). Data and plots are in `measurements/maxxbass/`; reproduce with `python -I nulltest/measure_maxxbass.py`.

## Method

- Every control is a VST3 parameter, so no state writing was needed (unlike RBass); `nulltest/refhost.py`
  `render_mb` sets them all, the rest at their defaults.
- **G** below is the generated part: what the MaxxBass monitor plays. Sine tones are analysed by synchronous
  detection over whole periods; level steps by the Hilbert envelope; the linear path by impulse responses.

## Results

| Item | Measured |
|---|---|
| Latency | 0 samples at 44.1, 48, 96 and 192 kHz (reported and measured), with Harmonics off and on |
| Structure | **Audio = HP3(F)·x + OrigBass·LP3(F)·x + Harmonics·G(x)**, exact to −148 … −152 dB; G does not depend on either fader (−277 dB) |
| Original path | HP3(F) and LP3(F) are bilinear Butterworth filters: the Audio path matches HP3 + g·LP3 to −78 … −101 dB for every Freq and Original Bass setting tried (32 / 80 / 256 Hz; −inf, −6, 0, +12 dB). At Original Bass 0 dB it is a flat all-pass (an impulse response with sum 1 and energy 1); linear (−93 dB between levels 0.01 and 0.5) |
| Monitors | Original Bass plays LP3(F)·x and MaxxBass plays G, both **pre-fader** (not changed by the Original Bass or Harmonics faders) |
| Input | a plain trim up to 0 dB; +6 and +12 act as 0 dB, as WBassX's fader already does. G follows it nonlinearly (−6 dB input → −4.4 dB G) |
| Harmonics fader | an exact gain on G up to **+10.96 dB**; settings above that give the same output (+11 and +12 → +10.96) |
| G content | a strong component at the input frequency itself plus mostly 2nd and 3rd harmonics. For example, −20 dBFS tones at Freq 80 and Decay −15, in dB re input: 40 Hz h1 −10.3, h2 −35.6, h3 −50.9, h4 −66; 60 Hz −6.6 / −37.8 / −47.0; 100 Hz −7.5 / −48.3 / −53.1 (`harmonics.csv`, 105 rows over Freq, Decay and f0) |
| Decay | acts mainly on the **even** harmonics, about 1 dB per dB: at 40 Hz h2 is −44.6 / −35.6 / −29.6 dB and h4 −75 / −66 / −60 for Decay −24 / −15 / −9, while h1 and h3 barely move (−10.3; −51 … −49). RBass's hidden Decay also scaled the even-harmonic product |
| Level and Ratio | at Ratio 1 G is compressed: h1 goes from −4.1 dB re input at −60 dBFS to −13.1 at 0 dBFS (50 Hz). Ratio above 1 is upward compression: quiet input gets G at up to +4 dB re input (a ceiling reached at −60 … −36 dBFS for Ratio 2 and 4), and loud input less. The harmonics grow faster than the input (`level.csv`, `level_ratio.png`) |
| Dynamics | a 50 Hz step −40 → −10 → −40 dBFS. At Ratio 1, G settles within 1 dB in 34 / 51 / 72 ms after the rise for Response 10 / 20 / 30, overshooting +1.5 / +3.0 / +3.8 dB; after the fall it undershoots −6 … −10 dB and settles in 48 … 58 ms. At Ratio 4 the rise settles faster (16 … 40 ms) with less overshoot (`dynamics_steps.png`) |
| Response | on a steady amplitude-modulated bass, 10 or 30 ms vs 20 ms changes G by −25 … −29 dB re G (an attack / release time, small on steady material) |
| High-pass | applies to G (not to the original path). At Freq 80, h1 of G is −4.1 / −21.6 / −36.1 dB at 20 Hz and −2.2 / −10.3 / −14.3 at 40 Hz for 6 / 12 / 24 dB/oct; equal above Freq |
| Stereo | each channel is processed on its own: a left-only input leaves the right output silent (−300 dB); a loud left channel does not change a quiet right channel's G (−47.98 dB both ways, h2 −90.2 both ways); Mono equals each Stereo channel (−274 dB). This differs from RBass, whose generated bass is a shared mono sum |
| Output clip | a hard clip at exactly ±1.0 on the final output (−89.5 dB against an ideal clip of the unclipped output; RBass clips at 32767/32768). G alone is never limited |
| Smoothing | none: Input, Original Bass and Harmonics changes take effect at the block boundary. A Freq change settles within about 4 ms (filter state); Input shows a 2 % settling of G over 5 ms (its dynamics) |
| vs RBass | not the same tuning. On the same tones (Freq 80), RBass's generated fundamental is 8 … 15 dB stronger and its harmonics 15 … 25 dB stronger (40 Hz at −30 dBFS: h2 −19.4 vs −42.3 dB re input) |

## What this means for Phase 4

The original path is exact and simple: the same bilinear HP3 / LP3 Butterworth pair WBass uses, with the Original
Bass gain on LP3 and no smoothing. The two monitors play LP3·x and G pre-fader. The Harmonics fader multiplies G,
capped at +10.96 dB. The output is hard-clipped at ±1. Processing is per channel, with no latency.

The identification work is G: a band below Freq feeding a generator that keeps a strong fundamental and adds mostly
2nd and 3rd harmonics. Decay is the gain of the even-harmonic term. G has an upward-compressing level control (Ratio)
with an attack / release set by Response, and the High-pass on G.

The WBass generator structure (an even product term with its own gain, high-passes at F, a level follower) is the
starting point. Its constants and detector have to be fitted again: MaxxBass's generated part is weaker, per channel,
and has an explicit Ratio. As for WBass, if no analytical detector matches, the level-to-gain block can be learned.

## WBassX model (Phase 4 identification)

Scripts: `nulltest/ident_mb_linear.py` (small-signal response), `ident_mb_k2.py` (two-tone kernels), `extract_g2.py` /
`extract_g3.py` (the gain over time), `mbmodel.py` (Python model), `render_trainset_mb.py`, `train_wbassx.py`; data in
`measurements/maxxbass/` (`g_small_signal.json`, `k_out_table.json`, `static_gain_50hz.json`, ...). F is the Freq
setting, D = 10^(Decay/20).

**Identified exactly** (residual given):

| Part | Finding |
|---|---|
| Structure | Audio = HP3(F) x + OrigBass LP3(F) x + Harmonics G; per channel; G does not depend on either fader |
| G at rest (very low level) | an exact LTI response: −K(F) LP3(F) HP_G (bilinear Butterworth sections). HP_G for 6 dB/oct: HP2(16 Hz)², 12 dB/oct: HP2(F) HP2(16 Hz), 24 dB/oct: HP2(0.8 F)² (0.009 … 0.035 dB, 0.2°; impulse responses at 1e-4, linear to −58 dB, bit-exact time-invariant). Below about 8 Hz the reference leaks a little DC (−50 … −98 dB), the mark of single-precision filter coefficients |
| Output scale K(F) | smooth but no simple formula fits better than 0.9 %, so WBassX carries the measured value for each of the 225 whole-Hz settings (2.77 at 32 Hz, 4.53 at 80 Hz, 5.59 at 256 Hz; each fitted to −46 … −77 dB) |
| Harmonic product | from 190 two-tone kernels per setting: 8 D · u · HP2_B(u), u = LP3(F) x, followed by the same HP_G, with HP2_B a Butterworth high-pass at F/3 held within 20 … 40 Hz. Sum and difference terms −46 … −50 dB at Freq 50 … 256 (difference terms only differ below 10 Hz, the DC-leak region). Decay is an exact gain on it (−9.00 / +6.00 dB for −24 / −9) |
| Product / linear | 7.98 ± 0.01 × D at every Freq: the level control's ceiling is exactly 1, and K(F) is a separate output scale |
| Decay feedback | the product feeds back: y = w (1 + 8 D HP2_B(y)); the 3rd harmonic grows with D², and the static harmonics match to h1 −56 … −62 dB, h2 −40 … −55 dB. The reference applies the loop with one sample of delay: when 8 D b0 \|w\| exceeds 1 (strong Decay, high Ratio, loud low bass) it oscillates at Nyquist, as seen in its own output, and y is held within ±1 |
| Level control | a gain g ≤ 1 on u ahead of the generator (w = g u): the 2nd harmonic predicted from it matches to 0.05 dB over −84 … 0 dBFS. It does not depend on Decay, but it does depend on the High-pass setting (a 20 Hz
tone at −6 dBFS: −10.7 / −1.7 / −0.4 dB for 6 / 12 / 24 dB/oct), and its level curves for different tone frequencies
are shifted copies of each other by about G's own small-signal response: the level control listens after HP_G. Static law (50 Hz, Freq 80, gain re its cap): Ratio 1 falls from 0 dB below −102 dBFS to −8 dB at −60 dBFS, then −9 dB to −36 dBFS and −17 dB at 0 dBFS; above Ratio 1 the low-level knee moves up about 12 dB per doubling of ln Ratio, and the high-level slope tends to 1 − 1/Ratio. At low Freq (≤ 45 Hz) and Ratio 1 the resting gain is 1/8 of the cap |
| Gain over time | extracted from renders by least squares through the exact structure (Gauss-Newton with the generator's exact Jacobian): the model then re-renders the reference's G at −50 dB median (−38 dB on bass notes, −27 … −33 dB on kicks). A fast drop at onsets, a release of hundreds of ms, and ripple within each cycle that adds odd harmonics at high level |

**Learned level control (current).** No analytical detector was found for the gain law (its knee and slope both move with
Ratio, plus the resting state). So WBassX keeps every identified part exact and learns only g: a GRU with 32 units reads
|x|, |u| and |z| (compressed with asinh; z = K(F) HP_G(u), the signal the level control listens to), ln Ratio, Response,
ln(F/80) and the High-pass setting, and gives ln g = −softplus(·) (so g ≤ 1). Without z and the High-pass input the
same network stopped at −22 dB on the generated part. It runs
on its own 48 kHz clock at any host rate. It was trained end-to-end on the reference's generated part: the generator's
forward and adjoint recursions run in C (`nulltest/mbfast.c`), and HP_G is applied as FFT convolutions on the GPU.
The training set has about 4900 reference renders (tones, steps, AM, two-tones, noise, kicks, bass lines, pinned
pairs, sweeps) over random Freq, Ratio, Response, Decay and High-pass. The C++ matches the trained model to −71 …
−117 dB on most examples (−43 dB on a kick). This is a learned model of the reference's gain control, not an
identification of its algorithm. Cost: about 12 % of one core for real-time stereo.

**Null test** ([docs/NULL_RESULTS_WBASSX.md](docs/NULL_RESULTS_WBASSX.md), 12 signals × 8 settings over Freq, Ratio,
Response, Decay, High-pass and the faders): median residual −42.4 dB on the whole output and −35.9 dB on the
generated part (Harmonics monitor). By signal (generated part): white noise −48 dB, quiet tones −52 dB, tones and sweeps
−36 … −40 dB, pink noise −38 dB, level steps −30 dB, bass notes −25 dB, kick-like transients −16 dB. The worst cases are
loud material at extreme settings (Freq 256 / Ratio 4 / Decay −12 with a −3 dBFS tone: +2.5 dB; Decay −9): there the
reference's own Decay loop goes unstable, and the structure reproduces it only roughly (with the reference's own gain
extracted it reaches −20 … −27 dB at Decay −9). At 96 kHz the null is within 1.8 dB of 48 kHz. This is not a null.
