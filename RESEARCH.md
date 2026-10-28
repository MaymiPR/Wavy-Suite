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
| Bass Rider | **WBass Rider** | WBassRider |
| Vocal Rider | **WVox Rider** (renamed from WVox Ride 2026-10-28 to match WBass Rider) | WVoxRider |
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
- **Knobs** are filmstrips of a knob turning under a fixed light (upper left, 320°): the flutes turn with the value
  (each capture's flute phase follows its pointer angle; on Ratio / Mix a step of 35° is almost one flute period, 36°,
  so it barely shows), the shading and the cast shadow stay. The pointer is a dark fin with a cream line on its ridge,
  its facets lit by their angle to the light, casting a soft shadow about (1.7, 5) px away, 0.45 deep. Bodies: small
  radius 32.4, cap ring 20; Ratio / Mix skirt 53.9, ten flutes at 44.3 ± 1.7, cap 32.5; Threshold skirt 73, flutes
  at 61.3 ± 2.3, cap 44. The knob centres sit at the GUIXML centres plus about (0.2, 1.0) (Threshold:
  (320.65, 333.35)). Cast shadows: the panel darkened by 0.35–0.42 under a disc offset (5–8, 16–25) px.
  `tools/fit_hybrid_knobs.py` fits the shading jointly over the captures, light terms on the screen's angle and flute
  terms on the knob's, as smooth functions (no pixels kept) into `Source/common/WavyHybridKnobData.h`.
- **Scales**: ticks every 14°, small knobs r 33.8 … 40 (1.7 px), Ratio / Mix r 56.2 … 64.3, Threshold 19 ticks
  r 72.2 … 80.4. Labels Arial Black 10.75 px stretched 1.2×; numbers Arial Bold 8.6 px; each placed at its measured
  ink centre. "0 THRESHOLD −48": Arial Black 12.6 px, one glyph per measured angle at r 85 around the knob.
- **VU**: window frame 197.5, 73.5, 250 × 147 (top corners radius 46, bottom 13); cream dial inset 7 px. Scale arc
  centre (320.7, 220.9), r 99.8, −57.7° … 60.1°, ticks standing outward on it (minor 2.3 px; at 12 and 24, and the
  stop at the 48 end, 6 px); red band r 99.8 … 107 from 0.2° to 60°, dividers at 7.7, 15.6, 24.6, 35.6, 46.5°; dots
  at r 117.2; numbers Times bold italic 14.5 px turned along the arc (48 −58.6° r 116.3, 24 −49.6° 116.4,
  12 −29.9° 116, 6 0.2° 115.5, 3 24.9° 116.8, 0 59° 117.1). The needle turns about (321.3, 222.1) (58.2° at rest):
  (124, 0, 0), 2 px, 114 px long, a cream rivet at 109 px, a soft shadow 5 px below it. Housing 174.5, 52.5 to
  469.5, 280.5, a raised box (light top and left faces, 5–6 px dark right and bottom faces), arched around Threshold
  (r 83.2 about (320.15, 333.35), a 5.6 px dark lip, 6 px fillets into the bottom edge, the panel inside shaded); screws (head 9, recess 11.2) at (198.4, 75.95), (196.6, 261.05), (449.2, 260.95); the limiter lamp at
  (444.5, 75.5), a black dome (r 8.6) in a bezel (r 11.2) bright on its upper left.
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

---

## 5. WDelay (H-Delay)

### Sources
- **D1** Local manual `H-Delay.bundle\Contents\Resources\H-Delay.pdf` (12 pages, H-Delay Hybrid Delay User Guide).
- **D2** Local `H-Delay.bundle\Contents\Resources\ParamXML\1000.xml` (69 parameters; table printed by
  `nulltest/paramxml_table.py`), `GUIXML\1000.xml`, `ToolbarXML\1001.xml`, `AlgXML\1001.xml` (algorithm `HBDL`: a
  tempo, track-info and MIDI listener; components `HBDM` mono, `HBDS` stereo, `HBDX` mono-to-stereo, plus "Lab"
  copies `HLD*`), `ProcessXML\1000.xml` / `1001.xml`, `PageTable\3950.xml`, presets in `Presets\1000.xml` (88).
- **D3** The installed VST3 (`WaveShell1-VST3 17.1`) as seen by a host (`nulltest/probe_hdelay.py`): components
  *H-Delay Mono* (uid 1847911073), *H-Delay Stereo* (1849387175), *H-Delay Mono/Stereo* (1850617249; pedalboard
  cannot load it, one input and two outputs); automatable parameters below; no side-chain bus.
- **D4** Screenshots of the real editor taken with `tools/MeterCapture` (git-ignored `reference/hdelay/`): stereo and
  mono at the defaults, MS sync, a Delay drag, Ping Pong, LoFi, Dual.

### Documented behaviour (D1)
- A "hybrid" delay: slap-back, ping-pong, tempo sync, modulation, filtering, flanging and phasing.
- **Delay** 1 … 3500 ms, or a note value when synced (default 1/8D, Host sync). Changing the delay time works like a
  tape machine changing its motor speed: the echoes already recorded change pitch until the new speed reaches the
  playback head.
- **Sync** Host (note values of the host tempo), BPM (note values of a BPM set in the plug-in, set by tapping),
  MS (milliseconds). The time display shows the nearest value when the mode changes.
- **Tap Pad**: the delay is the interval between the last two clicks (Host: quantised to the nearest note value;
  BPM: the tap sets the BPM).
