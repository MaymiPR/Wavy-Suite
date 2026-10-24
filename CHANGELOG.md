# Changelog

## Unreleased

- **WComp** (ready for the next release; Phase 5: in the installers' plug-in list, FocusCheck, sample-rate checks),
  a hybrid compressor modelled on H-Comp: the Hybrid-line editor (643 x 454, resizable)
  drawn entirely by Wavy on a new shared kit (`Source/common/WavyHybridUI`: the orange panel, bakelite and chrome
  knobs with printed scales, ivory buttons, a VU meter with needle, the amber BPM display) and a Hybrid toolbar style;
  the reference's parameter list and tapers (Ratio's own curve, log Attack / Release, Threshold 0 at the left), a
  side-chain input bus (ignored, as the reference does); selection, value entry, Alt-click reset and drag feel as
  measured on the reference.
- **WComp DSP** (Phase 4): the look-ahead peak compressor identified on the reference (detector, static curve to
  0.014 dB, makeup, attack, smoothing, mix, output, a 64-sample look-ahead limiter, tempo-synced release with the
  long-note fallback, linked stereo, Mode Mono), the two-state program-dependent release, and the four Analog modes
  (the last stage: filtered nonlinearity and shaped noise; mode 4 nulls at −82 … −100 dB on loud material). Null test
  median −37.2 dB (defaults −62 dB; noise, drums and deep slow settings are not nulls yet;
  docs/NULL_RESULTS_WCOMP.md). VU needle and limiter lamp driven (provisional VU ballistics). Unit tests (`WCompTests`),
  offline renderer (`WCompRender`).
- **WComp needle and lamp, as filmed on the reference**: H-Comp's editor filmed without FL (its OpenGL frames read back
  in `tools/MeterCapture gl=1`, which now also gives the plug-in a playing transport). The needle is a strip of 71
  positions at the filmed whole-degree angles about the measured pivot, with the reference's look: 2 px, tip 111.5,
  the rivet (radius 2.85, cream centre 1.15) and the shadow (the needle moved 4.7 px straight down, Gaussian 0.94 px,
  ×0.75), each measured against WComp's own render (`tools/VuRender`). GR: the
  deepest gain at once, the return through two one-poles (28.9 / 142.6 ms) on the linear gain — on drums it reads
  11.5 dB as H-Comp does (the provisional VU average read 6.2). In / Out: the peak level on their own dBFS face
  (48 … 0, 24 at the top, now drawn), with the reference's hold-and-release law. The limiter lamp glows amber and
  fades over its 0.833 s hold.
- **Wheel and Ctrl fine-tuning on WVox, WBass and WBassX, as measured on RVox, RBass and MaxxBass**: the wheel turns
  every control 1/72 of its range per notch, over the whole control (the faders took no wheel at all, the knobs 2.5
  times too much); Ctrl fine-tunes the faders at the reference's rates (0.19 of the normal rate on quicker mouse moves,
  0.037 on slow ones; WVox and WBass fell into JUCE's velocity mode with Ctrl, WBassX moved a flat quarter); WBass's
  Freq moves 1.53 times its track, as the reference's.
- **WComp knob feel as measured on H-Comp**: the mouse wheel moves 1/72 of a knob's range per notch (it moved 2.6 times
  that), Ctrl gives a quarter of the normal rate and keeps the slow / fast difference (it was a flat 1/6), the fast
  rate starts at 3 px mouse steps, and the stepped knobs have their own rates (Analog a position per 50 px; the synced
  Release note a value per 30 px slow, 20 fast, 120 / 66.7 with Ctrl). MeterCapture's drag script gained wheel moves.
- **WComp presets and value entry**: the reference's 26 factory presets (*Essentials* and *Studio*, Wavy names; array
  positions confirmed by loading presets into the reference), which leave Mix alone as the reference does; null test of
  every preset in docs/NULL_RESULTS_WCOMP_PRESETS.md. Typing a positive number into Threshold sets it negative ("18" →
  −18 dB), as the reference's ParamXML marks it (PositiveTextEntryToNegative); WBassX's Decay now does the same.
