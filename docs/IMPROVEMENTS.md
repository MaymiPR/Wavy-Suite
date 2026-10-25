# Improvement notes

Things that are good enough for now but could be pushed further later, and how. Each item says where it stands and
what the next step would be.

## The learned-block procedure (used for WBass, WVox and WBassX)

When part of a reference resists identification, keep everything that *is* identified exact and learn only that part:

1. **Expose the internals.** Make the offline renderer dump the signals around the unknown block (WVox:
   `WVoxRender dump=1` writes detector level, fast ratio and fitted slow ratio per sample).
2. **Render a training set** of short random clips of the material the plug-in is used on, across all settings and
   levels (WBass: `nulltest/render_trainset.py`; WVox: `nulltest/render_trainset_wvox.py`). Include very quiet input
   and plenty of steady tones and level steps; a network only behaves where it has seen data.
3. **Read the reference's own gain** where the plug-in is a pure gain stage (output / delayed input / fixed gain). It
   gives a direct, sample-by-sample target.
4. **Train a small GRU** in PyTorch (`C:\Users\wint1\.venvs\wavy-torch`): first supervised on that gain, then
   fine-tuned on the null residual itself. Training only on the residual can stall when the learned branch sits
   behind a `min()` (no gradient while it is inactive).
5. **Add structural guards** so the network cannot spoil cases the identified model already nulls (WVox: a floor at
   the static-law ratio of the loudest level of the last second, and the correction fades out while the fast ratio
   is flat). Without them the network cost 10–60 dB on steady tones.
6. **Train end-to-end through the exact structure when it is recursive** (WBassX): run the identified recursion and its
   exact adjoint in C (`nulltest/mbfast.c`) as a PyTorch autograd function, the filters as FFT convolutions; then no
   gain targets are needed, renders cost 0.1 s each, and thousands of them are cheap. Give the network the signal the
   reference listens to (WBassX: after the high-pass) — that alone was worth 14 dB.
7. **Export to a header** (`tools/export_gainnet.py`, `tools/export_wvoxnet.py`), run it on a fixed virtual clock so
   it behaves the same at every sample rate, check C++ against PyTorch (better than −70 dB), then the full null
   suite, the settings grid, other sample rates, unit tests and pluginval.

## Open items

### WVox
- **Real vocals have not been null-tested.** All "voice" signals are synthetic (harmonic tone, syllable envelopes,
  formants). Next: null WVox against RVox on a few real dry vocal stems at typical settings (Compression −6 to −24).
- **Automating Compression while audio plays** was never in the training data (one fixed setting per clip). Next:
  render clips with Compression ramps and steps, check the null, add them to training if needed.
- **AM at light compression** got worse with the learned path (−59 → −49 dB at Comp −12); level steps at Comp −6 /
  −12 went from about −100 to −80…−88 dB. Still inaudible; a targeted fine-tune could recover them.
- **Dense material at the heaviest settings** is close but not a null (noise −24 to −33 dB, voice about −30 dB at
  Comp −36). The slow path itself is still not understood (see MEASUREMENTS.md, "What is still not understood").
- Ideas not yet tried: a longer training run with more data, a larger network, input features from the gate.

### WBassX
- **The learned level control is about 8 dB short of the structure's ceiling** (random grid: −28 dB median against
  −37 dB with the reference's own extracted gain; `nulltest/oracle_grid_mb.py`). More data, more units and long
  fine-tunes gave a few dB each; envelope-follower inputs and feeding back the output gave nothing, and an explicit
  two-follower detector with a learned gain law (`train_wbassx_det.py`) was 8 dB worse than the GRU (−30.8 against
  −39.1 dB on the validation set). Next idea: a supervised pretraining stage on constrained extracted gains
  (`extract_g3.py`) mixed with the output loss.
- **Kicks** (−19 dB on the harmonics) are close to what the structure reaches with the reference's own gain (−21 …
  −28 dB). The rest is spread over all bands; part of it is the reference's monitor clipping at ±1 on the first peak.
- **Extreme settings** (strong Decay, high Ratio, loud low bass near Freq): the reference's Decay loop turns chaotic
  (Nyquist oscillation). Not reproducible sample by sample; WBassX goes unstable under the same condition and stays
  bounded (y within ±1).
- **Real material** (bass stems, full mixes) has not been null-tested; all signals are synthetic.

### WBass
- **Weak spots:** Freq 256 (created harmonics about −17 dB median), kick-like transients (about −22 dB), and Freq 160
  with Original Bass Out on very low input (30 Hz tone −4 dB). Next: more training renders of those cases.
- **Original Bass meter:** after a long steady tone stops, the reference bar sometimes snaps down at once instead of
  easing; not explained (rules tried in `nulltest/fit_meter_vox.py`). Not visible on music.

### Meters
- The same snap-down quirk exists on the RVox output meter. Everything else matches the reference within a few pixels.

## WComp

- **Release law and level detection**: the two-state law (CMA-ES, `nulltest/cma_hc_release.py`) plus the learned gain
  correction (`nulltest/train_wcomp.py`, MEASUREMENTS.md "WComp learned gain correction"). Still weakest: low notes
  under long attacks (bass at Attack 30 … 60 ms: −15 … −20 dB), Release 300 on voice (−22 dB), fast attacks on drums
  (−23 dB). The reference's level detection averages across low cycles in a way the held-peak structure does not;
  identifying that (rather than correcting it) would help most. More training data around those settings is the
  cheap next step (render_trainset_wcomp.py, then retrain; keep the steady-tone guard and the no-worse penalty).
- **Limiter**: H-Comp's limiter sees the true future output; WComp estimates it (−16 … −43 dB under heavy limiting).
- **VU print**: drawn about a centre about 0.4 px from the printed arc's (MEASUREMENTS.md "WComp needle"). The needle's
  edges are a little harder than the reference's (its strip images are resampled; within 0.1 in darkness).
- **Analog mode 3**: nulls −29 … −64 dB (median −40); a finer table near the transition (0.02 … 1) may help.

## WDelay
- **Glide tails:** in a glide's slow tail the interpolator's 1/32 phase changes on different samples than the
  reference's (−40 … −80 dB on broadband material while it settles). It needs the reference's exact arithmetic for
  the glide; neither double nor float32 variants of the update reproduce it.
- **The interpolator's window** is not quite Kaiser (up to 4e-4). LoFi shows it (−70 dB). It needs a clean
  measurement of the window at every phase: the sweep during a glide is too smeared.
- **Modulation:** the LFO's phase law (0.25 + rate × (t + 0.4805 s)) and the delay curve agree to a few samples, not
  to the sample (−5 … −16 dB on modulated material). Its fine structure is still to be identified (a control rate?
  float phase?).
- **96 kHz:** the first echo matches (−78 dB), the passes round the feedback loop do not (−28 dB).
- **Analog modes 1–3:** −40 … −55 dB including their noise. Mode 3 is the weakest (−40 dB); more fitting rounds or
  more data at low frequencies.

