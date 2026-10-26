# WDelay factory presets against H-Delay

`nulltest/nulltest_wdelay_presets.py`: each of WDelay's 88 factory presets (H-Delay's own presets with Wavy names)
rendered by WDelay's DSP (`tools/WDelayRender`) and by the reference set to the same values, on a drum loop, a bass
line and a voice-like signal (left, and 0.8 of it on the right), 48 kHz, each preset with its own Analog mode.
Residual = 20 log10(rms(ours - reference) / rms(reference)) in dB over 4 s after a 3 s silent pre-roll, Feedback
from the input's start (as docs/NULL_RESULTS_WDELAY.md).

Summary (2026-10-25): **median −53.0 dB** over all 264 renders.

| Analog | presets | median | worst |
|---|---|---|---|
| Off | 19 | −73.2 | −50.8 (Modulation Machine, drums) |
| 1 | 12 | −53.2 | −1.9 (Fast Siren, see below) |
| 2 | 21 | −51.5 | −40.2 |
| 3 | 22 | −41.1 | −25.8 (Saucer (Loud), drums) |
| 4 | 14 | −67.4 | −43.7 (Left Side Guitar, drums) |

Notes:
- Analog 1–3 add random noise as the reference does (−103 / −86 / −106 dBFS), which no null removes. Mode 3's model
  is the weakest part left (about −41 dB on every material; MEASUREMENTS.md "WDelay model", docs/IMPROVEMENTS.md).
- Fast Siren (1 ms, Feedback 150, HiPass and LoPass on) self-oscillates chaotically: a 1e-6 change of the input grows
  to −4 dB within 3 s in either plug-in, so it nulls only for its first fraction of a second.
- Before this round (Rate at 0.01 Hz instead of three significant digits, the old Analog 1 fit): Wide Echo −19 dB,
  Broken Machine −36 dB; now −43 and −71 dB (voice-like).

