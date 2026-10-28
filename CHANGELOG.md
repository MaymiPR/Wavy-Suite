# Changelog

## Unreleased

- **WBass Rider** riding, third pass (from how the reference configures its detector): Spill now works on the Live
  components (a note starts over the Artifacts factor times the long reference plus Spill, which also moves the floor
  with Artifacts on Studio), the weighting is the reference's two-section shelf at 30 Hz, and the release depths are
  Slow 12 / Fast 4 dB (ten fitted constants fewer). Live at Spill -15: -14.3 -> -24.2 dB; held-out bass parts at Fast
  / Sensitivity 25 -19.6 -> -24.2 and Artifacts 85 -21.2 -> -25.1 dB median; steady tones -41.5 -> -48.9 dB. One
  synthetic ghost-note case got worse (-34.5 -> -13.5 dB, a single borderline note).
- **WVox Rider** (ready for the next release; Phases 2 to 5), a real-time vocal level rider modelled on Vocal Rider:
  its 320 x 598 editor drawn by Wavy (the Target slider over the vocal meter, the Ride / Idle LEDs, the Attack switch,
  the Vocal and Music Sensitivity knobs, Spill on Live, the Range column with the Idle arrow, the Rider and Output
  faders, the Automation Off / Write / Read buttons), and the riding identified from the reference's audio: the
  weighted level read and its floor, a per-sample vocal detector that the ride waits on (so it rises slowly on a real
  voice), the hold and glide to the Idle value, Spill added to the detector's threshold on Live, the stereo mid; every
  constant is the reference's own hidden value. Automation Write sends the ride to the host's Rider Fader, Read plays
  it back; the fader can be grabbed. The reference's 13 factory presets with Wavy names. No latency. Null: level steps
  -74 dB, held-out vocal parts -47 .. -66 dB, presets median -60.9 dB (docs/NULL_RESULTS_WVOXRIDER.md); pluginval,
  FocusCheck, 44.1 / 96 kHz checked; in the installers' plug-in list. The music side-chain (Music Sensitivity) is not
  modelled yet.

