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
Ratio, plus the resting state). So WBassX keeps every identified part exact and learns only g: a GRU with 48 units reads
|x|, |u| and |z| (compressed with asinh; z = K(F) HP_G(u), the signal the level control listens to), ln Ratio, Response,
ln(F/80) and the High-pass setting, and gives ln g = −softplus(·) (so g ≤ 1). Without z and the High-pass input the
same network stopped at −22 dB on the generated part. It runs
on its own 48 kHz clock at any host rate. It was trained end-to-end on the reference's generated part: the generator's
forward and adjoint recursions run in C (`nulltest/mbfast.c`), and HP_G is applied as FFT convolutions on the GPU.
The training set has about 4900 reference renders (tones, steps, AM, two-tones, noise, kicks, bass lines, pinned
pairs, sweeps) over random Freq, Ratio, Response, Decay and High-pass. The C++ matches the trained model to −71 …
−117 dB on most examples (−43 dB on a kick). This is a learned model of the reference's gain control, not an
identification of its algorithm. Cost: about 7 % of one core for real-time stereo (SSE2 / NEON inner loops). The
level control recovers fully within 0.1 … 0.3 s of silence and, after silence, attacks over a time set by Response
(at Ratio 1, 10 ms into a burst the gain is still −6.9 / −3.9 / −2.7 dB for Response 10 / 20 / 30, settling in about
4 × Response; `probe_recovery_mb.py`, `recovery_F80.json`); onset-heavy training material (kicks, steps, bass lines,
a third of it at Ratio 1), a 48-unit network, about 13 900 training renders and long fine-tunes at a lower learning
rate brought kicks and steps 3 … 5 dB closer and the generated part from −35.9 to −42.7 dB.

**Null test** ([docs/NULL_RESULTS_WBASSX.md](docs/NULL_RESULTS_WBASSX.md), 12 signals × 8 settings over Freq, Ratio,
Response, Decay, High-pass and the faders): median residual −48.5 dB on the whole output and −42.7 dB on the
generated part (Harmonics monitor). By signal (generated part): white noise −49 dB, quiet tones −59 dB, tones and sweeps
−40 … −49 dB, pink noise −43 dB, level steps −34 dB, bass notes −27 dB, kick-like transients −19 dB. A grid of 120
random settings over every control with the four hardest signals (`nullgrid_wbassx.py`, `docs/NULL_GRID_WBASSX.json`):
median −35.2 dB whole output, −29.2 dB generated part (p90 −19 dB), with no parameter range standing out (generated
part −26 … −31 dB median in every Freq, Ratio, Decay and High-pass band). Against the
reference's own extracted gain (the ceiling of the structure) the grid reaches −37.1 dB median against the network's
−28.1 dB on the same 36 cases (`oracle_grid_mb.py`): bass notes −31 vs −24, pink noise −46 vs −36, the tone −39 vs
−31, but kicks −21 vs −18, so kicks are mostly a limit of the structure (part of it the reference's own monitor
clipping at ±1 on the first kick peak), the rest mostly the learned level control. Tried without gain: explicit
envelope followers of |z| as inputs (3 … 100 ms peak, 10 / 50 ms power), and the previous output sample as an input
(teacher-forced; worse in closed loop) — the network does not lack those. An explicit detector instead of the GRU (two
attack / release followers in cascade on |z| with learned time constants scaled by Response, Freq and Ratio, and a
learned per-sample gain law; `train_wbassx_det.py`) stopped at −30.8 dB on the validation set against −39.1 dB for the
GRU, weakest on kicks (−17 dB) and steps (−21 dB): the reference's attack is richer than two followers. Its fitted time
constants (about 5 … 11 ms attack, 22 ms release, then a 15 … 22 ms smoother) agree with the recovery probe. The worst cases are
loud material at extreme settings (Freq 256 / Ratio 4 / Decay −12 with a −3 dBFS tone: +2.5 dB; Decay −9): there the
reference's own Decay loop goes unstable and turns chaotic (Nyquist content −7 dB, h2 above h1, output pinned at ±1;
even the reference's own extracted gain reproduces it only to −4 dB), and the structure reproduces it only roughly (with the reference's own gain
extracted it reaches −20 … −27 dB at Decay −9). At 96 kHz the null is within 1.8 dB of 48 kHz. This is not a null.

### Meter ballistics (WBassX / MaxxBass)

Filmed with tools/MeterCapture (50 Hz bursts at −6 and −20 dBFS, then silence): the bars rise at once and release as a
one-pole on linear amplitude like RBass's, but faster: 55.8 … 56.4 dB/s over three releases, a time constant of
0.155 s (RBass 0.325 s). The scale: 3.79 px per dB with −40 dB at y 274.7 (the rounded ends inset the bar from the
tick marks); below −40 dB, while audio runs, a small dot stays at the bottom (y 279 … 283); with no audio at all,
nothing. The peak readouts (−6.0, then −4.9 from the release overshoot) match. WBassX: within 1 px on 88 % of frames,
3 px on 93 %.

### Value entry (WBassX / MaxxBass)

Measured with the MeterCapture `click` / `dbl` scripts (click or double-click, then type a value and Enter): a
double-click on a knob, a fader or the Freq slider opens an entry box with the whole value selected, and Enter applies
it (Ratio 2.5, Input −6, Freq 100 all took). A single click on the value text does nothing, Ctrl-click does nothing,
Alt-click resets to the default. The box: white, black text, a 1 px (100, 100, 100) line on the left and right only,
the system's blue selection (0, 120, 215); knob value x 106 … 159, y 374 … 387 (13 px text), fader x 553 … 583,
y 320 … 337 (15 px text), Freq x 380 … 429, y 297 … 309, showing the bare number. WBassX matches the boxes to the pixel
and the text within 1 … 2 px.

### Control feel and selection (WComp / H-Comp)

Measured with the MeterCapture drag / click scripts on the reference editor: knobs turn clockwise when dragged up
(Threshold toward −48 dB, since 0 dB sits on the left), at about 1000 px for the full range on slow moves (−18 →
−19.4 dB for 30 px), as WBassX's knobs; hovering shows nothing; a click selects a control (a thin yellow ring, colour
(222, 222, 6), on the knob's cap at radius 21 for the small knobs) and shows its value in a dark box (fill
(65, 49, 36), a light 1 px frame) on the value slot under the small knobs or on the cap of the large ones; a
double-click opens the white entry box there; Alt-click resets. WComp gives the same values on the same script.

Every knob, measured with the drag script (100 px moves, mouse steps of 1 … 30 px 12 ms apart) and the wheel
(`drag=x,y,n,0,wheel`, n notches; logs parsed by `nulltest/parse_drags.py`), 2026-10-23:
- **Continuous knobs** (Threshold, Ratio, Mix, Output, Punch, Attack, Release): the same share of their range per pixel
  whatever the knob — 0.001 per px on mouse steps up to 2 px, 0.002 from 3 px. **Ctrl** gives a quarter of either
  (0.00025 / 0.0005), so slow and fast still differ.
- **Analog** (5 positions): one position per 50 px dragged, at any speed and with Ctrl alike.
- **Release in BPM / Host** (the note, 20 values): one value per 30 px slow, 20 px fast, 120 px with Ctrl slow and
  66.7 px with Ctrl fast, counted from where the drag began.
- **Wheel**: 1/72 of the range per notch on the continuous knobs, one position per notch on the stepped ones; Ctrl
  changes nothing.
- WComp before: the same plain rates (fast from 4 px), Ctrl a flat 1/6 at any speed, Analog and the note by the
  continuous rate (a step per 250 / 50 px), the wheel 2.6 times the reference's (JUCE's 15 % per unit). Now it gives
  the reference's values on every one of these scripts (the wheel within the display's last digit).

### Wheel and Ctrl on the Waves controls (all plugins)

Measured the same way on RVox, RBass and MaxxBass (`nulltest/feel/`: 50 px drags, 200 px Ctrl drags in mouse steps of
1, 2, 3 and 10 px, each from the default value in a fresh session; 3-notch wheel moves), 2026-10-24:
- **Wheel**: every control takes it over its whole area (the faders take clicks on the thumb only), 1/72 of its range
  per notch as on H-Comp, a stepped control one step per notch; Ctrl changes nothing. Our controls: JUCE's wheel
  (2.5 times as far) on the knobs, none at all on the faders (their hit test is the thumb). Now the shared rule
  (`Source/common/WavyWheel.h`), the editors forwarding the wheel to the control under the mouse; measured scale
  factors where a control's range is not the reference's: WBassX Input 1.6 (its range is 2/3 of the reference's
  scale), Freq 1.2 on WBass and WBassX.
- **Faders and sliders**: the thumb follows the mouse (RBass's Freq at 1.53 times its track: the whole range in 98 px);
  no slow / fast difference without Ctrl. **Ctrl** gives about 0.19 of that rate on mouse steps of 3 px and more and
  about 0.037 on steps up to 2 px (RBass Freq 0.28 / 0.038, MaxxBass Freq 0.23 / 0.067). Ours: WVox's and WBass's
  faders switched to JUCE's velocity mode with Ctrl (slow moves did nothing), WBassX's moved a flat quarter. Now
  measured rates (`Fader` in WavyRenUI, `WavesDrag` for the Flat kit).
- **Knobs** (MaxxBass): as H-Comp's, Ctrl a quarter of the rate keeping the slow / fast split; WBassX's took a flat
  1/6 with Ctrl, now H-Comp's law.
- After the change our three give the reference's values on all of these scripts within the display's last digit
  (Freq on the wheel 1 Hz apart: its steps are whole Hz).
- **Ctrl pressed or released during a drag** (`nulltest/feel/midctrl_feel.sh`, modes `midctrl` / `ctrlup`: the drag
  starts plain, Ctrl goes down or up halfway, 10 px steps), 2026-10-23: on all four references the rate switches at
  once, both ways (H-Comp Threshold −30.0, RVox −14.0, MaxxBass Ratio 1.41). Ours kept the rate of the mouse-down when
  the host keeps the keyboard focus, as FL does: JUCE refreshes its modifier keys from key messages and mouse buttons
  only, and FL sends the plug-in no key messages. Reproduced in `MeterCapture hostwin=1` (the editor in a plain Win32
  window that keeps the focus); the controls now read the key itself during a drag (`ctrlHeldNow` in
  `WavyWheel.h`), and WComp, WVox, WBass and WBassX give the reference's values on both modes (WComp −30.0, WVox
  −14.3, WBassX 1.41).
- **The cursor during a fader drag** (RVox Gain, RBass Freq; MeterCapture now prints where the cursor is after the
  release), 2026-10-23: hidden while dragging, and put back where it was grabbed, moved as far as the thumb moved
  (RVox Gain: a 100 px drag ends on 235, a 60 px Ctrl drag on 223; RBass Freq: 234 and 268). WVox's and WBass's
  faders showed the cursor and left it where the mouse went; now as the reference (WVox 238 and 223, the 3 px from
  its Gain taper, −24.0 dB where RVox reads −23.4; WBass 233 and 268).


# WComp: measurements of the reference (Phase 3)

Measured on the user's own install (*H-Comp Stereo* / *H-Comp Mono* from the WaveShell, 48 kHz unless noted). Data and
plots are in `measurements/hcomp/`; reproduce with `python -I nulltest/measure_hcomp.py [section ...]`.

## Method

- Every user control is a VST3 parameter; `nulltest/refhost.py` `render_hc` sets them all through pedalboard (the rest
  at their defaults, **Analog Off** unless stated). Sync is set before Release: H-Comp does not keep a Release value
  set while it is synced to a note.
- What pedalboard cannot do (feed the side-chain bus, give a host tempo) goes through `tools/RefRender` (a small JUCE
  host; `refhost.render_ref`).
- **The sample-wise gain is observable.** The detector reads the sample peak (below), so an alternating ±a signal
  (Nyquist) is a constant level to it, and with Analog Off the output is `g[n]·x[n]`: the gain trace is `y / x`, exact
  at every sample. Static curves, attack, release, punch, mix and the limiter are measured that way; sine staircases
  check the same static curves.

## Results

| Item | Measured |
|---|---|
| Latency | 64 samples at 44.1 / 48 kHz, reported and compensated (an impulse comes out unchanged at Analog Off with the compressor idle); 128 / 256 at 88.2–96 / 192 kHz |
| Idle path | Analog Off, threshold 0 or ratio 1: an exact identity (impulse response 1.0, flat to 0.000 dB) |
| Detector | the **sample peak**: a sine, a square wave and an alternating signal of the same peak give the same gain reduction to 0.001 dB; an 8 kHz sine (6 samples a period) gets 0.94 dB less, as its samples miss the crest. Peaks are held over the look-ahead window: full reduction down to 400 Hz, less below (200 Hz −0.46 dB, 80 Hz −1.2, 30 Hz −1.8 dB of gain reduction at threshold −24 / ratio 4) (`freqdet`) |
| Static curve | no hysteresis (staircases up and down agree within 0.008 dB). A soft knee fitted over 20 curves (thresholds 0 … −48, ratios 1.5 … 50, 0.5 dB steps) to **0.11 dB RMS, 0.33 dB worst**: `GR = (1 − 1/r)·knee(Ld − T − 0.76)`, where **`Ld = 20·log10(peak + 0.00106)`** (a floor at −59.5 dBFS, which makes the curves at low thresholds approach the ratio only slowly) and the knee is quadratic over 3.0 dB (`knee.csv`, `knee.png`, `static.csv`) |
| Makeup gain | automatic, from threshold and ratio only: 0 for thresholds above −9 dB, then growing about linearly with the threshold below −9: **+0.47 dB per dB at ratio 50**, +0.31 at ratio 3, +0.10 at 1.5 (for example +2.76 dB at −18 / 3, +18.35 at −48 / 50). Dense grid in `makeup.csv` (25 thresholds × 18 ratios) |
| Attack | a one-pole on the **linear gain**: its 90 % / 63 % time ratio is 1.88–1.92 (one-pole on linear gain: 1.87; on dB: 2.30), and with the look-ahead counted the time constant is the Attack value (7 ms → 7.2, 20 → 20.0, 100 → 98.5 ms) |
| Look-ahead | the gain starts to fall **1.25 ms (60 samples) before** a step reaches the output, at every rate |
| Punch | delays the detector by **exactly 1 ms per unit**: the gain starts to move at (Punch − 1.25) ms after a step (Punch 1 → −0.25 ms, 10 → 8.75, 30 → 28.75 ms), so transients pass before the gain reacts |
| Release | after sustained compression the 63 % point is **0.75 × the Release value** (3 → 3.0, 100 → 76, 1000 → 753, 3000 → 2258 ms); the gain rises at a constant rate in dB and stops sharply at the threshold: the release acts on the detector's level, not on the gain |
| Program-dependent release | the release depends on how long the compression lasted, not on its depth: after 5 / 20 / 100 / 200 / 350 / 500 / 700 ms bursts the 63 % point is 0.7 / 2.8 / 31 / 43 / 65 / 73 / 76 ms (Release 100), then constant. Attack and ratio barely change it (`release_pd`) |
| Sync | Release = note × 240000 / tempo (ms), with the same 0.75 factor: **BPM** uses the plug-in's own tempo (120 by default), **Host** follows the host's tempo (60 BPM: 1/16 → 250 ms, 180 BPM → 83 ms) and falls back to 120 when the host gives none. Long notes at slow tempos fall back to the next shorter straight or dotted note (2 bars → 1 bar → 1/2D → 1/2 → 1/4D) once they pass about 3.3–4 s (the points measured are in `results.json` `sync_host`) |
| Mix | a linear blend of the dry input (no makeup) and the compressed signal, exact to 0.0002 |
| Output | a pure gain, ±18 dB, exact |
| Limiter Off / On | **both** keep the output under 0 dBFS with a look-ahead peak limiter, not a clipper: the gain ramps down over the last **63 samples** before a peak (an S-shaped ramp) and recovers in about 1.2 ms (Off) or 1.4 ms (On); a 1 kHz sine 6 dB over comes out undistorted (h3 −116 dB), a 100 Hz sine gets clipper-like odd harmonics (h3 −20 dB) because the gain recovers between its peaks. Off lets a single-sample spike through at 1.0006, On at 0.9997 |
| Stereo | linked: a loud left channel compresses a quiet right one by the same gain (−10.01 dB both); the Mono component equals a Stereo channel |
| Side-chain | the shell offers a side-chain bus, but H-Comp ignores it: a loud key does not compress a quiet main signal and a silent key does not stop a loud one; the manual has no side-chain |
| Sample rates | 44.1 … 192 kHz: the same makeup, static gain, attack (12.3 ms at 63 %) and release (72 ms) within 0.02 dB and 2 % |
| Analog modes | at −20 dBFS, small-signal response and harmonics (1 kHz at 0 dBFS, re fundamental), noise with silence in: **1** flat −0.26 dB, h2 −54, h3 −58, noise −102 dBFS; **2** a dip of −0.77 dB around 1–2 kHz, h3 −48 (odd harmonics), noise −83 dBFS rising toward high frequencies; **3** a level-dependent dip (−4.8 dB at 1 kHz for −40 dBFS, −3.1 at −20, −0.5 at 0), strong h2 / h3 (−22 / −33), noise −101; **4** flat within 0.1 dB, no noise (−109), only odd harmonics that are *larger at low levels* (h3 −27 dB re a −40 dBFS tone, −67 at 0 dBFS: crossover-like distortion). Full tables in `results.json` `analog` |