- **Feedback** 0 … 200 % (default 60): repeats decay below 100 and build up above it.
- **Ping Pong** (stereo components): two delay lines feeding each other, with a start direction (left or right).
  **Dual**: separate left and right times (the display's halves are dragged; the Delay knob follows the selected
  side); Feedback, Modulation and the filters stay common.
- **Phase L / R** invert each delay line (linked on the mono-to-stereo component).
- **Modulation**: Depth 0 … 100 (default 0), Rate 0.1 … 6000 Hz (default 0.1), a triangle LFO on the delay time
  (flanging and chorus up to frequency modulation).
- **LoPass** 20 Hz … 20 kHz or Off, **HiPass** Off or 20 Hz … 20 kHz (both Off by default), **Link** moves both
  together like a band-pass.
- **LoFi**: the reduced sample rate of early digital delays, keeping the delay range.
- **Mix** 0 … 100 (default 100), **Output** ±18 dB, **Analog** Off / 1 … 4 (default 2), an output meter
  0 … −48 dBFS with a clip light.

### Specifications (D3, measured)
- **Latency 0** samples, reported and real: the first echo of an impulse lands on the set time at 44.1, 48 and
  96 kHz (16 537, 18 000, 35 999 samples for 375 ms).
- The echo is not a single sample: at 48 kHz an impulse comes back as a symmetric kernel over ±14 samples
  (centre 0.666, side taps −0.020 / +0.021, −0.010, +0.007 …) whose sum is **2/3** (−3.5 dB) with Feedback 0 and
  Mix 100; at 96 kHz it is twice as wide (centre 0.333). So the wet path has a fixed-frequency low-pass and a gain of
  2/3, and it is probably interpolated. `PARAM_INTERPOLATION_LP_FREQ` (6280 Hz, display only) is a candidate.
- Mix 50: the dry signal at 0.707, so probably an equal-power mix law.
- **Delay changes glide**: in a running instance, changing Delay from 375 to 100 ms put the next echo at 148 ms, and
  100 → 50 ms put it at 52 ms. The read point moves at a limited speed (the documented tape behaviour). Loading a
  state with the new time jumps at once.
- **Harness pitfall**: parameters set through pedalboard before the instance's first block are shown, but the
  processing keeps its previous values (the echo stayed at 375 ms while Delay read 100 ms). Phase 3 sets settings
  through the state's preset array (`nulltest/wavesstate.py`), or renders a first block before changing anything.
  Likewise, values set from outside do not redraw the reference's editor; screenshots of other states are taken by
  clicking its buttons (MeterCapture `click`).

### Parameters (D2, D3), in preset-array order
The preset array holds the first 45 parameters (0–44); the right-channel copies (47–54) and the Dual / display
switches (55–68) are not stored in presets. Parameters 47–54 take their scale from a `Parent` (the left one).

