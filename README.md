# Super Velocity Curve

**Per-pad MIDI velocity curves** — when your DAW’s single global curve is not enough.

Free and open source. Built for **pad grids, drum layouts, and e-kits**, giving you **independent velocity curves, gates, and dynamics per note**: customize ghost snares, punchy kicks, cymbal weights, and pad sensitivity individually across your kit.

> **Status: beta (v0.2.x)** — core workflow works; expect rough edges. [Known limitations](#known-limitations) below.  
> <a href="https://www.tracktion.com/develop/pluginval"><img src="https://assets.tracktion.com/img/pages/develop/develop-logo-pluginval.png" alt="Verified by pluginval" width="110" align="right" /></a> **Quality:** Verified by pluginval at strictness 5 (VST3/AU in CI).

<iframe width="100%" height="415" src="https://www.youtube.com/embed/4KFOZPDS3r8" title="Super Velocity Curve Introduction Video" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

*Watch the [introduction video on YouTube](https://www.youtube.com/watch?v=4KFOZPDS3r8).*

## Support the project

If this helps your playing or teaching:

- [Buy Me a Coffee](https://buymeacoffee.com/azhang)
- [Ko-fi — Studio J](https://ko-fi.com/studioj)

## Who is this for?

| You | What you get |
|-----|----------------|
| **Finger drummers** (Launchpad, Maschine, FGDP, SamplePad) | Factory layouts, per-pad curves, ghost-note gates, retrigger guard |
| **E-kit players** routing into VST drums | Zone curves — soft rims, punchy kicks, different cymbal weights |
| **Producers** with uneven pad sensitivity | Shape dynamics *before* the sampler; histograms show what you actually play |
| **Teachers & demo** | Repeatable soft/loud tiers; profiles you can share as `.svcp` files |
| **Multi-DAW users** | Same profile in Logic, Reaper, Bitwig, Ableton — export/import, not re-tweaking |

**Velocity curve** is the term musicians already know. Super Velocity Curve means **independent per-pad velocity curves instead of one uniform curve for your whole controller**.

## Why not just use my DAW curve?

| Problem | Our approach |
|---------|----------------|
| One global velocity curve in your DAW | **Per-pad curves** — ghost snares, punchy kicks, separate cymbal weight |
| Pads feel different on Launchpad vs Maschine vs SPD-SX | **Factory profiles** for real layouts (GM, Launchpad 8×8, Maschine, SPD-SX, Yamaha FGDP) |
| Soft hits disappear or loud hits clip | **Input gates** + floor/ceiling — drop or clamp out-of-range hits |
| Double-triggering on sensitive pads | **Per-pad retrigger guard** (ms) |
| Aftertouch feels wrong on expressive kits | **Separate aftertouch curves** per pad |
| Moving between DAWs breaks your setup | **Export/import `.svcp` profiles** |
| MIDI 2.0 controllers arriving | **16384-entry LUT**, Auto / MIDI 1.0 / MIDI 2.0 output modes |

**A/B compare**, **live histograms**, **calibration wizard**, and **note remap** round out the workflow.

## Download & install

**→ [Install guide](docs/user/install.md)** — copy plugins, rescan, unsigned-build tips.

**Direct download (latest release binaries):**
- macOS: [SuperVelocityCurve-macOS-unsigned.zip](https://github.com/azazhang/SuperVelocityCurve/releases/latest/download/SuperVelocityCurve-macOS-unsigned.zip)
- Windows: [SuperVelocityCurve-Windows-unsigned.zip](https://github.com/azazhang/SuperVelocityCurve/releases/latest/download/SuperVelocityCurve-Windows-unsigned.zip)

Release notes & assets: [Releases page](https://github.com/azazhang/SuperVelocityCurve/releases/latest)

## Getting started

**→ [Getting started guide](docs/user/getting-started.md)** — per-DAW routing, curve editor, profiles, save/export.  
**→ [Watch intro video](https://www.youtube.com/watch?v=4KFOZPDS3r8)** — video walkthrough of setup and per-pad curve editing.

## Which plugin do I need?

Two builds per release: **Instrument** (for DAWs without 3rd-party MIDI FX support, e.g. Ableton, Cubase, FL Studio) and **MIDI FX** (for DAWs with native MIDI FX slots, e.g. Logic, Reaper, Bitwig).

| Format | Instrument | MIDI FX | Platforms |
|--------|------------|---------|-----------|
| VST3 | ✓ | ✓ | macOS, Windows |
| AU | ✓ | ✓ | macOS only |
| CLAP | — | ✓ | macOS, Windows |
| Standalone | ✓ | — | macOS app, Windows exe |

**Why two builds?** DAWs handle MIDI plugins differently. Hosts like Logic Pro, Reaper, and Bitwig load MIDI FX directly in the track signal chain. Hosts like Ableton Live, Cubase, and FL Studio do not support third-party VST3 MIDI FX slots directly — in those DAWs, load the **Instrument** build on a MIDI track and route its MIDI output to your sampler or drum instrument.

| DAW | Recommended Plugin Build | Format | How to load / route |
|-----|-------------------------|--------|---------------------|
| **Logic Pro** | Super Velocity Curve MIDI FX | AU (`aumi`) | MIDI FX slot above instrument |
| **Reaper** | Super Velocity Curve MIDI FX | VST3, CLAP, AU | Track Input FX or track FX chain before instrument |
| **Bitwig Studio** | Super Velocity Curve MIDI FX | CLAP (preferred), VST3 | Note FX chain before instrument |
| **Studio One** | Super Velocity Curve MIDI FX | VST3 | MIDI FX / Event FX slot |
| **Ableton Live** | Super Velocity Curve (Instrument) | VST3, AU | MIDI track instrument slot; route MIDI output to drum track |
| **Cubase / Nuendo** | Super Velocity Curve (Instrument) | VST3 | Instrument slot on MIDI track; route MIDI output to sampler |
| **FL Studio** | Super Velocity Curve (Instrument) | VST3 | Load on MIDI track / Patcher; route MIDI to sampler |
| **Standalone** | Super Velocity Curve | app / exe | Outputs processed MIDI via IAC Driver (macOS) or loopMIDI (Windows) |

## Features

- Per-pad velocity curves with gate handles, presets, floor/ceiling
- Factory profiles: GM, Launchpad 8×8, Maschine, SPD-SX, Yamaha FGDP
- Calibration wizard, histograms, A/B compare, note remap
- Humanize, sample-library compensation, zone routing by pad group
- Standalone app with MIDI in/out

## Known limitations

| Area | What to expect |
|------|----------------|
| **Quality** | v0.2.x — UI and edge cases still improving; report issues on GitHub |
| **macOS installs** | Download zip → unzip → double-click **Install Super Velocity Curve** (see [install](docs/user/install.md)) |
| **Ableton** | No third-party MIDI FX slot — use the **VST3 Instrument** build |
| **MIDI 2.0** | High-res LUT built in; **host UMP I/O** not wired yet — most setups use MIDI 1.0 |
| **Host coverage** | pluginval checks load/stability; full DAW smoke on every host is not complete |

## Contributing

Developers: [docs/README.md](docs/README.md) · [CONTRIBUTING.md](docs/developer/CONTRIBUTING.md).

## License

MIT — see [LICENSE](LICENSE).

---

**Version & changelog:** [CHANGELOG.md](CHANGELOG.md) · [Versioning policy](docs/developer/VERSIONING.md)
