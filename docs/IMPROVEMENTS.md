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

- **Release law** (the main open item): the two-state law, refitted by CMA-ES (`nulltest/cma_hc_release.py`), nulls
  voice and bass at the defaults (−62 / −59 dB) and drums at −32, but not deep, slow settings or noise (−10 … −35 dB).
  CMA-ES is the optimiser that works here; use it to compare structures (each run about 20 minutes).
  Open: Fat Kick on noise (−12 dB), Drum Punch and Vintage Drum Crunch on bass (−17 dB). What is known and what was tried is in
  MEASUREMENTS.md "WComp model": a start-up state, the settled program dependence, a Crest average of about 150 ms.
  Next: a state for the duration of the current event; validate on voice-like and bass line first.
- **Limiter**: H-Comp's limiter sees the true future output; WComp estimates it (−16 … −43 dB under heavy limiting).
- **VU print**: drawn about a centre about 0.4 px from the printed arc's (MEASUREMENTS.md "WComp needle"). The needle's
  edges are a little harder than the reference's (its strip images are resampled; within 0.1 in darkness).
- **Analog mode 3**: nulls −29 … −64 dB (median −40); a finer table near the transition (0.02 … 1) may help.
