<p align="center">
  <img src="docs/branding/wavy-banner.svg" alt="Wavy Suite" width="100%">
</p>

# Wavy Suite

Open-source effect plugins (**VST3**, **CLAP**, **AU**) for Windows, macOS and Linux, built with JUCE.
Each Wavy plugin is an independent, from-scratch recreation of a familiar mixing tool, modelled from its
public manual and measured against the real thing for private analysis. Mono / stereo variants live in one
plugin behind a **Mode** switch.

> Wavy Suite is an independent project. It is not affiliated with, endorsed by or sponsored by Waves Audio.
> Other companies' product names appear only to describe what a Wavy plugin is modelled on and belong to their owners.

## Plugins

| Wavy plugin | Modelled on | Status |
|---|---|---|
| **WVox** | vocal compressor / gate / limiter | UI done, DSP in progress |
| WBass, WMaxxBass, WComp, WDelay, WBass Ride, WVox Ride, WVox Chain, WVox Bend | | planned |

One plugin at a time. Research notes with sources: [RESEARCH.md](RESEARCH.md).

## Build

```
cmake -S . -B build
cmake --build build --config Release --target WVox_VST3
```

JUCE and the CLAP extensions are fetched by CMake. Windows builds use a static runtime.

## License

[AGPL-3.0](LICENSE). Reference renders and measurements made during development stay private and are not part of this repository.