## What this means for Phase 4

The control path is a look-ahead peak compressor: a 64-sample delay on the audio, the detector on the sample peak held
over the look-ahead window and delayed by Punch, the static curve with its floor and 3 dB knee, makeup from threshold
and ratio, attack smoothing on the linear gain, release on the detector level with a program-dependent two-stage
behaviour, a linked stereo gain, the mix, the output gain and a 64-sample look-ahead limiter. All of this is
deterministic and the gain is observable sample by sample, so it can be identified exactly from the saved traces.
The program-dependent release is the part most likely to need a fitted or learned block.

The Analog modes are separate nonlinear colourations (level-dependent EQ, harmonics, crossover distortion in mode 4,
noise in modes 1–3). Mode 4 has no noise, so it can be nulled sample by sample; modes 1–3 will be matched in response,
harmonics and noise spectrum.

## WComp model (Phase 4 identification)

Scripts: `nulltest/ident_hc_detector.py`, `ident_hc_struct.py` (detector, smoothing, order of the stages),
`ident_hc_release*.py`, `ident_hc_w0.py`, `ident_hc_real.py`, `ident_hc_rel3.py` (release), `ident_hc_analog.py`,
`fit_hc_analog*.py` (Analog stage); the C models `hcfast*.c`; tables in `measurements/hcomp/` (`static_h.npz`,
`makeup*.csv`, `release_law*.json`, `release_w0.json`, `analog_model.json`, `analog_noise.csv`), exported to
`plugins/WComp/WCompTables.h` by `tools/export_wcomp_tables.py`. All on the alternating probe (the gain is `y / x`
exactly) unless stated.

| Part | Finding |
|---|---|
| Detector | max of \|L\|, \|R\| (linked), held over the **next 64 samples** of the delayed audio (`h[t] = max a[t+1 .. t+64]`); Punch delays it by round(P · fs / 1000) samples; sizes ×2 at 88.2–96 kHz, ×4 at 176.4–192 kHz |
| Static curve | `x = (1 − 1/r) · max(0, 20 log10(peak + 0.001) − T)`, `GR = h(x)` with one measured table h: all 42 measured curves collapse onto it to **0.014 dB RMS** (x − h saturates at about 0.55–0.6 dB, the knee) |
| Makeup | `k(r) · max(0, −9 − T) · f(Release)`: k per ratio (interpolated in 1/r, 0.47 at 50, 0 at 1.1), f a table over the release time (0.257 at 3 ms, 1 at 100 ms, 1.041 at 3 s), independent of threshold, ratio and attack |
| Attack | a one-pole on the linear gain, falling only, time constant 0.99 × Attack, after the release stage (better than before it) |
| Smoothing | two moving averages of 38 and 27 samples on the gain (a free FIR fit leaves 6e-4 RMS) |
| Mix, Output | linear dry (no makeup) / wet blend, then the output gain |
| Limiter | a 64-sample look-ahead (lead 1) of the would-be output, `min(1, 1 / held)`, the same two moving averages. With a constant compressor gain this is exact (worst 0.0002; a spike overshoots to 1.002). When the compressor gain moves, the reference's limiter sees the true future output, which a 64-sample latency cannot give; the best estimate found (the current compressor gain times the peak of the pending input) nulls heavy limiting (+12 dB into it) at −16 … −43 dB. On: the same onset, recovers later (approximated by a 7-sample longer hold) |
| Analog stage | the **last** stage: after the output gain and the limiter, applied to the dry part too, per channel (its offsets stay the same in output units under Output −18 / +12, Mix 0 and heavy compression; applied to the Analog-Off output it nulls the reference at −73 dB with the limiter working) |
| Analog model | `y = (1 + gain) L(x) + H2( g( H1(x) ) ) + noise`: first-order sections (mode 3: a band-pass biquad first, f0 183 Hz, Q 0.52), g a measured table with free odd and even parts. Memoryless in level (a level step changes mode 3's dip at once, both ways). Nulls on loud material (peaks 0.3–0.7): mode 4 **−82 … −100 dB**, mode 1 −72 … −78, mode 2 −44 … −56 (its noise alone limits it to about −54 … −69), mode 3 −29 … −64 (median −40). Quiet signals null less (mode 4 at −60 dBFS: −40 dB), the offset being of the order of 0.001 |
| Analog noise | stationary, independent per channel (L / R correlation ≤ 0.05), unchanged under a tone: white Gaussian noise shaped to the measured spectrum (`analog_noise.csv`, 40 bands): −101.7 / −82.7 / −101.2 / −108.8 dBFS for modes 1–4 |
| Release | see below |

**Release.** In the gain-reduction domain, two states (`ident_hc_release2.py`, `ident_hc_w0.py`): a slow state S
charges toward the output R as a one-pole on the linear gain (142 ms; the reference's own "Crest" is 149.6 ms) and
releases at `Fs(S) / Release + As(S)` dB/s; R falls by the same law at an effective release time
`min(Release, Release · w0(S), K / (R − S)^p)` and never below S. This is what WComp uses, with its tables refitted
by CMA-ES (below). What is known about the reference's release:

- The rate is a function of the gain reduction itself, the same at every ratio (after a 1 s charge at Release 300 the
  reduction falls 86 / 110 / 167 / 306 / 1364 dB/s at 17 / 19 / 23 / 29 / 39 dB, ratios 10, 2, 50, 50, 50), not of
  the level over the threshold.
- After a short charge (10–50 ms) the reduction falls along one curve whatever its depth (to about 15.8 dB 4 ms after
  the end, 12.9 at 8 ms, 11 at 20 ms, from 23, 29 or 39 dB); longer charges slow that fast part (500 ms: down to
  27 dB at 4 ms from 29). Below about 12 dB there is no fast part. This fast part barely depends on the Release time.
- A steady 100 Hz tone keeps its reduction almost still between peaks (0.05 dB ripple at Release 300), where the
  model's w0 term releases ten times faster; setting w0 = 1 fixes tones but loses the voice-like signal (−62 → −8 dB).
- **Start-up state.** For the first 0.1–0.3 s after a reset H-Comp releases slowly, with no gain reduction to show
  for it: a pitch spike that releases at once after 0.3 s of quiet holds its reduction after 0.02 s (and builds up
  over repeated spikes). Every probe with a short quiet lead-in, including the burst set the current law was fitted on,
  measured this start-up rather than the settled behaviour. Measured from 0.5 s on, the current law's errors stay the
  same, so they are not start-up errors.
- **Settled behaviour** (1 s quiet lead-in, `ident_hc_rel4.py`, 312 probes): bursts of 2–5 ms release almost at once
  at every depth (gone within 4 ms); 20 ms bursts nearly so; 50 ms and longer release progressively more slowly, up to
  about 500 ms of compression, after which the release no longer changes. A 4 ms burst with Attack 7 ms releases at
  once too. A slow average of R with a time constant of **148–159 ms** (fitted twice) explains much of this, close to
  the reference's own Crest value of 149.6 ms.
- **Laws tried** (all in `nulltest/`, none adopted): the release rate against R and a 2-D fast part fitted on the
  start-up probes (`ident_hc_rel3.py`); the release time scaled by a table of the distance between R and its slow
  average (`ident_hc_rel4.py`, hcfast4.c), with the average charging only upward or following both ways (the second
  fits better); a second, 30 ms average (hcfast5.c); a learned correction (8 tanh units) of the release time from the
  distances to 156 / 30 / 5 ms averages (hcfast6.c), trained with exact gradients (`hcgrad.c`, `train_hc_release2.py`,
  0.4 s per gradient over 482 cases). The average-based laws track steady tones (100 Hz: −27 → −54 dB at threshold −30)
  and modulated signals better, but lose the voice-like signal and the bass line (−59 → −13 dB at the defaults): at a
  pitch spike the distance between R and its average is the same as after a 50 ms burst, yet the reference releases
  the first at once and the second over tens of milliseconds. Fitting stalls from every start (finite differences
  and exact gradients alike, also with softened gates): the loss is very rough in these parameters.
- **Repeated events build up memory.** 2 ms pulses every 7 ms release fully between the first pulses, but after
  about 100 ms (Release 100; 75 ms at Release 300, 250 ms at Release 30; sooner at greater depth) the reduction
  hardly recovers between pulses (5 dB pulses: 4.8 dB minimum after 0.5 s); 2 ms every 20 ms builds up only at depth.
  This is the second form's structure (a slow state charged by R, which slows the release once charged).
- **The memory lasts through the release tail.** After a 500 ms burst and a quiet gap, a 5 ms burst still releases
  slowly when the gap is 100 ms (the gain already back to 0.05 dB), and at once after 200 ms or more
  (`ident_hc_rel8.py collectmem`, 90 double-burst probes).
- **Refit of the second form with CMA-ES** (`cma_hc_release.py`, a derivative-free search: least squares and exact
  gradients stall on this law's rough error surface). Objective: the mean null (energy-weighted, in dB) over 722 cases:
  the settled probes, the memory probes, the realistic envelopes and the null-test material (×3), all past the
  start-up. Mean null −46.8 → −49.0 dB; the null test's median −37.2 → −39.6 dB. At the defaults the drum loop goes
  from −20.7 to −31.7 dB with voice-like and bass line unchanged (−62.5, −58.5); threshold −30 / ratio 4 / attack
  0.5 / release 50 improves by 8–20 dB (bass line −18.6 → −39.4). Lost: the 5 Hz AM tone at the defaults (−68 → −52)
  and steady tones at Release 1000. Putting the attack before the release fits worse (−46.1 dB).
- **The reference's presets as data.** The 26 factory presets' settings on program signals (`ident_hc_rel8.py
  collectpresets`, 104 cases ×3) added to the CMA-ES objective: the presets' null median −30.4 → −31.3 dB.
- **Start-up state modelled.** The start-up lasts 0.2–0.4 s at every Release time (30 ms … 3 s). WComp pulls its
  effective release time toward the Release time by exp(−t / 90 ms) after a reset (the time constant scanned on
  start-up probes and on program material from 0.05 s: the probes prefer a longer, step-like start-up, program
  material about 85 ms). Null test median −40.0 → −41.9 dB; threshold −24 / ratio 10 / Release 300 −21.9 → −31.5;
  the defaults lose about 3 dB, in their first half second only.
- **The memory follows the reduction the listener gets.** With a long attack (Fat Kick: Attack 33 ms) the target
  swings to 9 dB on every half-cycle of a kick but the output only reaches about 1.5 dB; the reference then releases
  almost to 0 between half-cycles, as after short events, while a slow state charged from R (the reduction before the
  attack) held 5.7 dB. Charging the slow state toward out + 0.05 (R − out), out the reduction after the attack (the
  previous sample's), and refitting (CMA-ES, 1034 cases: −42.7 → −43.7 dB): presets' null median −32.0 → −35.5 dB
  (Fat Kick drums −12.9 → −21.7, Drum Punch voice −21.6 → −38.0, Pumping Kit voice −33.1 → −44.5, Drum Knockout
  drums −24.7 → −41.7); the null test's worst case −10.5 → −16.7, its median −41.9 → −40.3. Charging purely from
  the output (β = 0) breaks: before the attack starts the output is 0 dB, the slow state takes its release branch
  and is floored at the target, so it jumps to the full reduction at once.
- **The memory does not drain at very short Release times.** On a sustained bass note with Attack 17.7 and Release
  13.5 ms (Vintage Drum Crunch) the reference keeps 6–9 dB between cycles; WComp's slow state, which released by the
  slow law at the Release time, never got above 0.9 dB and the output followed every dip (−8.4 dB on the bass line).
  The slow state now releases with a release time of at least 25.7 ms (fitted with the rest by CMA-ES; objective
  −43.70 → −43.81 dB): Vintage Drum Crunch bass −8.4 → −17.0 dB, Lo-Fi Radio bass −24.3 → −27.5; Drum Punch voice
  −38.0 → −33.5; presets' median −35.5 → −34.8; the null test the same or better everywhere (deep slow setting
  −31.5 → −33.5). Fat Kick on noise stays at −12 dB.
- **Final joint fit.** The charge blend and the memory's release floor fitted with all the tables (CMA-ES, 250
  generations): blend 0.032, floor 16.8 ms, objective −43.81 → −43.97 dB; the null test's median −40.3 → −41.4 dB
  (worst −16.8), the presets' median −34.8 → −35.6 dB (Vintage Drum Crunch bass −17.0 → −19.1, Lo-Fi Radio bass
  −27.5 → −31.1). This is the law WComp ships with.
- **Tried: letting R fall below the slow state.** On noise with Attack 33 ms (Fat Kick) the slow state, charged from
  the output, and the floor R ≥ S hold each other at about 2 dB while the reference stays at 0–0.3 dB. Without the
  floor (the memory then only slows the release) the refit reaches −42.09 dB against −43.81 with it, so the floor
  stays (`hcfast2.c hc_set_r_floor`, `WCompTables.h rFloorAtS`).
- **Reference quirk (tempo sync).** After running synced (BPM or Host) and switching Sync to ms, H-Comp keeps the
  synced release time internally while its Release parameter shows another value; it takes the shown value again
  only once Release itself changes (and, coming from Host, not even then in one test). A fresh instance follows its
  parameters exactly (Host sync: the host's tempo, 17.86 ms for 1/64T at 140 BPM, −90 dB). WComp always follows its
  parameters, so for nulls after a synced preset, reload H-Comp. The preset null test uses a fresh reference per
  preset for this reason (with one shared instance two presets showed −8 … −10 dB that were really −31 … −37).

### WComp learned gain correction

**Why.** One-parameter sweeps from Threshold −24, Ratio 4 (`nulltest/sweep_wcomp.py`) showed the weak nulls were wider
than the null-test grid: voice, drums, bass and noise all −22 … −26 dB there; long attacks on bass −15 dB (15 ms) to
−8 dB (30 … 60 ms); Release 300 on voice −17. On a steady 55 Hz tone H-Comp's reduction depends on the attack time
(−7.0 / −6.1 / −4.8 / −3.4 dB of gain at Attack 0.5 / 7 / 30 / 100 ms) while WComp's stays at −6 dB, and at 1 kHz
neither depends on it; a pilot tone through both (`pilot_cycle.py`) shows H-Comp's gain simply settling higher, not
dipping inside the cycles: its level detection averages across the low waveform's cycles in a way the identified
structure (a held peak, then the release law, then the attack on the gain) does not. Removing the release floor
R ≥ S does not create the dependence.

**What.** A GRU (64 units) reads WComp's own states every 16 samples (the held peak against the threshold, the target
and release states R and S, the reduction after the attack and out, the settings; WCompRender `trace=`) and corrects
the compressor's gain in dB, held over the step (`plugins/WComp/WCompNet.h`, `GainNet` in WCompDsp.h). Guards: no
correction without reduction, nor while the target stayed within 0.05 dB over 20 ms (a steady tone above about
750 Hz, which the analytical model nulls already: without this guard the correction cost 1 kHz tones 20 … 30 dB).
Trained (`nulltest/train_wcomp.py`, PyTorch) on 4300 cases rendered through both (`render_trainset_wcomp.py`):
random settings and signals, hot drums / bass / plucks / voice up to +3 dBFS, the null test's and the presets'
settings on every signal at random levels; Mix 100, Output cancelling the makeup. The loss is the null itself for
the weak cases (worse than −35 dB) plus a penalty on every case more than 0.5 dB worse than without the correction
(or than −60 dB if it was better), the weak cases alone for the first 300 epochs. What did not work: the mean null in
dB over all cases (the good cases' gradients drowned the weak ones), training without the steady-tone guard, too
little data at the common settings (the default drum loop at its +2.7 dBFS level went from −31.6 to −24 dB until the
common settings were added densely).

**Result.** 200 unseen cases: the weak ones −25.8 → −36.6 dB, all −48.3 → −52.6, 106 better and 15 worse by more than
1 dB (the worst: −96.3 → −73.4). Null test (168 cases): median −41.4 → −47.0 dB, worst −16.8 → −24.5, 70 cases
better and one worse by more than 1 dB (pink noise at Ratio 50, −72.9 → −71.2). Presets: median −35.6 → −43.7 dB,
worst −11.9 → −20.8, 82 better and 4 worse (all from −59 … −78 dB, still there: Fat Kick voice −77.7 → −72.4). The
sweeps: the base setting −44 / −31 / −47 / −41 dB (voice, drums, bass, noise); bass under long attacks −31 (15 ms),
−15 (30 ms), −20 dB (60 ms); Release 300 on voice −22.5. Steady 55 Hz under Attack 30: −4.93 dB (H-Comp −4.79). Cost:
about 1 % of a core more (60 s of stereo at 48 kHz: 1.39 → 1.99 s).

### WComp needle (filmed)

**How it was filmed.** Outside FL, H-Comp's editor renders (about 62 OpenGL frames a second, 48 draw calls each) but
its frames never reach the window, which stays white (pedalboard and tools/MeterCapture, since 2026-10-21; the window
is visible, top-most and in the foreground; the GPU and the web view are not the cause). `tools/MeterCapture gl=1`
points the WaveShell's import of `gdi32!SwapBuffers` at a hook that reads the back buffer just before each swap: exact
design pixels at 63 fps, without FL and without the screen. The editor takes its meter values from the processing only
while the host's transport advances (with pedalboard's empty play head the needle stays put), so MeterCapture now
gives the plug-in a playing transport. Probes: `nulltest/make_vu_probe.py` (tones, 5 / 50 / 300 ms bursts, drums,
voice), `make_vu_cal.py` (a level staircase and sine / square pairs) and a set of descending steps; for GR the probe
carries a −54 dBFS 6170 Hz pilot whose gain through the compressor shows its gain during the silences too
(`pilot_gain.py`). Angles: `vu_angles.py`; pivot: `pivot_fit.py`; fits: `fit_gr_needle.py`, `fit_io_needle.py`;
the print: `measure_vu_face.py`. Summary: `measurements/hcomp/needle.json`.

- **A strip of 71 positions.** 11185 filmed frames show exactly 71 needle angles: frame k = round (70 v), each at a
  whole number of degrees from vertical, −58, −57, −55 … 57, 59 (a line fit per position: within 0.15 degrees of the
  whole degree; the table is in `WCompNeedle.h`). The needle turns about (323.42, 220.86) (pixel centres at +0.5; the
  lines of 800 frames meet there within 0.1 px), 2.7 px right of the scale's centre.
- **The needle's look** (`needle_core_fit.py`, `needle_shadow_fit.py`, `rivet_cmp.py`; compared with WComp's dial
  rendered by `tools/VuRender`, `compare_needle.py`). (124, 0, 0), 2 px across (equivalent width 2.0–2.4 px on both),
  from under the pivot's bump to its tip at 111.5 px; the rivet at 104.7 px: a red disc of radius 2.85 with a cream
  (248, 238, 158) centre of radius 1.15. The shadow is the needle's outline moved straight down whatever the angle:
  (−0.01, +4.70) px, a neutral darkening (×0.747 on R, G and B at its middle), Gaussian across (depth 0.266, σ 0.94 px),
  from under the bump to 110.2 px (fitted on 23 and 7 positions of the two faces alike). WComp draws it as three
  strokes fitted to that profile; measured the same way on WComp's render: (−0.01, +4.70), 0.265, σ 0.94, to 109.9.
