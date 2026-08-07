# Install Super Velocity Curve

## Mac

### Step 1 — Download

1. Open [Releases](https://github.com/azazhang/SuperVelocityCurve/releases/latest).
2. Download **`SuperVelocityCurve-macOS-unsigned.zip`**.

### Step 2 — Unzip

Double-click the zip file. Open the folder that appears.

### Step 3 — Install

1. **Quit your music app** (for example **Logic Pro → Quit Logic Pro**).
2. Double-click **`Install Super Velocity Curve`**.
3. Wait until the window says installation finished.
4. Press **Return** to close the window.

Plug-ins go in **your user Library** (`~/Library/Audio/Plug-Ins/…`). You do not need to copy them to the system Library folder.

If Mac blocks the installer the first time:

1. Open **System Settings** → **Privacy & Security**.
2. Click **Open Anyway** next to the blocked installer message.
3. Double-click **`Install Super Velocity Curve`** again.

### Step 4 — Open your DAW

1. Open your music app.
2. Rescan plug-ins if prompted (Logic: **Settings → Plug-in Manager → Reset & Rescan Selection**).

### Step 5 — Add the plugin

| DAW | Recommended Target Build | Format / Location |
|-----|--------------------------|-------------------|
| **Logic Pro** | Super Velocity Curve MIDI FX | Track → **MIDI FX** slot (AU) |
| **Reaper, Bitwig, Studio One** | Super Velocity Curve MIDI FX | Track Input FX / Note FX / MIDI FX slot (CLAP or VST3) |
| **Ableton Live, Cubase, FL Studio** | Super Velocity Curve (Instrument) | Instrument slot on MIDI track; route MIDI output to drum sampler |
| **Without a DAW (Standalone)** | Super Velocity Curve | **Applications** (macOS) / `Super Velocity Curve.exe` (Windows) |

See [Getting started](getting-started.md) for step-by-step DAW routing guides and first-use workflow.

---

## Windows

1. Download **`SuperVelocityCurve-Windows-unsigned.zip`** from [Releases](https://github.com/azazhang/SuperVelocityCurve/releases/latest).
2. Unzip the file.
3. Copy the folder **`Super Velocity Curve.vst3`** to `C:\Program Files\Common Files\VST3\`.
4. Copy **`Super Velocity Curve MIDI FX.vst3`** to the same folder if your DAW supports 3rd-party MIDI FX (Reaper, Bitwig, Studio One, etc.).
5. Copy **`Super Velocity Curve MIDI FX.clap`** to `C:\Program Files\Common Files\CLAP\` if your host uses CLAP.
6. If Windows shows a security warning: right-click the unzipped folder → **Properties** → check **Unblock** → OK.
7. Quit your DAW, reopen it, and rescan plug-ins.

---

## Manual install (Mac)

If you prefer to copy files manually without running the automatic installer:

| Folder in the zip | Destination |
|-------------------|-------------|
| `Super Velocity Curve.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `Super Velocity Curve MIDI FX.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `Super Velocity Curve.component` | `~/Library/Audio/Plug-Ins/Components/` |
| `Super Velocity Curve MIDI FX.component` | `~/Library/Audio/Plug-Ins/Components/` |
| `Super Velocity Curve MIDI FX.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `Super Velocity Curve.app` | `/Applications` |

In Finder, press **Cmd+Shift+G** and paste `~/Library/Audio/Plug-Ins/VST3/` to open the user plug-ins folder.

> **Note on macOS unsigned builds:** macOS requires removing Gatekeeper quarantine flags and refreshing the Audio Component registrar cache for newly copied plugins.  
> Run **`Install Super Velocity Curve`** from the unzipped folder, or run the following terminal command to un-quarantine manually:
> ```bash
> xattr -cr ~/Library/Audio/Plug-Ins/VST3/Super* ~/Library/Audio/Plug-Ins/Components/Super* ~/Library/Audio/Plug-Ins/CLAP/Super*
> killall -9 AudioComponentRegistrar 2>/dev/null || true
> ```

---

## Troubleshooting

**Logic Pro does not list the plug-in**  
Ensure you load **Super Velocity Curve MIDI FX** in the track **MIDI FX** slot (not the instrument slot). Quit Logic Pro completely, run **`Install Super Velocity Curve`** (which clears Gatekeeper flags and restarts macOS `AudioComponentRegistrar`), reopen Logic, then go to **Settings → Plug-in Manager → Reset & Rescan Selection**. Plug-ins belong in your user Library (`~/Library/Audio/Plug-Ins/`).

**Ableton Live, Cubase, or FL Studio does not load the MIDI FX build**  
DAWs like Ableton Live, Cubase, and FL Studio do not support third-party VST3 MIDI FX plugins in native MIDI insert slots. Load the **`Super Velocity Curve` (Instrument)** build on a MIDI track instead, and route its MIDI output into your drum sampler track.

**Windows DAW does not detect the VST3 plugin**  
Ensure you copy the *entire* `.vst3` folder (e.g. `Super Velocity Curve MIDI FX.vst3`), not individual files from inside it, into `C:\Program Files\Common Files\VST3\`. If Windows blocked the zip archive, right-click the zip/folder → **Properties** → check **Unblock** → **OK**.

**Plugin missing or showing an old version**  
Download a fresh zip from [Releases](https://github.com/azazhang/SuperVelocityCurve/releases/latest), unzip, run **`Install Super Velocity Curve`**, quit your DAW completely, and trigger a plug-in rescan.

**Mac blocks installer ("cannot be opened because it is from an unidentified developer")**  
Open **System Settings → Privacy & Security**, scroll down to Security, and click **Open Anyway** next to the blocked installer notice. Alternatively, right-click (or Control-click) **`Install Super Velocity Curve`** and select **Open**.

**Still stuck?**  
[Open an issue on GitHub](https://github.com/azazhang/SuperVelocityCurve/issues) with your OS version, DAW, and what happened.