| # | ID | Label | Range | Default | In VST3 |
|---|---|---|---|---|---|
| 0 | PARAM_BYPASS_PEDAL | — | toggle (display only) | 0 | bypass |
| 1 | PARAM_SYNC_ON | Sync | BPM / Host / ms (0 / 1 / 2) | Host | sync |
| 2 | PARAM_BPM | BPM | 30 … 300, 1 | 120 | bpm |
| 3 | PARAM_DELAY_TIME_MS | Delay | 1 … 3500 ms, `DelayScale` (Var 2.8547), 1 ms (1001 host steps, 10 ms steps at the top) | 365.85 (reset); a host sees 375 | delay_sec |
| 4 | PARAM_DELAY_TIME_BPM | Delay | 20 note values: 1/64T 1/64 1/64D 1/32T 1/32 1/32D 1/16T 1/16 1/16D 1/8T 1/8 1/8D 1/4T 1/4 1/4D 1/2T 1/2 1/2D 1B 2B | 11 (1/8D) | delay_bpm |
| 5 | PARAM_TAP_PAD | Tap Pad | toggle (display only) | 0 | — |
| 6 | PARAM_MIX | Mix | 0 … 100, 1 (ignored on preset load) | 100 | mix |
| 7 | PARAM_FEEDBACK | Feedback | 0 … 200, 1 | 60 | feedback |
| 8, 9 | PARAM_INV_L / R | Phase L / R | + / − | + | phase_l, phase_r |
| 10 | PARAM_PING_PONG | — | Stereo / PingPong | Stereo | pingpong |
| 11 | PARAM_NARROW_FB | — | toggle (display only) | 0 | — |
| 12 | PARAM_DIRECT_GAIN | — | −42 … 0 dB (display only) | 0 | — |
| 13 | PARAM_LIMITER_RELEASE | Release | 1 … 1000 ms, log (display only): an output limiter's release | 10 | — |
| 14 | PARAM_OUT_GAIN | Output | ±18 dB, 0.1 | 0 | output |
| 15 | PARAM_FB_HI_FREQ | LoPass | 19 … 20 138.8 Hz, `FreqScale`; Off at the top (stored 20 100 or 20 139) | Off | lopass |
| 16 | PARAM_FB_HI_Q | — | 0.5 … 2 (display only) | 0.95 | — |
| 17 | PARAM_FB_LO_FREQ | HiPass | 19 … 20 138.8 Hz, `FreqScale`; Off at the bottom (19) | Off | hipass |
| 18 | PARAM_FB_LO_Q | — | 0.5 … 2 (display only) | 0.8 | — |
| 19 | PARAM_DEPTH | Depth | 0 … 100 %, stored with fractions (0.69 shows as 1 %) | 0 | depth |
| 20 | PARAM_RATE | Rate | 0.1 … 6000 Hz, `RateScale` (881 host steps) | 0.1 | rate |
| 21 | PARAM_INTERPOLATION_LO_FI | LoFi | HiFi / LoFi | HiFi | lofi |
| 22 | PARAM_INTERPOLATION_LP_FREQ | — | 1000 … 24 000 Hz, log (display only) | 6280 | — |
| 23 | PARAM_ANALOG | Analog | Off / Mod1 … Mod4 (ParamXML's initial value 0; the default preset and a fresh instance: Mod2) | Mod2 | analog |
| 24–28 | DITHER_NORM / GAIN1 / FREQ1 / GAIN2 / FREQ2 | analog noise −98 dB, shaped (+30 dB at 200 Hz, −12.1 dB at 11.15 kHz), as H-Comp | | | — |
| 29–35 | THD, THD_SUB / LOW / HF FREQ / GAIN | analog harmonics −29.5 dB, shaped (sub 125 Hz +22.3, low 250 Hz 0, HF 8.88 kHz +7.1 dB), as H-Comp | | | — |
| 36–38 | ATHD_GAIN / THRESH / NORM | even harmonics −8.3 dB above −20.1 dB, as H-Comp | | | — |
| 39–43 | IN / OUT_VU L / R, DISPLAY_VU | meters (−30 … 0 dB, clip at 0.999999) | | | — |
| 44 | PARAM_CLIP_LIGHT | Clip | the clip light | | — |
| 45 | PARAM_PING_PONG_START_DIRECTION | ppg start | Left / Right | Left | ping_pong_start_direction |
| 46 | PARAM_TAP_MODE | TAP mode | toggle | 0 | — |
| 47–54 | *_RIGHT | DlyMsR, DlyBPMRt, FeedbackR, HiFreqR, LoFreqR, DepthR, RateR, TapR | as the left ones | | delay_sec_r, delay_bpm_r |
| 55 | PARAM_DUAL | Link | Dual off / on (1 = linked) | 1 | — |
| 56 | PARAM_BALANCED_FB | BalancedFB | On / Off | Off | — |
| 57–68 | DUAL_LABEL, DISPLAY_R_WHEN_PINGPONG, BPM_DUAL, DELAY_TIME_*_MAIN / LEFT / DISPLAY, REFRESH_DISPLAY | display switches | | | — |

The analog block (24–38) carries H-Comp's numbers exactly, so H-Delay probably uses the same Analog stage; WComp's
measured Analog model is the starting point. The display-only "narrow feedback", "direct gain", "balanced feedback"
and the filter Qs (0.95 low-pass, 0.8 high-pass) are fixed design values to be checked in Phase 3. The VST3 has no
right-channel Feedback, filter or modulation, although ParamXML lists them, so Dual only splits the times.

### GUI geometry (D2, D4)
View **749 × 292 px** below the Hybrid toolbar (the same toolbar as H-Comp: logo, undo / redo, preset box,
previous / next, A→B, Setup A, Save, menu); the same orange panel, knob, button and display family as H-Comp.
- **Tap** pad 22, 58, 99 × 101 (a black rubber pad with the Waves mark, "TAP" below); **LoFi** button 32, 185.
- **Delay** big knob 130, 27, 152 × 154 (scale "1/64T … 2Bar" when synced, "1 … 3500" in MS); while it is dragged a
  value box appears in its centre. Delay BPM and Delay ms share the knob (and the R copies for Dual).
- Button row (stereo) 290 … 432, y 38: **ØL**, **PING PONG**, **DUAL**, **ØR**; mono shows a single **Ø**.
- **Display** 292, 57, 137 × 74 (amber segment digits): the note value and the BPM ("1/8 D 120 BPM") in Host / BPM,
  four digits ("0375") in MS; with Ping Pong the time and an "L" / "R" start-direction box; with Dual "L" and "R"
  halves with a value each. **Sync** radio BPM / HOST / MS 288, 142, 148 × 33.
- **Feedback** big knob 442, 27 (0 … 200); clip light 613, 39; **output meter** (two bars on stereo, 15 × 177 each
  at 608 and 623, y 77) on a cream scale 0, 3, 6, 12, 24, 36, 48.
- Right column: **Dry/Wet** 663, 26; **Output** 663, 106; **Analog** stepped knob 663, 191 (73 × 76 each).
- Bottom row (96 × 106 knobs): **Depth** 134, 175 and **Rate** 239, 175 under a "MODULATION" bracket; **HiPass**
  360, 175, **Link** 454, 199 (a small knurled wheel moving both filters), **LoPass** 495, 175 under "FILTERS".
- The plug-in's name in large type at the lower left. Value texts are shown while a knob is dragged; Alt resets
  (`OptionReset`); text entry on the knobs and the displays (`ReciveTextEntery`). A 40 px drag up on Delay in MS
  moved it 375 → 415 ms.

### Presets (D2)
88 presets, all RealWorld arrays of 45 values (all taken by the reference: `nulltest/read_ref_presets_hd.py` →
`build/ref_presets_HDelay.json`): Full Reset, PA Default, 22 "Classics", 11 "Modulation", 11 "Ambience",
22 "Special FX" (several "Watch Out": Feedback over 100), and 20 signed by engineers whose names are in the preset
names. 34 are for the stereo components only. They use Host or ms sync (none BPM), Ping Pong on 39, LoFi on 30, every
Analog mode, Mix mostly 100. They will be carried over by their values with Wavy names (no person's or product's
name).

### Notes for the next phases
- **Delay line**: interpolated reads (2/3 wet gain, a fixed-frequency kernel), a glide on time changes (the "tape"
  law: how fast the read point moves, and whether it is a slew or a one-pole), host tempo and BPM sync with the
  20 note values, the tap pad.
- **Feedback** above 100 %: what keeps it bounded (`PARAM_LIMITER_RELEASE` 10 ms suggests a limiter in the loop or
  at the output; the clip light).
- **Filters** in the feedback loop (the IDs say FB): their slopes and Qs (0.95 / 0.8), Off positions, and Link.
- **Modulation**: a triangle LFO up to 6 kHz on the delay time; its depth in samples or ms, and whether left and
  right are in phase.
- **LoFi**: a reduced-rate delay line (`PARAM_INTERPOLATION_LP_FREQ` 6280 Hz is the likely anti-alias filter).
- **Ping Pong / Dual / phase** routing on the stereo and mono-to-stereo components; the M>S component needs a JUCE
  host for measurements (pedalboard cannot load it).
- **Analog** modes: compare with WComp's measured stage; modes 1–3 add noise, so nulls use Analog Off and Mod4.
- **Nulls**: with Depth 0 and Analog Off or Mod4 a sample-exact null should be possible. With modulation the null
  depends on matching the LFO's phase from the start; with modes 1–3 only levels and spectra can be matched.
- **UI**: the H-Comp kit (`WavyHybridUI`) covers the panel, knobs, buttons and display; new are the tap pad, the
  vertical bar meter with its scale, the Link wheel, the delay display's layouts (note / BPM, ms, Ping Pong, Dual)
  and the stepped note scale on the Delay knob.

## 6. WBass Rider (Bass Rider)

### Sources
- **B1** Local manual `Bass Rider.bundle\Contents\Resources\Bass Rider.pdf` (10 pages, Bass Rider User Guide; the copy
  in `Plug-Ins V17\Documents` is identical).
- **B2** Local `ParamXML\1000.xml` (47 parameters; `nulltest/paramxml_table.py`), `GUIXML\1000.xml`,
  `ToolbarXML\1001.xml`, `AlgXML\1001.xml` (algorithm `BLVR`: components `BSLS` / `BSLM` Bass Rider Stereo / Mono,
  `BSVS` / `BSVM` Bass Rider Live Stereo / Mono), `ProcessXML\1000.xml` / `1001.xml` (no side-chain inputs, up to
  192 kHz), presets in `Presets\1000.xml` (17), skins in `Webp.zip` (2x images, git-ignored `reference/wbassrider/`).
- **B3** The installed VST3 (`WaveShell1-VST3 17.1`) as seen by a host (`nulltest/probe_bassrider.py`): components
  *Bass Rider Mono* (uid −1142096219), *Bass Rider Stereo* (−1140620123), *Bass Rider Live Mono* (−512295259),
  *Bass Rider Live Stereo* (−510819163); no side-chain bus.
- **B4** Frames of the real editor read back from its OpenGL drawing (`tools/MeterCapture gl=1`, 320 × 560,
  git-ignored `reference/wbassrider/`).

### Documented behaviour (B1)
- Rides bass levels automatically, **note by note**: it detects each note's attack, sustain and release and sets a
  gain per note towards a target, leaving the note's own envelope alone (unlike a compressor).
- **Target** 0 … −42 dBFS (default −14), a slider over the **Target Energy Meter** (the input's energy, 0 … −42 dBFS);
  it also calibrates the Rider fader's 0. Meant to sit near the input's average level.
- **Note detection**: **Sensitivity** 0 … 100 (default 50; higher detects more notes), **Response** Slow / Fast
  (default Slow; when the detector resets for the next note: Slow for long releases, Fast for short ones).
- **Ignore**: **Spill** −50 … −3 dBFS (default −40; signal below it is not detected as notes), **Artifacts** 0 … 100
  (default 50; tolerance to buzz, crackles, squeaks and modulation within a note that would trigger detection).
- **Range**: **Max** −11 … +12 dB (default +12), **Min** −12 … +11 dB (default −12), the **Effective Range** shown
  between them (1 … 24 dB); the **Idle arrow** (−12 … +12 dB, default 0) is the Rider fader's value when no bass plays.
- **Rider fader** −12 … +12 dB: the automatic gain; it can be grabbed for manual gain, and riding resumes on release.
- **Output** −12 … +12 dB (default 0) after the riding, with a clip LED.
- **Components**: Bass Rider (Mono, Stereo), latency 42 ms, "round, transparent"; Bass Rider Live (Mono, Stereo),
  latency 5 ms, "adds a little punch to note start events".

### Specifications (B3, measured)
- **Latency** reported and compensated: 42.00 ms (1852 samples at 44.1 kHz, 2016 at 48, 4032 at 96) and Live 5.00 ms
  (221 / 240 / 480). With it removed, the output lines up with the input (cross-correlation lag 0).
- **Static level at the defaults**: an 80 Hz sine settles at −11.53 dBFS rms out for any input from −20 to −3 dBFS
  rms (gain +8.47 … −8.53 dB), and at +12 dB gain (the Max range) for −30 and −40. So Target −14 aims at an energy
  measure that reads an 80 Hz sine 2.47 dB below its rms (the weighting filters, or a peak / energy definition):
  Phase 3.
- **VST3 parameters** (what a host sees, 10 plus bypass): spill (471 steps of 0.1 dB), range_max, range_min, output
  (0.1 dB), sensitivity (1001 steps), target (421 steps of 0.1 dB), idle (shown in dB), response (Slow / Fast),
  artifacts (1001 steps). The Rider fader is not a host parameter (no ride automation out).

### Parameters (B2, B3), in preset-array order
The preset array holds the 46 stored values (0–45; 46 is display-only). Target is `*` (kept) in all but the first
two presets.

| # | ID | Label / control | Range | Default | In VST3 |
|---|---|---|---|---|---|
| 0–3 | IN / OUT_VU L / R | meters (display only) | −32 … 0 dB | | — |
| 4 | PARAM_OUT_GAIN | Output | ±12 dB, 0.1 | 0 | output |
| 5 | PARAM_OUT_GAIN_TC | OutAtt | 0.01 … 100 ms, log (presets 5.01) | 5 | — |
| 6 | PARAM_DIRECT_DELAY | DirectDelay | 0 … 100 ms (42: the look-ahead, the latency) | 42 | — |
| 7 | PARAM_RIDE | Rider fader | ±12 dB, 0.01 (display only) | 0 | — |
| 8, 9 | PARAM_RIDE_GAIN_MIN / MAX | Range Min / Max | −12 … +11 / −11 … +12 dB, 0.1 | −12 / +12 | range_min, range_max |
| 10 | PARAM_RIDE_RANGE | Effective range | 1 … 24 (display only) | | — |
| 11 | PARAM_USER_RIDE_TOUCH | the fader grabbed | toggle (display only) | 0 | — |
| 12 | PARAM_RIDE_TARGET0 | Target | −42 … 0 dB, 0.1 | −14 | target |
| 13, 14 | PARAM_ATTACK0 / RELEASE0 | PBAtt / PBRel | 0.1 … 1000 / 1 … 10 000 ms, log | 2 / 100 | — |
| 15, 16 | PARAM_WEIGHT_HPF_FREQ / GAIN | detector weighting high-pass | 1 … 2000 Hz / −140 … 0 dB (presets 29.9 Hz, −30 dB) | 30 / −30 | — |
| 17, 18 | PARAM_WEIGHT_HSH_FREQ / GAIN | detector weighting high shelf | 20 … 20 000 Hz / −140 … +24 dB (presets 5094 Hz, +0.1 dB) | 5093 / 0 | — |
| 19, 20 | PARAM_ENERGY_METER / MUSIC_METER | Energy (the Target meter) / Music | −42 / −48 … 0 dB (display only) | | — |
| 21 | PARAM_BAD_LPF_FREQ | BADLpf: the note detector's low-pass | 20 … 20 000 Hz (presets 998) | 1000 | — |
| 22 | PARAM_BAD_ILLEGAL_HPF_FREQ | IllegalHpf | 20 … 20 000 Hz (presets 2005) | 2000 | — |
| 23 | PARAM_MAX_POW2_DELAY | MaxDly | 0 … 4 | 2 | — |
| 24 | PARAM_BAD_NOTE_OFF_GATE | NoteOffGate | −100 … 0 dB | −38 | — |
| 25, 26 | PARAM_BAD_NOTE_ATTACK / RELEASE | NoteAtt / NoteRel | 0.001 … 100 / 1 … 10 000 ms | 1 / 1 | — |
| 27, 28 | PARAM_BAD_SLOW_ATTACK / RELEASE | SlowAtt / SlowRel | (presets 29.854 / 1000 ms) | 30 / 1000 | — |
| 29, 30 | PARAM_BAD_REF_ATTACK / RELEASE | RefAtt / RefRel | (presets 1 / 5011.9 ms) | 1 / 5000 | — |
| 31 | PARAM_BAD_NOTE_ON_THRESH | Sensitivity | 0 … 100, 0.01 | 50 | sensitivity |
| 32 | PARAM_BAD_SUSTAIN_THRESH | SustainThresh | −140 … 0 dB | −1 | — |
| 33 | PARAM_BAD_NOTE_OFF_THRESH | Response | Slow / Fast (0 / 1) | Slow | response |
| 34 | PARAM_PARK_GAIN | Idle arrow | 0.02 … 0.98: the fraction of the range (0.5 → 0 dB at ±12) | 0.5 | idle |
| 35 | PARAM_PARK_DB | the Idle value in dB (display only) | ±12 | | — |
| 36, 37 | PARAM_PARK_ATTACK / RELEASE | ParkAtt / ParkRel: the glide to the Idle value | 200 … 20 000 ms | 271 | — |
| 38 | PARAM_PITCH_INDEX | (display only) | 0 … 4 | | — |
| 39–43 | BAD_ATTACK / SUSTAIN / IDLE, BAD_RATIO_MIN / MAX | the detector's state (display only) | | | — |
| 44 | PARAM_BAD_NOTE_ON_GATE | Artifacts | −20 … −3 dB, shown 0 … 100 linearly (−11.5 → 50) | −11.5 | artifacts |
| 45 | PARAM_BAD_GATE | Spill | −50 … −3 dB (typed positive means negative) | −40 | spill |
| 46 | PARAM_RIDE_GAIN | Attenuation (display only) | | | — |

The hidden values read like a design sheet for Phase 3: a note detector ("BAD") on a low-passed (about 1 kHz)
signal with note, slow and reference envelopes (attack / release pairs), note-on and note-off gates and a sustain
threshold; a weighted energy (high-pass 30 Hz at −30 dB, high shelf 5.1 kHz) for the level; a gain smoother (attack
2 ms, release 100 ms); an idle "park" glide (271 ms); the 42 ms direct delay as look-ahead. All presets share every
hidden value.

### GUI geometry (B2, B4)
View **320 × 560 px** including the two-row Renaissance toolbar (RVox's ToolbarXML, its preset row 316 px wide
instead of 367). The panel (B4, `reference/wbassrider/shot_default_running.png`):
- Dark top panel: **Target** slider 59, 45, 210 × 22 over the **Energy** meter 71, 48, 184 × 15; "Detect" group:
  **Sensitivity** knob 24, 90, 44 × 44 (a dashed ring scale), **Response** Slow / Fast toggle 104, 106, 37 × 17;
  "Ignore" group: **Spill** 178, 90 and **Artifacts** 253, 90 (44 × 44, 71-frame knob strips).
- Cream lower panel: **Range** column (Max 36, 198 and Min 36, 225 thumbs on 45 × 228 tracks; the **Idle** arrow
  marker 35, 222, 12 × 211; the **Range** value box 41, 466), the **Rider** fader 128, 189, 58 × 279 on a ±12 dB
  scale (6 dB marks, 1 dB ticks) with its value box, the **Output** fader 235, 212, 43 × 234 with its value box and
  a **clip LED** 279, 183. Faders have shadows; the Range column shades the band between Min and Max, and the Idle
  value is drawn as a line across to the Rider scale.
- Bottom: a bass clef and the product name in large type on dark leather.
- Text entry and Alt-reset on the knobs and faders (`ReciveTextEntery`, `OptionReset`); the fader text follows the
  thumb.

### Presets (B2)
17 presets, all RealWorld arrays (Full Reset, PA Default, and 15 for bass styles: slow / fast, fretless, upright,
picked, slap, fret noise, stage, a suppressor with Max 0). They change only Output, Range Min / Max, Sensitivity,
Response, Idle, Artifacts and Spill (and Target in the first two). They will be carried over by their values with
Wavy names, and their null test will load the reference's own preset states (as WDelay's, `RefRender state=`).

### Notes for the next phases
- **UI (Phase 2)**: the Renaissance toolbar narrower (316); new parts: the fader family (Rider, Output, Range
  Min / Max with the shaded band and the Idle arrow), the Target slider over the Energy meter, the dashed-ring
  Sensitivity knob and the Response toggle; the Rider fader moves by itself and can be grabbed. The Live components
  differ only by a badge (65 × 30 at 246, 520).
- **Components**: Mono / Stereo by the bus layout, Studio / Live as a switch (Mode), which changes the latency
  (42 / 5 ms).
- **Measurement (Phase 3)**: the gain is observable sample by sample (output / input with the latency removed, a
  pilot tone for the quiet parts); the energy measure and its weighting; the static law (Target, Min / Max, Idle);
  the gain smoother; note-on / note-off detection against Sensitivity, Response, Spill and Artifacts on synthetic
  notes (attacks, decays, legato, ghost notes, buzz); the look-ahead (42 / 5 ms) and what Live changes; stereo
  linking.
- **Nulls**: steady material should null once the static law and smoother are exact; the note detector is the hard
  part (a learned block is the fallback, as in WComp).

## 7. WVox Rider (Vocal Rider)

### Sources
- **V1** Local manual `Plug-Ins V17\Documents\Vocal Rider.pdf` (11 pages, Vocal Rider User Guide; a copy is in the
  bundle's `Resources`).
- **V2** Local `ParamXML\1000.xml` (46 parameters; `nulltest/paramxml_table.py`), `GUIXML\1000.xml`, `ToolbarXML`,
  `AlgXML\1001.xml`, `ProcessXML\1000.xml` / `1001.xml` (components `RDRM` / `RDRS` Vocal Rider Mono / Stereo, `LRDM`
  / `LRDS` Vocal Rider Live Mono / Stereo; `RDLM` / `RDLS` name a 600 × 818 view with text boxes for every hidden
  value, a lab view the shell does not offer), presets in `Presets\1000.xml` (13), skins in `Webp.zip` (380 2x
  images) and `PNG_.zip`, `Skin\1001` (skin names: Side-Chain, D-Show). Extracted to git-ignored
  `reference/wvoxrider/skin/`.
- **V3** The installed VST3 (`WaveShell1-VST3 17.1`) as a host sees it (`nulltest/probe_voxrider.py`, and
  `tools/RefRender`, which now prints the input buses and enables a mono side-chain on mono components): *Vocal Rider
  Mono* (uid −510562340), *Vocal Rider Stereo* (−509086244), *Vocal Rider Live Mono* (989902242), *Vocal Rider Live
  Stereo* (991378338).
- **V4** Frames of the real editor read back from its OpenGL drawing (`tools/MeterCapture gl=1 in=…`, 320 × 598,
  git-ignored `reference/wvoxrider/shots/`), with voice-like syllables (`reference/wvoxrider/syllables.f32`).

### Documented behaviour (V1)
- Rides vocal levels automatically in real time, like a hand on the fader, with **zero latency and no pre-scan**;
  it does not colour the sound (gain only). Meant as the last plug-in on a vocal track or group.
- Splits the mix into **vocals** (the track) and **music** (everything else, fed post-fader to the **side-chain**).
- **Target** 0 … −42 dBFS (default −21): the vocal level the riding aims at; it also calibrates the Rider fader's 0.
  A transparent screen over the vocal level meter.
- **Vocal Sensitivity** −12 … +12 dB (default 0): the vocal detector's threshold, which tells vocal content from noise
  floor, spill and ambience and finds the starts and ends of words. **Vocal activity display**: Ride / Idle LEDs.
- **Attack** Fast / Slow (default Slow): the detection timing and the fader's riding speed.
- **Music Sensitivity** −12 … +12 dB (default 0; the host shows an Off below −12): how much the riding follows
  changes of the instrumental level (needs the side-chain). Its **LED** goes from soft to bright green as the music
  affects the riding more.
- **Spill** −12 … −48 dBFS (default −48), Live components only: signal below it is ambient stage noise, not vocal.
- **Range Min** −12 … +11 dB (default −6), **Range Max** −11 … +12 dB (default +6), the **Effective Range** between
  them (1 … 24 dB); the **Idle arrow** (−12 … +12, default 0) is the Rider fader's value when no vocal is detected.
- **Rider fader** −12 … +12 dB: the automatic gain; grabbed with the mouse it is a manual gain, and riding resumes on
  release (a control surface's Manual button does the same).
- **Automation** Off / Write / Read: Off rides from the internal engine; Write sends the fader's movements to the
  host's automation lane as if written by hand; Read plays the lane back and ignores the engine.
- **Output** −12 … +12 dB (default 0) after the riding, with a clip LED.

### Specifications (V3, V4, measured)
- **Latency 0** on all four components (reported and real: cross-correlation lag 0). Studio and Live differ in the
  Spill control (and a LIVE badge), not in latency.
- **Buses**: every component has a side-chain bus (*Mono SideChain* 1 channel, *Stereo SideChain* 2) besides its
  main input, though `ProcessXML` lists no side-chain inputs. A first offline check (a steady −21 dBFS buzz, music
  a buzz stepping from −40 to −6 dBFS on the side-chain, Music Sensitivity −12 / 0 / +12) showed **no effect of the
  side-chain** on the gain: Phase 3 checks it on the editor (the Music meter and LED) and with real music.
- **Static level at the defaults** (a 180 Hz buzz, harmonics falling 6 dB / octave, after 4 s): gain +6.00 dB (the
  Max range) at −48 and −36 dBFS rms, +0.69 at −27, −3.48 at −21, −6.00 (the Min range) at −15 and louder. So the
  default Target −21 is not "−21 dBFS rms in, 0 dB gain", and the slope between −27 and −21 is not −1 dB / dB (the
  riding may still be moving, or the law is not a full correction): Phase 3.
- **VST3 parameters** (10 plus bypass; Spill only on Live): target (421 steps of 0.1 dB), vocal_sensitivity (±12,
  0.1), music_sensitivity (Off, −12 … +12), range_max, range_min (0.1 dB), idle (shown in dB, ±11.5 as in WBass
  Rider), **rider_fader** (±12 dB, 0.1: unlike Bass Rider the fader *is* a host parameter, which is how Write sends
  automation), output, attack (Slow / Fast), spill (−48 … −12).

### Parameters (V2), in preset-array order
The presets store 44 values (0–43); 44 (Spill, `DontSaveToPreset`) and 45 (Attenuation, display only) are not in
them. Hidden values are on no control of the shipped view.

| # | ID | Label / control | Range | Initial (presets) |
|---|---|---|---|---|
| 0 | PARAM_RIDE_TARGET | Target | −42 … 0 dB, 0.1 | −21 |
| 1 | PARAM_PLAYBACK_THRESH | Music Sensitivity | −12.1 (Off) … +12 dB, 0.1 | 0 |
| 2, 3 | PARAM_RIDE_GAIN_MIN / MAX | Range Min / Max | −12 … +11 / −11 … +12 dB, 0.1 | −6 / +6 |
| 4, 5 | PARAM_RIDE_GAIN_ATTACK / RELEASE | RideAtt / RideRel: the fader smoother | 0.01 … 100 / 1 … 1000 ms, log | 0.35 (Slow presets 0.55) / 1000 |
| 6 | PARAM_VAD_THRESH | Vocal Sensitivity | ±12 dB, 0.1 | 0 |
| 7, 43 | PARAM_VAD_ATT / PARAM_VAD_REL | VADAtt / VADRel | 0.01 … 1000 ms, log | 0.01 / 0.01 |
| 8, 9 | PARAM_WEIGHT_HPF_FREQ / GAIN | weighting high-pass (shelf?) | 20 … 2000 Hz / −140 … 0 dB | 100 / −18 (−18.1) |
| 10, 11 | PARAM_WEIGHT_LPF_FREQ / GAIN | weighting low-pass (shelf?) | 2 … 20 kHz / −140 … 0 dB | 6500 (6502) / −6 |
| 12–15 | PARAM_VOCALS_BIAS / RMS_ATTACK / RELEASE / HOLD | the vocal level's rms follower | −100 … 0 dB; 5 … 500, 100 … 10 000, 200 … 20 001 ms | −42; 10 (Slow presets 50), 1009, 5000 (5001) |
| 16–19 | PARAM_PLAYBACK_BIAS / RMS_ATTACK / RELEASE / HOLD | the music level's rms follower | as above | −42; 105 (104.9), 2013 (2014), 5000 (5001) |
| 20 | PARAM_OUT_GAIN | Output | ±12 dB, 0.1 | 0 |
| 21 | PARAM_RIDE | Rider fader | ±12 dB, 0.01 | 0 |
| 22 | PARAM_VAD | the vocal activity (Ride / Idle LEDs, display only) | 0 / 1 | |
| 23–25 | PARAM_VOICE_LOUDNESS (_RIDED), PLAYBACK_LOUDNESS | the vocal meter (before / after riding), the Music meter | −42 / −60 / −24 … 0 dB (display only) | |
| 26–29 | PARAM_IN / OUT_VU L / R | meters (display only) | −48 … 0 dB | |
| 30 | PARAM_RIDE_AUTOMATION_STATE | Automation Off / Write / Read | 0 … 2 | Off |
| 31 | PARAM_PARK_ONOFF | Park (the idle glide on) | toggle | 1 |
| 32 | PARAM_PARK_GAIN | Idle arrow (fraction of the range) | 0.02 … 0.98, 0.01 (presets also 0 and 1) | 0.5 |
| 33, 34, 38 | PARAM_PARK_ATTACK / RELEASE / HOLD | the glide to the Idle value; the hold before it | 200 … 20 000 / 1 … 10 000 ms | 1205, 1205; 50 (Slow presets 500) |
| 35, 36 | PARAM_OUT_GAIN_ATTACK / RELEASE | OutAtt / OutRel | 0.01 … 100 / 1 … 1000 ms | 10 (Slow presets 50) / 50 (49.9) |
| 37 | PARAM_USER_RIDE_TOUCH | the fader grabbed (display only) | toggle | 0 |
| 39 | PARAM_RIDE_RANGE | Effective range (display only) | 1 … 24 | |
| 40 | PARAM_OUT_CLIP | clip LED (display only) | toggle | |
| 41 | PARAM_PARK_DB | the Idle value in dB (display only) | ±12 | |
| 42 | PARAM_RESPONCE | Attack Slow / Fast | 0 / 1 | Slow |
| 44 | PARAM_VAD_ABS_THRESH | Spill (Live) | −48 … −12 dB | −48 |
| 45 | PARAM_RIDE_GAIN | Attenuation (display only) | | |

The hidden values sketch the design: a **voice activity detector** (VAD threshold, attack / release) on a weighted
level (a 100 Hz high-pass at −18 dB and a 6.5 kHz low-pass at −6 dB, probably shelves as in Bass Rider's sheet);
**rms followers** for the vocal and for the music (attack, release, hold, a −42 dB bias); a fader smoother (RideAtt
/ RideRel 1000 ms); a **park** glide to the Idle value (1205 ms, hold 50 or 500 ms); an output smoother. The presets
tie four hidden values to Attack: Slow presets have RideAtt 0.55, VocAtt 50, OutAtt 50, ParkHold 500 ms; Fast
presets 0.35, 10, 10, 50 (the ParamXML initial values are the Fast ones while Attack starts Slow). Phase 3 checks
whether switching Attack alone changes them.

### GUI geometry (V2, V4)
View **320 × 598 px** including the two-row toolbar (as Bass Rider's, 316 wide). The panel (V4,
`reference/wvoxrider/shots/default_400.png`, `live_400.png`):
- Dark top panel: **Target** slider 59, 42, 208 × 20 over the **vocal level** meter 71, 47, 184 × 12 (yellow bar);
  the **Ride / Idle** LEDs, the **Fast / Slow** switch below them; a "Sensitivity" group with the **Vocal** and
  **Music** knobs (71-frame strips, 0 at the top, − and + at the ends) and the Music LED (a round lens, right). Live
  adds the small **Spill** knob 71, 111, 36 × 34 and moves Vocal / Music right.
- Cream lower panel as Bass Rider's: **Range** column (Max 28, 204 and Min 28, 234 thumbs on 26 × 233 tracks; the
  **Idle** marker 33, 230, 12 × 212 with a hover text; the Range box 41, 473), the **Rider** fader 114, 197, 72 × 279
  (a wide thumb with red centre lines, ±12 scale on both sides; a "graph" area 65, 230, 84 × 212 shades the band
  between Min and Max and draws the Idle line), a small lamp above it (UserTouch 114, 148), the **Output** fader
  231, 219, 47 × 236 with a clip LED 280, 192, and value boxes below each fader.
- **Automation** OFF / WRITE / READ radio buttons 94, 522, 132 × 26 (yellow, dark red, dark green), a lamp above.
- A music level bar (PlaybackLoudnessBar 299, 251, 21 × 238, 103 frames) is listed in the GUIXML; Phase 2 checks
  where (and whether) it shows.
- Bottom: dark leather with the product name in large type (and a LIVE badge on Live).

### Presets (V2)
13 presets (Full Reset and 12 riding styles: smooth, moderate, fast, low, high, rocky, loud, a VCA, an attenuator, a
hard-limit ride, a de-compression with the full ±12 range). Each stores Target, Music and Vocal Sensitivity, Range
Min / Max, Output, Idle, Attack and the Attack-tied hidden values above; the rest are shared. They will be carried
over by their values with Wavy names, and their null test will load the reference's own preset states.

### Notes for the next phases
- **UI (Phase 2)**: start from WBass Rider's editor (toolbar, Range column, Rider and Output faders, Idle marker,
  value boxes, clip LED, the knob and switch looks) and add the Ride / Idle LEDs, the two Sensitivity knobs, the
  Music LED, the Automation radio buttons, the wider Rider thumb and the Live layout with Spill; film behaviour with
  MeterCapture drag scripts (text entry, Alt-reset, grabbing the Rider, Idle hover).
- **Components**: Mono / Stereo by the bus layout, Studio / Live as the Mode switch (Spill shown only on Live); a
  side-chain bus on both.
- **Automation**: the Rider fader is a host parameter here; Write sets it from the engine (host automation then
  records it), Read takes the host's value; grabbing it is a manual gain.
- **Measurement (Phase 3)**: the gain is observable sample by sample (output / input, no latency; a pilot tone for
  the quiet parts); the vocal level measure and its weighting; the static law (Target, Min / Max, Idle) and the
  smoother (RideAtt / RideRel); the VAD (word starts and ends) against Vocal Sensitivity, Attack and Spill on
  syllables, noise and breaths; the park glide and hold; Music Sensitivity with music on the side-chain; stereo
  linking. Vocal material: synthetic syllables plus real vocal phrases (sample packs on this machine, if any).
- **Nulls**: with zero latency there is no look-ahead, so the riding is causal; the VAD decisions are the likely hard
  part (as Bass Rider's note decisions).
