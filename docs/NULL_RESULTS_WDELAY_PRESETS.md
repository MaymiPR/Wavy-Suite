# WDelay factory presets against H-Delay's own presets

`nulltest/nulltest_wdelay_refpresets.py`: each of H-Delay's 88 factory presets loaded into the reference as the
reference loads it (the preset array from its preset file, on the user's install, written into its state; RefRender
`state=`), against WDelay's DSP (`tools/WDelayRender`) with the matching WDelay preset (Wavy names), its values as
stored, as WDelay uses a preset's values (MEASUREMENTS.md "WDelay model": the two parameter paths). Drum loop, bass
line and a voice-like signal (left, and 0.8 of it on the right), 48 kHz, each preset with its own Analog mode.
Residual = 20 log10(rms(ours - reference) / rms(reference)) in dB over 4 s after a 3 s silent pre-roll; Feedback is
held at 0 through the pre-roll and set to the preset's at the input's start (the reference's first block puts random
Analog noise into its line, which a loop above 100 % would grow before the input).

Summary (2026-10-25): **median −53.2 dB** over all 264 renders.

| Analog | presets | median | worst |
|---|---|---|---|
| Off | 19 | −78.1 | −50.8 (Modulation Machine, drums) |
| 1 | 12 | −53.2 | +2.9 (Fast Siren, see below) |
| 2 | 21 | −51.6 | −37.6 (Thin Straw) |
| 3 | 22 | −41.2 | −26.5 (Saucer (Loud), drums) |
| 4 | 14 | −69.5 | −43.9 (Left Side Guitar, drums) |

Notes:
- The first version of this test set the reference's parameters to WDelay's values, which hid two differences the
  user found by ear: the reference takes a preset's values exactly (Depth 0.69, Rate 281.7: set by a knob they would
  act as 1 and 282), and nine presets store LoPass 20 139, which the reference shows as "Off" but runs as a filter at
  20.1 kHz. WDelay now does both.
- Analog 1–3 add random noise as the reference does (−103 / −86 / −106 dBFS), which no null removes. Mode 3's model
  is the weakest part left (about −41 dB on every material; docs/IMPROVEMENTS.md).
- Fast Siren (1 ms, Feedback 150, HiPass and LoPass on) self-oscillates chaotically: a 1e-6 change of the input grows
  to −4 dB within 3 s in either plug-in, so it nulls only for its first fraction of a second.