- **WBass Rider** (ready for the next release; Phases 4 and 5): the riding DSP, identified from about 1,200 probe
  signals on the reference and its riding on a corpus of 36 bass parts: the weighted level read and the gain law
  (Target - level + 0.68 dB), the 42 ms look-ahead (5 ms in the Live components), a note detector with a windowed
  note level by pitch band, slow and long envelopes, follow and hold, release and re-attack rules by Response and
  Sensitivity, the Artifacts gate, the note end and the 32 dB/s glide to the Idle value (also before the first note,
  and following the arrow in silence), one gain for both channels from the mid. The reference's 17 factory presets
  with Wavy names (Target left where it is, as the reference's). The knob and Target values follow a drag. Null:
  steady tones and steps -41 .. -49 dB, a bass line -30 dB, real bass parts -20 .. -25 dB median (docs/
  NULL_RESULTS_WBASSRIDER.md); pluginval, FocusCheck, 44.1 / 96 kHz checked; in the installers' plug-in list.
- **WBass Rider** (Phase 2: the editor; the audio is delayed by the component's latency and passes at the Rider's idle
  gain plus Output until the riding DSP), a note-by-note bass level rider modelled on Bass Rider: its 320 x 560 editor
  drawn by Wavy (resizable) — the Target slider's glass thumb over the Energy meter, the Detect / Ignore groups with
  the dashed-ring Sensitivity knob, the Response switch, Spill and Artifacts with their tick rings, the Range column
  (Max / Min thumbs, the lit band between them, the Idle arrow and its line), the Rider fader on its +-12 dB scale,
  the Output fader with its clip LED, the value boxes and the name plate; the caps, the meter, the Target thumb and
  the panels are drawn from shading models fitted on the reference (`tools/fit_bassride_parts.py`, no image data in
  the repository). Selection as measured on the reference: a click marks a control in yellow, its value shows while
  the mouse is on it. The parameters, ranges and defaults of the reference's VST3; Mode Stereo / Mono / Live Stereo /
  Live Mono (the reference's four components; 42 / 5 ms latency). The narrow Renaissance toolbar (320 px, no Skins
  box) is new in the shared kit. MeterCapture's script screenshots read OpenGL editors back with `gl=1`.
- **WBass Rider editor, second pass, against Bass Rider's frames and filmed behaviour**: the caps fitted on the
  reference's 1x frame (outline and lip), the exact scales, the clef and name plate, the leather's scratches, the
  value boxes; text entry by double-click where the reference has it (not on the Rider), Spill's typed positives read
  as negative, out-of-range values ignored; the Idle arrow as its own control (its value box at the left edge, the
  host's text over +-12 dB); the Rider's blue grab and its return to riding on release; hidden cursor during drags;
  the lasso with its yellow / blue group (member drags move nothing, as the reference). Mean difference to the
  reference frame 14.1 -> 8.6 levels.
- **WBass Rider editor, third pass, and its behaviour with audio**: the knobs' domes and pointers fitted on the
  reference's frame and their scales measured tick by tick round the domes' true centres; the Response switch and
  the clip LED as drawn by the reference (the LED latching until clicked); the lit band's warm tone; screws and value
  boxes. Mean difference 8.6 -> 7.6 levels (knobs 17 -> 8, switch 17 -> 3, LED 25 -> 10). The knob cursor over the
  knobs and Output; no hover effects, as the reference. A provisional riding model fitted on films of the reference
  (`WBassRiderProvisional.h`) moves the Rider and the Energy meter as the reference's do (steady tones within 0.1 dB,
  a bass line within about 1-2 dB), until the DSP of Phase 4. MeterCapture's script mode streams the OpenGL frames
  with an action log and saves the cursor.
- **WDelay** (Phase 2: the editor; the audio passes through until the DSP), a hybrid delay modelled on H-Delay: the
  Hybrid-line editor (749 x 292, resizable) drawn by Wavy — the Delay and Feedback knobs in chrome skirts, the knurled
  modulation and filter knobs, the right column's small knobs, the tap pad, the Link wheel, the amber time display
  (note and tempo, ms, Ping Pong's start side, Dual's two sides), the output meter's red triangles and the clip light —
  on H-Delay's panel shading; its parameter list and tapers (the Delay taper as read from the reference), Mono / Stereo /
  M>S modes; knob, wheel, Ctrl, display and Link feel as measured on the reference. The Hybrid kit gains these knob kinds
  (fitted with a centre per layer), a row of independent toggles and the panel's second look; the Hybrid toolbar follows
  a wider editor.
- **WDelay editor, second pass, pixel by pixel against H-Delay**: the display's segment font fitted per size and the
  note figures placed as the reference places them (T and D, the fraction's dashes, 1 BAR, Dual's and Ping Pong's
  layouts), its sunk window and glass; the tap pad fitted released and pressed, with its selection line; the meter's
  housing, scale law (within 0.3 px), numbers and triangles; H-Delay's own button look (frame, faces, bevels, shadows,
  Arial Bold labels); one control selected at a time as the reference (knob ring, button and group outlines, tap pad);
  the panel's edges, black print, brackets, the name, the lamp and the Link wheel. Mean difference to the reference
  13.9 → 8.0 levels. A note longer than 3500 ms at the tempo is not taken (the reference stops at 1 bar at 120 BPM).
- **WDelay: modulation, filters, resolutions and presets, from more probing of H-Delay**:
  - **Modulation:** the LFO is the reference's float triangle (±1, from 0 rising, a step of 4 × rate / fs, held at
    its ends). In Host / BPM sync, Rate counts cycles per beat. The glide and the LFO act on a delay 16 samples longer.
    Modulation nulls went from −5 … −16 dB to −56 … −84 dB.
  - **Filters:** HiPass / LoPass act on the line's output, which matters once the delay moves.
  - **Resolutions:** parameters are used at the reference's resolution (Delay, Feedback, Mix, BPM, Depth and the
    filters in whole units, Output in 0.1 dB, Rate to three significant digits as shown, at least 0.01 Hz: 3074 Hz
    acts as 3070). Triplet notes are a hair short, as the reference has them. The Rate rounding took fast-modulation
    settings from −10 … −54 dB to −67 … −84 dB, and the presets Wide Echo and Broken Machine from −19 / −36 dB to
    −43 / −71 dB.
  - **Analog 1** refitted: −50 → −54 dB in the null suite.
  - **96 kHz:** the LFO runs twice as fast there, as the reference's (modulation +2.6 → −48 dB below 20 kHz).
  - **Presets as the reference loads them:** a factory preset's values are used exactly as stored (Depth 0.69,
    Rate 281.7, LoPass 20 139 as a filter at 20.1 kHz though shown Off), while a value set from a knob or the host
    stays at the reference's resolution. Nine presets' LoPass corrected. The preset null test now loads the
    reference's own presets (`nulltest/nulltest_wdelay_refpresets.py`; RefRender takes `state=`).
  - **Presets:** the reference's 88 presets, with Wavy names (`tools/make_presets.py`), and their null test
    (`nulltest/nulltest_wdelay_presets.py`).
