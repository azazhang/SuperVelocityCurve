# Super Velocity Curve

<p align="center">
  <strong>Per-pad MIDI velocity curves</strong> — customize response curves, ghost-note gates, retrigger guards, and note remapping <em>independently for every pad in your kit</em>.
</p>

<p align="center">
  <a href="https://github.com/azazhang/SuperVelocityCurve/releases/latest"><img src="https://img.shields.io/badge/version-v0.2.x--beta-blue.svg" alt="Version" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License" /></a>
  <a href="https://www.tracktion.com/develop/pluginval"><img src="https://img.shields.io/badge/pluginval-passed%20(level%205)-success.svg" alt="Verified by pluginval" /></a>
  <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey.svg" alt="Platforms" />
</p>

<p align="center">
  <a href="docs/user/install.md"><strong>📖 Install Guide</strong></a> &nbsp;|&nbsp;
  <a href="docs/user/getting-started.md"><strong>🚀 Getting Started</strong></a> &nbsp;|&nbsp;
  <a href="https://github.com/azazhang/SuperVelocityCurve/releases/latest"><strong>⬇️ Download Release</strong></a> &nbsp;|&nbsp;
  <a href="https://ko-fi.com/studioj"><strong>☕ Support Project</strong></a>
</p>

---

<p align="center">
  <a href="https://www.youtube.com/watch?v=4KFOZPDS3r8">
    <img src="https://img.youtube.com/vi/4KFOZPDS3r8/maxresdefault.jpg" alt="Super Velocity Curve Introduction Video" width="85%" style="border-radius: 8px;" />
  </a>
  <br />
  <sub>▶ <em>Click image to watch the video introduction on YouTube</em></sub>
</p>

---

## 🎯 Who is this for?

| User | What Super Velocity Curve gives you |
|:-----|:------------------------------------|
| **Finger Drummers** *(Launchpad, Maschine, FGDP, SamplePad)* | Factory controller profiles, ghost-note input gates, per-pad curves, retrigger guard |
| **E-Kit Players** routing into VST drums | Zone curves — soft rim hits, punchy kicks, independent cymbal weight |
| **Producers** with uneven pad sensitivity | Shape dynamics *before* the sampler; live histograms show actual playing strength |
| **Teachers & Demos** | Repeatable soft/loud tiers; shareable `.svcp` profile files |
| **Multi-DAW Users** | Same profile across Logic, Reaper, Bitwig, Ableton — export/import without re-tweaking |

> **Why per-pad velocity curves?** DAWs only provide one global velocity curve across your entire controller. Super Velocity Curve gives you **independent curves per pad/note**, so a soft snare ghost note doesn't force your kick drum to sound weak.

---

## ⚡ Why not just use your DAW's global curve?

| Problem | Our Approach |
|:--------|:-------------|
| **One global curve** in your DAW | **Per-pad curves** — shape ghost snares, punchy kicks, and cymbals separately |
| **Uneven hardware pads** across controllers | **Factory profiles** for GM, Launchpad 8×8, Maschine, SPD-SX, Yamaha FGDP |
| **Soft hits missing or loud hits clipping** | **Input gates** + floor/ceiling — drop or clamp out-of-range hits |
| **Double-triggering** on sensitive pads | **Per-pad retrigger suppression** (ms) |
| **Expressive kits** feel unresponsive | **Separate aftertouch curves** per pad |
| **Moving between DAWs** breaks settings | **Export/import `.svcp` profiles** |
| **MIDI 2.0 controllers** arriving | **16,384-entry LUT**, Auto / MIDI 1.0 / MIDI 2.0 output modes |

Includes **A/B comparison**, **live hit histograms**, **calibration wizard**, and **note remapping**.

---

## 📦 Download & Installation

* **Installer Guide:** [docs/user/install.md](docs/user/install.md) (step-by-step setup and security tips)
* **Direct Binary Downloads (Latest Release):**
  * 🍏 **macOS:** [SuperVelocityCurve-macOS-unsigned.zip](https://github.com/azazhang/SuperVelocityCurve/releases/latest/download/SuperVelocityCurve-macOS-unsigned.zip)
  * 🪟 **Windows:** [SuperVelocityCurve-Windows-unsigned.zip](https://github.com/azazhang/SuperVelocityCurve/releases/latest/download/SuperVelocityCurve-Windows-unsigned.zip)
* **Release Notes:** [Releases page](https://github.com/azazhang/SuperVelocityCurve/releases/latest)

---

## 🎛️ DAW Compatibility & Plugin Selection

Two builds are included in every release:
* **`Super Velocity Curve MIDI FX`**: For DAWs with native MIDI FX slots (Logic Pro, Reaper, Bitwig, Studio One).
* **`Super Velocity Curve` (Instrument)**: For DAWs that require instrument routing for 3rd-party MIDI plugins (Ableton Live, Cubase, FL Studio).

| DAW | Recommended Plugin Build | Format | Insertion Point / Routing |
|:----|:-------------------------|:-------|:--------------------------|
| **Logic Pro** | Super Velocity Curve MIDI FX | AU (`aumi`) | Track **MIDI FX** slot above instrument |
| **Reaper** | Super Velocity Curve MIDI FX | VST3, CLAP, AU | Track **Input FX** or track FX chain before instrument |
| **Bitwig Studio** | Super Velocity Curve MIDI FX | CLAP (preferred), VST3 | **Note FX** chain before instrument |
| **Studio One** | Super Velocity Curve MIDI FX | VST3 | **MIDI FX / Event FX** slot |
| **Ableton Live** | Super Velocity Curve (Instrument) | VST3, AU | **MIDI track** instrument slot; route MIDI output to drum track |
| **Cubase / Nuendo** | Super Velocity Curve (Instrument) | VST3 | **Instrument slot** on MIDI track; route MIDI output to sampler |
| **FL Studio** | Super Velocity Curve (Instrument) | VST3 | Load on **MIDI track** or in Patcher; route MIDI to sampler |
| **Standalone** | Super Velocity Curve | `.app` / `.exe` | Processed MIDI via IAC Driver (macOS) or loopMIDI (Windows) |

---

## ✨ Features

* **Per-pad velocity curves** with gate handles, presets, and floor/ceiling limiters
* **Factory profiles:** GM, Launchpad 8×8, Maschine, SPD-SX, Yamaha FGDP
* **Calibration wizard**, live histograms, A/B audition, note remapping
* **Humanization**, sample-library compensation, zone routing by pad group
* **Standalone application** with dedicated MIDI input/output selection

---

## ⚠️ Known Limitations

* **Beta Version (v0.2.x):** UI and edge-case handling are actively refined. Report issues on [GitHub Issues](https://github.com/azazhang/SuperVelocityCurve/issues).
* **macOS Unsigned Builds:** Download zip → unzip → double-click **`Install Super Velocity Curve`** (see [install guide](docs/user/install.md)).
* **Ableton / Cubase / FL Studio:** Do not support 3rd-party VST3 MIDI FX slots directly — use the **Instrument** build on a MIDI routing track.
* **MIDI 2.0:** High-resolution LUT engine is active; host UMP I/O is in development (defaults to MIDI 1.0).

---

## 🤝 Contributing & Community

* **Developers:** See [docs/README.md](docs/README.md) and [CONTRIBUTING.md](docs/developer/CONTRIBUTING.md).
* **Support the Project:** [Buy Me a Coffee](https://buymeacoffee.com/azhang) · [Ko-fi — Studio J](https://ko-fi.com/studioj)
* **License:** [MIT License](LICENSE) · [CHANGELOG.md](CHANGELOG.md)
