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
| MaxxBass | **WMaxxBass** | WMaxxBass |
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
