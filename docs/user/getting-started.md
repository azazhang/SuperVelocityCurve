# Getting started

**Super Velocity Curve** provides **per-pad (per-note) MIDI velocity curves** — allowing you to shape velocity curves, input gates, retrigger suppression, and note remapping **independently for every pad in your kit**, rather than applying a single global curve across your entire controller.

> **Video Tutorial:** Watch the [video introduction on YouTube](https://www.youtube.com/watch?v=4KFOZPDS3r8) for a visual walkthrough of installation, pad selection, and per-pad curve editing.

## Quick workflow

1. **Load the plugin** in your DAW ([Install guide](install.md)).
2. Pick a **factory profile** matching your controller (GM, Launchpad 8×8, Maschine, Yamaha FGDP, etc.).
3. Click a **pad** in the Pad Layout grid (or hit the physical pad on your controller).
4. In **Pad Settings**, configure **name**, **MIDI note**, and **channel** to match your hardware layout.
5. Drag the **velocity curve** control points — gold/violet gate handles on left/right set input cutoff ranges.
6. Play pads — **live hits** appear dynamically on the curve, and **histograms** monitor your playing dynamics.

## Which plugin build should I use?

DAWs handle MIDI processing differently. Refer to the table below to choose between the **MIDI FX** build (native track MIDI FX slot) and the **Instrument** build (MIDI routing track).

| DAW | Recommended Build | Format | Where to load / How to route |
|-----|-------------------|--------|------------------------------|
| **Logic Pro** | Super Velocity Curve MIDI FX | AU (`aumi`) | MIDI FX slot directly above software instrument |
| **Reaper** | Super Velocity Curve MIDI FX | VST3, CLAP, AU | Track Input FX or track FX chain before instrument |
| **Bitwig Studio** | Super Velocity Curve MIDI FX | CLAP (preferred), VST3 | Note FX chain before instrument |
| **Studio One** | Super Velocity Curve MIDI FX | VST3 | MIDI FX / Event FX slot |
| **Ableton Live** | Super Velocity Curve (Instrument) | VST3, AU | MIDI track instrument slot; route MIDI output to drum track |
| **Cubase / Nuendo** | Super Velocity Curve (Instrument) | VST3 | MIDI track instrument slot; route MIDI output to sampler track |
| **FL Studio** | Super Velocity Curve (Instrument) | VST3 | MIDI track / Patcher; route MIDI output to sampler |
| **Standalone** | Super Velocity Curve | app / exe | Outputs processed MIDI via IAC Driver (macOS) or loopMIDI (Windows) |

---

## Ableton Live

Ableton Live does not support third-party plugins in its native MIDI FX slot.

1. Create a MIDI track with **`Super Velocity Curve.vst3`** (Instrument).
2. On your **drum track** (e.g. Drum Rack / Kontakt), set **MIDI From** → the Super Velocity Curve track.
3. Super Velocity Curve processes incoming MIDI velocity and sends the shaped MIDI directly to your drum track.

## Logic Pro

1. On a software instrument track, open the **MIDI FX** slot (above the instrument).
2. Load **`Super Velocity Curve MIDI FX.component`** (AU).
3. Your drum sampler (Kontakt, Drum Kit Designer, etc.) stays in the instrument slot.

## Reaper

1. Add **`Super Velocity Curve MIDI FX`** as a **track input FX** (or container FX before the instrument).
2. **macOS:** AU `.component`, VST3, or CLAP (`Super Velocity Curve MIDI FX.clap`).
3. **Windows:** VST3 or CLAP.
4. Ensure MIDI reaches the track before the drum plugin.

## Bitwig Studio

1. On a note lane, add **`Super Velocity Curve MIDI FX`** as a **Note FX**.
2. CLAP is the preferred format on Bitwig; VST3 also works.

## Cubase / Nuendo

Cubase does not support third-party VST3 MIDI FX plugins directly on MIDI insert slots.

1. Create an Instrument track with **`Super Velocity Curve.vst3`** (Instrument).
2. On your sampler / drum track (Groove Agent, Kontakt, etc.), set MIDI Input → **Super Velocity Curve - Out**.
3. Alternatively, run **Super Velocity Curve Standalone** and route MIDI via IAC Driver (Mac) or loopMIDI (Windows).

## FL Studio

FL Studio does not support third-party VST3 MIDI FX plugins directly on the Channel Rack.

1. Add **`Super Velocity Curve.vst3`** (Instrument) or load it inside **Patcher**.
2. Route the output MIDI port from Super Velocity Curve into your FPC or drum sampler channel.

## Pad setup

Each pad maps incoming MIDI (note + channel) to its own curve.

1. Select a pad in **Pad Layout**.
2. In **Pad Settings**, edit name, MIDI note (0–127), channel.
3. **Add pad** / **Delete pad** to build a custom kit.
4. Play the pad — grid flash + curve hit marker should respond.

### Save your work

- **Save Profile** — stores under `[My] …` (factory templates are read-only until saved).
- Switching profiles with unsaved edits prompts **Save / Discard / Cancel**.
- **Export** / **Import** `.svcp` files for backup and sharing.

## A/B compare

1. **Capture A** → edit curve → **Hear A** toggles audition.
2. Gold overlay = alternate curve.

## Collapsible panels

- **> Histograms** — per-pad and global velocity
- **> MIDI routing & remap** — channel filter, humanize, note remap
- **> Calibration wizard** — curve from live playing
- **v Pad settings** — right column; drag section bottom edge to resize

## Curve editor tips

- **Gold / violet gate handles** — input range and output at gates.
- **Double-click** add point; **right-click** remove.
- Live hits show a crosshair on the curve with **in→out** velocity label.

## Virtual MIDI (Standalone)

- **macOS:** Audio MIDI Setup → **IAC Driver**.
- **Windows:** [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html).