- **GR.** v is the compressor's linear gain before makeup (steady tones: within 1.5 degrees of 31.75 + 117.17 g). The
  deepest gain since the last frame shows at once (a 5 ms burst moves the needle to 38); the return is two one-poles
  in cascade on the linear gain, 28.9 and 142.6 ms (97 % of frames within one step of the pilot-gain prediction).
  This reads more than a mean: on the probe's drums (threshold −30, ratio 4) H-Comp's needle averages 11.5 dB, the
  law on WComp's own gain 11.4, WComp's former VU average of the gain 6.2 (voice: 5.6 / 5.8 / 2.0).
- **In / Out.** v = (peak dBFS + 48) / 48, the peak over the frame's interval (a sine and a square of the same peak
  read alike; −48 dBFS and below rest on the stop). A rise shows at once and starts a hold that decays from that
  moment (amplitude, 0.327 s: 26.6 dB/s into silence), so a burst's needle falls from its onset, and at the end of a
  300 ms burst it jumps down to where that fall has reached. The first drop after a rise shows the level at once
  (unless the hold is still higher: a long tone drops straight to its silence); every later drop decays toward the
  level with the same 0.327 s until the next rise. 97.5 % of frames within one step (probe, staircase, descending
  steps; Out on the reference's own output).
- **Two faces.** GR: 48 24 12 6 3 0, the red band from 6 to 0. In / Out: dBFS, 48 36 24 18 12 6 3 0 with 24 at the top,
  the red band from 24 to 0, other dividers, ticks and dots (`needle.json`).
- **The limiter lamp** lights whenever the output limiter (or the clip, with Limiter Off) acts: at full, fading to
  0.64 (0.64 + 0.36 e^−t/0.393 s), off 0.833 s after the last trigger; a new trigger brings it back to full. Lit, the
  dome glows amber (142, 115, 0) at its centre, dark by radius 8.
- **What the earlier readings were.** The "6 on H-Comp, 9–10 on WComp" seen in FL came from WComp's needle of that
  time (the deepest reduction per frame through a VU movement in dB); the provisional VU average that replaced it
  read about half of H-Comp. WComp now uses the filmed laws (`plugins/WComp/WCompNeedle.h`); filmed the same way
  (screen grab, 38 fps), its GR needle averages 11.5 dB on the drums (H-Comp 11.5) and 5.9 on the voice (5.6).
- **Residual.** The printed arc's centre is (320.3, 220.7), about 0.4 px up-left of the centre WComp draws its print
  about; the numbers, dots and ticks were measured individually and sit within about 0.5 px.

# WDelay: the editor (Phase 2)

Measured on the user's own install (*H-Delay Stereo* / *Mono*, `tools/MeterCapture` screenshots and drag scripts on the
real editor; screenshots stay in the git-ignored `reference/hdelay/`), 2026-10-24.

### Parameter tapers (read from the reference: each parameter set to 101 normalised values, its text read back)
- **Delay**: 1 ms per 1/1000 of the range up to 501 ms at the middle (exact), then a curve to 3500 ms that no simple law
  fits within the display's 1 ms (a power law is 57 ms off, a cubic 14 ms); WDelay keeps the reference's values at
  201 points and interpolates between them.
- **Rate** 0.1 … 6000 Hz and both **filters** 19 … 20 138.8 Hz: logarithmic (the middle at 24.5 Hz and 619 Hz, the
  geometric means); HiPass is Off at the bottom, LoPass Off at the top (from 20 100 Hz). **BPM** 30 … 300,
  **Feedback**, **Mix**, **Depth**, **Output**: linear. The reference keeps fractional values (its presets store Depth
  0.69 %, Delay 243.9 ms), so WDelay's values are continuous and only their texts are rounded.

### Drawn appearance (fitted on the screenshots)
- **Panel**: a base colour (183.5, 100.5, 36) with a broad warm highlight (+42.5, +55.9, +15.8) centred at (107, 73)
  over about 100 × 106 px, darkened from y 229 to 0.75 at the bottom and toward the side edges (0.77 left, 0.70 right,
  over about 21 px), a bright line under the toolbar; residual 2.4 levels rms (the grain).
- **Knobs**, fitted as H-Comp's (`tools/fit_hybrid_knobs.py`, captures of Feedback, Mix, Output and the four small knobs
  typed to nine values): Delay and Feedback are fluted black knobs in a chrome skirt of radius 63.4, the flute ring
  (mean radius 49, ten flutes ±2.3 px) 1.1 px right of the skirt's centre and the cap 1.1 px right and 2.3 px down (a
  raised knob seen a little from above; fitting all three about one centre blurred it), the filmstrip's flutes up to
  3° off the pointer's angle (each capture's own flute phase is used); the pointer fin from radius 45.5 to 64.5. The
  small knobs (radius 32.4, centres at (181.4, 223.2), (286.2, 222.6), (407.2, 222.6), (542.3, 222.6)) have a knurled
  rim; the right column's (radius 23.65) a light line on the cap from the centre to 13.5 px. Scales: 21 ticks of 14.1°
  from 39° to 321° on every knob (Analog 5), radii 64.3 … 72.2 / 33.5 … 38.7 / 24.7 … 30.6.
- **Meter**: two red triangles, one per channel, pointing inward along the cream scale; the printed marks lie at
  y = 83.3 + 157.3 (1 − 10^(−dB/40)) within 0.5 px (0 at 83.3, 6 at 129.3, 24 at 201, 48 at 231.7; rest at 239.3).
- **Display** 292, 66, 139 × 72: dark glass with a smoky streak; amber segment characters (255, 180, 37 at their
  brightest): the note as a stacked fraction and T / D, the tempo in 13.5 × 23 px digits with "BPM" under it (Host,
  BPM), four digits of ms (MS), the time and the start side in an amber box (Ping Pong), "L" and "R" halves (Dual).
- **Toolbar**: H-Comp's, its preset box 106 px wider (the editor is 749 px wide, H-Comp's 643).

### Control feel (MeterCapture drag, click and wheel scripts)
- **Knobs**: H-Comp's law on every knob — 0.001 of the range per px on mouse steps up to 2 px, 0.002 from 3 px, Ctrl a
  quarter of either; the wheel 1/72 of the range per notch; Analog one position per 50 px; the Delay note one value
  per 50 px slow, two per 50 px fast (H-Comp's note law, kept). WDelay gives the same values on these scripts within
  the display's last digit (Feedback 60 → 70 / 70 → 50, Output −1.8 → −1.6 with Ctrl, Delay in MS 375 → 425 ms slow,
  −100 ms fast, Rate 0.10 → 0.17 Hz).
- **Display**: dragged or wheeled with the same law, it sets the time in MS and the tempo over its digits in BPM
  (+27 BPM for 50 px fast, +12 for three notches). (Over a note value it seemed to do nothing; filmed again it steps
  the note, see "Ping Pong and Dual" below.)
- **Link** wheel: both filters by the same share of their range, with the knobs' law, as far as the one nearer its end
  allows (HiPass 154 → 77 Hz and LoPass Off → 10.0 kHz for 50 px fast down; three notches 77 → 103 Hz,
  10.0 → 13.3 kHz; WDelay 13.4).
- **Test artefact**: the drag script moves the mouse out of the editor after each drag; the next drag then loses part
  of its travel on both the reference and ours (Rate 0.10 → 0.12 instead of 0.17 Hz). `away=0` keeps the mouse over the
  editor; even so some scripted drags on the reference do nothing at random, so laws are read from fresh sessions.

### Second pass (pixel by pixel), 2026-10-24
New captures (git-ignored): the display at all 19 note values that fit at 120 BPM (the wheel on the note knob), the tap
pad held down and after the release, MS with Dual, BPM with Dual at 1 bar. Mean difference over the editor below the
toolbar 13.9 → 8.0 levels; the panel's flat parts within 1.6, no colour bias left.
- **Display font** (`tools/fit_hdelay_display.py`: a 7-segment renderer fitted to the reference's amber coverage, R − B
  of its pixels): cells, strokes, insets and slants per size — the tempo and ms digits 12.85 × 22.65 px, strokes
  2.45, slant 0.175, pitch 17.05, their 1 one stroke; a note's figures 9.85 × 16, strokes 2.0, slant 0.19; T and D
  fitted on their own; the fraction's bar one dash per denominator digit (pitch 13.6); "BAR", "BPM", Ping Pong's and
  Dual's sizes each fitted. The figures of a note are not evenly spaced (a bitmap font): their positions are fitted on
  every note (1 at 302 or 308, T / D at 321.7 or 331.7, the denominators each their own). Coverage rms 0.04 … 0.11 of
  full per item; every display state 4 … 5 levels off the reference.
- **Notes too long**: a note whose time at the tempo passes 3500 ms is not taken (at 120 BPM the knob stops at 1 bar;
  2 bars are 4 s). WDelay holds the longest that fits.
- **Glass**: the reference's grey as a 6 px grid of medians over the captures (glyphs masked), its lip (171, 147), a
  sunk window (1 px black rim, a dark inner ring, the panel lit along its right and bottom edges, darkened on the left).
- **Tap pad** (`tools/fit_hdelay_tap.py`): two zones (the body by its depth in from the outline, the face by its depth
  in from its frame) with per-side profiles, released and pressed (pressed: the top lip dark, the face sunk, its lower
  edge lit); fit 6.0 / 6.7 levels rms, render 3.8 / 4.1 off outside the mark. Its cast shadow: 0.44 deep, (0.24,
  1.81) px. Selected after a click: a 1 px yellow line just outside the pad.
- **Meter housing** (`tools/fit_hdelay_meter.py`): the frame by depth per side (7.8 rms, the corners), the window as a
  rank-3 product of column and row profiles with the print masked (2.5 rms); the scale y = 83.70 + 157.82 (1 −
  10^(−dB/40)) fitted on the ticks within 0.3 px; ticks 7 px from both sides; numbers in a bold serif, red (136, 12, 10)
  to 24, olive below, at their measured ink boxes; the triangles 10 × 8 px, a pink lip on top, a shadow under them,
  their point 1 px below the scale's −∞ at rest. Mean difference 5.7.