- **WDelay DSP** (Phase 4, `plugins/WDelay/WDelayDsp.h`):
  - **What it models:** H-Delay's delay path as identified. A 32-phase Kaiser-sinc interpolator with 31 taps round
    the delay, the tape glide (T + 16 samples), the linked 1 / (1 + peak) gain stage in the loop (10 ms recovery),
    the loop's extra sample, RBJ HiPass / LoPass with the reference's frequency warp, LoFi's 6277 Hz interpolator,
    the triangle LFO on the target, Ping Pong's crossed lines, Dual, Phase on the delay's input and the equal-power
    mix. The Analog stage colours the input, with H-Delay's own modes 1–3 fitted and mode 4 shared with H-Comp.
  - **Nulls** (`docs/NULL_RESULTS_WDELAY.md`): −95 … −104 dB at the defaults, with filters, Ping Pong and Feedback
    up to 200 %. Analog 4 −83 … −90 dB; Analog 1–3 −40 … −55 dB, their noise included.
  - **Known limits:** glide tails −40 … −80 dB, LoFi −70 dB, modulation −5 … −16 dB, the feedback loop at 96 kHz.
  - **Tools:** unit tests (85 checks), an offline renderer (`WDelayRender`) and the null suite
    (`nulltest/nulltest_wdelay.py`). WComp's Analog stage takes a table set; WDelayRender renders its pre-roll in
    whole blocks, as RefRender does.
- **WDelay Phase 3: measurements of H-Delay** (`nulltest/measure_hdelay.py`, `measurements/hdelay/`, MEASUREMENTS.md):
  the 32-phase Kaiser-sinc interpolator, the tape glide law, the 1 / (1 + peak) gain stage in the loop, feedback,
  filters, LoFi, modulation, routing, mix and the Analog modes. `RefRender` gains a silent pre-roll and timed
  parameter changes.
- **WDelay Ping Pong and Dual, as filmed on H-Delay**:
  - **Ping Pong's display:** the layout is Dual's left half plus the start box. The box and its L / R letter are fitted
    on the reference's pixels. A click flips the start side and selects the box.
  - **Dual's sides:** clicking a side puts the Delay knob on it, with an "R" box after DELAY for the right side. The
    choice is kept while Ping Pong is on. Switching Dual on starts the right side from the left's time.
  - **Display drags:** notes on the display step with a drag or the wheel, in every layout. The tempo drags in Dual and
    Ping Pong. `Snapshot --click=x,y` renders states that only a click sets; editor tests cover the clicks.
- **MeterCapture** `away=0`: the drag script keeps the mouse over the editor between drags (leaving it lost travel on
  the next drag, on the references too); the cursor's position after each release is printed.
- **WComp** (ready for the next release; Phase 5: in the installers' plug-in list, FocusCheck, sample-rate checks),
  a hybrid compressor modelled on H-Comp: the Hybrid-line editor (643 x 454, resizable)
  drawn entirely by Wavy on a new shared kit (`Source/common/WavyHybridUI`: the orange panel, bakelite and chrome
  knobs with printed scales, ivory buttons, a VU meter with needle, the amber BPM display) and a Hybrid toolbar style;
  the reference's parameter list and tapers (Ratio's own curve, log Attack / Release, Threshold 0 at the left), a
  side-chain input bus (ignored, as the reference does); selection, value entry, Alt-click reset and drag feel as
  measured on the reference.
- **WComp DSP** (Phase 4): the look-ahead peak compressor identified on the reference (detector, static curve to
  0.014 dB, makeup, attack, smoothing, mix, output, a 64-sample look-ahead limiter, tempo-synced release with the
  long-note fallback, linked stereo, Mode Mono), the two-state program-dependent release, and the four Analog modes
  (the last stage: filtered nonlinearity and shaped noise; mode 4 nulls at −82 … −100 dB on loud material). Null test
  median −37.2 dB (defaults −62 dB; noise, drums and deep slow settings are not nulls yet;
  docs/NULL_RESULTS_WCOMP.md). VU needle and limiter lamp driven (provisional VU ballistics). Unit tests (`WCompTests`),
  offline renderer (`WCompRender`).