| group | preset | Analog | drum loop | bass line | voice-like |
|---|---|---|---|---|---|
| Essentials | Full Reset | 2 | -54.8 | -54.8 | -51.0 |
| Essentials | Live Sound Start | Off | -57.4 | -75.4 | -82.8 |
| Essentials | Ping Pong | 3 | -34.1 | -41.5 | -41.1 |
| Essentials | Basic Stereo | 1 | -52.4 | -53.8 | -57.9 |
| Essentials | Basic Stereo LoFi | 4 | -69.5 | -69.4 | -69.2 |
| Essentials | LoFi Slap | 4 | -70.2 | -70.9 | -71.0 |
| Essentials | Band-Pass Echo | 2 | -49.5 | -54.9 | -52.6 |
| Essentials | Basic Echo | 1 | -52.2 | -53.3 | -57.3 |
| Essentials | Tape Echo | 3 | -38.7 | -40.1 | -42.7 |
| Essentials | Dub Echo | 3 | -38.7 | -40.5 | -42.6 |
| Essentials | Wide Echo | 3 | -39.3 | -41.2 | -42.7 |
| Essentials | Tape Runaway (Loud) | 1 | -53.2 | -53.7 | -54.5 |
| Essentials | Quarter Ping Pong | 2 | -48.8 | -53.2 | -50.3 |
| Essentials | Just Right Echo | 3 | -38.4 | -42.1 | -43.0 |
| Essentials | Filtered Dub 1/4T | 3 | -30.6 | -40.8 | -41.2 |
| Essentials | Vintage Echo | 1 | -54.7 | -55.6 | -59.2 |
| Essentials | Straight Eighths | 2 | -54.0 | -54.8 | -51.6 |
| Essentials | Sixteenths | 2 | -53.1 | -54.7 | -51.5 |
| Essentials | Dark Dub 1/8D | 2 | -47.4 | -53.2 | -50.4 |
| Essentials | Old School Insert | 3 | -37.9 | -42.0 | -42.1 |
| Essentials | Filtered Echo | 3 | -34.0 | -42.8 | -42.7 |
| Essentials | Filtered Bounce 1/8T | 1 | -40.1 | -48.7 | -51.7 |
| Essentials | Slap Insert | 1 | -52.4 | -53.7 | -55.5 |
| Essentials | Basic Slap | 1 | -48.3 | -51.6 | -55.9 |
| Essentials | Chorus Echo | 3 | -39.2 | -41.1 | -42.6 |
| Essentials | Chorus | Off | -70.4 | -69.8 | -69.0 |
| Essentials | Springy Drums | 2 | -50.5 | -52.7 | -50.3 |
| Essentials | Rap Vocal Echo | 3 | -33.9 | -41.4 | -41.2 |
| Essentials | Left Side Guitar | 4 | -43.9 | -57.2 | -65.0 |
| Essentials | Deep Chorus | 4 | -53.3 | -61.5 | -62.0 |
| Essentials | Light Flanger Insert | 3 | -38.8 | -43.1 | -44.5 |
| Essentials | Metal Doubler | 4 | -90.9 | -90.4 | -85.2 |
| Essentials | Flanger | Off | -52.9 | -70.4 | -80.0 |
| Essentials | Modulated Feedback | Off | -62.3 | -78.1 | -79.1 |
| Essentials | Modulation Machine | Off | -50.8 | -63.6 | -74.5 |
| Essentials | Far Away | 3 | -30.2 | -41.8 | -41.0 |
| Essentials | Bathroom | 2 | -50.8 | -53.6 | -52.1 |
| Essentials | Flutter Echo | 2 | -54.0 | -54.8 | -51.4 |
| Essentials | Tiled Room | 1 | -63.2 | -56.8 | -49.0 |
| Essentials | Identity Blur | 3 | -35.0 | -41.2 | -41.2 |
| Essentials | Lost Space | 3 | -41.2 | -42.4 | -39.8 |
| Essentials | Rotating Speaker | 2 | -44.3 | -47.9 | -42.4 |
| Essentials | Spring Stereo | 2 | -56.6 | -55.3 | -52.6 |
| Essentials | Spring Mono | 4 | -62.7 | -68.7 | -78.6 |
| Essentials | Next Room | Off | -90.6 | -88.5 | -89.2 |
| Essentials | Dawn | 2 | -41.1 | -53.2 | -51.4 |
| Essentials | Thin Straw | 2 | -37.6 | -44.8 | -46.0 |
| Essentials | Old Radio | 3 | -34.7 | -41.2 | -41.4 |
| Essentials | Whisker Wobble | 1 | -66.8 | -60.3 | -37.0 |
| Essentials | Rough Edge | 4 | -75.4 | -79.2 | -77.7 |
| Essentials | Broken Machine | 4 | -61.3 | -67.9 | -76.2 |
| Essentials | Horror Voices | 2 | -50.1 | -52.8 | -50.2 |
| Essentials | Strange Voices | 4 | -63.2 | -64.6 | -65.9 |
| Essentials | Low Rumble (Loud) | 3 | -38.3 | -43.2 | -46.6 |
| Essentials | Fast Siren | 1 | -7.5 | 2.9 | 2.6 |
| Essentials | Springy Wobble | 2 | -48.1 | -52.4 | -50.0 |
| Essentials | Robot Voice | 2 | -53.2 | -53.9 | -51.3 |
| Essentials | Saucer (Loud) | 3 | -26.5 | -39.6 | -40.3 |
| Essentials | Phantom | Off | -66.3 | -82.5 | -81.8 |
| Essentials | Dark Haunt (Loud) | 2 | -56.3 | -57.1 | -51.5 |
| Essentials | Grainy Rain | 2 | -48.0 | -51.9 | -49.6 |
| Essentials | Telephone | 3 | -38.2 | -41.8 | -40.5 |
| Essentials | Broken Filter | 3 | -43.9 | -42.9 | -39.5 |
| Essentials | Machines Talking | 2 | -48.6 | -52.8 | -51.1 |
| Essentials | LoFi Endless Loop | 4 | -69.7 | -71.2 | -70.8 |
| Essentials | Siren Drive (Loud) | 4 | -59.7 | -66.8 | -53.6 |
| Essentials | Hard Slap | Off | -100.4 | -96.8 | -96.4 |
| Essentials | Simple Echo | Off | -63.7 | -66.9 | -71.6 |
| Studio | Bouncing Balls | Off | -66.3 | -69.5 | -71.3 |
| Studio | Top Echo | Off | -70.7 | -71.1 | -70.2 |
| Studio | Vocal Echo | Off | -100.4 | -96.8 | -96.4 |
| Studio | Beat Vocal 1 | 4 | -77.6 | -74.7 | -75.1 |
| Studio | Beat Vocal 2 | 4 | -76.4 | -73.4 | -73.8 |
| Studio | Long Vocal | 2 | -54.1 | -54.7 | -51.3 |
| Studio | Psycho Spin | 2 | -53.1 | -53.9 | -50.6 |
| Studio | Vocal 1/16D | 3 | -38.4 | -41.8 | -43.0 |
| Studio | Blur Echo | 3 | -33.9 | -42.0 | -41.4 |
| Studio | Filtered Studio Echo | Off | -69.7 | -70.5 | -70.4 |
| Studio | Studio Vocal Slap | Off | -122.9 | -112.4 | -109.2 |
| Studio | Long Slow Vocal | 3 | -35.4 | -42.4 | -42.6 |
| Studio | Vocal FX | 1 | -46.7 | -49.3 | -53.5 |
| Studio | Go-To Echo | 1 | -40.6 | -48.9 | -53.3 |
| Studio | Panning Radio | Off | -69.1 | -70.1 | -71.1 |
| Studio | Off Stage | Off | -83.6 | -95.0 | -94.3 |
| Studio | Rotor (Loud) | 4 | -65.1 | -64.6 | -63.6 |
| Studio | Quarter Note Echo | Off | -67.8 | -78.5 | -78.6 |
| Studio | Eighth Note Echo | Off | -81.0 | -82.1 | -81.5 |
| Studio | Rock Vocal Echo | Off | -101.3 | -104.3 | -104.4 |
