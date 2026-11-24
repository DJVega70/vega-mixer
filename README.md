# Vega Productions Mixer

A standalone Windows mixing console with recording, in the Vega Productions DAW look.
8 input channels, Reverb and Delay returns, a master bus with a tape machine, multitrack
recording, and lossless export to WAV, FLAC and AIFF.

## Get the app
1. Push this folder to GitHub (repo `DJVega70/vega-mixer`).
2. Open the repo's **Actions** tab, then click the latest **Build Windows** run.
3. Under **Artifacts**, download **VegaProductionsMixer-Windows**, unzip it and run `Vega Productions Mixer.exe`.

## Quick start
1. Click **AUDIO** (top right). Pick your interface (e.g. *Windows Audio* + MiniFuse 2) and tick the inputs you want.
2. On each strip, set **INPUT** to a mono input (`In 1`) or a stereo pair (`In 1-2`).
3. Press **Rec** on the channels you want to record. **REC MIX** on the master also records the stereo mix.
4. Press **REC** in the top bar (or the space bar). Press **STOP** when you're done.
   Takes go to `Documents\Vega Mixer\Takes\Take <date time>\` as 32-bit float WAV.
5. Click **EXPORT** to convert takes to **WAV 16/24/32f, FLAC 16/24, or AIFF 16/24**. You can also normalise and add dither.

## The strip, top to bottom
| Section | What it does |
|---|---|
| INPUT | Input source. On the Reverb and Delay returns it picks the reverb type or delay time. |
| CONSOLE | VU meter (0 VU = -18 dBFS). **Click the VU** for Trim, phase and low/high cut. On returns, click it for the reverb or delay settings. HI/MID/LO = console tone (10 kHz / 1 kHz / 100 Hz). |
| TAPE | Channel tape saturation (Off / 15 ips / 30 ips). |
| INSERTS | 3 slots. Choose **Vega EQ** (4-band parametric), **Vega Comp**, or **Vega Gate** (gate/expander). They run top to bottom. Click a slot to edit it, right-click to change, bypass or remove it. Bypassed slots show grey. |
| SENDS | Post-fader send to the Reverb or Delay return. The cyan bar under it sets the send level. |
| OUTPUT | The master bus, or a direct hardware output pair (Out 3-4, ...). |
| REC POST / REC DRY | Record the processed channel or the raw input. |
| Pan, Rec, S, M, fader | Double-click the dB box to type a level. Double-click the name to rename it. Right-click the name for colour. |

**Master:** the output pair, console VU, and TAPE (switches the tape machine on/off and sets its speed). Inserts are Vega EQ, Bus Comp and Limiter. REC MIX turns mix recording on or off. M mutes the monitor output, but recording continues.

**Machine panel:** master tape machine. Formula 456/499/GP9 sets how much saturation you get. DRIVE, IPS (15/30) and CAL (+3/+6/+9) adjust the tone, and **On** / **Bypass** give you A/B. **PRESET** saves, renames and deletes whole-mixer presets (three factory presets are included).

**BPM** (top bar): drag up or down, or double-click to type. It sets the delay sync. **Ctrl +/-/0** zooms. The app saves your mix and audio setup automatically when you close it.

## Recording what Windows is playing (YouTube, Spotify, browser...)
1. On any strip, set **INPUT** to **System Audio (what Windows plays)**.
2. Arm that strip (**Rec**) and press **REC**.

On Windows 10 (2004 or later) and Windows 11, the mixer's own output is left out of the capture. That means you can monitor on the same speakers without feedback.
On older Windows it captures everything, so mute the monitor (**M** on the master) while recording.
The status bar shows which mode is active. Only record audio you're allowed to use.

## Build locally
```
cmake -B build -A x64
cmake --build build --config Release
```
Headless engine test: `cmake -B build -DVEGA_BUILD_TESTS=ON`, then run `VegaEngineTest`.

Fonts: Barlow Condensed and Share Tech Mono (SIL Open Font License).
