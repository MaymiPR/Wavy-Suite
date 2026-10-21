# Wavy Suite — Research

Phase 1 evidence for each plugin. One section per plugin, written before any code. "Local" sources are files shipped
in the user's own Waves V17 install (read for analysis; none of the Waves files are copied into this repo).

Sources
- **S1** Local manual `Plug-Ins V17\RVox.bundle\Contents\Resources\RVox.pdf` (7 pages, "Renaissance Vox User Guide").
- **S2** Local `ParamXML\1000.xml` — parameter names, ranges, scale types, defaults.
- **S3** Local `GUIXML\1000.xml` — control rectangles, fonts, colors, view size.
- **S4** Local `ProcessXML\1000.xml`, `AlgXML\1001.xml` — components (mono/stereo), algorithm name `RVOX`.
- **S5** Web search, 2026-10-17 (reviews and shop pages; secondary, not manufacturer): [Joseph Nilo, "Waves RVox explained"](https://josephnilo.com/blog/waves-rvox-explained/), [Toolfarm](https://toolfarm.com/buy/waves_renaissance_vox), [Sweetwater](https://www.sweetwater.com/store/detail/RenVox--waves-renaissance-vox-plug-in/reviews).
- **S6** Official product page, fetched 2026-10-17: [waves.com/plugins/renaissance-vox](https://www.waves.com/plugins/renaissance-vox). Official manual PDF linked from it: [renaissance-vox.pdf](https://assets.wavescdn.com/pdf/plugins/renaissance-vox.pdf) (matches local S1). The earlier guess `waves.com/plugins/rvox` is a 404.

### Product-page facts (S6)
- Described as a classic compressor / limiter for vocals; three controls: Gate, Compression, Output Gain.
- Gate described as gentle downward expansion, not a hard gate; compressor = soft-knee curve; limiting/level maximizing run automatically (no limiter control); smart metering.
- Components: mono and stereo only. Skins: light, dark, legacy.
- **Published latency:** 64 samples @ 44.1/48 kHz, 128 @ 88.2/96 kHz, 256 @ 176.4/192 kHz (67 on TDM 48 kHz). Max sample rate 192 kHz.

## Naming (Wavy identity, recognisable, own branding)

| Waves original | Wavy plugin | Product folder |
|---|---|---|
| Renaissance Vox (RVox) | **WVox** | WVox |
| Renaissance Bass (RBass) | **WBass** | WBass |
| MaxxBass | **WBassX** (renamed 2026-10-18: the first name held the whole product mark) | WBassX |
| H-Comp | **WComp** | WComp |
| H-Delay | **WDelay** | WDelay |
| Bass Rider | **WBass Ride** | WBassRide |
| Vocal Rider | **WVox Ride** | WVoxRide |
| CLA Vocals | **WVox Chain** | WVoxChain |
| Vocal Bender | **WVox Bend** | WVoxBend |

Mono / stereo / mono→stereo / live components are one plugin with a **Mode** switch.

---

## 1. WVox (Renaissance Vox)

### Documented behaviour (S1)
- Dynamics processor for vocals that puts compression, gating and limiting behind three controls (p.2).
- Components: mono and stereo, otherwise identical (p.2). Skins: Light, Dark, Legacy; same controls, values unchanged on skin switch (p.2).
- **Compression** fader: one control combining threshold and ratio. Lowering it increases gain reduction *and* output level; output is held by a peak limiter. Range printed as "-36 dB to -36 dB" (typo in manual; S2 says -36…0). Double-click enters a value (p.5).
- **Gain**: the *output ceiling* of the limiter, not a plain fader. At high Gain settings the manual says make-up gain keeps the output level fairly even as Compression changes (p.5). Compression is **post-gate** (p.5).
- **Gate**: downward expander, described as very gentle with a **soft knee** (the manual notes some reduction even with a very low threshold), range −∞…0 dB; superimposed on the input energy meter (p.6).
- **Total Compression display**: sum of compressor + gate gain reduction, peak value below, click to reset hold (p.5).
- **Output meters**: full-scale, peak value below, click to reset; −∞…0 dB (p.6).
- **Input Energy meter** left side, reference for setting the gate (p.6).

### Parameters (S2)
| ID | Label | Range | Step | Default | Notes |
|---|---|---|---|---|---|
| PARAM_COMP_THRESH | Thresh (Compression) | −36…0 dB | 0.1 | state 1000 (=0 dB, top) | positive text entry maps to negative |
| PARAM_GATE_THRESH | Gate | −80…0 dB | 0.1 | −80 dB | DisplayGate (−80 shows as off) |
| PARAM_GATE_RELEASE | GateRel | 10…250 ms | 0.1 | reset 130 ms | **hidden**, not on the GUI |
| PARAM_OUT_VU_L / _R | Out VU | −36…0 | 0.1 | display only | meters |
| PARAM_ENERGY | Energy | −80…0 dB | 0.1 | display only | input energy meter |
| PARAM_ATTN | Attn | −36…0 dB | 0.1 | display only | total gain reduction |
| PARAM_OUTPUT_GAIN | Gain | −36…0 dB | 0.1 | state 1000 (=0 dB) | limiter ceiling |
| PARAM_SKIN_TYPE | Skin | 0…2 | toggle | 0 | Light/Dark/Legacy |

### GUI geometry (S3)
View **371 × 357 px**. Controls (l, t, w, h): Compression 135,99,102,207 (value text box 135,109,102×248, Arial 17 Win, rgb 211/213/215); Gate 41,105,33,195 (text 37,89,42×16, Arial 17, rgb 90); Gain 297,105,33,195; Energy meter 50,121,15,156; Total compression (Attn) 145,121,82,155; Out VU L 307,122,5,155 stereo / 311,122,5,155 mono; Out VU R 315,122,5,155; Skin button 23,2,49,15. Mono hides Out VU R.

### Open questions for Phase 3
- Gate knee: manual says soft; earlier measurement suggested hard. Re-measure.
- Latency: published per sample rate; measure.
- Limiter type/lookahead; compressor detector (peak vs RMS); ballistics.

---

## 2. WBass (Renaissance Bass)

### Sources
- **B1** Local manual `Plug-Ins V17\Documents\RBass.pdf` (9 pages, Renaissance Bass User Guide).
- **B2** Local `RBass.bundle\Contents\Resources\ParamXML\1000.xml`, `GUIXML\1000.xml`, `ToolbarXML\1001.xml` (identical to RVox's), `AlgXML\1001.xml`, `ProcessXML\1000.xml`, skins in `PNG_.zip` (Light 15000, Dark 15002; private, extracted to the git-ignored `reference/`).
- **B3** Factory and artist presets: `Waves\Data\Presets\Renaissance Bass\` (49 `.xps` files; the `RealWorld` line is the full 18-value parameter array).
- **B4** Official product page, read 2026-10-18: [waves.com/plugins/renaissance-bass](https://www.waves.com/plugins/renaissance-bass) (overview and Tech Specs tab).
- **B5** The installed VST3 (`WaveShell1-VST3 17.1`) as seen by a host: components *RBass Mono* and *RBass Stereo*; automatable parameters Bypass, Frequency (32–256 Hz), Intensity (−24…+24 dB), Out Gain (−30…0 dB); reported latency 0.

### Documented behaviour (B1, B4)
- Psychoacoustic bass enhancer: it generates harmonics of the low end so the ear infers a fundamental that small speakers cannot reproduce (the "missing fundamental"); it does not generate sub-harmonics. Based on the MaxxBass technology, with a clip-free signal flow.
- **Freq** (32–256 Hz): the frequency from which harmonics are generated; the input below it is removed and replaced by generated harmonics.
- **Original Bass In / Out**: whether the original signal below Freq is mixed back with the generated harmonics.
- **Intensity** (−24…+24 dB): level of the generated harmonics; the manual suggests starting below 0 dB and warns high settings can distort.
- **Gain** (−∞…0 dB in the manual, −30…0 dB in ParamXML): output level.
- Meters: Original Bass (input energy below Freq), Created Harmonics (generated harmonics, depends on Intensity and Freq), Output (full scale, infinite peak hold, click to reset; clip light, click to reset).
- Product page: clip protection simplifies gain staging; skins Light / Dark / Legacy, same controls.

### Specifications (B4)
- Latency 0 samples at 44.1 – 192 kHz (3 on TDM 48 kHz). Components: mono and stereo only (no M/S, no surround). Maximum sample rate 192 kHz.

### Parameters (B2), in preset-array order
| # | ID | Label | Range | Default | On GUI | In VST3 |
|---|---|---|---|---|---|---|
| 0 | PARAM_IN_GAIN | Input | −30…0 dB, 0.1 | 0 | no | no |
| 1 | PARAM_FREQ | Freq | 32…256 Hz, logarithmic, 1 Hz | 80 (reset) | yes (horizontal slider) | yes |
| 2 | PARAM_REPONSE | Response | 0…30 ms, 0.1 | 30 (reset) | no | no |
| 3 | PARAM_RATIO | Ratio | 0.25…1, 0.01 | 0.63 | no | no |
| 4 | PARAM_CLIP | Clip | toggle | 0 | clip light | no |
| 5 | PARAM_MB_GAIN | MaxxBass (Intensity) | −24…+24 dB, 0.1 | 0 | yes (big fader) | yes |
| 6 | PARAM_HP_INOUT | Hi Pass (Original Bass In/Out) | toggle | 0 | yes (switch) | **no** |
| 7 | PARAM_OUT_GAIN | Gain | −30…0 dB, 0.1 | 0 | yes | yes |
| 8 | PARAM_DECAY | Decay | 0…1, 0.1 | 1 | no | no |
| 9–12 | MAXXBASS_VU, ORIGBASS_VU, VU_L, VU_R | meters | −30…0 | display only | yes | no |
| 13 | PARAM_SMOOTH_MULT_TC | SmoothTc | 0…1000 | 20 | no | no |
| 14 | PARAM_TC | Tc | 0…2000 ms | 20 | no | no |
| 15 | PARAM_XFADE_MULT_TC | XFadeTc | 0…5000 | 10 | no | no |
| 16 | PARAM_SMOOTH_EN | Smooth | toggle | 1 | no | no |
| 17 | PARAM_SKIN_TYPE | Skin | 0…2 | 0 | toolbar | no |

The first factory/artist preset read (Low Boost Elec. Fiddle) holds `0 86 5 0.63 0 -1 1 0 1 * * * * 20 20 10 1`, so the hidden parameters (Input 0, Response 5 ms, Ratio 0.63, Decay 1, smoothing 20 / 20 / 10, Smooth on) are part of every preset and must be kept by WBass even though they have no control.

### GUI geometry (B2), dark skin
View **371 × 392 px**, the same toolbar strip as RVox. Controls (l, t, w, h): Intensity fader 135, 99, 102, 207 (value text white-grey 211/213/215, Arial 17); Out Gain fader 297, 105, 33, 195 (text 292, 85, 42 × 16); Frequency horizontal slider 120, 334, 132 × 41 (text 249, 340, 42 × 16); Original Bass In/Out toggle 41, 88, 33 × 38; Created Harmonics meter 145, 121, 82 × 155 (readout 143, 287, 82 × 25, Arial 16); Original Bass meter 50, 121, 15 × 156 (readout 39, 303); Output meters 307 / 315, 122, 5 × 155 stereo, 311 mono (readouts 278 / 315, 303, mono 295); clip light 302, 103, 23 × 13; Skins 23, 2, 49 × 15. Meter scale 3 / 9 / 15 / 21 / 27 dB in the centre window.

### Presets (B3, converted)
All 45 factory and artist presets of the reference (Full Reset excluded) are carried over as WBass factory presets by
their setting values only (`nulltest/read_ref_presets.py` converts the "States" files through the reference itself,
`tools/make_presets.py` writes `plugins/WBass/WBassPresets.h`). Every name is new: no artist names, no Waves product
names, no other trademarks; the name says what the setting is for. The same is done for WVox (21 presets; the
reference's "States" presets use its linear scales, checked against the two it converts itself).

### Notes for the next phases
- The Original Bass switch is not automatable through the VST3; Phase 3 must set it through the plugin state to measure both positions.
- Hidden constants (Response, Ratio, Decay, smoothing) shape the sound; WBass keeps them as non-automatable parameters with these defaults so presets round-trip.
- The harmonic generator (which harmonics, their phase, level dependence, the "clip-free" stage) is the part most likely to be hard to null; the filters and gains should null.

---

## 3. WBassX (MaxxBass)

### Sources
- **M1** Local manual `Plug-Ins V17\Documents\MaxxBass.pdf` (13 pages, MaxxBass User Guide; the same file ships in the bundle).
- **M2** Local `MaxxBass.bundle\Contents\Resources\ParamXML\1000.xml`, `GUIXML\1000.xml`, `ToolbarXML\1001.xml`,
  `AlgXML\1001.xml` (algorithm `MXBS`), `ProcessXML\1000.xml`, `PageTable\3950.xml`, and the presets in
  `Presets\1000.xml` (22 presets; there is no separate preset folder in `Waves\Data\Presets`). No skin folder: the
  editor is the newer flat Waves design, one look.
- **M3** The installed VST3 (`WaveShell1-VST3 17.1`) as seen by a host: components *MaxxBass Mono* and *MaxxBass
  Stereo*; **every control is an automatable parameter** (input_gain, orig_bass_gain, harmonics_gain, ratio,
  response, harmonics_decay, highpass, frequency, monitor); reported latency 0.
- **M4** A screenshot of the real editor taken with `tools/MeterCapture` (kept in the git-ignored `build/`).

### Documented behaviour (M1)
- Psychoacoustic bass enhancer (the "missing fundamental"): below the crossover **Frequency** (32–256 Hz) the bass
  gets harmonics generated for it; above it the signal passes unaltered. RBass is described as based on this
  technology.
- **Mix section:** Input (manual: 0 dB … −inf; the VST3 goes to +12 dB), **Original Bass** level of the low band
  (+12 dB … −inf), **MaxxBass** (Harmonics) level of the generated harmonics (+12 dB … −inf; default −inf, so the
  default preset adds nothing).
- **Harmonics section:** high-pass on the generated harmonics: 6 dB/oct at 16 Hz ("wideband", mostly DC removal),
  12 dB/oct at Frequency (default), 24 dB/oct at Frequency. **Decay** (−9 … −24 dB): the level step from each
  harmonic to the next; nearer 0 is richer.
- **Dynamics section:** an **upward compressor on the harmonics only**: **Ratio** 1:1 (none; the harmonics follow
  the original bass's dynamics) to 4:1 (harmonics louder, less dynamic range); **Response** 10–30 ms sets attack and
  release together, reacting to the original bass.
- **Monitor** selector: Audio (the output), MaxxBass (harmonics solo), Original Bass (the low band being removed,
  solo). It changes what is heard, not only the meter.
- **Output meter** −40…0 dBFS for the monitored source, peak value under each bar (infinite hold), red clip light;
  a click resets peak and clip. The graph shows the original-bass band (blue) and the harmonics band (orange) as
  curves that follow Frequency, the levels, the high-pass and Decay.
- Usage notes: harmonics typically −18 … −8 dB for mixing / mastering; Frequency 50–90 Hz; small speakers: original
  bass off, high-pass 24 dB/oct, Ratio 2:1.

### Parameters (M2, M3), in preset-array order
| # | ID | Label | Range | Default | On GUI | In VST3 |
|---|---|---|---|---|---|---|
| 0 | PARAM_IN_GAIN | Input | gain scale −inf … +12 dB | 0 dB | vertical fader | input_gain |
| 1 | PARAM_FREQ | Freq | 32 … 256 Hz, logarithmic, 1 Hz | 80 Hz | horizontal slider | frequency |
| 2 | PARAM_FBF_GAIN | Decay | −24 … −9 dB, 0.1 | −15 dB | knob | harmonics_decay |
| 3 | PARAM_ATTACK | Response | 10 … 30 ms, 0.1 | 20 ms | knob | response |
| 4 | PARAM_AGC | Ratio | 1 … 4, logarithmic, 0.01 | 1.00:1 | knob | ratio |
| 5 | PARAM_MONITOR | Monitor | Audio / MaxxBass / Original Bass | Audio | three buttons | monitor |
| 6, 7 | PARAM_VU_L, PARAM_VU_R | output meters | −40 … 0 dB, clip at 0.999999, peak hold | display | yes | no |
| 8, 9 | PARAM_LP_GRAPH, PARAM_HP_GRAPH | graph curves | display data | — | graph | no |
| 10 | PARAM_OUTF_ORDER | HighPass | 6 / 12 / 24 dB/oct | 12 dB/oct | popup | highpass |
| 11 | PARAM_CLIP | Clip | toggle | off | clip light | no |
| 12 | PARAM_ORIG_GAIN | Original Bass | gain scale −inf … +12 dB | 0 dB | vertical fader | orig_bass_gain |
| 13 | PARAM_MB_GAIN | MaxxBass | gain scale −inf … +12 dB | −inf | vertical fader | harmonics_gain |

Presets do not change the Input fader (`IgnoreOnPresetLoad`). Some older factory presets hold values outside the
0–2 range in the HighPass slot (3, 4, 5, 6, 17); they will be converted through the reference itself, as for RBass. Converted (Phase 5, `nulltest/read_ref_presets_mb.py`): those
High-pass states wrap modulo 3 (3 and 6 → 6 dB/oct, 4 → 12, 5 and 17 → 24); every other value follows the scales of the
Phase 2 tapers, which reproduce all 16 presets the reference converted itself.

### GUI geometry (M2, M4)
View **800 × 395 px** including the toolbar strip (logo, undo / redo, preset box, previous / next, A→B, Setup A,
save, menu; no Skins button). Graph 3, 38, 534 × 243 (dB grid +6 … −24, frequency marks 16 / 32 / 64 / 128 / 256);
Frequency slider 86, 292, 278 × 20 (value text 380, 297); Ratio, Response and Decay knobs 50 × 50 at 108, 174 and
420, 314 (values under them, Arial 12); HighPass popup 341, 327, 51 × 21; Input fader 31 px wide, Original Bass and
MaxxBass faders 31 × 222 at 592 / 630, 89 (values under them); output meters 20 × 170 at 708 / 754, 118 (peak text
above), clip light 718, 84; Monitor buttons 69 × 15 in a 82 × 74 box at 702, 298. Some rectangles are relative to
their panel, so Phase 2 will measure positions on the screenshot.

### Presets (M2)
22 presets: 12 factory (Full Reset, PA Default, Ultralow Extender, Light, Medium, Aggressive, Multimedia, PC Laptop,
an effects preset, BoomBaxx, Hi-fi / Lo-fi system enhancer) and 10 artist presets in six artist groups. They will
be carried over by their setting values only, with new Wavy names (no artist names, no Waves product names).

### Notes for the next phases
- **Shared engine?** RBass is "based on MaxxBass technology", and its hidden parameters (Response, Ratio, Decay)
  mirror MaxxBass's controls. If the structure identified for WBass (crossover, harmonic generator, gain block,
  limiter) carries over with Ratio / Response / Decay / HighPass as variables, most of WBass can be reused. A first
  quick comparison (`build/mb_vs_rb.py`, one bass line) was inconclusive; Phase 3 measures it properly. If the gain
  block has to be learned again, Ratio and Response become inputs of the network.
- At Original Bass 0 dB the dry path is not a plain pass-through (residual +4.7 dB against the input on a bass
  line), consistent with an all-pass crossover sum like RBass's "Original Bass In".
- Monitor modes change the audio output and must be implemented.
- The editor needs widgets the Renaissance kit does not have: the band graph, vertical faders, rotary knobs, a popup
  selector, radio buttons, and a toolbar without Skins. Phase 2 is larger than for WBass.

---

## 4. WComp (H-Comp)

### Sources
- **C1** Local manual `H-Comp.bundle\Contents\Resources\H-Comp.pdf` (10 pages, H-Comp Hybrid Compressor User Guide).
- **C2** Local `H-Comp.bundle\Contents\Resources\ParamXML\1000.xml` (44 parameters), `GUIXML\1000.xml`,
  `ToolbarXML\1001.xml`, `AlgXML\1001.xml` (algorithm `HCMP`, a tempo listener), `ProcessXML\1000.xml` / `1001.xml`,
  `PageTable\3950.xml`, presets in `Presets\1000.xml` (26 presets).
- **C3** The installed VST3 (`WaveShell1-VST3 17.1`) as seen by a host: components *H-Comp Mono* and *H-Comp Stereo*;
  automatable parameters threshold, ratio, mix, attack, punch, release, sync, analog, output, limiter, meter_select,
  releasebpm; a **side-chain input bus** ("Stereo SideChain", disabled by default).
- **C4** A screenshot of the real editor taken with `tools/MeterCapture` (git-ignored `reference/wcomp/`).

### Documented behaviour (C1)
- A "hybrid" compressor: modelled transformer / tube / transistor behaviour plus plug-in precision.
- **Threshold** 0 … −48 dB (default −18), **Ratio** 1 … 50 (default 3), **Attack** 0.5 … 100 ms (default 7),
  **Release** 3 ms … 3 s (default 100 ms) or note values when synced.
- **Automatic makeup gain** keeps the compressed and uncompressed levels about the same.
- **Punch** 0 … 30 (default 1): lets transients through the compressor even with very fast attack (internally
  `PARAM_SIDE_CHAIN_DELAY`, so a delay of the detector against the audio).
- **Mix** 0 … 100 (default 100): parallel compression with the dry input.
- **Sync**: ms (manual release), Host (release in note values of the host tempo), BPM (note values of a BPM set in the
  plug-in, 30 … 300 or Off).
- **Analog** Off / 1 … 4 (default 2): 1 transformer or tube, 2 transistor, 3 driven tube / transistor tone 1,
  4 driven tone 2 **without noise**; modes 1–3 add harmonics and a small amount of noise.
- **Limiter** On: an output peak limiter; Off (default): a clipper instead.
- **Output** ±18 dB (default 0). **Meter**: In / GR / Out (default GR), a VU needle 0 … −48 dB; GR shows the total
  attenuation including mix and limiter.

### Specifications (C3, measured)
- Latency 64 samples at 44.1 / 48 kHz, 128 at 88.2 / 96 kHz, 256 at 192 kHz (1.33 ms), reported and compensated
  exactly (an impulse comes out at 0 after compensation).
- Mono and stereo components (WComp: one plug-in with a Mode switch), plus the side-chain bus.

### Parameters (C2, C3), in preset-array order
| # | ID | Label | Range | Default | In VST3 |
|---|---|---|---|---|---|
| 0 | PARAM_THRESH | Threshold | 0 … −48 dB, 0.1 | −18 | threshold |
| 1 | PARAM_RATIO | Ratio | 1 … 50, "RatioScale", 0.01 | 3 | ratio |
| 2 | PARAM_KNEE | Knee | 0 … 1 (display only) | 0.2 | — |
| 3 | PARAM_SIDE_CHAIN_DELAY | Punch | 0 … 30, 0.1 | 1 | punch |
| 4 | PARAM_MIX | Mix | 0 … 100, 0.1 (not changed by presets) | 100 | mix |
| 5 | PARAM_OUTPUT | Gain | ±18 dB, 0.1 | 0 | output |
| 6 | PARAM_ATTACK | Attack | 0.5 … 100 ms, log | 7 | attack |
| 7 | PARAM_RELEASE | Release | 3 … 3000 ms, log | 100.25 | release |
| 8 | PARAM_RELEASE_TIME_BPM | ReleaseBars | 20 note values (1/64T … 2 bars) | 5 (1/32D) | releasebpm |
| 9 | PARAM_SYNC_ON | Sync | BPM / Host / ms | ms | sync |
| 10 | PARAM_BPM | BPM | 30 … 300 | 120 | — |
| 11–18 | PEAK, LIM_COUNT, METER_TYPE, METER_DISPLAY, IN/OUT_VU L/R | meters, limiter LED, meter select (In / GR / Out) | | GR | meter_select |
| 19–21 | PDR_REL_SEED, PDR_RAT, PDR_ATT_SEED | program-dependent release: release seed 56.23 ms, ratio 10, "Crest" 149.62 ms (display only) | | | — |
| 22, 23 | FREQ, NUM_FILTERS | side-chain filter, 120 Hz (reset; initial 102), order switch "Warm / Smooth" | | | — |
| 24 | MODE | detector "Fast / Slow", designer switch "ARC / Manual" | | 1 | — |
| 25, 26 | LIM_RELEASE, INV_REL | limiter release 23.99 ms; release type "Electro / Opto" | | 0 | — |
| 27 | PARAM_LIMIT_CLIP | Limiter On / Off | | Off | limiter |
| 28 | PARAM_ANALOG | Analog Off / 1 … 4 | | 2 | analog |
| 29–33 | DITHER_NORM / GAIN1 / FREQ1 / GAIN2 / FREQ2 | analog noise: −98 dB, shaped (+30 dB at 200 Hz, −12.1 dB at 11.2 kHz) | | | — |
| 34–40 | THD, THD_SUB / LOW / HF FREQ / GAIN | analog harmonics −29.5 dB, shaped: sub 125 Hz +22.3 dB, low 250 Hz 0 dB, HF 8.9 kHz +7.1 dB | | | — |
| 41–43 | ATHD_GAIN / THRESH / NORM | even harmonics −8.3 dB above a −20.1 dB threshold | | | — |

The display-only parameters (2, 19–26, 29–43) are never stored in a preset; their reset and initial values differ for
some of them (PDR release 56.23 vs 750, side-chain filter 120 vs 102 Hz), so the Analog mode or the release type
probably sets them internally. Phase 3 measures what each mode does rather than trusting these numbers. The GUIXML
places designer controls for all of them below the visible area (y 550 … 690 of a 643 × 699 developer view), with
three toggles at y 472: "ARC / Manual" (PARAM_MODE), "Electro / Opto" (PARAM_INV_REL), "Warm / Smooth"
(PARAM_NUM_FILTERS).

### GUI geometry (C2, C4)
View **643 × 454 px** including the toolbar (logo "WAVES HYBRID LINE", undo / redo, preset box, previous / next, A→B,
Setup A, Save, menu). A skeuomorphic orange panel with photographic knobs (bitmap sequences), all to be drawn by
WComp itself:
- Analog stepped knob (5 positions) 39, 30, 97 × 107; Output knob 505, 30 (value text 599, 85 on hover).
- Meter select In / GR / Out radio 243, 23, 156 × 33; Limiter toggle 414, 23, 56 × 34 with a limiter LED at 436, 67
  (GUIXML control rectangles; the drawn frames are smaller, see "Drawn appearance").
- **VU meter** 224, 105, 199 × 116: a backlit cream dial (scale 48 … 0 in GR), a red needle, a screw-head pivot,
  chrome screws in the black housing; separate needle images for GR (`PARAM_PEAK`) and In / Out
  (`PARAM_METER_DISPLAY`), 71 steps each.
- Ratio big knob 6, 142, 161 × 143 (scale 1 … 50, angles 40 … 320); Mix big knob 473, 142 (Dry 0 … 100 Wet).
- **Threshold** large knob 238, 249, 169 × 180 (0 … −48, angles 53 … 307).
- Punch 9, 294, Attack 114, 295, Release 431, 294 (97 × 107 each, angles 39 … 321); Release and ReleaseBPM share the
  same knob; Sync radio BPM / HOST / MS 542, 310, 59 × 71; BPM LED display 507, 405 ("BPM OFF", digit bitmaps).
- Value texts appear on hover (`ShowOnHover`), 37 × 13 px below each knob; Alt resets (`OptionReset`); text entry on
  all knobs (`ReciveTextEntery`).

### Drawn appearance (measured on the reference's screenshots, reference/wcomp, git-ignored)
- **Knobs** are filmstrips of one fixed render: across the travel only the pointer moves (frames at 9 values compared
  after rotating them back: the body differs by 2–3 grey levels, the pointer region by 30–80). The pointer is a dark
  fin with a cream line on its ridge, its two facets lit by their angle to a light at 320°. Bodies: small radius 32.4,
  cap ring 20; Ratio / Mix skirt 53.9, ten flutes at 44.4 ± 1.6, cap 32.5; Threshold skirt 73, flutes at 61.8, cap 44.
  The knob centres sit at the GUIXML centres plus about (0.2, 1.0) (Threshold: (320.65, 333.35)). Cast shadows: the
  panel darkened by 0.35–0.42 under a disc offset (5–8, 16–25) px. `tools/fit_hybrid_knobs.py` fits the shading as
  smooth functions of radius and angle (no pixels kept) into `Source/common/WavyHybridKnobData.h`.
- **Scales**: ticks every 14°, small knobs r 33.8 … 40 (1.7 px), Ratio / Mix r 56.2 … 64.3, Threshold 19 ticks
  r 72.2 … 80.4. Labels Arial Black 10.75 px stretched 1.2×; numbers Arial Bold 8.6 px; each placed at its measured
  ink centre. "0 THRESHOLD −48": Arial Black 12.6 px, one glyph per measured angle at r 85 around the knob.
- **VU**: window frame 197.5, 73.5, 250 × 147 (top corners 27); cream dial 204.5, 80.5, 236 × 135; scale arc centre
  (321.3, 228.5), r 105, −55° … 56.5°; numbers Times italic 14.5 px turned along the arc at r 122.5
  (48 −54.25°, 24 −46.6°, 12 −28.05°, 6 0°, 3 22.1°, 0 55.6°). Housing 174.5, 52.5, 295.5 × 224, arched (r 85.5)
  around Threshold.
- **Buttons**: 1 px black frames: In / GR / Out 245, 27, 151 × 21; Limiter 417, 27, 52 × 21; Sync 547, 315, 50 × 61;
  19 px faces. **Display** 506, 404, 90.5 × 34, a segment font (6 × 9 px cells; tempo digits 12 × 20).

### Presets (C2)
26 presets, all RealWorld arrays: Full Reset, PA Default, 21 genre / instrument presets (drums, kick, bass, piano,
guitars, strings, vocal, mastering, parallel, "Broken Radio"), and presets carrying artist names or other products'
names ("Dave Aron - Rock Vocal", "FOHDave Kit Parallel Comp", "160 Style Drum Crunch"). They will be carried over by
their values with Wavy names. The default preset stores the limiter slot as 1 while the manual gives Off as the
default: the slot's polarity is checked in Phase 3.

### Notes for the next phases
- **Noise:** Analog modes 1–3 add noise, which cannot null sample by sample; nulls use Analog Off and 4, and the noise
  is matched in level and spectrum (the dither parameters give a starting point).
- **Punch** is a detector-versus-audio delay: the 64-sample latency is probably the audio delay that lets the
  detector look ahead (Punch then moves the detector back), to be confirmed.
- **Program-dependent release** ("PDR", "Crest", "Electro / Opto"): the release is likely not a plain one-pole; WVox's
  identification tools (static law, attack / release probes, the learned-block procedure) apply.
- **Automatic makeup gain** is a function of threshold and ratio, to be measured on the static curve.
- **Side-chain bus:** pedalboard cannot feed it; Phase 3 needs a JUCE host (MeterCapture or a small tool) to measure it.
- **Tempo sync:** release note values from the host's tempo (AlgXML `IsTempoListener`) or the BPM setting.
- **UI:** the largest yet: procedurally drawn photoreal knobs, a backlit VU meter with needle ballistics, chrome toggle
  buttons, an LED digit display, on the toolbar layout of the Hybrid line.