- **WComp release refit**: the release law's tables refitted by CMA-ES on probes from a settled state, repeated-event
  and memory probes, realistic envelopes and the null-test material (null-test median −37.2 → −39.6 dB; drums at the
  defaults −20.7 → −31.7 dB, voice and bass unchanged); then the presets' settings added to the fit and the
  reference's start-up state modelled (slow release in the first fraction of a second); the release memory now
  charges from the reduction after the attack (long-attack presets: Fat Kick drums −12.9 → −21.7 dB; presets' median
  −32.0 → −35.5 dB); the memory drains no faster than a 16.8 ms release (sustained bass); a final joint fit:
  null-test median −41.4 dB, presets' median −35.6 dB.
- WComp Phase 3: measurements of the reference H-Comp (`nulltest/measure_hcomp.py`, `measurements/hcomp/`, MEASUREMENTS.md).
- **WBassX presets, value entry and meters** (Phase 5): 22 factory presets (*Essentials* and *Studio*, settings
  from the reference's own set converted by the reference itself, Wavy names), which leave Input alone as the
  reference does; double-click a knob, fader or the Freq slider to type a value, Alt-click to reset (measured on the
  reference, the entry box matches it to the pixel); output meters with the reference's faster release (0.155 s),
  its scale and its floor dot, within 1 px on 88 % of frames.
- **WBassX DSP** (Phase 4): the original path, the harmonic generator with its Decay feedback, the High-pass settings,
  faders, monitors and output clip identified exactly; the harmonics compressor learned from renders of the reference
  (median null −48.5 dB on the whole output, −42.7 dB on the harmonics; docs/NULL_RESULTS_WBASSX.md). Unit tests
  (`WBassXTests`) and an offline renderer (`WBassXRender`) for the null test.
- WBassX Phase 3: measurements of the reference MaxxBass (`nulltest/measure_maxxbass.py`, `measurements/maxxbass/`, MEASUREMENTS.md).
- Meters: peak holds read -Inf in silence (an empty frame no longer counts as -80 dB).
- **Factory presets** for WVox (21) and WBass (45), in two groups, *Essentials* and *Studio*, in the preset menu
  (Prev / Next step through them) and in the host's program list. The settings come from the reference's own
  presets; every name is Wavy's own (no artist, product or other trademarked names). Generated by
  `tools/make_presets.py`.
- **WBass**, a psychoacoustic bass enhancer (harmonic generator), joins the suite: dark skin on the reference's pixel grid; DSP from the Phase 4 identification, with the gain control of the generated bass learned
  from renders of the reference (a small GRU, ~6 % of one core); null test median −36.8 dB; Original Bass and Smooth are two-choice parameters
  so a restored state always snaps to on or off. Meters move like the reference's (measured from its editor: instant
  rise, 0.325 s one-pole release, 26.7 dB/s in silence), independent of the host's buffer size; editors redraw at
  60 frames per second.
- **WBassX**, a bass enhancer modelled on a harmonics-generator with its own compressor, filters and
  monitor modes: the flat editor (800 x 395, resizable) on a new shared kit (`Source/common/WavyFlatUI`: gain faders
  with the reference's taper, knobs, frequency slider, slope selector, monitor buttons, band graph) and the single-row
  toolbar style; full parameter list.
- **WVox null**: the compressor's slow path gets a small learned correction (a GRU trained on renders of the
  reference, ~3.5 % of one core), guarded so steady signals keep the identified paths exactly. Worst case of the
  settings grid −10 → −23 dB; dense noise at heavy compression −10…−13 → −24…−27 dB, voice-like −21…−31 → −30…−40 dB;
  AM at light compression is the one notable loss (−59 → −49 dB).
- **WVox meters** measured from RVox's editor and matched: Output as above, Energy with two release stages (0.341 s
  then 0.094 s), Gain reduction shows the deepest reduction of each frame (short bursts are no longer missed).

## 0.1.0

First release of the Wavy Suite, with one plugin:

- **WVox**, a three-control vocal compressor / gate / limiter (VST3, CLAP, AU; Windows, macOS, Linux).
  - Dark skin on the reference's pixel grid, resizable 75–200 %, Waves-style toolbar (Setup A/B with copy, undo / redo, presets with Save, Mode Stereo / Mono, window size), click a value to type it.
  - DSP from measurements: four-phase true-peak detector, exact static law, identified look-ahead (64 / 128 / 256 samples) and gate; fitted slow path.
  - Null test against the reference: steady material nulls (−95 to −120 dB at any setting); dynamic material is close (median −71 dB over a 672-case settings grid; dense noise at heavy compression −10 to −25 dB).
- One installer per operating system for the whole suite.