- **WComp needle and lamp, as filmed on the reference**: H-Comp's editor filmed without FL (its OpenGL frames read back
  in `tools/MeterCapture gl=1`, which now also gives the plug-in a playing transport). The needle is a strip of 71
  positions at the filmed whole-degree angles about the measured pivot, with the reference's look: 2 px, tip 111.5,
  the rivet (radius 2.85, cream centre 1.15) and the shadow (the needle moved 4.7 px straight down, Gaussian 0.94 px,
  ×0.75), each measured against WComp's own render (`tools/VuRender`). GR: the
  deepest gain at once, the return through two one-poles (28.9 / 142.6 ms) on the linear gain — on drums it reads
  11.5 dB as H-Comp does (the provisional VU average read 6.2). In / Out: the peak level on their own dBFS face
  (48 … 0, 24 at the top, now drawn), with the reference's hold-and-release law. The limiter lamp glows amber and
  fades over its 0.833 s hold.
- **WComp learned gain correction**: a small GRU reads the compressor's own states and corrects its gain where H-Comp's
  level detection departs from the identified structure (low notes under long attacks, long releases, deep
  compression); none on steady tones or without reduction. Null test median −41.4 → −47.0 dB (worst −16.8 → −24.5, one
  case worse by more than 1 dB), presets −35.6 → −43.7 dB (Drum Punch on bass −17 → −43, Vintage Drum Crunch on bass
  −19 → −43, Fat Kick on noise −12 → −31); unseen cases −25.8 → −36.6 dB on the weak ones. About 1 % of a core.
- **Wheel and Ctrl fine-tuning on WVox, WBass and WBassX, as measured on RVox, RBass and MaxxBass**: the wheel turns
  every control 1/72 of its range per notch, over the whole control (the faders took no wheel at all, the knobs 2.5
  times too much); Ctrl fine-tunes the faders at the reference's rates (0.19 of the normal rate on quicker mouse moves,
  0.037 on slow ones; WVox and WBass fell into JUCE's velocity mode with Ctrl, WBassX moved a flat quarter); WBass's
  Freq moves 1.53 times its track, as the reference's.
- **Ctrl during a drag, on all four plug-ins**: pressing or releasing Ctrl while a control is already being dragged
  now switches the fine adjustment on or off, as on the references. In FL, which keeps the keyboard focus, the rate
  stayed as it was at the mouse-down. WBassX's knobs take the reference's Ctrl law (a quarter of the rate, keeping the
  slow / fast split; it was a flat 1/6). `MeterCapture hostwin=1` and the `midctrl` / `ctrlup` drag modes reproduce it.
- **WVox and WBass faders hide the cursor while dragging**, as RVox and RBass do, and put it back on the thumb
  (where it was grabbed, moved as far as the thumb moved).
- **WComp knob feel as measured on H-Comp**: the mouse wheel moves 1/72 of a knob's range per notch (it moved 2.6 times
  that), Ctrl gives a quarter of the normal rate and keeps the slow / fast difference (it was a flat 1/6), the fast
  rate starts at 3 px mouse steps, and the stepped knobs have their own rates (Analog a position per 50 px; the synced
  Release note a value per 30 px slow, 20 fast, 120 / 66.7 with Ctrl). MeterCapture's drag script gained wheel moves.
- **WComp presets and value entry**: the reference's 26 factory presets (*Essentials* and *Studio*, Wavy names; array
  positions confirmed by loading presets into the reference), which leave Mix alone as the reference does; null test of
  every preset in docs/NULL_RESULTS_WCOMP_PRESETS.md. Typing a positive number into Threshold sets it negative ("18" →
  −18 dB), as the reference's ParamXML marks it (PositiveTextEntryToNegative); WBassX's Decay now does the same.
