# Validation

## WVox Rider

| Check | Result |
|---|---|
| Unit tests (`WVoxRiderTests`, 65 checks, 2026-10-28: the parameters as the reference's, the buses (a side-chain), no latency, the audio path, Automation Read and Write, the editor's Studio / Live layouts and the Music knob's disabled look, the 13 factory presets (their values, Attack, Automation Off in Full Reset, an Idle of 0 / 1 kept exactly), and the riding against the reference's measured values: the start, the law, the rise after 12 and 4 dB drops, the hold and glide to Idle at Slow and Fast, Spill on Live and off at -48) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WVox Rider.vst3` (Windows, local, 2026-10-28, with the presets) | SUCCESS, exit code 0 |
| The built VST3 (hosted by `tools/RefRender`) against the offline renderer (`nulltest/validate_wvoxrider.py`), stereo, 3 settings at 48 and 96 kHz | identical |
| The C++ ride against the model (`nulltest/check_wvoxrider_port.py`, 25 cases) | identical (0.0000 dB) |
| FocusCheck (the space bar reaches the host after clicks on the Vocal and Music knobs, the Attack switch, the Rider fader, the Idle arrow, the Automation buttons and Target; 2026-10-28) | PASS |
| Null test against the reference | [NULL_RESULTS_WVOXRIDER.md](NULL_RESULTS_WVOXRIDER.md): steps -74 dB, held-out vocal parts -47 .. -66 dB at ten settings |
| Factory presets against the reference's own (13, loaded into its state; three held-out vocal parts) | [NULL_RESULTS_WVOXRIDER_PRESETS.md](NULL_RESULTS_WVOXRIDER_PRESETS.md): median -60.9 dB, worst -37.9 |
| Sample rates | 44.1 kHz -63.5 dB, 96 kHz (Fast) -53.2 dB on held-out vocal parts |
| Components | Stereo -60.3 dB, Live -60.4 / -46.6 dB (Slow / Fast) |
| CPU (stereo at 48 kHz through the offline renderer) | about 0.6 % of one core |
| Music side-chain | not tested: the reference uses it in no offline host; to be checked in FL Studio |
| Tested by the user in FL Studio | not yet |

## WBass Rider

| Check | Result |
|---|---|
| Unit tests (`WBassRiderTests`, 84 checks, 2026-10-27: the parameters as the reference's (ranges, defaults, texts, the Idle arrow's place in the range), the components' latency (42 / 5 ms), the audio path, the ride against the reference's measured law, look-ahead, hold, glide and stereo mid, the Idle value before and after notes, the state, the editor and its selection, the 17 factory presets and Target left alone) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBass Rider.vst3` (Windows, local, 2026-10-27, and again 2026-10-28 after the third pass) | SUCCESS, exit code 0 |
| The built VST3 (hosted by `tools/RefRender`) against the offline renderer, stereo, 3 settings at 48 and 96 kHz (2026-10-28) | identical |
| The C++ ride against the fitted model (`nulltest/check_wbassrider_port.py`, 7 cases, 2026-10-28) | within 0.09 dB at 99 % |
| FocusCheck (the space bar reaches the host after clicks on the knobs, the Response switch, Target, the faders and the Idle arrow; 2026-10-27) | PASS |
| Null test against the reference, 48 kHz | [NULL_RESULTS_WBASSRIDER.md](NULL_RESULTS_WBASSRIDER.md): steady tones and level steps -48 .. -49 dB; the bass line -30 dB at most settings; real bass parts held out from the fit -22 .. -26 dB median (2026-10-28) |
| Sample rates | 44.1 kHz -30.2 dB, 96 kHz -30.0 dB on the bass line |
| Components | Live Mono -26.0 dB (Spill -25: -21.2, Spill -15: -24.2), Stereo (different bass on L / R) -27.7, Live Stereo -26.8 |
| Factory presets against the reference's own (17; bass line and two corpus parts) | [NULL_RESULTS_WBASSRIDER_PRESETS.md](NULL_RESULTS_WBASSRIDER_PRESETS.md): median -25.2 dB |
| CPU (stereo at 48 kHz through the offline renderer) | about 0.7 % of one core |
| Tested by the user in FL Studio (2026-10-27) | good |

## WDelay

