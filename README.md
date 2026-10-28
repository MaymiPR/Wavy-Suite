<p align="center">
  <img src="docs/branding/wavy-banner.svg" alt="Wavy Suite" width="100%">
</p>

# Wavy Suite

Open-source effect plugins (**VST3**, **CLAP**, **AU**) for Windows, macOS and Linux, built with JUCE.
Each Wavy plugin is an independent, from-scratch recreation of a familiar mixing tool: modelled from its public
manual, then measured against the real thing and null-tested, with the numbers published as they are.
Mono / stereo variants live in one plugin behind a **Mode** switch.

> Wavy Suite is an independent project. It is not affiliated with, endorsed by or sponsored by Waves Audio.
> Other companies' product names appear only to describe what a Wavy plugin is modelled on and belong to their owners.

## Download

One installer per operating system installs every plugin in the suite — get it from
[Releases](https://github.com/MaymiPR/WavySuite/releases):

| System | File | Installs to |
|---|---|---|
| Windows 10 / 11 (64-bit) | `WavySuite-<version>-Windows-Setup.exe` | `Common Files\VST3\Wavy Suite`, `Common Files\CLAP\Wavy Suite` |
| macOS 10.13+ (Apple Silicon and Intel) | `WavySuite-<version>-macOS.pkg` | `/Library/Audio/Plug-Ins/{Components, VST3/Wavy Suite, CLAP/Wavy Suite}` |
| Linux x86-64 (glibc 2.35+) | `WavySuite-<version>-Linux.tar.gz` | `~/.vst3/Wavy Suite`, `~/.clap/Wavy Suite` (run `install-linux.sh`) |

The installers are not code-signed or notarized; see [packaging/](packaging) for the first-run steps on each system.

## Plugins

| Wavy plugin | Modelled on | Status |
|---|---|---|
| **WVox** | vocal compressor / gate / limiter (three-control) | released: steady material nulls, dynamic material is close (see below) |
| **WBass** | psychoacoustic bass enhancer (harmonic generator) | ready for the next release: close, not a null (median −36.8 dB, see below) |
| **WBassX** | psychoacoustic bass enhancer with harmonics compressor, filters and monitor modes | ready for the next release: close, not a null (median −48.5 dB, see below) |
| **WComp** | hybrid compressor with punch, tempo-synced release, analog modes and limiter | ready for the next release: close, not a null on every material (median −47.0 dB, see below) |
| **WDelay** | hybrid delay with tape-style time changes, ping-pong, modulation, filters, LoFi and analog modes | ready for the next release: nulls on the delay path (−84 … −104 dB), close with modulation and Analog (see below) |
| **WBass Rider** | note-by-note bass level rider with target, note detection, range and idle controls | ready for the next release: close, not a null on every bass part (bass line −30 dB, real bass −20 … −25 dB, see below) |
| **WVox Rider** | real-time vocal level rider with a music side-chain, range and idle controls | ready for the next release: nulls on vocals (−47 … −74 dB, see below); the music side-chain not yet |
| WVox Chain, WVox Bend | | planned, one at a time |

### WVox Rider null test (honest numbers)

Residual of WVox Rider against the reference, lower is better: **−74 dB** on level steps and **−47 … −66 dB** on vocal
parts held out from the identification, at Studio and Live, Slow and Fast, other Targets, ranges and Idle values,
Stereo, and at 44.1 and 96 kHz. Attack Fast is the weakest (about −50 dB), at word onsets.

- How: the reference's riding was identified from probes and a corpus of 36 vocal parts (FL Studio's own vocal
  phrases). It rides only on the samples its vocal detector passes, which is why it rises slowly on a real voice;
  every constant is the reference's own hidden value, none is fitted. Details:
  [docs/NULL_RESULTS_WVOXRIDER.md](docs/NULL_RESULTS_WVOXRIDER.md),
  [MEASUREMENTS.md](MEASUREMENTS.md#wvox-rider-phase-4-the-riding-identified-2026-10-28).
- The reference's own 13 presets, loaded into it, against WVox Rider's (Wavy names): **median −60.9 dB**, worst −37.9
  ([docs/NULL_RESULTS_WVOXRIDER_PRESETS.md](docs/NULL_RESULTS_WVOXRIDER_PRESETS.md)).
- Not covered yet: the **Music side-chain** (Music Sensitivity). The reference uses the music only when its host
  reports the side-chain active, which no offline host here does; WVox Rider rides without it for now.

### WBass Rider null test (honest numbers)

Residual of WBass Rider against the reference, lower is better. The level read, the gain law, the 42 ms look-ahead,
the smoothing and the hold null at **−41 … −49 dB** on steady tones and level steps; a bass line at **−30 dB** at
nearly every Sensitivity × Response setting (−25 … −28 dB at Sensitivity 75 / 100), at 44.1 and 96 kHz alike.

- Real bass parts (FL Studio's own bass samples played as bass lines) held out from the fit: **median −20 … −25 dB**
  per setting, the worst parts −10 … −17 dB. Nearly all of that is a note ridden afresh by one side and not the
  other, after which the two hold 1 … 4 dB apart until the next note; in a mix that is one note slightly louder or
  quieter than with the reference, not an artefact. It is weakest at Response Fast and Sensitivity 100.
- The reference's own 17 presets, set on both: **median −24.8 dB**; Live components −26 dB, Stereo −27 dB.
- How: the reference's note detector (a windowed note level by pitch band, slow and long envelopes, follow and hold,
  release and re-attack rules, the Idle glide) was identified from about 1,200 probe signals and its riding on a
  corpus of 36 bass parts; its constants are fitted, and some rules (which notes are ridden afresh at Fast) are
  matched to what the probes show rather than known. Details:
  [docs/NULL_RESULTS_WBASSRIDER.md](docs/NULL_RESULTS_WBASSRIDER.md),
  [docs/NULL_RESULTS_WBASSRIDER_PRESETS.md](docs/NULL_RESULTS_WBASSRIDER_PRESETS.md),
  [MEASUREMENTS.md](MEASUREMENTS.md#wbass-rider-phase-4-the-riding-model-identified-2026-10-26).

### WDelay null test (honest numbers)

Residual of WDelay against the reference, lower is better. The delay path nulls: defaults, ms and synced times,
the filters, Ping Pong from either side and Feedback up to 200 % at **−84 … −104 dB**; modulation at **−61 … −81 dB**;
the reference's own 88 factory presets, loaded as it loads them (each on drums, bass and voice, with its own Analog
mode): **median −53.2 dB**.

- Analog modes: mode 4 nulls at −80 … −90 dB; modes 1–3 add random noise as the reference does, which no null can
  remove, and mode 3's model is the weakest part (about −41 dB).
- Limits: the slow tail of a glide (−40 … −80 dB while it settles), LoFi (−70 dB), and Feedback above 100 % with the
  filters on, where the loop self-oscillates chaotically (the preset Fast Siren): such a loop nulls only for its first
  fraction of a second, in any model short of a bit-exact one. At 96 kHz the audible band nulls at −46 … −61 dB.
- How: H-Delay's interpolator (a 32-phase Kaiser sinc), its tape glide, the gain stage in the loop, its float
  triangle LFO, the filters, Ping Pong's routing and every parameter's resolution were identified on the reference
  by nulling; the Analog modes are fitted models (mode 4 is H-Comp's). Details:
  [docs/NULL_RESULTS_WDELAY.md](docs/NULL_RESULTS_WDELAY.md),
  [docs/NULL_RESULTS_WDELAY_PRESETS.md](docs/NULL_RESULTS_WDELAY_PRESETS.md),
  [MEASUREMENTS.md](MEASUREMENTS.md#wdelay-model-phase-4-identification-2026-10-25).

### WComp null test (honest numbers)

Residual of WComp against the reference over 14 signals × 12 settings (the default, fast and slow releases, heavy
ratios, Mix, the output limiter, the four Analog modes), lower is better: **median −47.0 dB**, worst −24.5 dB; the
reference's 26 factory presets (each on voice, drums, bass and pink noise): **median −43.7 dB**, worst −20.8 dB.

- At the default settings: tones and sweeps −84 dB (a null), voice-like −61 dB, bass line −57 dB, white noise −43 dB,
  drums −33 dB, pink noise −32 dB.
- Weakest: low notes under long attacks (30 … 60 ms: −15 … −20 dB on a bass line), a long release on voice (300 ms at
  threshold −24: −22 dB), fast attacks on drums (about −23 dB), and a few presets on drums (Hard Squash −21 dB).
- Analog modes: mode 4 nulls at −82 … −100 dB on loud material; modes 1–3 add random noise as the reference does,
  which no null can remove (mode 2, the default, is limited to about −54 … −69 dB by its noise alone).
- How: the detector, static curve, makeup, attack, smoothing, mix, limiter, tempo sync and the Analog stage were
  identified on the reference; the program-dependent release is a fitted two-state law (its tables found by CMA-ES on
  1034 measured cases), not the reference's algorithm, and a small learned correction (a GRU over WComp's own states,
  trained on 4300 renders of the reference) fixes most of what that structure misses: low notes under long attacks,
  long releases, deep compression (null test −41.4 → −47.0 dB, presets −35.6 → −43.7). Two reference quirks are documented: a slow start-up after a
  reset (modelled) and a hidden synced release time that survives switching Sync back to ms (not copied: reload H-Comp
  before nulling after a synced preset). The VU needle and limiter lamp follow the reference's, filmed from its editor's
  OpenGL frames. Details: [docs/NULL_RESULTS_WCOMP.md](docs/NULL_RESULTS_WCOMP.md),
  [docs/NULL_RESULTS_WCOMP_PRESETS.md](docs/NULL_RESULTS_WCOMP_PRESETS.md), [MEASUREMENTS.md](MEASUREMENTS.md#wcomp-model-phase-4-identification).

### WVox null test (honest numbers)

Residual of WVox against the reference, `20 log10(rms(difference) / rms(reference))`, lower is better:

- **Steady tones, any setting** (Compression, Gate, Gain, input level, 44.1–192 kHz, mono and stereo): −95 to −120 dB (a null).
- **Compression 0** with any Gate / Gain: −120 dB and below (a null).
- **Level steps, sweeps, speech-like material:** −30 to −65 dB, worse at heavy compression.
- **Dense noise at heavy compression:** −24 to −33 dB (was −10 to −25 dB before the learned slow path).
- Over a 672-case grid of settings: median −70 dB, worst −23 dB. The compressor's slow path is the fitted model plus a
  small learned correction (trained on renders of the reference, guarded so steady signals stay exact).
  Details: [docs/NULL_GRID.md](docs/NULL_GRID.md), [docs/NULL_RESULTS.md](docs/NULL_RESULTS.md).

### WBassX null test (honest numbers)

Residual of WBassX against the reference over 12 signals × 8 settings (Freq, Ratio, Response, Decay, High-pass, the
Original Bass and Harmonics faders), lower is better: **median −48.5 dB** on the whole output and **−42.7 dB** on the
created harmonics alone.

- Per signal (created harmonics, median over the settings): white noise and quiet tones −49 to −59 dB; tones, sweeps and
  pink noise −40 to −49 dB; level steps −34 dB; bass notes −27 dB; kick-like transients −19 dB.
- Over 120 random settings of every control with the hardest signals (bass notes, kicks, a loud tone, pink noise):
  median −35.2 dB on the whole output, −29.2 dB on the harmonics (`nulltest/nullgrid_wbassx.py`).
- Worst cases: loud material at extreme settings (Freq 256 with Ratio 4 and Decay −12, or Decay −9), where the
  reference's own harmonic feedback goes unstable.
- How: the filters, the harmonic generator with its Decay feedback, the faders, monitors and clip were identified
  exactly; the harmonics compressor (Ratio, Response) is a small recurrent network (48 units, ~7 % of one core) trained
  on renders of the reference, so it is a learned model of that part, not its algorithm.
  Details: [docs/NULL_RESULTS_WBASSX.md](docs/NULL_RESULTS_WBASSX.md), [MEASUREMENTS.md](MEASUREMENTS.md#wbassx-model-phase-4-identification).

### WBass null test (honest numbers)

Residual of WBass against the reference over 12 signals × 8 settings (Freq, Intensity, Gain, Original Bass In / Out),
lower is better: **median −36.8 dB** on the whole output and **−31.5 dB** on the created harmonics alone.

- Per signal (median over the settings): tones at or above Freq, sweeps, white noise −43 to −49 dB; bass notes,
  level steps, pink noise and tones below Freq −31 to −40 dB; kick-like transients −28 dB.
- Worst cases: Freq 160 with Original Bass Out on very low input (30 Hz tone −4 dB, kicks −9 dB, 60 Hz tone −16 dB):
  the output is then only the created bass, and its level is off there.
- How: the filters and the harmonic generator were identified exactly; the gain control of the created bass is a small
  recurrent network (32 units, ~6 % of one core) trained on renders of the reference, so it is a learned model of that
  part, not its algorithm. Meters move like the reference's (measured from its editor).
  Details: [docs/NULL_RESULTS_WBASS.md](docs/NULL_RESULTS_WBASS.md), [MEASUREMENTS.md](MEASUREMENTS.md#wbass-model-phase-4-identification).

How it was worked out: [RESEARCH.md](RESEARCH.md) (documented behaviour), [MEASUREMENTS.md](MEASUREMENTS.md) (what was measured and identified),
[docs/VALIDATION.md](docs/VALIDATION.md) (unit tests, pluginval, CI), [docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md) (open items and the learned-block procedure).

## Build

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

JUCE and the CLAP extensions are fetched by CMake. Plugins land in `build/plugins/<Name>/<Name>_artefacts/Release`.
The installers are made by [.github/workflows/release.yml](.github/workflows/release.yml) when a `v<version>` tag is pushed (which publishes them) or when the workflow is started by hand; ordinary pushes do not run it.
To add a plugin to the installers, add it to [packaging/plugins.txt](packaging/plugins.txt).

## License

[AGPL-3.0](LICENSE). Measurement data (JSON, CSV, plots) is in `measurements/`; the reference plugins and their artwork are not part of this repository.