- **WComp release refit**: the release law's tables refitted by CMA-ES on probes from a settled state, repeated-event
  and memory probes, realistic envelopes and the null-test material (null-test median −37.2 → −39.6 dB; drums at the
  defaults −20.7 → −31.7 dB, voice and bass unchanged); then the presets' settings added to the fit and the
  reference's start-up state modelled (slow release in the first fraction of a second); the release memory now
  charges from the reduction after the attack (long-attack presets: Fat Kick drums −12.9 → −21.7 dB; presets' median
  −32.0 → −35.5 dB); the memory drains no faster than a 16.8 ms release (sustained bass); a final joint fit:
  null-test median −41.4 dB, presets' median −35.6 dB.
- WComp Phase 3: measurements of the reference H-Comp (`nulltest/measure_hcomp.py`, `measurements/hcomp/`, MEASUREMENTS.md).
- **WBassX presets, value entry and meters** (Phase 5): 22 factory presets (*Essentials* and *Studio*, settings
  from the reference's own set converted by the reference itself, Wavy names), which leave Input alone as the
  reference does; double-click a knob, fader or the Freq slider to type a value, Alt-click to reset (measured on the
  reference, the entry box matches it to the pixel); output meters with the reference's faster release (0.155 s),
  its scale and its floor dot, within 1 px on 88 % of frames.
- **WBassX DSP** (Phase 4): the original path, the harmonic generator with its Decay feedback, the High-pass settings,
  faders, monitors and output clip identified exactly; the harmonics compressor learned from renders of the reference
  (median null −48.5 dB on the whole output, −42.7 dB on the harmonics; docs/NULL_RESULTS_WBASSX.md). Unit tests
  (`WBassXTests`) and an offline renderer (`WBassXRender`) for the null test.
- WBassX Phase 3: measurements of the reference MaxxBass (`nulltest/measure_maxxbass.py`, `measurements/maxxbass/`, MEASUREMENTS.md).
- Meters: peak holds read -Inf in silence (an empty frame no longer counts as -80 dB).
- **Factory presets** for WVox (21) and WBass (45), in two groups, *Essentials* and *Studio*, in the preset menu
  (Prev / Next step through them) and in the host's program list. The settings come from the reference's own
  presets; every name is Wavy's own (no artist, product or other trademarked names). Generated by
  `tools/make_presets.py`.
- **WBass**, a psychoacoustic bass enhancer (harmonic generator), joins the suite: dark skin on the reference's pixel grid; DSP from the Phase 4 identification, with the gain control of the generated bass learned
  from renders of the reference (a small GRU, ~6 % of one core); null test median −36.8 dB; Original Bass and Smooth are two-choice parameters
  so a restored state always snaps to on or off. Meters move like the reference's (measured from its editor: instant
  rise, 0.325 s one-pole release, 26.7 dB/s in silence), independent of the host's buffer size; editors redraw at
  60 frames per second.
- **WBassX**, a bass enhancer modelled on a harmonics-generator with its own compressor, filters and
  monitor modes: the flat editor (800 x 395, resizable) on a new shared kit (`Source/common/WavyFlatUI`: gain faders
  with the reference's taper, knobs, frequency slider, slope selector, monitor buttons, band graph) and the single-row
  toolbar style; full parameter list.
- **WVox null**: the compressor's slow path gets a small learned correction (a GRU trained on renders of the
  reference, ~3.5 % of one core), guarded so steady signals keep the identified paths exactly. Worst case of the
  settings grid −10 → −23 dB; dense noise at heavy compression −10…−13 → −24…−27 dB, voice-like −21…−31 → −30…−40 dB;
  AM at light compression is the one notable loss (−59 → −49 dB).
- **WVox meters** measured from RVox's editor and matched: Output as above, Energy with two release stages (0.341 s
  then 0.094 s), Gain reduction shows the deepest reduction of each frame (short bursts are no longer missed).

## 0.1.0

First release of the Wavy Suite, with one plugin:

- **WVox**, a three-control vocal compressor / gate / limiter (VST3, CLAP, AU; Windows, macOS, Linux).
  - Dark skin on the reference's pixel grid, resizable 75–200 %, Waves-style toolbar (Setup A/B with copy, undo / redo, presets with Save, Mode Stereo / Mono, window size), click a value to type it.
  - DSP from measurements: four-phase true-peak detector, exact static law, identified look-ahead (64 / 128 / 256 samples) and gate; fitted slow path.
  - Null test against the reference: steady material nulls (−95 to −120 dB at any setting); dynamic material is close (median −71 dB over a 672-case settings grid; dense noise at heavy compression −10 to −25 dB).
- One installer per operating system for the whole suite.