| Check | Result |
|---|---|
| Unit tests (`WDelayTests`, 101 checks, 2026-10-25: a preset's exact values reach the DSP until the parameter is moved, and survive a session reload; the parameter texts and tapers, Rate at its shown resolution, the display's note and Dual / Ping Pong layouts and clicks, the DSP laws (interpolator, glide, gain stage, LFO, filters, Ping Pong, Phase), an impulse through the processor at the default 1/8D, the 88 factory presets load and leave Mix alone, state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WDelay.vst3` (Windows, local, 2026-10-25) | SUCCESS, exit code 0 |
| The built VST3 (hosted by `tools/RefRender`) against the offline renderer, 3 modulated settings at 48 and 96 kHz | identical (−400 dB) |
| FocusCheck (the space bar reaches the host after clicks on the knobs, the display, the buttons and the tap pad; 2026-10-25) | PASS |
| Null test against the reference, 48 kHz | [NULL_RESULTS_WDELAY.md](NULL_RESULTS_WDELAY.md): the delay path (defaults, ms times, filters, Ping Pong either side, Feedback up to 200 %) −84 … −104 dB; modulation −61 … −81; glides inside the input −60 … −83; LoFi −70; Analog 4 −80 … −90, Analog 1 / 2 / 3 −53 / −51 / −40 (their random noise included) |
| Sample rates | 44.1 kHz as 48 kHz; 96 kHz below 20 kHz −46 … −61 dB on the delay path, the band round the 24 kHz kernel cutoff differs (NULL_RESULTS_WDELAY.md) |
| Factory presets against the reference's own presets (88, loaded into its state as it loads them; drums, bass, voice-like; each with its own Analog mode) | [NULL_RESULTS_WDELAY_PRESETS.md](NULL_RESULTS_WDELAY_PRESETS.md): median −53.2 dB; Analog Off −78.1, mode 3 −41.2; Fast Siren chaotic (self-oscillation through the filters) |
| CPU (stereo at 48 kHz through the offline renderer, Feedback 60, Analog 2) | about 1.6 % of one core |

## WComp

| Check | Result |
|---|---|
| Unit tests (`WCompTests`, 631 checks, 2026-10-24: the learned correction leaves steady high tones within 0.1 dB and moves a low note under a long attack toward the reference; the needle laws against filmed values; the 26 factory presets load and leave Mix alone; typed values on Threshold read as negative; latency at 44.1 … 192 kHz, idle identity, static curve and makeup, block-size invariance, stereo link, limiter Off / On ceiling, Mix 0, tempo sync and the long-note fallback, Analog Off identity, Analog noise levels and independent channels, Analog 4 crossover offset, processor: side-chain ignored, state round trip, Mode Mono) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WComp.vst3` (Windows, local, 2026-10-24, with the learned correction) | SUCCESS, exit code 0 |
| The built VST3 (hosted by `tools/RefRender`) against the offline renderer, 5 signals × 5 settings, sync BPM / Host / no host tempo (`nulltest/check_wcomp_plugin.py`) | identical (−69 … −212 dB with the learned correction, 2026-10-24; the larger values from the plug-in's default Release 100.25 against 100) |
| FocusCheck (the space bar reaches the host after clicks on Threshold, Ratio, the meter and Limiter buttons, Sync and the BPM display; 2026-10-24) | PASS |
| Knob feel against the reference (MeterCapture drag and wheel scripts on every knob, plain and Ctrl, mouse steps 1 … 30 px; 2026-10-24) | the reference's values on every script (the wheel within the display's last digit) |
| Sample rates: the quick null test at 44.1 / 48 / 96 kHz (5 settings × 7 signals, with the learned correction, 2026-10-24) | median −43.1 / −42.9 / −41.8 dB (before −40.4 / −40.2 / −38.6); at 96 kHz the correction improves 14 cases by more than 1 dB and worsens none; the analytical model itself is a few dB weaker there on some cases |
| CPU (60 s of stereo at 48 kHz through the offline renderer) | 1.39 s without the learned correction, 1.99 s with it (about 1 % of a core more) |
| Analog stage: C++ against the fitted model | the difference is the added noise alone (−82.9 … −109.3 dBFS) |
| Null test against the reference | [NULL_RESULTS_WCOMP.md](NULL_RESULTS_WCOMP.md): median −47.0 dB over 168 cases, worst −24.5 (with the learned gain correction, 2026-10-24; before −41.4 / −16.8, and −37.2 / −9.6 before the CMA-ES refit); 70 cases better and one worse by more than 1 dB; defaults (Analog Off) voice-like −60.5, bass −57.4, drums −32.9, white / pink noise −42.9 / −31.5 (measured from 0.2 s, inside the start-up); weakest: noise bursts with Analog on, the 1 kHz tone under Analog 3, the output limiter on voice (−24.5 … −28) |
| The learned correction on 200 unseen cases (random settings and signals, `nulltest/train_wcomp.py` TEST) | the weak cases (worse than −35 dB without it) −25.8 → −36.6 dB, all −48.3 → −52.6; 106 better, 15 worse by more than 1 dB (the worst −96.3 → −73.4) |
| Factory presets against the reference (each preset on voice-like, drum loop, bass line, pink noise; Analog Off and the preset's mode) | [NULL_RESULTS_WCOMP_PRESETS.md](NULL_RESULTS_WCOMP_PRESETS.md): median −43.7 dB (Analog Off, from 0.5 s, a fresh reference per preset; before −35.6); 82 better, 4 worse by more than 1 dB (all still −58 … −72); weakest: Hard Squash on drums (−20.8), Light Kit on drums (−26.4), Parallel Blend / Heavy Drums / Pumping Kit on drums (−29 … −30) |
| VU needle against the reference (both filmed with MeterCapture with the same probe: H-Comp from its OpenGL frames, WComp by screen grab; threshold −30, ratio 4; 2026-10-23) | GR: mean reading drums 11.5 / 11.5 dB, voice 5.6 / 5.9, tones and bursts 5.3 / 5.4; In: mean frame within 1.4 of 70, 90 % of frames within 1.5 steps; the laws alone against the films: 97 % (GR) and 97.5 % (In / Out) of frames within one step (MEASUREMENTS.md "WComp needle") |

## WVox

| Check | Result |
|---|---|
| Unit tests (`WVoxTests`, 332 checks: latency, static law, gate, gate and compressor together, attack / release, finite output on extreme input, block-size invariance, stereo link, processor parameters, state round trip, Mono mode, factory presets, learned slow path leaves steady tones and Compression 0 exactly as without it) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WVox.vst3` (Windows, local, 2026-10-18) | SUCCESS, exit code 0; its optional "vst3 validator" step was skipped (no Steinberg validator path set) |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Meters against the reference's editor (`tools/MeterCapture`, bass line + kicks, Compression −12 dB): bar height difference | Energy, Output, Gain reduction within 2.0 / 2.3 / 2.0 px on 90 % of frames |
| Null test against the reference | [NULL_RESULTS.md](NULL_RESULTS.md) (signal set) and [NULL_GRID.md](NULL_GRID.md) (672 settings) |
| Learned slow path: C++ against the trained PyTorch model on the null-test signals | −70 dB or lower |
| Learned slow path at 44.1 / 96 kHz (5 signals × 3 settings) | same gains as at 48 kHz; steady tones unchanged |
| CI on version tags (or started by hand): build, unit tests and pluginval on Windows / macOS / Linux, auval on macOS, one installer per OS | `.github/workflows/release.yml` |

## WBassX

| Check | Result |
|---|---|
| Unit tests (`WBassXTests`, 67 checks, 2026-10-20: the identified structure against the reference's own measurements (generated part at rest, 4 settings, 48 / 44.1 / 96 kHz, within 0.15 dB; Decay feedback h2 / h3 within 0.3 / 0.6 dB), original path and its all-pass, Original Bass monitor pre-fader, Input trim stops at 0 dB, Audio = original + Harmonics × G, monitor pre-fader, Harmonics capped at +10.96 dB, Decay on the even harmonics, High-pass on the generated part, output clip, block-size invariance, channels independent, 96 kHz, processor and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBassX.vst3` (Windows, local, 2026-10-20) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Factory presets (22): values from the reference's presets converted by the reference itself (16; the other 6 by the same scales, which reproduce all 16 exactly); Input untouched on preset load | unit tests pass |
| Value entry against the reference (MeterCapture `dbl` / `click` scripts: double-click and type on a knob, a fader and Freq; single click on the value; Ctrl- and Alt-click) | same parameter values after every step; entry boxes match to the pixel |
| Output meters against the reference's editor (MeterCapture, −6 / −20 dBFS bursts) | bar height within 1 px on 88 % of frames, 3 px on 93 %; steady levels and the silence dot exact |
| Learned level control: C++ against the trained PyTorch model | −71 … −117 dB on most examples (−43 dB on a kick) |
| Sample rates: null at 96 kHz against 48 kHz (24 cases) | within 1.8 dB |
| Null test against the reference | [NULL_RESULTS_WBASSX.md](NULL_RESULTS_WBASSX.md): median −48.5 dB (whole output), −42.7 dB (generated part); random-settings grid (480 cases) −35.2 / −29.2 dB; not a null, see MEASUREMENTS.md "WBassX model" |

## WBass

| Check | Result |
|---|---|
| Unit tests (`WBassTests`, 29 checks: original path In / Out, latency 0, Intensity exact on the generated part, low-level generated gain, Decay switches the harmonic product, output clip, Gain trim, block-size invariance, silence, stereo, processor parameters and state round trip) | all pass (Windows, local) |
| pluginval, strictness level 10, `--skip-gui-tests`, on `WBass.vst3` (Windows, local, 2026-10-18) | SUCCESS, exit code 0 |
| FocusCheck (the space bar reaches the host after clicks on the editor) | PASS |
| Meters against the reference's editor (`tools/MeterCapture`, bass line + kicks): bar height difference | within 3.5 px (0.7 dB) on 90 % of frames (Original Bass); output and harmonics within 8 px, the rest being the generated level |
| Learned gain block: C++ against the trained PyTorch model on held-out renders | −74 … −91 dB |
| Sample rates: null at 44.1 / 96 kHz against 48 kHz (5 signals) | within 0.2 dB |
| Null test against the reference | [NULL_RESULTS_WBASS.md](NULL_RESULTS_WBASS.md): median −36.8 dB (whole output), −31.5 dB (generated part); not a null, see MEASUREMENTS.md "WBass model" |
