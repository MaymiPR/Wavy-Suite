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
| **WBassX** | psychoacoustic bass enhancer with harmonics compressor, filters and monitor modes | DSP done, testing: close, not a null (median −48.5 dB, see below) |
| WComp, WDelay, WBass Ride, WVox Ride, WVox Chain, WVox Bend | | planned, one at a time |

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