- **Buttons** (the H-Delay look): a 2 px black frame and 2 px dividers; faces (215, 209, 186) lit (173, 166, 139),
  three lit rows on top, two lighter rows and a 4-row bevel at the bottom, dark columns next to the dividers; labels
  Arial Bold 10.2 px unstretched (the best of ten system fonts on the reference's ink); a sharp shadow 0.63 of the
  panel for 6 rows under each button, 0.87 under a lit one. Selection as the reference: one control at a time — a knob
  (its ring), a group (Sync: the yellow line over its frame's outer edge), a button of the row (the line round its
  face), LoFi, the tap pad.
- **Panel edges**: plateaus, not a fall-off — 0.89 inside the left edge, 0.82 inside the right, 0.795 along the bottom;
  a 1 px black outline; under the toolbar a brown-grey row (115, 96, 81) and a pale lip (not orange).
- **Print**: black (H-Comp's is a dark brown); the brackets' strokes end 2 px from their words. The name: "DELAY" in
  Arial at the reference's 81 × 18 px from x 42; our W in Arial Black narrowed to the reference's first letter.
- **Knobs**: the Delay / Feedback tables gained the flutes' second and third harmonics and 16 rim bins (no measurable
  change: the remaining 20 levels on the flute ring come from the edge's sub-pixel position, the chrome's fine streaks
  and the cap's scratches, which these smooth fits do not hold); the shadows refitted with less of the print masked.
- **Lamp and Link**: hand-fitted on their pixels (the lamp's bezel bright only on top, its red arc 4 … 7 o'clock, the
  Link wheel's 8 square teeth about (473.2, 218.3), the ring of 19.2 about (473.8, 219.2), the arc's top at y 195).
- **Not matched**: the text's hinting (the reference's glyphs sit on whole pixels; JUCE draws outlines), the toolbar's
  own content (ours), the knobs' fine texture.

### Ping Pong and Dual (filmed), 2026-10-25
Captures of the reference with Ping Pong and Dual switched on and off in each Sync mode, the start box and Dual's sides
clicked, and notes, times and the tempo dragged on the display (`reference/hdelay/ppdual`, git-ignored).
- **Two independent switches.** Ping Pong and Dual can both be on. Ping Pong's layout then shows, and the Delay knob sets
  the left side.
- **Ping Pong's layout** is Dual's left half, the same pixels: the small note and the tempo (Host / BPM) or the left time
  (MS), with no L / R labels and no divider. On the right is the start box.
- **The start box** (`tools/fit_hdelay_display.py ppbox`, rms 0.038 of full coverage):
  - An amber (255, 180, 0) box 391.2 … 412.8 × 95 … 122, leaning 0.04 px right per px up, with corners of 2.1.
  - The letter is straight strokes that stop short of each other, as on a segment display: a 1.27 px black core in a
    1.65 px stroke at 0.53.
  - Unselected, its rim is a black line down the left and rows 0.27 darker above and below.
  - A click flips the start side and selects the box, drawn as a yellow line round it at half strength.
- **Dual's sides.**
  - Clicking the left or right time or note chooses the side the Delay knob sets, and dragging it sets that side's value.
  - On the right side, a small box follows DELAY under the knob: black, 11 × 10 px at (235, 169), with a half-dark
    shadow along its left and bottom and the R cut out of it in the panel's colour. It is drawn over the knob's shadow.
  - The chosen side is kept while Ping Pong is on, then shown again when it goes off.
  - Switching Dual on starts the right side from the left's time.
- **Display drags.**
  - A note steps 1 value per 40 px of drag, as on the knob, in every layout. The second pass had this wrong: the note
    was not draggable.
  - In MS, a drag on the right time moves it 1 ms per px.
  - In BPM, the tempo drags in Dual's and Ping Pong's layouts too: 20 px → 5 to 6 BPM.
- **Hit areas** are the reference's control rectangles (GUIXML); where two overlap, the first listed takes the point:
  | Layout | Mode | Area (x, y, w, h) | Sets |
  |---|---|---|---|
  | Ping Pong | | (390, 94, 26, 29) | the start box |
  | Dual, Ping Pong | MS | (293, 100, 66, 22) | the left time |
  | Dual | MS | (321, 78, 104, 55), the rest of it | the right time |
  | Dual, Ping Pong | notes | (298, 95, 34, 34) | the left note |
  | Dual | notes | (392, 95, 34, 34) | the right note |
  | Dual, Ping Pong | BPM | (338, 94, 42, 21) | the tempo |
  | Plain | MS | the whole display | the time |
  | Plain | notes | (300, 80, 45, 49) | the note |
  | Plain | BPM | the rest of the display | the tempo |
- **Mean difference** to the reference: the display within 3.8 … 4.7 levels in every layout; the start box 2.2 … 4.2;
  the R mark 2.5.
- **Button labels** sit within 0.25 px of the reference's. What is left is its pixel-snapped glyphs.

## WDelay: H-Delay measurements (Phase 3), 2026-10-25
The reference rendered offline through `tools/RefRender` (`nulltest/hdelay.py`). A run of `nulltest/measure_hdelay.py`
writes `measurements/hdelay/results.json` and the echo kernels (`kernel_*.csv`). Unless stated: 48 kHz, Analog Off,
Feedback 0, Mix 100, Delay 375 ms, small impulses (0.001).
- **Harness.**
  - H-Delay shows but does not process values set before its first block, so RefRender now renders a silent pre-roll
    (`preroll=`) and sets the values after its first block.
  - A delay time set in the same block as Sync is not taken. Times are set as timed changes (`at=`, also inside the
    pre-roll, sample exact).
  - Choices are set by their normalised value; the shell does not parse its own choice texts.
  - Rates of 1 kHz and above must be typed in its own form ("1.00k"): "1000" is not read as 1000 Hz.

### The delay line
- **Interpolation.**
  - The echo of an impulse is a windowed sinc. Its window is Kaiser, half-length 15.89 samples, β 4.93, cut off at
    Nyquist. Fitted within 7e-6 (`kernel_48k.csv`); the same at 44.1 kHz.
  - The read position is quantised to 1/32 sample, one phase above the true position: the delay is
    (⌊32 d⌋ + 1) / 32. At a whole-sample delay the kernel is the 1/32 phase.
  - A sweep of 688 echoes during a slow glide gives fractions within 0.0005 of a phase. The delays agree with this
    rule within 0.01 samples rms; a few are one phase off.
  - A static time is a whole number of samples: Host and BPM notes at any tempo give the same kernel.
  - At 96 kHz the same filter in time: cut off at 24 kHz, half-length 31.06, β 4.71, the read 1/16 sample late.
- **The glide** (the documented tape behaviour). After a change of time the delay moves as
  d' = max(−1, (T − d) / T) per unit time:
  - an exponential approach with a time constant equal to the new time T;
  - the playback runs at most twice its normal speed;
  - no limit when the delay grows, because (T − d) / T < 1.
  Measured with a 500 Hz tone's phase on 375 → 100, 50, 365, 1000 and 3500 ms: the model within 0.013 … 0.092 ms rms.
- **Gain stage** in the loop, before the delay line:
  - The wet gain is 1 / (1 + peak): an impulse of a comes back at a / (1 + a) (1.0 → 0.500, 0.5 → 0.667, 0.25 →
    0.800, 0.1 → 0.909), with the kernel unchanged.
  - It takes the peak at once, the sample itself included.
  - The gain recovers toward 1 as a one-pole of 10 ms (per-sample 0.997919; Phase 4 refit, the 9.3 ms first read
    from a poor log fit).
  - Linked across the channels: a 1.0 impulse on L brings R's 0.01 down to 0.5 as well.
  - A steady sine of amplitude A comes back at 1 / (1 + A), with odd harmonics: h3 −48 dB at A 0.5, −59 at 0.1.
  - This stage is what keeps Feedback above 100 % bounded.
- **Feedback.**
  - Each echo is Feedback / 100 of the one before, exactly at small levels: 0.1, 0.3, 0.6, 1.0 (less the gain
    stage's 1/(1 + 0.001)), 1.5, 2.0.
  - The kernel is applied again on each pass (the second echo is the kernel convolved with itself).
  - Each pass round the loop adds 1 sample of delay.
- **Filters** in the loop (the second echo is filtered twice):
  - HiPass and LoPass are RBJ biquads with ParamXML's Qs: HiPass Q 0.8, LoPass Q 0.95.
  - They match within 0.05 dB at HiPass 100 / 1000 Hz and LoPass 1000 Hz.
  - LoPass 5000 is 0.3 … 0.7 dB lower than the biquad near its cutoff; to be refined.
- **LoFi** swaps the interpolator for one cut off at 6277 Hz (`PARAM_INTERPOLATION_LP_FREQ` 6280), with the same
  window (half-length 15.80, β 4.87) and 1/32 phase. It is flat to 4 kHz, −1.05 dB at 5 kHz, −4.5 dB at 6 kHz, then
  −56 … −68 dB above 10 kHz. No aliasing products: a 1 kHz tone gives only odd harmonics below −78 dB.
- **Modulation.**
  - The LFO moves the target time: T = Delay × (1 + 0.5 × Depth / 100 × triangle).
  - The glide law smooths it, so the swing shrinks as the rate rises: Depth 100 at 0.25 / 1 / 4 Hz gives 93.5 / 74.1 /
    31.5 ms peak to peak around 100 ms.
  - Linear in Depth (25 gives exactly a quarter of the triangle's span); the same on both channels.
  - Fitted within ±0.3 ms (`nulltest/fit_hd_lfo.py`).
  - At audio rates it gives FM sidebands at Delay 5 ms: Rate 100 Hz, first sideband +9 dB.
- **Sync.**
  - Host and BPM note values land on the note's time (500.0, 444.44, 300.0 and 225.0 ms), less what the glide has
    not yet covered.
  - Without Dual the two times are linked: setting Delay R moves both (the last one set wins).

### Routing, mix and output
- **Mix** is equal-power: dry cos(π/2 · m), wet sin(π/2 · m) (25 %: 0.924 / 0.383). **Output** is an exact dB gain on
  both. Nothing limits the output (+18 dB passes a 0.9 sine at 7.15).
- **Phase L / R** invert their side's input to the delay line; the dry signal stays. (In Ping Pong, Phase R gives
  (L − R) / √2 into the line; Phase 4.)
- **Ping Pong** takes (L + R) / √2 into one line.
  - The first echo comes on the start side, the second on the other side at the same level, then every further pass
    at Feedback² (Feedback 30: 0.707, 0.706, 0.064, 0.064, 0.0057 …).
  - So the loop is: the start side's line → the other side's line at gain 1 → back at gain Feedback².
  - Each line adds its sample.
- **Mono** behaves as the stereo component. **Mono/Stereo** feeds its single input into Ping Pong at √2.

### Analog
The stage colours the input, before the dry / wet split (Phase 4: with Mix 50 its noise comes back in the echo, at a
correlation of 0.50 at the delay's lag), so the dry and the delayed signal both carry it.
- **Mode 4** is H-Comp's: WComp's fitted model applied to H-Delay's Analog-Off output nulls it at −100 dB (peaks 0.55)
  and −79 dB (peaks 0.06).
- **Modes 1–3 are not H-Comp's**, although ParamXML carries the same numbers: no H-Comp mode nulls them better than
  −31 dB.
- Small-signal responses at −30 dBFS (noise in, `results.json` `analog`):
  - mode 1 a low cut (−3.6 dB at 30 Hz, −0.3 dB in the mids);
  - mode 2 a high shelf (−0.67 dB below 200 Hz, −1.55 dB above 4 kHz);
  - mode 3 a deep dip near 100 Hz (−13.9 dB) that depends on level;
  - mode 4 within 0.9 dB.
- Noise with silence in: −100.9 / −83.7 / −104.0 / −106.8 dBFS for modes 1–4, and −122 dBFS even with Off.
- Phase 4 fits modes 1–3 as WComp's were fitted.

### For Phase 4
The delay path is identified to the sample: interpolation, glide, gain stage, feedback, filters, LoFi, routing, mix.
Sample-exact nulls should be possible with Depth 0 and Analog Off or 4. Still to be pinned down:
- the last 1/32 step of the glide;
- the LoPass near its cutoff;
- whether the gain stage reads the loop's sum or the input;
- how Feedback 200 % settles;
- the modulation at audio rates;
- the Analog modes 1–3.

## WDelay model (Phase 4 identification), 2026-10-25
`plugins/WDelay/WDelayDsp.h` (shared by the plug-in and `tools/WDelayRender`) against the reference through the same
host (`nulltest/nulltest_wdelay.py`).
- **Harness.**
  - Both sides get the same silent pre-roll, rendered in whole blocks as `tools/RefRender` renders it: a 3 s
    pre-roll is 144 384 samples, not 144 000, which matters while a glide or the LFO runs.
  - The reference's first block runs at its defaults, Analog Mod2 included, and puts that noise into its delay line.
    Cases therefore start with Feedback 0 and set the case's Feedback at the input's start.
- **Interpolator.**
  - A Kaiser-windowed sinc, half-length 511/32 and β 4.968, its edge value kept. Its 31 taps sit around ceil(d).
  - The read is (⌊32 d⌋ + 1) / 32. A whole d is read 1/32 late, about the tap d itself.
  - Impulses null at −95 dB.
  - The window is not quite Kaiser (up to 4e-4 off). The sinc hides that in the normal kernel, but LoFi, the same
    window under a sinc at 6277 Hz, shows it: LoFi nulls at −70 dB.
- **Gain stage:** the recovery is a one-pole of exactly 10 ms. Nulls went from −50 to −100 dB with it.
- **Glide.**
  - d' = max(−1, (T − d) / (T + 16)), each sample's step taken before its read (with the LFO: on D = d + 16, below). The offset is sharp: 15.5 or 16.5
    lose 30 dB on a falling glide.
  - Glides that run inside the input null at −60 … −83 dB. In the slow tail of a glide the delay drifts across a
    1/32 phase every many samples, and a sample's difference in when that happens leaves −40 … −50 dB on broadband
    material. Double- or float-precision variants of the update do not reproduce the reference's steps; this stays a
    known limit.
- **Filters.**
  - RBJ biquads, HiPass Q 0.8 and LoPass Q 0.95, on the line's OUTPUT (after the read; with a moving delay the two
    places differ, and only the output nulls).
  - Their frequency is warped: w0 = 2 atan(π f / fs), so LoPass 4000 sits at 3912 Hz and 15000 at 11 859 Hz.
  - Nulls −90 … −100 dB.
- **Phase L / R** invert the side's input to the delay.
- **Ping Pong.**
  - The left line feeds the right at gain 1, the right the left at Feedback²; (±L ± R) / √2 goes into the start
    side's line.
  - Nulls −97 … −100 dB, either start side.
- **Modulation** (second round, from the user's feedback "the modulation depth does not null").
  - The LFO is a float triangle between ±1, from 0 rising, a step of (float)(4 rate / fs) a sample, clamped at each
    end where it turns (the overshoot is lost). Each half-cycle is therefore a whole number of steps, ceil(2 / step):
    rates whose half-period falls just under a whole number are the sensitive ones.
  - It moves the target: Tm = (T + 16)(1 + 0.5 depth tri), and the glide runs on D = d + 16:
    D' = max(−1, (Tm − D) / Tm), the step before the read.
  - In Host / BPM sync Rate counts cycles per beat at 60 BPM: the LFO runs at Rate × tempo / 60.
  - Rate is taken as shown, to three significant digits and at least 0.01: 3074 acts as 3070, 1537.24 as 1540,
    281.7 as 282, 47.84 as 47.8, 0.537 as 0.54. Found by rate scans (`nulltest/probe_hd_mod.py`) and the delay trace
    of a steady sine (`probe_hd_lfo_trace.py`, the period over 85 cycles): before the rounding 3074 Hz nulled at
    −10 … −54 dB, after it −76 … −84 dB, as every other rate.
  - Nulls on modulated material −67 … −84 dB (Depth 39 / 100, Rate 1 … 6000 Hz, delays 30 / 750 ms).
- **Parameter resolutions: two paths** (found from the user's feedback "some presets do not have the same
  settings"; `nulltest/nulltest_wdelay_refpresets.py` loads the reference's own preset arrays into its state).
  - Set from outside (a host, automation, a knob), a value is used at its ParamXML resolution: Delay, Feedback,
    Mix, BPM, Depth and the filters whole units (Depth 0.69 acts as 1), Output 0.1 dB, Rate to three significant
    digits and at least 0.01 (3074 acts as 3070, 281.7 as 282, 47.84 as 47.8). LoPass from about 20 099 up is Off.
  - Loaded from a preset, a value is used exactly as stored: Depth 0.69, Rate 281.7 and 47.84 (the delay trace's
    period: 1004 samples, not 47.8's 1006). LoPass is Off only at exactly 20 100, its reset value; nine factory
    presets store 20 139, which the reference shows as "Off" but runs as a filter at 20.1 kHz (impulse nulls −100 dB
    against a 20 139 Hz filter, +0.9 dB against Off). A stored Delay is still taken in whole ms (6.6 acts as 6).
  - WDelay does the same: a factory preset's values are kept in the state (wavy::presetExactTag) and used while a
    parameter stays where the preset put it; moved, it is used at its resolution.
  - Triplet notes are a hair short (× (1 − 3e-9)): 1/8T at 120 BPM is 7999 samples, not 8000.
- **Self-oscillation with the filters** (Feedback above 100 % with HiPass and/or LoPass on, as in the preset Fast
  Siren) is chaotic: in WDelay's own model a 1e-6 change of the input grows to −4 dB within 3 s. Such a loop nulls
  for its first fraction of a second and then cannot, by any model short of a bit-exact one. Without the filters the
  same loop nulls at −62 … −73 dB.
- **Other rates.**
  - 44.1 kHz behaves as 48 kHz (−95 dB).
  - At 96 kHz the delay settles one sample short (an echo at T − 1 + 1/16). The interpolator has 62 taps, cut off at
    24 kHz. An impulse's first echo nulls at −78 dB.
  - At 96 kHz the LFO runs twice as fast: its step is 4 rate × factor / fs (factor 2, the interpolator's). Found
    from the delay trace (a 0.6 s period at Rate 0.8); the modulated case went from +2.6 to −48 dB.
  - The 96 kHz nulls are dominated by the band above 20 kHz, round the kernel's 24 kHz cutoff, where the kernels
    differ (`nulltest_wdelay.py --fs 96000 --band 20000` measures below 20 kHz): the delay path −46 … −61 dB below
    20 kHz, against −28 … −60 dB over the whole band; loops at high Feedback −31 … −44 dB.
- **Analog** colours the input. Mode 4 is H-Comp's. Modes 1–3 are fitted with WComp's model on H-Delay's own data
  (`nulltest/ident_hd_analog.py`, `fit_hc_analog2.py` with ANALOG_PREFIX=hd), exported by
  `tools/export_wdelay_tables.py`.
  - Fitted nulls (median over the 54-case probe set): mode 1 −53.0 dB (refitted in the second round; its low dip
    as a band-pass before H1), mode 2 −41.6, mode 3 −41.3 (band-pass 71 Hz, Q 0.41). The quietest probes are
    limited by the modes' own noise.
  - Low-level transfer functions (`nulltest/probe_hd_analog_tf.py`, a multitone at −60 dBFS per tone, Mix 0):
    modes 1 and 2 agree with the reference to −50 … −85 dB at every frequency. Mode 3 is off by −18 … −40 dB below
    1 kHz (the reference 1 dB lower below 20 Hz, up to 1 dB higher around 100 Hz), and the ratio is not a smooth
    filter: mode 3 is still nonlinear at these levels (its single-sine response at −50 dBFS differs from the
    multitone's), so a correction EQ cannot fix it. It needs a different structure (docs/IMPROVEMENTS.md).
  - Noise with silence in, Mix 0: −102.7 / −85.5 / −106.0 / −108.9 dBFS; independent per channel.
  - In the null suite: Analog 1 −53 … −56 dB, 2 −51 … −55, 3 −40 … −42 (the noise included), 4 −83 … −90.
- **CPU:** about 1.6 % of one core for stereo at 48 kHz (`WDelayRender`, Feedback 60, Analog 2).

## WBass Rider: the reference editor (Phase 2), 2026-10-25
Bass Rider's editor read back from its OpenGL frames (`tools/MeterCapture gl=1`; with `drag=...,hover|click` scripts the
screenshots now come from the same read-back), its skin in the git-ignored `reference/wbassrider/`.
- **Layout**: 320 x 560 including the two-row toolbar (36 px). The top panel (y 36 .. 162, x 6 .. 313) and the cream
  panel (y 162 .. 510, a trapezoid: x 5 .. 314 at its top, 0 .. 318 at its foot) carry a gentle shading and grain
  (rms about 3 and 2 levels), fitted as a colour per row times a factor per column after removing the lines and wells.
- **Scales**: +12 dB at y 222, 0 at 327, -12 at 432 (8.75 px per dB) for the Rider, the Output and the Range column;
  the Rider's marks every 3 dB (numbered every 6, 2 px thick at +-12 and 0), ticks every 0.5 dB.
- **Faders**: Range Max's cap ends at its value, Range Min's starts there; the lit band between them reaches the
  Rider's scale; the Idle arrow and a light red line sit at the Idle value (its place in the range, PARAM_PARK_GAIN).
- **Knobs**: a 71-frame strip turning +-132 degrees (frame 0 at -132, 35 at -3, 70 at +131); dome graded ~196 to
  ~110 top to bottom, a black line at its edge, a bright arc at the top and a reflection near the bottom.
- **Selection**: hovering shows nothing; a click marks the control in yellow (a ring on knobs, a frame on caps and
  the switch) until another control is clicked; the selected control's value shows while the mouse is on it — knobs
  in their value slot under the label, Range Max / Min in a box right of the cap — in a dark box with a 1 px white
  frame. The Response switch's click toggles it and marks the preset modified.
- **Caps as drawn**: each fader cap carries a 1 px dark outline and a dark lip under it (Rider: x 133 .. 186,
  y 294 .. 362 at 0 dB; Range Max x 41 .. 79, y 200 .. 219 at +12; Output x 238 .. 276, y 319 .. 338 at 0); fitted on
  the frame itself (`tools/fit_bassride_parts.py`), since the 2x skin reduced to 1x shifts their ridges by a row.
- **Scales, exactly**: the minor ticks are not on a dB grid: five half-pixel ticks (grey 101) at 5, 9, 13, 17, 21 px
  below each 3 dB mark; the Output's 3 dB marks are 6 px long (11 at +-12 and 0). Numbers are centred (x 117.5 / 202
  / 291), Arial about 10 px.
- **Drags** (MeterCapture drag scripts): the faders and the Idle arrow move 8.75 px per dB, the Target 4.31 px per dB,
  the knobs 0.1 per px on slow moves (the suite's knob feel); the cursor is hidden during every drag and comes back
  where the thumb went. The Range box below the column and the band do not drag.
- **The Rider grabbed**: it follows the mouse, outlined in blue (its own selected look, as the Idle arrow's); on
  release it goes back to riding at once.
- **Text entry**: a double-click opens it on the knobs, Target, Output and Range Max / Min (in their value slots);
  a value outside the range is ignored (Target 25), Spill reads a typed positive number as negative (30 -> -30); the
  Rider, the Idle arrow and the Range box take none. A single click and typing does nothing.
- **Idle**: dragging the arrow shows its value in a box fixed at the left edge (3, 323, the IdleHover control); the
  host's text for Idle maps it over a fixed +-12 dB (0.5 shows 0.0 whatever the range), while the arrow sits at its
  place in the current range.
- **Lasso**: a background drag draws a 1 px white rectangle and selects the controls it touches (in the control order
  Spill, Rider, Range Max, Range Min, Output, Sensitivity, Target, Idle, Response, Artifacts): the first yellow, the
  others blue; clicking a member makes it the yellow one; dragging a member moves nothing; with the mouse on the
  yellow one every member's value shows.
- **Hover and animations** (MeterCapture script mode now streams every OpenGL frame with an action log, and saves the
  cursor): hovering changes no pixel anywhere; the cursor over Sensitivity, Artifacts and Output (the DisplayCursor
  controls) is an arrow with a small up / down arrow (hotspot 6, 7), elsewhere the system arrow; the Response switch
  flips in one frame (no slide). The knobs' scales are static: only the dome turns.
- **Knob scales, measured round the domes' true centres** (x 46 / 200 / 275, y 111.5, half a pixel above the GUIXML
  rectangles'): Sensitivity has 12 dashes at +-(11.3 + 22.67 k) degrees, 16 degrees long and 4 px deep, from radius 20
  at the top drifting out to 22.3 at the ends (grey 131, the last pair 100); Spill and Artifacts 13 ticks at 0, +-23,
  +-46, +-69, +-91, +-115, +-135 degrees, each with its own length (radius 20.3 .. 27.3) and grey (100 .. 147). The
  dome is fitted on the frame (a grey per radius and two angular harmonics), the pointer a 2 px core from radius 2.5 to
  17.5 (grey 87 .. 104).
- **Response switch, clip LED, band**: the switch's block is identical in both states, 15 px apart, the slot's gradient
  mirrored (darkest by the block); the LED is a dark 15 x 10 frame whose inside changes by row (off 55 .. 87, lit
  red-orange 173 .. 245 with a red glow beside and below); the lit band is a warm light (255, 253, 209) at 0.21.
- **With audio** (films of steady 80 Hz tones and a bass line, `nulltest/read_br_film.py`: the Energy meter's lit
  length, the Rider cap's place by template, the clip LED, and the reference's own output gain):
  - the Rider shows the gain being applied (within its 1 px reading, 0.1 dB); the reference rides note by note: a
    note gets one gain, close to Target minus the note's weighted energy, held for the note; a level change inside a
    held tone moves it only partly (-10 -> -30 dBFS: -1.5 -> +1.6 dB); soft notes right after loud ones are often not
    new notes; at a note's start the shown gain overshoots and settles within 0.1 s (the audio, 42 ms later, already
    at the settled gain); after a note it waits about 50 ms and returns to the Idle value within about 0.1 s;
  - the Energy meter reads a steady 80 Hz sine 1.9 dB under its rms (-20 -> -21.9), low notes lower (45 Hz about
    -6 dB, a high-pass weighting), rises at once and falls 37 dB per second at most;
  - the clip LED latches at the first clip and stays lit until clicked;
  - a provisional model of all this (`plugins/WBassRider/WBassRiderProvisional.h`, fitted by
    `nulltest/ride_provisional.py`) drives WBass Rider's editor until the DSP is identified: filmed in a host the same
    way, its meter is within 0.0 dB (median) of the reference's on steady tones and 1.1 dB on the bass line, its Rider
    within 0.1 / 1.8 dB.


## WBass Rider: Phase 3 measurements (the reference's audio), 2026-10-26

`nulltest/measure_bassrider.py` renders the reference (Bass Rider Mono / Live Mono / Stereo, 48 kHz, a fresh
instance per render: it keeps note state between renders) and reads the applied gain as output over input, sample
by sample (a least-squares ratio over 2 ms). pedalboard compensates the reported latency, so the output lines up
with the input (cross-correlation lag 0). Each finding names its subcommand.

### The level the rider reads (`static`, `weighting`, `envelope`)
- **Static law**: a steady note gets `gain = Target − E + 0.68 dB`, clamped to Range Min / Max, where E is the
  level read (below). For an 80 Hz sine that is `Target + 5.48 − peak`: checked at Targets −14, −24, −4 from −60
  to 0 dBFS, to 0.01 dB everywhere off the clamps. The output therefore settles at Target + 0.68 dB of E.
- **Weighting**: an RBJ low shelf, **31.35 Hz, −28.90 dB, Q 0.5075**, fits the reading against frequency (12 Hz to
  1.8 kHz, 35 points, linear in level) to 0.023 dB rms, 0.06 dB max. The hidden parameters' "high-pass 30 Hz at −30
  dB" is this shelf (with exactly 29.9 Hz / −30 dB and Q free: 0.065 dB rms). Above it the reading is flat; at 80 Hz
  it is −4.81 dB, at 40 Hz −11.5, at 20 Hz −19.7.
- **Envelope**: neither peak nor rms. Against a 1 kHz sine, two equal tones read +5.62 dB (rms would say +3.01, peak
  +5.93), three tones +8.91, a low-passed square +4.75 (more than its own peak, +2.55), band noise of the same rms
  +5.50. The **analytic (quadrature) envelope's power through a 2 ms attack / 100 ms release one-pole** (the hidden
  PBAtt / PBRel) predicts all four to 0.18 dB rms; on the raw waveform it misses the square by 2.4 dB.

### When riding starts and stops (`gate`, `noteoff`, `idle`)
- **Spill has no effect on steady tones**: riding starts at the same level for Spill −50 … −3.
- **Start**: below 300 Hz riding starts at −48.75 dBFS (peak); above, the threshold rises like a low-pass (+0.5 dB
  at 500 Hz, +1.9 at 700, +4.8 at 1 kHz, +8.0 at 1.4 kHz, +10.9 at 1.8 kHz; a Butterworth-like 2nd-order low-pass at
  830 … 1020 Hz fits to 0.15 … 0.26 dB: the hidden BADLpf, 998 Hz). Above about 1.85 kHz nothing is ridden (the
  hidden IllegalHpf, 2 kHz).
- **A note ends when the level falls 38 dB below its peak** (the hidden NoteOffGate −38): a legato drop of 36 dB
  keeps the note, 38 ends it; a note at −30 dBFS that drops to −60 never ends. Response does not move this.
- **Idle glide**: after a note ends the gain moves to the Idle value in a straight line, **32 dB/s**, starting
  about 14 ms before the drop reaches the output.

### Within a note (`steps`)
- **Look-ahead 42 ms**: the gain starts to move exactly 42 ms before a louder part reaches the output, and settles
  in about 20 ms (at 2 / 4 / 6 / 10 / 22 ms: 17 / 41 / 59 / 81 / 98 % of the way, in linear gain): the 2 ms energy
  attack in series with a ~5 ms gain smoother (the hidden OutAtt, 5 ms).
- **The gain only goes down within a note**: a legato step up is followed; a legato step down is held, unchanged
  for 8 s. A new note (after a 60 ms gap) gets its own gain at once.

### New notes (`artifacts`, `release`)
- **Artifacts** gates re-attacks against the earlier peak (the hidden note-on gate, −20 … −3 dB, 50 → −11.5): a
  re-attack is a new note only if it comes within about 2 / 6 / 10 / 14 / 18 dB of the earlier note's peak at
  Artifacts 100 / 75 / 50 / 25 / 0, whatever the floor it rises from.
- **A note must be released first**: before a re-attack counts, the level must have dipped by a depth set by
  Response and Sensitivity, for 20 … 30 ms (2 ms at Sensitivity 100):

  | Response | Sens 0 | Sens 50 | Sens 75 | Sens 100 |
  |---|---|---|---|---|
  | Slow | 12 … 13 dB | 12 … 13 dB | 1 … 2 dB | < 1 dB |
  | Fast | 4 … 5 dB | 1 … 2 dB | | < 1 dB |

  The hidden values name note / slow / ref envelopes (1 / 1 ms, 30 / 1000 ms, 1 / 5000 ms) and a sustain threshold
  (−1 dB): the release rule is theirs; its exact form is for Phase 4 to fit (the learned-block fallback stands).

### Live and stereo (`live`, `stereo`)
- **Live**: look-ahead about 5 ms (the gain starts 4 … 5 ms before the step; reported 221 samples), the same
  smoothing and the same note detection (a dip and re-attack gives the same new note).
- **Stereo**: one gain for both channels, read from the **mid, (L + R) / 2**: L −6 / R −30 dBFS (in phase) rides
  at the mid's −11.5; R = −L reads silence (gain at Idle); L 1 kHz and R 1.2 kHz read as the two-tone sum.

## WBass Rider: Phase 4, the riding model identified, 2026-10-26

The model (`nulltest/brmodel.c`, the same as `plugins/WBassRider/WBassRiderRide.h` line for line) was built from
targeted probes on the reference (`nulltest/br_probes.py`: 641 short signals, each with the reference's gain at set
times) and its gain traces on bass material (`nulltest/br_traces.py`: the bass line and synthetic parts at several
settings); `nulltest/br_fit.py` fits the note detector's constants by CMA-ES on both (`br_export.py` writes them into
the C++). What the probes established:

- **The level read** (Phase 3) is a power follower of the quadrature envelope; the quadrature pair is an IIR allpass
  pair (12 Hz .. 20 kHz, 0.23 degrees at 48 kHz; `nulltest/design_hilbert.py`, one set per rate), since the gain's ramp
  starts exactly at the look-ahead (the envelope adds no delay of its own).
- **The attack phase**: from a note's start the gain follows the law, both ways, while the note's level N stays over
  a slow envelope S (a peak follower: a ~20 ms rise, falling at most a few dB/s, tracking anything slower exactly) and
  for about 50 ms after it falls under; then the gain holds, only going down. On single decaying notes (200 Hz,
  -6 dBFS, decays of 0.05 .. 2 s and none) the reference's held gain is +1.38 / +0.97 / +0.70 / +0.45 / +0.18 / 0 dB
  over a steady note's; a fixed settle time or a smoothed law cannot give that curve, the attack-phase rule does
  (within 0.12 dB before the fit). It also takes the quadrature pair's onset overshoot out of the held gain.
- **New notes**, two ways: a note stays under S by a depth (Response, Sensitivity) for ~23 ms, and a rise from there
  is a new note if it is within the Artifacts gate of a long reference R (a peak follower falling ~2 dB/s); or, still
  sounding, it rises over S by a threshold (Sensitivity). After a deep drop the new note's gain is limited by the
  energy follower still falling (43 dB/s) from the note before, which is why re-attacks soon after a drop read
  partway between the old gain and the new note's.
- **The end of a note**: its level stays 38 dB under its peak for ~60 ms (a 21 ms gap between notes does not end
  one); the gain then glides to the Idle value at 32 dB/s. While it glides, any legal note within 38 dB of the last
  note's peak is a new note (no floor, no Artifacts gate; measured: after a -20 dBFS note the gate stays at -57.6
  dBFS for 300 ms, the glide's length, and is back to the Artifacts gate by 400 ms). An ignored note (outside the
  gates, within 38 dB of the last note) freezes the gain where it is.
- **Spill**: no effect found on any probe (steady tones, legato drops, re-attacks, gaps, starts from silence,
  high-frequency bleed, -50 .. -3 dB). The parameter is kept (stored, automatable) and does nothing yet.
- **Constants** (the fit, `measurements/wbassrider/detector_fit.json`; cost 710 over 641 probes and 10 traces): the
  slow envelope rises in 12 ms and falls at most 6.5 dB/s (1342 ms), the reference 1.4 dB/s (6051 ms); a note is
  released after 23.7 ms under S, holds 67 ms after its level falls under S, ends 63.8 ms after falling 38 dB under
  its peak, and for 27 ms after that takes any note within 38 dB. The per-setting thresholds (rise over S, the attack
  margin, the release depth) are in the file; some are not monotonic in Sensitivity where the probes cannot tell two
  paths apart.

### The null (Phase 4, `nulltest/nulltest_wbassrider.py`; the plug-in's own processor, `WBassRiderRender`)

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -40.3 | 0.067 | 0.067 |
| steps 1 kHz | -47.9 | 0.028 | 0.029 |
| bass line | -28.1 | 0.073 | 1.238 |
| bass line, Target -24 | -28.1 | 0.074 | 1.238 |
| bass line, Sensitivity 80 | -25.1 | 0.454 | 1.653 |
| bass line, Artifacts 20 | -28.2 | 0.073 | 1.238 |
| synthetic part 6 (held out) | -8.5 | 0.188 | 10.952 |
| synthetic part 7, Fast (held out) | -15.4 | 0.125 | 4.564 |
| synthetic part 8, range -6 .. +6 (held out) | -16.7 | 0.116 | 3.233 |

The level law, the look-ahead, the smoothing and the hold null to -40 .. -48 dB; the bass line to -28 dB. The
synthetic parts (ghost notes at a tenth of the level, random gaps) are where the note detector still decides
differently: borderline gate decisions (a new note accepted or not) cost the whole note several dB. Example: synthetic
part 6 at 2.38 s, a note 13 dB over a quiet passage is a new note in the reference, ignored here because the long
reference R (a peak follower) has fallen 1.5 dB less than the reference's. The C++ matches the fitted model to 0.07 dB
(99 %, `nulltest/check_wbassrider_port.py`).

### Phase 4, second pass: the note level is windowed by pitch, 2026-10-26

- **The detector's timings step with pitch**: the shortest dip that releases a note is 23.3 ms at 200 Hz .. 1 kHz,
  42.7 ms (2048 samples) at 80 .. 140 Hz, 85.3 ms (4096) at 60 Hz, 96.9 ms at 40 Hz; a note ends ~30 ms after a drop
  of 42 dB at 175 Hz .. 1 kHz, ~50 ms at 90 .. 150 Hz, ~93 ms at 50 .. 80 Hz. The note level N is therefore the peak of
  the low-passed input over the last 1024 / 2048 / 4096 samples, the window picked by the pitch (the hidden
  PARAM_PITCH_INDEX and PARAM_MAX_POW2_DELAY); the model picks the shortest window spanning `pitch_c` periods of a
  zero-crossing period estimate (fitted: 1.6 periods). N has no onset overshoot (the reference's R reaches a burst's
  true level within 6 ms at 50 Hz and never above it), and R and S follow N.
- **R** falls 1.74 dB/s after ~0.1 s (measured 0.3 .. 6 s after a drop, two floors): the 5000 ms release, on
  amplitude.
- **Probes**: 683 (42 more at 40 .. 200 Hz). The null now (`WBassRiderRender`, the C++ matching the model to 0.09 dB
  at 99 %):

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -40.4 | 0.065 | 0.065 |
| steps 1 kHz | -48.0 | 0.028 | 0.029 |
| bass line | -30.0 | 0.058 | 0.811 |
| bass line, Target -24 | -30.2 | 0.062 | 0.998 |
| bass line, Sensitivity 80 | -25.9 | 0.434 | 0.877 |
| bass line, Artifacts 20 | -30.0 | 0.058 | 0.733 |
| synthetic part 6 (held out) | -16.0 | 0.156 | 2.763 |
| synthetic part 7, Fast (held out) | -20.6 | 0.132 | 3.324 |
| synthetic part 8, range -6 .. +6 (held out) | -16.7 | 0.098 | 3.233 |

### Phase 4, third pass: the hold is constant, the windows pinned, 2026-10-26

- **Outside the attack phase the gain holds where it is**, whatever the level read does: a 71 Hz note at -4.4 dBFS
  followed legato by a 122 Hz note at -4.5 (which reads 3.5 dB louder through the weighting) keeps the first note's
  gain to the hundredth. (The earlier "only goes down" came from legato steps up, which are attacks over S.)
- **The window steps**, measured finely on the release timer: 1024 samples from 175 Hz up (165 Hz is 2048), 2048 from
  86 Hz (90 .. 165 Hz), 4096 at 74 Hz and under, intermediate 50 .. 58 ms at 78 .. 82 Hz. The model's window is the
  shortest spanning 3.3 periods (pinned from these, not fitted), and a zero crossing counts only when both halves of
  its cycle reach within 12 dB of the note level (a stopped note's filter ringing no longer shrinks the window).
- **Not modelled yet**: with Response Fast, a 100 Hz note followed legato by a much quieter (10 dB +) 40 Hz note is
  ridden afresh (Slow, and 60 / 150 Hz, are not); plain decays and level drops never re-ride at either setting.
- **The null now**:

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -41.8 | 0.071 | 0.071 |
| steps 1 kHz | -48.2 | 0.029 | 0.030 |
| bass line | -30.6 | 0.092 | 0.163 |
| bass line, Target -24 | -29.7 | 0.118 | 0.416 |
| bass line, Sensitivity 80 | -26.1 | 0.410 | 0.722 |
| bass line, Artifacts 20 | -30.6 | 0.092 | 0.182 |
| synthetic part 6 (held out) | -34.3 | 0.123 | 0.468 |
| synthetic part 7, Fast (held out) | -21.3 | 0.166 | 4.325 |
| synthetic part 8, range -6 .. +6 (held out) | -28.3 | 0.031 | 2.419 |

### Phase 4, fourth pass: Response Fast and pitch-band re-rides, 2026-10-26

- **Fast re-rides on a pitch change**: a note followed legato by a quieter note is ridden afresh at Response Fast only
  when the new note is at 40 .. 45 or 80 .. 90 Hz (from 60, 100 and 150 Hz alike; 35, 50 .. 75 and 95 Hz up hold; at
  Slow the same happens once the drop passes the release depth, e.g. 100 -> 85 Hz 14 dB down). These are the bands
  just over each window boundary, where a period is longer than a quarter window: the re-attack is read on a block
  level (the highest positive peak in each quarter-window block), which wobbles there, and any rise after a release
  counts (the fitted threshold is 0.002 dB). The model reproduces the map but for 35 Hz (it re-rides, the reference
  does not).
- **Window bands** set by pitch, measured: 1024 samples over 172 Hz, 2048 over 78 Hz, 4096 under.
- **The block level also gates** the re-attack against the long reference (a ghost after a gap no longer passes on
  the windowed level still holding the note before).
- **Open**: two overlapping notes beating (a 4 dB rise) are not a new note in the reference, though they are here.
- **The null now**:

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -42.3 | 0.067 | 0.067 |
| steps 1 kHz | -48.6 | 0.029 | 0.030 |
| bass line | -30.6 | 0.062 | 0.171 |
| bass line, Target -24 | -30.4 | 0.063 | 0.220 |
| bass line, Sensitivity 80 | -27.7 | 0.218 | 0.646 |
| bass line, Artifacts 20 | -30.7 | 0.062 | 0.204 |
| synthetic part 6 (held out) | -33.2 | 0.074 | 0.536 |
| synthetic part 7, Fast (held out) | -21.1 | 0.169 | 1.961 |
| synthetic part 8, range -6 .. +6 (held out) | -34.1 | 0.007 | 0.378 |

  Fresh synthetic parts (seeds 13, 14 Fast, 15 Sensitivity 75, from the gain traces): -18.7 / -20.1 / -17.0 dB.

### Phase 4, fifth pass: Sensitivity and the follow phase, 2026-10-27

- **The mismatch**: through the plug-in, the bass line nulled at -30 dB at Sensitivity 0 .. 50 (Slow) and 0 / 50
  (Fast) but -14 .. -19 dB at Fast 25 / 75 / 100 and Slow 100. The fit had music traces only at Fast 50, so the
  per-Sensitivity release depths at Fast sat on the wrong side of the bass line's thresholds (Fast 25 needs over 4 dB,
  Fast 50 between ~4 and 4.8); and at Sensitivity 100 the gain kept following a fading note too long.
- **How long the gain follows a decaying note** (`followstop` probes: 100 Hz notes from silence decaying 6 .. 60 dB/s):
  the same at Slow and Fast and at 50 / 100 / 200 Hz; the reference follows until the note is a margin under a
  one-pole follower of the note level, so a slow fade is followed throughout (at Sensitivity 75 and 100: 9 dB/s and
  slower never stop) and a faster decay holds after a fixed lag in dB (Sensitivity 75: 1.8 dB at 15 .. 45 dB/s, the
  stop at 346 / 231 / 176 / 120 / 63 / 36 ms for 10 / 11 / 12 / 15 / 30 / 60 dB/s); at Sensitivity 0 it holds after
  ~20 ms, at 25 after 20 .. 40 ms, at 50 after 26 .. 70 ms. The old criterion (over the rate-limited slow envelope S)
  could not give both. Modelled: F, a one-pole follower of N's power (fitted 6.5 ms up, 365 ms down), and the attack
  continues while N > F + margin (Sensitivity; fitted +1.5 / +1.9 / +0.2 / 0.0 / -0.8 dB at 0 .. 100).
- **Refit** with 53 decay probes and 9 more traces (the bass line at Fast 25 / 75 / 100 and Slow 25 / 75 / 100,
  synthetic parts at Fast 75, Slow 100, Fast 25), then `br_polish.py`: each per-setting value on a grid in turn (the
  narrow valleys between release thresholds that the CMA-ES steps over). Two polish moves that gained under 0.5 % of
  the cost but cost held-out parts were set back by hand (Slow 50 depth 13.0, Sensitivity 0 margin 1.5).
- **The bass line through the plug-in, every Sensitivity x Response** (residual, dB):

| Sensitivity | 0 | 25 | 50 | 75 | 100 |
|---|---|---|---|---|---|
| Slow, before | -29.8 | -30.5 | -30.6 | -27.7 | -15.3 |
| Slow, now | -30.2 | -30.6 | -30.2 | -28.0 | -25.3 |
| Fast, before | -29.9 | -14.0 | -30.5 | -16.5 | -19.2 |
| Fast, now | -30.3 | -30.5 | -30.3 | -28.5 | -26.6 |

  (these bass-line settings are now in the fit; held out: synthetic part 15 at Sensitivity 75 -15.2 -> -17.2 dB,
  part 6 at Slow 75 -23.4 -> -24.5; parts 6 and 13 at Sensitivity 100 0.3 .. 1.9 dB worse).
- **The null now**:

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -41.5 | 0.074 | 0.074 |
| steps 1 kHz | -48.6 | 0.029 | 0.030 |
| bass line | -30.2 | 0.073 | 0.160 |
| bass line, Target -24 | -30.2 | 0.103 | 0.200 |
| bass line, Sensitivity 80 | -28.1 | 0.196 | 0.622 |
| bass line, Artifacts 20 | -30.3 | 0.103 | 0.185 |
| synthetic part 6 (held out) | -34.5 | 0.097 | 0.388 |
| synthetic part 7, Fast (held out) | -21.1 | 0.122 | 1.874 |
| synthetic part 8, range -6 .. +6 (held out) | -34.5 | 0.050 | 0.347 |

- **Open**: part 6 at Fast nulls at -16 dB at every Sensitivity from one passage: a 77 Hz note fading after its
  release, where the pitch read wobbles over the 78 Hz band edge and the window switch reads as a rise (the reference
  holds). Hysteresis on the band switch (`band_hyst`, kept at 0) helps part 7 (-21 -> -27.5 dB at 2 %) but not part 6
  and breaks the bass line at Fast from 3 %. Notes right after a note-off (part 13, -15 .. -20 dB at every setting)
  as before.

### Phase 4, sixth pass: a corpus of real bass parts, 2026-10-27

- **Material**: `nulltest/bass_corpus.py` builds 36 twelve-second bass parts from FL Studio's own pack samples (acoustic,
  80s pick, Ibanez, slap, Temptated, a picked 5-string and a "Deeper Purple" pluck multisample: Ogg Vorbis in WAV,
  decoded by `tools/OggDecode.cpp`, cached in the git-ignored `reference/`) and three synthetic voices (808 sub, filtered
  saw, slow swells): bass lines with rhythms, accents, ghost notes, rests, staccato / legato / ringing notes; parts
  0 .. 23 fit, 24 .. 35 are held out. `br_corpus_ref.py` records the reference's applied gain on every part at 10
  settings (default, Fast, Sensitivity 0 / 75 / 100, Fast 25 / 100, Artifacts 15 / 85, Target -24);
  `br_corpus_eval.py` compares.
- **Where the committed model stands on it** (median residual over the held-out parts): default -24.7, Fast -20.0,
  Sensitivity 0 -24.4, 100 -21.8, Fast 25 -18.4, Fast 100 -23.4, Sensitivity 75 -21.6 dB; the worst parts -6 .. -17 dB.
  The level read is right (on 173 of 175 notes the reference's held value lies on our law's path over the attack);
  85 % of the error energy is in holds at the wrong value (both sides holding, typically 0.3 .. 0.5 dB apart, 1 .. 4 dB at
  the 90th percentile), the rest in rides taken or missed.
- **Found**:
  - Response Fast in the reference is the same as Slow over most of a part; it adds a few re-rides as notes decay,
    and holds between them. The model crept up instead (re-riding on every flutter of a released note) because the
    pitch read took strong harmonics (slap, plucks) for the pitch, flipping the window, and any rise of a released
    note counted. A pitch read on the input low-passed again (4 poles near 200 .. 250 Hz) and a real rise threshold
    for a released note (~2 dB; the decay-and-step probes show +2.6 dB at Fast after 12 dB of decay) bring the
    corpus at Fast from -14 to -19 dB (both kept as model options, `pitch_lp` and `on_rise`, not yet in the plug-in).
  - A new note on a decaying note (288 decay-and-step probes in `br_probes.py`): the threshold is a rise of ~1.7 ..
    1.9 dB over a slow envelope falling at most ~8.7 dB/s (release ~1000 ms); Slow and Fast agree except after a
    deep decay (Fast takes +2.6 dB after 12 dB, Slow nothing up to +9).
  - The hold's moment after an attack: at the reference's lock the windowed note level has not yet fallen from its
    peak; the energy read is 0.3 .. 0.5 dB under its peak; no fixed time, window, energy-window mean or follower on
    N or E tested explains the held values better than the model already does (median 0.28 dB). Open.
- **Not adopted yet**: refitting on the corpus (`br_fit.py` now includes it, weight `CW`; `br_polish.py globals`)
  improves the held-out corpus at Fast 25 (-18.4 -> -23.5), Sensitivity 0 (-24.4 -> -27.1) and 100 (-21.8 -> -22.7)
  and removes the worst cases (Fast worst -6.1 -> -13.4), but loses the original bass line at Fast (-30 -> -16 dB: one
  release at 5.16 s whose outcome flips on tiny parameter changes) and ~1 .. 3 dB on the synthetic seeds.
- **A learned lock** (`nulltest/br_lock.py`; brmodel.c lock modes, off by default). Labels: on each note the model
  starts and the reference rides, the reference's lock is where our law last meets the value it then holds. The
  model writes its detector state every attack millisecond (19 inputs: time since the start, N and E against their
  peaks and followers, slopes, window, Sensitivity, Response, the rule's own test); the lock does not feed back into
  them, so they are exact.
  - The ceiling (an oracle locking at the labelled moments, training corpus): default -23.6 -> -26.5, Sensitivity 0
    -23.3 -> -28.6, Artifacts / Target settings +1 .. +3 dB, but Fast, Fast 25 and Sensitivity 100 barely gain (there
    the reference rarely holds 30 ms between notes: the error is which notes are ridden, not when they lock).
  - A one-shot lock network (hazard per millisecond) is worse than the rule everywhere: its lock times miss by a
    median 10 .. 15 ms, mean ~40 ms, and it cannot resume following.
  - A follow / hold network every millisecond (16 hidden units; 93.0 % of held-out milliseconds right against the
    rule's 87.8 %; after its first hold it follows again only where the rule does) on the held-out corpus: default
    -24.7 -> -26.1, Sensitivity 75 -21.6 -> -23.5, Artifacts 85 -21.1 -> -24.2, Target -24 -23.2 -> -24.3, Fast
    unchanged, Sensitivity 100 -22.0 -> -21.5, Fast 100 -23.7 -> -22.8; the original bass line loses ~6 dB at Fast
    0 / 25 (-30 -> -24). Not adopted: no clear win, and the plug-in keeps the rule.

### Phase 4, seventh pass: which notes Response Fast rides afresh, 2026-10-27

- **Where the error is**: on the corpus at Fast the decisions at a note's onset are mostly right (ridden or not on
  93 % of 616 notes), but re-rides inside a note are not: the reference re-rides 74 times away from any onset, the
  model 97 times, and only 22 coincide (`nulltest/br_corpus_eval.py --events` and per-note tables). The model re-rode
  on every small wobble of a released note (a rise of 0.003 dB counted), in chains (its slow envelope stays up, so a
  re-ridden decaying note is released again within a millisecond), where the reference holds through plain decays.
- **Plain decays from silence** (`nulltest/br_probes.py`, 52 new probes; sines decaying 10 .. 45 dB/s at 34 .. 198 Hz,
  Response Fast at Sensitivity 0 / 50 / 100 and Slow): the reference re-rides inside a smooth decay only at 40 .. 48 Hz
  (and 85 .. 88 Hz at some rates and Sensitivities), never at 34 .. 38, 50 .. 80, 90 .. 198 Hz. At 40 .. 44 Hz the
  re-ride comes exactly when the note is released (within +-7 ms of the model's release, at Fast 0 / 50 / 100: the
  note 4.3 .. 4.8 dB under S), 12 .. 30 ms later at 46 .. 48 Hz, 30 .. 90 ms later at 85 .. 88 Hz (5.0 .. 5.6 dB under
  S). Elsewhere a released note needs a real rise: the decay-and-step probes (200 Hz) put it at 3 .. 4.5 dB at
  Sensitivity 0, 2 .. 3 at 50, 1 .. 2 at 100.
- **What else separates the cases**: the 10 dB/s decays reach the depth only once the note is more than the Artifacts gap
  under the long reference R (no re-ride: the gate applies); and the bass line's 42 Hz note at 5.15 s is ridden afresh
  at Fast 50 / 75 / 100 (the reference's gain to +12 dB) but not at Fast 0 / 25, as the 20 dB/s probe at Fast 0 is not.
- **Modelled** (`brmodel.c` `rr_band` 4, the plug-in's `reRide*`): at Response Fast and Sensitivity 50 and up, a held
  note at a pitch in 39 .. 47 Hz is ridden afresh once it is 4.4 dB under S for the release time, released or not,
  within the gates, and S restarts from the note; elsewhere a released note's rise on the block level must pass a
  threshold by Sensitivity (`on_rise`, fitted 3.5 / 1.5 / 3.6 / 1.0 / 1.05 dB at 0 .. 100; it was 0.003 dB everywhere).
  The Fast release depths at Sensitivity 0 / 25 are now 7.7 / 7.6 dB (were 4.7 / 5.8). Refit with `br_polish.py` on
  the probes, traces and the training corpus; of its moves only these were kept: the attack margins, rise thresholds
  over S and Slow depths it also moved (up to 1.5 dB, Slow 100's depth 6.2 -> 12.4) won 2 .. 3 dB on the training
  parts but lost 1 .. 2 dB on held-out ones (Artifacts 85 -21.1 -> -18.9, Target -24 -23.2 -> -21.6, Fast 100 -1.3).
- **The corpus** (median residual, held-out / training parts, dB):

| setting | before | now |
|---|---|---|
| Fast | -20.0 / -13.6 | -22.0 / -14.8 |
| Fast 25 | -18.4 / -13.6 | -19.6 / -19.0 |
| Fast 100 | -23.7 / -17.2 | -23.7 / -17.6 |
| Sensitivity 100 | -22.0 / -15.1 | -22.0 / -16.5 |
| default, Sensitivity 0 / 75, Artifacts 15 / 85, Target -24 | | unchanged |

  The probe cost (1148 probes) is 819 against 1014 before. The worst held-out part at Fast (part 33, an 80s pick bass
  ringing at 139 .. 156 Hz) goes from -6.1 to -19.5 dB.
- **The bass line through the plug-in, every Sensitivity x Response** (residual, dB): Slow -30.2 / -30.6 / -30.2 /
  -28.0 / -25.3, Fast -30.3 / -30.5 / -29.3 / -28.0 / -26.2 at 0 / 25 / 50 / 75 / 100 (fifth pass: Fast 50 -30.3,
  75 -28.5, 100 -26.6). The C++ matches the model to 0.08 dB at 99 % (`check_wbassrider_port.py`, now also on a 42 Hz
  decay at Fast and a corpus part at Fast 25).
- **Not solved**: counted as events, the re-rides inside notes still mostly disagree on real bass (at Fast 20 of the
  reference's 74 coincide with the model's 89): the pure-sine rule is right, but on the corpus most of the reference's
  re-rides come from something these probes do not show (overlapping ringing notes ending, real notes' wobble at
  pitches outside the band). Also open: the band at 85 .. 88 Hz (re-rides 30 .. 90 ms after the release, at some rates
  and Sensitivities only), the reference's second re-ride in a long 42 Hz decay (20 .. 40 % sooner than the model's:
  its slow envelope seems to hold after a re-ride), and the end of a fading note (about 42 dB under its peak in the
  reference, 38 in the model: ~60 ms later). Synthetic part 7 at Fast loses 2 dB (-21.1 -> -19.0).
- **The null now**:

| case | residual (dB) | gain error median (dB) | 90 % (dB) |
|---|---|---|---|
| steady 80 Hz -20 dBFS | -41.5 | 0.074 | 0.074 |
| steps 1 kHz | -48.6 | 0.029 | 0.030 |
| bass line | -30.2 | 0.073 | 0.160 |
| bass line, Target -24 | -30.2 | 0.103 | 0.200 |
| bass line, Sensitivity 80 | -28.1 | 0.196 | 0.622 |
| bass line, Artifacts 20 | -30.3 | 0.103 | 0.185 |
| synthetic part 6 (held out) | -34.5 | 0.097 | 0.388 |
| synthetic part 7, Fast (held out) | -19.0 | 0.131 | 1.874 |
| synthetic part 8, range -6 .. +6 (held out) | -34.5 | 0.050 | 0.347 |

### Phase 4, eighth pass: a released note's rise, and drops onto lower notes, 2026-10-27

- **Where the error on real bass is**: per 10 ms block, 66 .. 77 % of the corpus's error energy is both sides holding
  more than 1 dB apart, and 81 .. 99 % of that follows a different ride decision (not a different lock on the same
  ride). On the sampled basses (not the 808, saw and swell voices) at Fast and Sensitivity 100, 66 % of it followed a
  re-ride the model took inside a note and the reference did not; every such re-ride came from a released note's rise
  read on the block level (the peak of each quarter window), which jumps with a real note's harmonics and phase.
- **The rise is read on the note level N** (`block_rise` 0; the 4th pass's reason for the block level, re-rides at
  40 .. 45 Hz, is now the band rule), **gated on the block level** (`gate_block` 2: a quiet note right after a gap,
  while N still holds the note before, stays out; reading the gate on N too lost 3 dB on synthetic part 6). Rise
  thresholds re-polished: 2.45 / 1.88 / 1.78 / 1.00 / 1.87 dB at Sensitivity 0 .. 100 (the decay-and-step probes:
  3 .. 4.5 / 2 .. 3 / 1 .. 2).
- **Legato drops at Fast** (64 new probes: a held note at 55 or 100 Hz, then one at 41 .. 130 Hz, 6 .. 20 dB lower)
  and **overlapping notes** (A fading, B from 0.8 s at another pitch, -6 .. +4 dB, A stopping at 1.6 s; scratch probes):
  - at 41 Hz every drop re-rides, 20 dB under the long reference too: the band re-ride is not gated by Artifacts
    (`rr_band` 6, legal only);
  - from 100 Hz onto 50 .. 78 Hz (the window doubles) a deep enough drop re-rides (10 dB at 78 Hz, 14 at 69, 20 at
    50 / 62), always moving the gain +3.0 .. 3.3 dB; from 55 Hz onto 50 .. 78 Hz never. And at 90 Hz, from both,
    +0.7 .. 3 dB. Neither is modelled yet;
  - when the louder of two overlapping notes stops, the quieter one left is ridden afresh if it is lower in pitch
    (100 -> 69 / 41, 55 -> 41, 78 -> 41 Hz) and not if higher.
- **The corpus** (median residual, held-out / training, dB; before = the seventh pass):

| setting | before | now |
|---|---|---|
| default | -24.7 / -23.6 | -24.7 / -23.6 |
| Fast | -22.0 / -14.8 | -24.0 / -19.6 |
| Fast 25 | -19.6 / -19.0 | -19.6 / -19.4 |
| Fast 100 | -23.7 / -17.6 | -23.2 / -18.5 |
| Sensitivity 100 | -22.0 / -16.5 | -22.9 / -18.3 |
| Artifacts 15 | -23.6 / -23.6 | -24.7 / -23.6 |
| Sensitivity 0 / 75, Artifacts 85, Target -24 | | within 0.1 |

  Re-rides inside notes at Fast: the model's 89 -> 59, of which 24 match the reference's 74 (was 20); on the sampled
  basses the model's false re-rides now carry 9 % of the held error at Fast (was 66 %); what remains is mostly
  re-rides the reference takes and the model does not (at onsets 49 %, inside notes 33 %).
- **The bass line through the plug-in**: Slow -30.2 / -30.6 / -30.2 / -28.0 / -25.3, Fast -30.3 / -30.5 / -29.3 /
  -28.0 / -26.2 at Sensitivity 0 .. 100 (unchanged). The C++ matches the model to 0.08 dB at 99 %. The null table as
  in the seventh pass but synthetic part 7, Fast -18.8 dB (was -19.0).

### Phase 4, ninth pass: the Idle value, and the factory presets, 2026-10-27

- **The Idle arrow in the reference** (films with `tools/MeterCapture gl=1`, now able to play audio through a drag
  script, `in=`; and renders with Idle / Range changed between blocks, the gain read on a -54 dBFS 3 kHz pilot):
  - the arrow keeps its place in the range: Range Max 12 -> 7.5 moves it from 0 to -2.2 dB (as WBass Rider's);
  - a sounding note keeps its gain whatever the arrow does; when the note ends the gain glides to the new Idle value
    at 32 dB/s; in silence after a note an Idle (or Range) change glides the gain to it at 32 dB/s, so the Rider
    follows the arrow as it is dragged; an ignored note (within 38 dB of the last) holds the gain;
  - **before any note**: the gain starts at 0 dB (not at the Idle value) and, once the look-ahead has filled, glides
    to the Idle value at 32 dB/s, and follows it, while the input is silent: a quiet 80 Hz tone at -72.75 dBFS holds
    the gain at 0 whatever Idle does, -73 lets it glide; 40 Hz glides at -72, 200 Hz holds at -74 (Target and Spill
    change nothing): silence there is the level read E under -77.7 dB. WBass Rider held its gain until the first note
    (Idle changes did nothing until then) and started at the Idle value; both now as the reference (`pre_silence`,
    `pre_hold`), within 0.1 dB of its glides.
- **Artifacts above 50**: the bass line nulled at -12.5 dB at Artifacts 100 and -18.6 at 68.2 (Slow), from held notes
  that started following the law again when a quiet note rose over the follower F, bypassing the Artifacts gate; the
  reference keeps them held. Now a held note follows again only within the gates (`hold_latch` 2): Artifacts 100
  -12.5 -> -37.6 dB, 68.2 -18.6 -> -24.8; nothing else moves (corpus Artifacts 85 training -19.1 -> -20.5). And at
  Response Fast the band re-ride is gated by Artifacts above 50 (ungated at 50 and under: `rr_band` 7; the bass line
  at Fast, Artifacts 68.2 -17.8 -> -24.6).
- **Factory presets**: the reference's 17 (`nulltest/read_ref_presets_br.py`, `tools/make_presets.py` ->
  `plugins/WBassRider/WBassRiderPresets.h`), with Wavy names; the 15 that store Target as "*" leave it where it is
  (`FactoryPreset::leave`). The array positions were checked by loading each array into the reference's own state
  and reading its parameters back (Artifacts 77.1, 100, 85.3, 74.1, 60, 0 as converted). Against the reference
  (each preset on the bass line and held-out corpus parts 30 and 25; `nulltest/nulltest_wbassrider_presets.py`,
  docs/NULL_RESULTS_WBASSRIDER_PRESETS.md): median -24.8 dB; the worst, "Fast, Artifacts Ignored" on the bass line,
  -9.9 (the drops onto lower notes at Fast, not modelled yet).
- The null table is unchanged (bass line -30.2 dB); the C++ matches the model (now also checked at Artifacts 100).


## WVox Rider: Phase 3 measurements (the reference's audio), 2026-10-28

`nulltest/measure_voxrider.py` renders the reference (Vocal Rider Mono / Live Mono / Stereo, 48 kHz, a fresh instance
per render) and reads the applied gain as output over input, sample by sample (a least-squares ratio over 1–20 ms; a
−80 dBFS pilot at 3001 Hz exposes the gain in silences). There is no latency, so nothing needs aligning. Each finding
names its subcommand. A behaviour analysis of the installed binaries gave hypotheses to test (its notes stay in the
git-ignored `reference/wvoxrider/`); everything below is measured on the audio.

### The static law (`explore`, `settle`, `static`, `ranges`)
- A steady tone settles within about a second and stays (checked to 30 s).
- **Law**: in linear gain, `gain = ½·gMin + c · Target / level`, clamped to [gMin, gMax] (gMin, gMax: Range Min /
  Max as gains). The ½·gMin offset is why the output is not held at Target: a 3 dB louder input leaves the variable
  part exactly 3 dB lower (ratios 1.403 … 1.416 per 3 dB, 10^(3/20) = 1.4125), but the total gain falls by less,
  more so near Min. With c = 0.738 on a 1 kHz sine (rms level) it predicts every setting measured (Target −12 / −21 /
  −30, Range ±12, ±6, −3 … +9, −12 … 0, ±1, levels −36 … −6 dBFS) to 0.1 dB.
- **c** splits into the weighting at 1 kHz (−0.42 dB, below) and +3.06 dB: the level read is the **peak** power of
  the weighted signal (a sine's peak is +3.01 dB over its rms), as a fast-attack / slow-release power follower reads.
- **Output** is a plain gain after the ride (−6 → −6.00, +3 → +3.00 dB relative).

### The level read (`weighting`, `bias`)
- **Weighting** (steady sines, −21 dBFS rms, through the law; re 1 kHz): −13.4 dB at 50 Hz, −8.55 at 100, −3.52 at 200,
  −0.48 at 500, flat to 2 kHz, then −1.44 at 4 kHz, −2.62 at 6.5 kHz, −4.02 at 10 kHz, −5.74 at 16 kHz. Two shelves,
  each made of two identical first-order sections with half the gain (low shelf at the hidden HPFFreq / HPFGain 100 Hz
  / −18 dB, high shelf at LPFFreq / LPFGain 6.5 kHz / −6 dB), fit this to 0.15 dB rms (0.50 max) with exactly those
  values; freed, 102.4 Hz / −17.4 dB and 6.92 kHz / −6.1 dB give 0.12 dB rms. So the hidden "HPF" and "LPF" are
  shelves, not pass filters.
- **Floor**: below about −45 dBFS the level read flattens: a power floor at −42 dB in the follower's own units (the
  hidden VOCBias −42; −44.6 dBFS rms at 1 kHz after c), added to the level (checked at −30 … −54 dBFS: the floor
  comes out at −44.6 ± 0.2 each time).

### Vocal detection (`onset`, `spill`)
- **Onset**: a steady tone is ridden from a level that does not depend on Target and moves with Vocal Sensitivity:
  −42 / −50 / −58 / −64 / −70 dBFS for −12 / −6 / 0 / +6 / +12 dB (2 dB grid). A detector that fires while the
  short-term power exceeds `10^((−VocalSensitivity − 14)/10)` times the follower's level plus its floor predicts
  −42.3 / −51.9 / −58.4 / −64.5 / −70.6 (for a steady tone short- and long-term power are equal, so the onset sits
  that far under the floor). Below it the gain parks at the Idle value.
- **Spill** (Live components): an absolute gate. Riding starts 2 dB (rms) under the Spill value (−45 → −47, −40 →
  −42 … −18 → −20 dBFS: the same 2.6 dB as c), and Spill −48 (its minimum) is off. The Studio components ignore it.

### Timing (`steps`, `idle`, `gaps`)
- **Gain down** (the level rises): time to 63 % of the change 39–58 ms at Attack Slow, 7.7–11.4 ms at Fast (the
  hidden VocAtt 50 / 10 ms, which Attack sets).
- **Gain up** (the level falls): the same at Slow and Fast, slow and getting faster as it goes: 6 dB down takes 555 ms
  to 63 %, 12 dB 1373 ms (t90 764 / 1789 ms). The follower's release (VocRel 1009 ms) and the ride's (RideRel
  1000 ms) both act here: to fit in Phase 4.
- **When the voice stops** (silence): the gain stays exactly where it is for **500 ms at Slow, 50 ms at Fast** (the
  hidden ParkHold, which Attack sets), then **glides to the Idle value**: about −6.1 dB/s steady when it comes down
  (from +6), +4 … +5 dB/s, getting faster, when it goes up (from −5.8). The Idle value is Min + Idle × (Max − Min).
- **Gaps between words**: shorter than the hold, the gain does not move; longer, it glides (about 6.6 dB/s) until the
  voice returns. Back at the same level the gain recovers in about 0.3 s (faster than after a level drop: the follower
  held the level through the gap, VocHold 5 s); back louder it comes down in the attack time.

### Stereo (`stereo`)
- The Stereo component reads the **mid**, (L + R) / 2: one channel alone reads 6 dB under both, anti-phase reads
  nothing (parked at Idle), L with R 6 dB lower reads −2.5 dB. Both channels get the same gain.

### Music side-chain (not measured yet)
- With the side-chain bus enabled and fed (tools/RefRender, and tools/MeterCapture with its new `sc=` input), the
  reference's Music knob keeps its disabled picture, its LED stays dark and Music Sensitivity changes nothing. The
  reference uses the music only while its host reports the side-chain active, which these JUCE hosts do not achieve;
  this needs a test in FL with a real side-chain send.


## WVox Rider: Phase 4, the riding identified, 2026-10-28

The model (`nulltest/vrmodel.c`, the same as `plugins/WVoxRider/WVoxRiderRide.h` line for line;
`nulltest/check_wvoxrider_port.py`: the C++ against the model, 25 cases, worst difference 0.0000 dB) was built from
the reference's renders: 26 probes and a vocal corpus (`nulltest/vocal_corpus.py`: 84 sung and spoken phrases of FL
Studio's own Vocals packs strung into 36 parts of 20 s; parts 0 .. 23 to identify, 24 .. 35 held out), each at ten
settings (`nulltest/vr_ref.py`, 620 renders), and targeted drops. A first pass (a debounced detector, every
constant fitted by CMA-ES) reached -22 dB on the probes and only -12 dB on the corpus: the reference rose much more
slowly on real vocals than on tones. What the second pass established:

- **The ride moves only on the samples the detector passes.** After a level drop the gain does not rise at one rate
  but at stepped ones: on a 1 kHz sine at 48 kHz every rate is a multiple of 0.362 dB/s, i.e. k / 24 of 8.69 dB/s (the
  hidden RideRel, 1000 ms per neper), with k the samples of each 24-sample half period that pass. Just after a 12 dB
  drop 10 pass (3.62 dB/s); settled, 21 (7.61 dB/s: the three samples nearest each zero crossing never clear a
  threshold 14 dB under the peak). The steps depend on the drop, not on the level before it (drops of 1 .. 12 dB from
  -15 and -27 dBFS, Range ±12). So the detector decides per sample (the hidden VADAtt / VADRel, 0.01 ms) and the ride
  has no debounce: on a voice, whose peaks are brief, it rises slowly (about 2 dB/s on the corpus).
- **The follower** runs on the weighted power plus the floor (VOCBias -42 dB) and starts at the floor: up by at most
  the attack's rate (VocAtt 50 / 10 ms by Attack), down by at most the release's (VocRel 1009 ms) on samples the
  detector passes and the hold's (VocHold 5000 ms) on the others, all multiplicative per sample with squared
  coefficients (the behaviour analysis saw them squared). Since it only rises on samples above it, it slows near a
  sine's peak, which gives the measured gain-down shape (Phase 3's "t63 39 .. 58 ms").
- **The start**: the gain starts at 1/2 gMin + 1 (+1.94 dB at Range -6 .. +6) and the park hold counts from the first
  sample (the reference holds through an initial silence shorter than the hold).
- **Spill** (Live) adds an absolute level to the detector's threshold: a sample passes when its short-term power is
  over 10^((-Vocal Sensitivity - 14) / 10) times the follower **plus** the Spill power. So the steady onset sits about
  2 dB (rms) under Spill (Phase 3), and after a drop from -18 to -30 dBFS at Spill -30 nothing is ridden for 0.9 s
  (to -31 dBFS: 2.2 s) until the follower has fallen; a separate gate cannot do that. Spill -48 (its minimum) is off.
- **Park**: as Phase 3 (v toward the Idle value at 1205 ms per neper, 7.21 dB/s; hold 500 / 50 ms).

**No fitted constants**: with this structure every constant is the reference's hidden ParamXML / preset value (law
scale c 0 dB, floor -42 dB, threshold 14 dB, VocAtt 50 / 10, VocRel 1009, VocHold 5000 ms squared; VADAtt / VADRel
0.01 ms; RideAtt 0.55 / 0.35, RideRel 1000 ms; ParkAtt / ParkRel 1205 ms; ParkHold 500 / 50 ms). The level drops
then match to 0.002 dB rms and the Live drops to 0.003 .. 0.06 dB. A local CMA-ES from these values over 17
constants (`nulltest/vr_fit.py fit`, 500 cases) found no better point in 176 candidates (mean null -63.8 dB at the
start, the best other candidate -44.5); single nudges (VocAtt Fast 8 / 12 ms, VADAtt / VADRel 0.1 ms, RideAtt Fast
0.2 / 0.6 ms, ParkHold Fast 45 / 55 ms) all make Fast worse. Steady tones at 150 Hz .. 8 kHz are within 0.022 dB.

The model over the 620 renders (median null; `vr_fit.py report`): probes -70.5 dB, training parts -60.8, held-out
parts -62.3 (per setting -50 .. -69 dB; Attack Fast is the weakest, about -50, at word onsets). The plug-in against
the reference on fresh renders: [docs/NULL_RESULTS_WVOXRIDER.md](docs/NULL_RESULTS_WVOXRIDER.md), -47 .. -74 dB,
including Stereo, Live, 44.1 and 96 kHz.

Not identified (no probe needs them, or none can reach them): the role of the detector's fixed 50 ms factor seen in
the behaviour analysis; OutAtt / OutRel (they smooth the Output control only; Output moves are not measured); the
Music side-chain (see Phase 3).


## WBass Rider: Phase 4, third pass, from the reference's detector configuration, 2026-10-28

A behaviour analysis of the installed Bass Rider binary (its notes stay in the git-ignored reference/wbassrider/)
showed how the plug-in configures its detector blocks; each lead was then checked on the reference's audio:

- **Spill** is wired to the detector only in the Live components; the Studio components use a fixed value. Steady
  40 / 80 / 200 Hz notes after silence (`Bass Rider Live Mono`, Spill -40 / -25 / -10, 0.25 dB bisection) are
  ridden from **Spill - 8.69 dB** peak at Artifacts 50 and **Spill - 18.88 dB** at Artifacts 0; Studio ignores Spill.
  That is an additive gate: a note starts when its level is over the Artifacts gain times (the long reference R plus
  the Spill level), as amplitudes (from silence R equals the note: predicted -8.82 / -19.08 dB). At Spill's default
  (-40) it is the floor fitted before on Studio (-48.75 dB): Studio is the same gate with Spill fixed at -40.
- **Weighting**: built like Vocal Rider's, two identical first-order shelves of half the gain: the hidden HPF 30 Hz /
  -30 dB as two -15 dB sections (the fitted RBJ shelf, 31.35 Hz / -28.9 dB / Q 0.51, was close: Q 0.5 is what the
  pair gives). Steady 80 Hz -41.5 -> -48.9 dB; probe cost 969 -> 925.
- **Release depth** by Response only: fixed at Slow 12 / Fast 4 dB in place of ten fitted values (fit cost 2965 ->
  2932 before refitting).
- **Sensitivity**: the binary derives the note-on threshold from a straight line in Sensitivity; as fixed lines the
  fit got worse, so the per-Sensitivity tables stay fitted (their mapping to the reference's threshold is not known).

The detector's other constants were refitted with these held (`nulltest/br_fit.py` CMA-ES, 25 generations; cost
2891). Results: docs/NULL_RESULTS_WBASSRIDER.md, "Third pass". Not taken from the binary: the octave-band high-passes
(fs / 2^round(log2(fs / 20)) and three octaves up) that likely choose the window by pitch, and its 4 + 4 first-order
allpass quadrature pair; both are leads for another pass.

## WVox Chain: Phase 3 measurements (the reference's audio), 2026-10-28

`nulltest/measure_voxchain.py` renders the reference CLA Vocals (Stereo component, 48 kHz) through
`nulltest/cv_ref.py` (pedalboard, aligned for the 193-sample latency) and `tools/RefRender` (for a host tempo and the
Mono/Stereo component). Each finding names its subcommand; the data (`.npz`, logs) is in the git-ignored
`reference/wvoxchain/meas/`.

**How the stages are reached.** The children's bypass slots (preset array 39–53) and the colours cannot be written into
the state: the plug-in reads them back as 0. The colours and faders work as host parameters, so each stage is measured
against the chain with **every colour clear**, where the wet stages are muted and the others hold their fixed settings.
The state's Noise slot (67) does take, and is off in every measurement but its own. **Renders are deterministic**: two
renders of the same input null to −200 dB, the reverb, delays and doubler included, so every stage can be nulled.

### The chain with every colour clear (`base`, `deess`)
- **Linear part**: L = R; the impulse response is mostly polarity-inverted (taps +0.278, −0.721, −0.130, then a positive
  tail decaying by about 0.78 per sample), +0.55 dB at DC; the magnitude +0.54 dB at 30 Hz, +0.37 at 1 kHz, −0.12 at
  10 kHz, −0.90 at 16 kHz, −1.71 at 20 kHz.
- **The always-on de-esser**: nothing below 3 kHz; above about 4 kHz a tone is turned down from about −13 dBFS peak, by
  nearly 1 dB per dB above that (6 kHz: −0.9 at −12, −6.2 at −6, −11.1 at 0 dBFS; 8–10 kHz down 13 dB at 0 dBFS). Attack
  within 1 ms; after a drop it holds about 5 ms, then releases (−5.0 dB at 10 ms, −2.9 at 20, −0.8 at 50, 0 by 100 ms).
  Noise at −12 dBFS rms is already 1.8 dB down at 5 kHz.

### Gains, Balance, the Noise, Mono/Stereo (`gains`, `noise`)
- **Sensitivity** and **Output** are plain gains, exactly the fader's dB (±5, ±10 → ±5.00, ±10.00 dB) at a linear level;
  Sensitivity comes before the dynamic stages.
- **Balance** turns down the far channel only: −6 → L 0, R −6.00 dB; +3 → L −3.00, R 0.
- **Noise** (the state's slot 67, on by default): white, uncorrelated L / R, −108.3 dBFS rms with every colour clear
  (−106.6 with the defaults, the Compress makeup lifting it); flat within ±2 dB from 30 Hz to 20 kHz.
- **Mono/Stereo** (RefRender): L and R identical with every colour clear, the same response as the Stereo component's.

### Bass and Treble (`eq`: each colour and fader −10 … +10 against every colour clear, −40 dBFS noise)
Full curves in `eq.npz`; at fader 0 Treble's colours change nothing, Bass's set a subsonic high-pass.
- **Bass Sub / Lower**: a low shelf / bump at about 50–100 Hz (Lower reaching higher), fader +10 → +11.8 / +12.6 dB at
  50 Hz, −10 → −11.2 / −11.8; plus a subsonic high-pass (−2.2 dB at 30 Hz) at every setting.
- **Bass Upper**: at 0 a steeper high-pass (−10 dB at 30 Hz, −2.9 at 50); boosts peak near 100 Hz (+10.1 at fader
  +10) with a −1.3 dB dip at 1 kHz; cuts are a broad low shelf (−11.7 at 30–100 Hz) with a +1.4 dB bump at 1 kHz.
- **Treble Bite**: a bell at about 5 kHz, ±12 / −10.4 dB at ±10. **Top**: a high shelf from about 2 kHz, +12.6 / −9.9 at
  16 kHz. **Roof**: a high shelf from about 3–5 kHz, +11.3 / −13.5, its boosts with a −1.3 dB dip at 500 Hz.
- **The fader law is not linear in dB** and differs between cut and boost (Bass Sub at 50 Hz: +2.5 → +2.8, +5 → +4.8,
  +10 → +11.8; −2.5 → −2.3, −10 → −11.2): Phase 4 fits each colour's parameters per fader position.

### Compress (`compress`: a 1 kHz tone stepping −45 … 0 dBFS, against every colour clear)
- **Push**: a makeup gain and a ceiling: the gain below −9 dBFS is −3.9 / −2.1 / +1.4 / +5.5 / +8.9 dB at fader −10 / −5 /
  0 / +5 / +10, and the output is held near the same level above (at 0 dBFS in, −6.2 … −4.1 dB). Timing (fader 0, a
  −30 → −6 dBFS step): settles in about 72 ms; recovers within 1 ms after the drop.
- **Spank**: more makeup (+1.5 / +3.3 / +7.0 / +10.7 / +13.7) and a softer knee from about −27 dBFS at fader +10; attack
  t63 6 ms, release t63 104 ms.
- **Wall**: makeup +1.5 / +3.3 / +6.7 / +10.8 / +14.2, a harder knee; attack 10 ms, release 17 ms.

### The wet stages (`reverb`, `delay`, `pitch`: a −20 dBFS impulse, the output less the output with the colour clear)
- **Every wet stage's level follows its fader linearly in dB, 1.2 dB per step**, and the fader leaves its shape alone.
- **Reverb**: Tight T60 0.94 s, first arrival 28 ms, L 1.5 dB over R; Large T60 1.69 s, first 3.7 ms; Chamber T60
  3.48 s, first 193 ms. L / R nearly uncorrelated (+0.01 … +0.08). Energy at fader 0: −19.9 / −20.4 / −15.4 dB.
- **Delay** (synced to the host's tempo; pedalboard plays at 120 bpm): ping-pong, about 6 dB lower per bounce, R first.
  Slap: one echo at a **1/16 note** (RefRender at 60 / 90 / 120 / 140 / 180 bpm: 249.2 / 166.7 / 125.0 / 107.1 /
  83.4 ms). Eight: **1/8 note** bounces (120 bpm: 249, 507, 749 ms; at 60 bpm the first arrives at 481.5 instead of
  500). Quarter: about a 1/4 note, its echoes spread (first arrivals read short of the note: 481 ms at 120 bpm).
- **Pitch** (a doubler): one voice per side, delayed and slowly pitch-modulated (0.2 Hz): Stereo 9.2 ms L / 17.4 ms R,
  ±7 cents; Wide 11.7 / 39.7 ms; Spreader 3.9 / 9.4 ms. Its level at fader +10 is about the dry level.

### The Sensitivity LED (MeterCapture `gl=1`, a 1 kHz tone stepping −40 … +8 dBFS)
The LED's frame (of 24) is **round(23 · (Y + 144) / 144)**, Y the state's curve (X −144 / −20 / −12 / −4 / 0 dB → Y −144 /
−109.56 / −72 / −34.43 / 0; the ParamXML's defaults are not the live ones) applied, piecewise linearly, to the input's
**rms** level (a sine's peak −3 dB): every step from −40 to +8 dBFS matches. It follows within a frame and falls to off
within about 60 ms of silence.

### The control structure (behaviour analysis of the binary, checked against the audio)
A behaviour analysis of `CLA Vocals.dll` with REA (notes and evidence in the git-ignored `reference/wvoxchain/`) and
the chain's own presets (`nulltest/voxchain_children.py` names every child setting of `Presets/1010.xml` / `1011.xml`
from the children's ParamXML) give the structure; each point below is also **measured** above unless marked:
- Each fader sets **one child parameter on a straight line** (slope × fader + offset), per colour. The wet faders set
  only the gain stage after their effect, 1.2 dB per step (measured: 1.2 dB per step, shape unchanged); a wet colour's
  Clear mutes that gain stage while the effect keeps running.
- **Bass** Sub / Lower: the SSL Channel's LF band at 1.2 dB per step, its high-pass at 45 Hz (measured: the subsonic
  −2.2 dB at 30 Hz), the LF frequency 60 Hz (Sub) or 100 Hz (Lower; measured: Lower reaches higher). **Upper**: the
  V-EQ3's LF gain, its high-pass switched in from fader 0 up (measured: the steep high-pass at 0 and above, none on cuts).
- **Treble** Bite: the SSL's HMF bell (4.5 kHz in the chain's preset; measured bell near 5 kHz); Top: the SSL's HF shelf;
  Roof: the V-EQ3's HF band.
- **Compress**: Clear bypasses the L1 and the 1176; a colour loads their sub-presets; the fader moves the 1176's input
  (the direction and the rest of the law to be fitted in Phase 4: measured, the gain rises with the fader).
- With every colour clear the SSL Channel, the V-EQ3 and both de-essers stay in the path (measured: the fixed filter and
  the high-frequency compression, matching the "Main" de-esser's 5 kHz crossover).
- **Balance** turns down one side of the first gain stage (measured). The Sensitivity LED shows a child's own meter
  through the X / Y curve (measured: the input's rms level).
- Not settled by either: why the state rejects the bypass and colour slots, the Noise's source, the latency's split
  (it adds up to 193 samples), and the tempo law of the Quarter delay (in the H-Delay / SuperTap children).
