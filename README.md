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
| **WBass** | psychoacoustic bass enhancer (harmonic generator) | in progress: UI done, DSP pending |
| WMaxxBass, WComp, WDelay, WBass Ride, WVox Ride, WVox Chain, WVox Bend | | planned, one at a time |

### WVox null test (honest numbers)

Residual of WVox against the reference, `20 log10(rms(difference) / rms(reference))`, lower is better:

- **Steady tones, any setting** (Compression, Gate, Gain, input level, 44.1–192 kHz, mono and stereo): −95 to −120 dB (a null).
- **Compression 0** with any Gate / Gain: −120 dB and below (a null).
- **Level steps, sweeps, speech-like material:** −35 to −65 dB, worse at heavy compression.
- **Dense noise at heavy compression:** −10 to −25 dB (audibly different gain movement).
- Over a 672-case grid of settings: median −71 dB. Details: [docs/NULL_GRID.md](docs/NULL_GRID.md), [docs/NULL_RESULTS.md](docs/NULL_RESULTS.md).

How it was worked out: [RESEARCH.md](RESEARCH.md) (documented behaviour), [MEASUREMENTS.md](MEASUREMENTS.md) (what was measured and identified),
[docs/VALIDATION.md](docs/VALIDATION.md) (unit tests, pluginval, CI).

## Build

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

JUCE and the CLAP extensions are fetched by CMake. Plugins land in `build/plugins/<Name>/<Name>_artefacts/Release`.
The installers are made by [.github/workflows/release.yml](.github/workflows/release.yml) on every push; tagging `v<version>` publishes them.
To add a plugin to the installers, add it to [packaging/plugins.txt](packaging/plugins.txt).

## License

[AGPL-3.0](LICENSE). Reference renders and measurement data made during development stay private and are not part of this repository.
