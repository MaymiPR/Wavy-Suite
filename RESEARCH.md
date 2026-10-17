# Wavy Suite — Research

Phase 1 evidence for each plugin. One section per plugin, written before any code. "Local" sources are files shipped
in the user's own Waves V17 install (read for private analysis only; none of it is copied into this repo).

Sources
- **S1** Local manual `Plug-Ins V17\RVox.bundle\Contents\Resources\RVox.pdf` (7 pages, "Renaissance Vox User Guide").
- **S2** Local `ParamXML\1000.xml` — parameter names, ranges, scale types, defaults.
- **S3** Local `GUIXML\1000.xml` — control rectangles, fonts, colors, view size.
- **S4** Local `ProcessXML\1000.xml`, `AlgXML\1001.xml` — components (mono/stereo), algorithm name `RVOX`.
- **S5** Web search, 2026-10-17 (reviews and shop pages; secondary, not manufacturer): [Joseph Nilo, "Waves RVox explained"](https://josephnilo.com/blog/waves-rvox-explained/), [Toolfarm](https://toolfarm.com/buy/waves_renaissance_vox), [Sweetwater](https://www.sweetwater.com/store/detail/RenVox--waves-renaissance-vox-plug-in/reviews).
- **S6** Official product page, fetched 2026-10-17: [waves.com/plugins/renaissance-vox](https://www.waves.com/plugins/renaissance-vox). Official manual PDF linked from it: [renaissance-vox.pdf](https://assets.wavescdn.com/pdf/plugins/renaissance-vox.pdf) (matches local S1). The earlier guess `waves.com/plugins/rvox` is a 404.

### Product-page facts (S6)
- "Classic compressor/limiter optimized for vocals"; three controls: Gate, Compression, Output Gain.
- Gate = "gentle downward expansion", not a hard gate; compressor = soft-knee curve; limiting/level maximizing run automatically (no limiter control); smart metering.
- Components: mono and stereo only. Skins: light, dark, legacy.
- **Published latency:** 64 samples @ 44.1/48 kHz, 128 @ 88.2/96 kHz, 256 @ 176.4/192 kHz (67 on TDM 48 kHz). Max sample rate 192 kHz.

## Naming (Wavy identity, recognisable, own branding)

| Waves original | Wavy plugin | Product folder |
|---|---|---|
| Renaissance Vox (RVox) | **WVox** | Wavy WVox |
| Renaissance Bass (RBass) | **WSub** | Wavy WSub |
| MaxxBass | **WMaxx** | Wavy WMaxx |
| H-Comp | **WComp** | Wavy WComp |
| H-Delay | **WDelay** | Wavy WDelay |
| Bass Rider | **WBass Ride** | Wavy WBassRide |
| Vocal Rider | **WVox Ride** | Wavy WVoxRide |
| CLA Vocals | **WVox Chain** | Wavy WVoxChain |
| Vocal Bender | **WVox Bend** | Wavy WVoxBend |

Mono / stereo / mono→stereo / live components are one plugin with a **Mode** switch.

---

## 1. WVox (Renaissance Vox)

### Documented behaviour (S1)
- Dynamics processor for vocals: "compression, gating, and limiting ... into just three controls" (p.2).
- Components: mono and stereo, otherwise identical (p.2). Skins: Light, Dark, Legacy; same controls, values unchanged on skin switch (p.2).
- **Compression** fader: one control combining threshold and ratio. Lowering it increases gain reduction *and* output level; output is held by a peak limiter. Range printed as "-36 dB to -36 dB" (typo in manual; S2 says -36…0). Double-click enters a value (p.5).
- **Gain**: the *output ceiling* of the limiter, not a plain fader. At high gain "makeup gain maintains a relatively even output level when the Compression setting is changed" (p.5). Compression is **post-gate** (p.5).
- **Gate**: downward expander, "very gentle", **soft knee** ("some gain reduction may be perceived even if the Gate threshold is set very low"), range −∞…0 dB; superimposed on the input energy meter (p.6).
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