| group | preset | Analog | drum loop | bass line | voice-like |
|---|---|---|---|---|---|
| Essentials | Full Reset | 2 | -54.8 | -54.8 | -51.0 |
| Essentials | Live Sound Start | Off | -63.5 | -69.9 | -83.1 |
| Essentials | Ping Pong | 3 | -34.2 | -41.4 | -41.0 |
| Essentials | Basic Stereo | 1 | -51.9 | -53.8 | -57.8 |
| Essentials | Basic Stereo LoFi | 4 | -69.2 | -69.4 | -68.1 |
| Essentials | LoFi Slap | 4 | -52.6 | -50.7 | -49.0 |
| Essentials | Band-Pass Echo | 2 | -42.3 | -43.3 | -42.4 |
| Essentials | Basic Echo | 1 | -52.2 | -53.3 | -57.2 |
| Essentials | Tape Echo | 3 | -38.7 | -40.1 | -42.7 |
| Essentials | Dub Echo | 3 | -38.7 | -40.5 | -42.7 |
| Essentials | Wide Echo | 3 | -39.3 | -41.1 | -42.7 |
| Essentials | Tape Runaway (Loud) | 1 | -53.1 | -53.7 | -54.2 |
| Essentials | Quarter Ping Pong | 2 | -48.9 | -53.0 | -50.3 |
| Essentials | Just Right Echo | 3 | -38.4 | -42.1 | -42.9 |
| Essentials | Filtered Dub 1/4T | 3 | -30.6 | -40.8 | -41.2 |
| Essentials | Vintage Echo | 1 | -54.7 | -55.6 | -59.2 |
| Essentials | Straight Eighths | 2 | -54.2 | -54.8 | -51.6 |
| Essentials | Sixteenths | 2 | -54.1 | -54.8 | -51.5 |
| Essentials | Dark Dub 1/8D | 2 | -47.4 | -53.1 | -50.4 |
| Essentials | Old School Insert | 3 | -37.9 | -42.0 | -42.1 |
| Essentials | Filtered Echo | 3 | -34.0 | -42.8 | -42.7 |
| Essentials | Filtered Bounce 1/8T | 1 | -40.1 | -48.7 | -51.7 |
| Essentials | Slap Insert | 1 | -52.4 | -53.7 | -55.5 |
| Essentials | Basic Slap | 1 | -48.3 | -51.6 | -55.9 |
| Essentials | Chorus Echo | 3 | -39.2 | -41.1 | -42.6 |
| Essentials | Chorus | Off | -70.3 | -69.7 | -69.2 |
| Essentials | Springy Drums | 2 | -50.5 | -52.7 | -50.2 |
| Essentials | Rap Vocal Echo | 3 | -33.9 | -41.4 | -41.1 |
| Essentials | Left Side Guitar | 4 | -43.7 | -59.1 | -64.7 |
| Essentials | Deep Chorus | 4 | -53.3 | -61.5 | -62.0 |
| Essentials | Light Flanger Insert | 3 | -38.9 | -43.1 | -44.5 |
| Essentials | Metal Doubler | 4 | -90.9 | -90.3 | -85.3 |
| Essentials | Flanger | Off | -52.2 | -78.1 | -78.9 |
| Essentials | Modulated Feedback | Off | -64.7 | -79.4 | -79.1 |
| Essentials | Modulation Machine | Off | -50.8 | -66.9 | -74.5 |
| Essentials | Far Away | 3 | -30.3 | -41.8 | -41.1 |
| Essentials | Bathroom | 2 | -50.6 | -53.5 | -52.0 |
| Essentials | Flutter Echo | 2 | -54.0 | -54.2 | -50.9 |
| Essentials | Tiled Room | 1 | -62.7 | -56.6 | -49.1 |
| Essentials | Identity Blur | 3 | -34.5 | -40.7 | -40.6 |
| Essentials | Lost Space | 3 | -41.1 | -42.4 | -39.8 |
| Essentials | Rotating Speaker | 2 | -42.9 | -45.2 | -40.2 |
| Essentials | Spring Stereo | 2 | -56.6 | -55.3 | -52.6 |
| Essentials | Spring Mono | 4 | -62.7 | -68.7 | -78.6 |
| Essentials | Next Room | Off | -90.6 | -88.5 | -89.2 |
| Essentials | Dawn | 2 | -41.1 | -53.2 | -51.4 |
| Essentials | Thin Straw | 2 | -49.6 | -51.5 | -46.5 |
| Essentials | Old Radio | 3 | -34.6 | -40.7 | -41.3 |
| Essentials | Whisker Wobble | 1 | -64.0 | -58.5 | -37.2 |
| Essentials | Rough Edge | 4 | -74.4 | -78.4 | -77.5 |
| Essentials | Broken Machine | 4 | -48.4 | -61.8 | -71.4 |
| Essentials | Horror Voices | 2 | -50.0 | -52.3 | -50.2 |
| Essentials | Strange Voices | 4 | -63.8 | -64.5 | -63.9 |
| Essentials | Low Rumble (Loud) | 3 | -38.2 | -43.2 | -46.5 |
| Essentials | Fast Siren | 1 | -1.9 | -18.8 | -5.7 |
| Essentials | Springy Wobble | 2 | -48.8 | -52.5 | -50.2 |
| Essentials | Robot Voice | 2 | -53.2 | -53.9 | -51.3 |
| Essentials | Saucer (Loud) | 3 | -25.8 | -39.7 | -39.2 |
| Essentials | Phantom | Off | -68.7 | -78.7 | -83.3 |
| Essentials | Dark Haunt (Loud) | 2 | -56.3 | -57.1 | -51.4 |
| Essentials | Grainy Rain | 2 | -48.0 | -51.9 | -49.6 |
| Essentials | Telephone | 3 | -38.2 | -41.8 | -40.5 |
| Essentials | Broken Filter | 3 | -43.9 | -42.9 | -39.5 |
| Essentials | Machines Talking | 2 | -48.7 | -52.9 | -51.0 |
| Essentials | LoFi Endless Loop | 4 | -70.4 | -71.5 | -70.7 |
| Essentials | Siren Drive (Loud) | 4 | -60.9 | -66.8 | -51.6 |
| Essentials | Hard Slap | Off | -100.4 | -96.8 | -96.4 |
| Essentials | Simple Echo | Off | -64.3 | -63.1 | -73.2 |
| Studio | Bouncing Balls | Off | -64.6 | -65.1 | -72.1 |
| Studio | Top Echo | Off | -70.7 | -71.2 | -70.8 |
| Studio | Vocal Echo | Off | -100.4 | -96.8 | -96.4 |
| Studio | Beat Vocal 1 | 4 | -77.2 | -74.5 | -75.0 |
| Studio | Beat Vocal 2 | 4 | -76.1 | -73.2 | -73.7 |
| Studio | Long Vocal | 2 | -54.1 | -54.7 | -51.3 |
| Studio | Psycho Spin | 2 | -53.1 | -54.0 | -50.6 |
| Studio | Vocal 1/16D | 3 | -38.4 | -41.8 | -43.0 |
| Studio | Blur Echo | 3 | -33.9 | -42.0 | -41.4 |
| Studio | Filtered Studio Echo | Off | -70.2 | -69.9 | -70.6 |
| Studio | Studio Vocal Slap | Off | -122.9 | -112.4 | -109.2 |
| Studio | Long Slow Vocal | 3 | -35.4 | -42.4 | -42.5 |
| Studio | Vocal FX | 1 | -47.7 | -49.3 | -53.5 |
| Studio | Go-To Echo | 1 | -40.6 | -48.9 | -53.3 |
| Studio | Panning Radio | Off | -69.1 | -70.7 | -70.8 |
| Studio | Off Stage | Off | -83.2 | -97.2 | -94.1 |
| Studio | Rotor (Loud) | 4 | -65.2 | -64.6 | -63.6 |
| Studio | Quarter Note Echo | Off | -63.0 | -77.4 | -77.0 |
| Studio | Eighth Note Echo | Off | -71.4 | -81.9 | -72.4 |
| Studio | Rock Vocal Echo | Off | -66.1 | -98.6 | -76.3 |
