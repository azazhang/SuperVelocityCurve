# Changelog

## Unreleased

## v0.2.28 (2026-06-24)

### Fixed
- **UI:** Fixed a critical bug where the UI would not respond for the first 10 seconds when opened. This was caused by `juce::Timer` being initialized on background thread instantiation inside `VelocityEngine`; resolved by refactoring state cleanup to use `juce::MessageManager::callAsync`.

## v0.2.27 (2026-06-23)

### Fixed
- **Engine:** Replaced `std::shared_mutex` with lock-free atomic pointer exchange (RCU pattern) in `VelocityEngine` to resolve Logic Pro sleep/wake deadlocks on macOS.
- **DAW State Integration:** Implemented host display update triggers via `updateHostDisplay()` to ensure DAW projects correctly save/recall the plug-in state when edits are made.
- **Import/Export:** Fixed profile name synchronization on startup, state restore, and import, and reset the profile dirty baseline on import to avoid incorrect save prompts.
- **Aftertouch:** Fixed aftertouch curve merging and synchronized Floor/Ceiling inspector slider settings for the active curve target (velocity or aftertouch).

## v0.2.26 (2026-06-07)

### Fixed
- **Live MIDI display:** Hit queue now drops oldest events when full instead of silently discarding new hits during fast playing
- **Live MIDI display:** Audio thread wakes the UI via `AsyncUpdater` when hits are pending, so live readout does not depend on timer polling alone
- **Engine sync:** Profile apply uses one lock and swaps pad state atomically instead of clear + N per-pad locks (reduces audio-thread blocking)
- **Curve editing:** Pad/curve edits sync a single pad to the engine instead of re-applying the entire profile; removes duplicate full apply on mouse-up
- **Routing edits:** MIDI routing/remap changes update routing only, without rebuilding all pad curves

## v0.2.25 (2026-06-13)

### Fixed
- **Live MIDI display:** UI timer no longer stops while the window is open, so pad hits and meters keep updating without moving the mouse
- **Live MIDI display:** Drains the hit queue each tick and shows only the latest hits, preventing stale-note flash storms after idle gaps
- **Curve editing:** Velocity curve changes apply to MIDI output while dragging, not only after mouse-up

## v0.2.24 (2026-06-13)

### Fixed
- **Engine:** Pad curves/gates/retrigger now resolve on the physical MIDI note before note remap (fixes Roland SPD-SX and similar profiles)
- **Engine:** Library compensation no longer builds a `VelocityCurve` on the audio thread
- **Engine:** MIDI 2.0 wire velocity now scales to the full 16-bit range (0–65535)
- **Engine:** Note remap table supports multiple source channels for the same note
- **UI:** Note remap checkbox stays in sync when rows are added or cleared
- **UI:** Typed MIDI note values in pad settings commit immediately
- **Dev build:** Standalone target no longer fails post-build sign when `COPY_PLUGIN_AFTER_BUILD` is on

## v0.2.23 (2026-06-13)

### Fixed
- **macOS installer:** Restart `AudioComponentRegistrar` after AU install so Logic Pro discovers plug-ins in `~/Library` without copying to `/Library`
- **macOS installer:** Removed developer-only legacy bundle cleanup from the shipped user install path

### Changed
- **Dev:** One-time legacy bundle removal moved to `scripts/dev/cleanup-legacy-plugin-bundles.sh` (not shipped in release zips)
- **Docs:** Install steps tell users to quit the DAW before installing; clarify user Library is the correct plug-in location

## v0.2.22 (2026-06-13)

### Fixed
- **macOS installer:** Plug-in bundles with spaces in their names (e.g. `Super Velocity Curve.vst3`) are now copied correctly; v0.2.21 only installed the standalone `.app`
- **macOS installer:** Script no longer reports failure after a successful install when run from an already-unzipped folder

### Added
- **CI:** `verify-macos-installer.sh` runs the same install path as double-clicking **Install Super Velocity Curve** before macOS release zips are published

## v0.2.21 (2026-06-13)

### Fixed
- **macOS release zip:** Universal merge step now includes **Install Super Velocity Curve**, `install-macos.sh`, and `README.txt` (were dropped in v0.2.20 zip)

## v0.2.20 (2026-06-12)

### Fixed
- **Light theme black widgets:** Sync JUCE `LookAndFeel_V4` colour scheme with app theme; refresh slider text-box labels, toggles, and text buttons on theme change
- **Pad settings layout:** Fixed toggle text overlap; themed slider/toggle/button drawing; consistent inspector content background
- **GUI regressions (v0.2.18–19):** Removed curve static image cache; label/toggle/installer layout fixes

### Changed
- Reverted forced synchronous CoreGraphics peer (restores sharp Retina curve rendering)
- Release zip includes double-click **Install Super Velocity Curve**; user `install.md`; CI packages installer assets
- macOS install script clears quarantine and ad-hoc signs bundles

## v0.2.19 (2026-06-12)

### Fixed
- **GUI regressions from v0.2.18:** Removed curve static image cache (caused Retina blur and dark/light theme mismatch on the plot)
- **Pad settings layout:** Reverted broken custom toggle drawing; increased toggle row height; removed duplicate inspector background paint
- **Labels:** Transparent backgrounds so custom label drawing no longer paints stray dark fills

### Note
- Logic WindowServer GPU at idle remains under investigation; v0.2.18 cache experiment reverted

## v0.2.18 (2026-06-12)

### Fixed
- **Logic GPU:** Disable CALayer backing on all macOS hosts; stop UI timer completely when idle; cache static curve chrome (grid/background); draw curve dynamically during edits; partial pad hit repaints; remove per-frame hit-marker repaints and histogram idle polling
- **Curve editing:** Profile commits deferred until drag ends (no store thrash while moving control points)
- **Duplicate MIDI note:** Rejected pad edits no longer overwrite grid/curve; MIDI note commits on drag-end only
- **Light theme:** Grid contrast, custom toggle buttons, pad inspector background, status bar as proper label with auto-clear
- **Pad grid:** Hover repaints only the affected pad cell

## v0.2.17 (2026-06-12)

### Fixed
- **Logic GPU / WindowServer idle surge:** Disable CALayer-backed async view in Logic (`wantsLayerBackedView` → synchronous CoreGraphics); UI timer drops to 1 Hz when idle and only runs at 10 Hz during hit animations / pending MIDI

## v0.2.16 (2026-06-12)

### Fixed
- **Ableton AU type:** Instrument AU now registers as Music Device (`aumu`) instead of Music Effect (`aumf`)
- **Ableton VST3 GUI:** Pad canvas background fill, contrasting pad labels in Light mode, forced text-editor/label drawing in custom L&F, host scale-factor + visibility theme refresh
- **Ableton plugin scan:** Ad-hoc deep `codesign` on local install + post-build (fixes “code has no resources” scanner failures)
- **Light theme:** Stronger primary/muted text contrast

### Note
- **Ableton + MIDI FX VST3:** Live does not support VST3 MIDI-effect plugins; use `Super Velocity Curve.vst3` (Instrument) on a MIDI track

## v0.2.14 (2026-06-12)

### Fixed
- **Logic freeze on open:** Removed MIDI-thread UI wake + `sendLookAndFeelChange` storms; deferred heavy editor startup to next message-loop tick
- **Light theme text:** `applyColourToAllText` on profile/pad name fields; combo theming helper
- **Theme + cache:** Buffered views invalidated on appearance change (pads/curve stay light in Light mode)

### Changed
- UI poll timer: steady 4 Hz while visible (was start/stop logic that could stall Logic)
- Direct download URLs documented (`releases/latest/download/...`)

## v0.2.13 (2026-06-12)

### Fixed
- **GPU:** Removed `setBufferedToImage` (was caching stale dark pixels and adding compositor layers); flat fills instead of per-frame gradients on pads/curve/background
- **Light theme:** Pads and curve plot now repaint with the active theme; combo/text editor colours fixed in pad settings

## v0.2.12 (2026-06-12)

### Fixed
- **GPU / idle CPU:** Editor UI timer now stops when idle (wakes on MIDI hits only); curve/pad/histogram views cache to buffered images
- **Light theme:** Form labels, combo text, and slider value boxes use dark primary text; stronger contrast palette

## v0.2.11 (2026-06-12)

### Changed
- **Sample-library compensation:** UI renamed from “Library compensation / Library mix” to clarify generic VI velocity shapes (not a preset library)

## v0.2.10 (2026-06-12)

### Fixed
- **A/B audition:** Blue curve line now matches the curve you hear when toggling Hear A / Hear B
- **GPU / WindowServer:** Idle editor no longer repaints pad hits every frame; UI timer throttles when nothing is animating
- **About:** Encoding glitch in description text; theme colours refresh when opened
- **Light theme:** Darker secondary/muted text for hints, routing labels, and live-hit readout

### Changed
- MIDI routing **Library blend amount** renamed **Library mix** with tooltip explaining the compensation mix
- `install-macos.sh` removes legacy plugin bundle names before install

## v0.2.9 (2026-06-10)

### Changed
- **Brand:** User-facing name standardized to **Super Velocity Curve** (three words) — plugin bundles, DAW list names, docs, About panel
- README marketing: audience hooks (finger drummers, e-kits, producers, teachers, multi-DAW)
- `Source/Config/AppBranding.h` + `scripts/check-branding.sh`

## v0.2.8 (2026-06-10)

### Fixed
- **Windows CI:** `HistogramBank` per-pad storage heap-allocated (~2 MiB was overflowing the 1 MiB default stack when tests stack-allocated `VelocityEngine`)
- **Layout tests:** JUCE GUI initialiser required on headless Windows runners

## v0.2.7 (2026-06-10)

### Changed
- User-facing README: consistent DAW table (Reaper AU on macOS), known limitations, support links; versioning/changelog at footer only
- `docs/README.md` doc map; agent docs (`AGENTS.md`) separated from musician path

### Fixed
- Launchpad pad cells: note name and channel on separate lines (no truncation)
- Live hits label persists ~4 s after last hit (readable in→out)
- Edit AT curve button toggles back to velocity curve
- About developer line encoding (`azazhang / Studio J`); Ko-fi support link
- Calibration wizard resets on expand; validates hit separation; proper preview layout
- Histogram legend explains 60–90 velocity band

### Added
- [docs/developer/AGENTIC_QA.md](docs/developer/AGENTIC_QA.md) — agent scenario packs for UI regression

## v0.2.6 (2026-06-10)

### Fixed
- Pad grid vertical scrollbar appears immediately after profile load (not only after window resize)
- Idle editor CPU/GPU: meter repaints only when active; histograms refresh on hits (not every timer tick)
- MIDI note shows value and name (e.g. `36 (C1)`) in pad grid and inspector
- Spurious “Unsaved profile changes” dialog from `profileBox.clear()` during list rebuild; template profile-name field no longer marks template dirty
- MIDI channel changes apply without full engine rebuild on every intermediate slider tick
- Profile switch no longer double-refreshes UI (suppressed redundant `onProfileChanged` during programmatic switch)

### Added
- Layout tests: vertical scrollbar after `setProfile`, MIDI note display format

## v0.2.5 (2026-06-10)

### Fixed
- **Zone routing:** `NoteOff` and polyphonic aftertouch use the same output channel as the matching `NoteOn` (fixes hanging notes)
- **Velocity gate:** Suppress orphan `NoteOff` when the paired `NoteOn` was dropped by the gate
- **HistogramBank:** Replace per-pad `unordered_map` with fixed `[16×128]` slots (audio/UI thread safety)
- **Retrigger guard:** Lock-free per-note timestamps on the audio thread (no `unique_lock` on `padMutex`)

### Added
- Engine regression tests: zone-routing note-off channel, gated note-off suppression, retrigger note-off passthrough

## v0.2.4 (2026-06-10)

### Fixed
- **pluginval state crash:** Editor destructor now clears `onProfileChanged`; `setStateInformation` no longer notifies UI from a possibly-destroyed editor (async `syncFromProcessorState` when editor is open)
- **iCloud builds:** `validate-plugins-local.sh` strips extended attributes before pluginval on macOS bundles

### Added
- Layout test: state restore after editor destroyed (pluginval regression)

## v0.2.3 (2026-06-10)

### Added
- **Layout/UI automated tests** (`LayoutTests`): min-window curve bounds, collapsible sections, pad curve/inspector merge, Launchpad horizontal scroll, headless editor layout
- `EditorLayout` / `PadUiMerge` — testable layout and pad-state merge logic
- **CI-3:** Release workflow builds `arm64` + `x86_64` in parallel, merges with `scripts/lipo-macos-universal.sh`

### Changed
- `PluginEditor::resized()` uses shared `EditorLayout` computation
- `qa-iterate.sh` runs layout tests via ctest; `--pluginval` also runs clap-validator

## v0.2.2 (2026-06-10)

### Fixed
- **MIDI 2 architecture:** `MidiVelocityTransport` — normalized curve mapping, explicit MIDI 1 wire encoding, parallel UMP `PacketX2` note-on output in MIDI 2 mode
- **CLAP CI:** `clap-validator` replaces pluginval for CLAP (macOS bundle + Windows flat DLL); pluginval does not support CLAP
- **Docs:** BACKLOG/README version sync after 0.6.x → 0.2.x reversion; Intel universal build root-cause in `CI.md`; [WINDOWS_TESTING.md](docs/developer/WINDOWS_TESTING.md)

### Added
- Engine tests: MIDI 2 UMP encoding, MIDI 1 wire quantize, `scripts/validate-clap.sh`

### Known limitation
- JUCE `processBlock` / `MidiBuffer` remains MIDI 1 bytestream; UMP packets are produced for MIDI 2 mode and await host/CLAP MIDI-2 I/O wiring

## v0.2.1 (2026-06-10)

### Fixed (QA audit — 13 items)
- Gate leakage / right-gate discontinuity: analytical `evaluateMappedOutput` (no LUT interpolation bleed)
- Polyphonic aftertouch note remap; channel pressure no longer hijacked by MIDI note 0 pad
- Note remap table row recycling (`rowIndex` updated on reuse)
- ComboBox dropdown arrows in custom LookAndFeel
- Floor/ceiling inspector sliders stay in sync
- Curve control handles align with floor/ceiling-scaled plot
- Library blend slider label in MIDI routing panel
- Calibration wizard only captures hits when section expanded + selected pad
- DAW state restore notifies editor (`fromValueTree(..., true)`)
- Standalone MIDI output queued off audio thread (no `sendMessageNow` in `processBlock`)

### Known limitation (unchanged)
- JUCE `MidiMessage` output velocity remains 7-bit; MIDI 2 mode shapes float path but host wire format may quantize

## v0.2.0 (2026-06-09)

### Versioning
- Reset from 0.6.x → **0.2.0** to reflect beta quality ([VERSIONING.md](docs/developer/VERSIONING.md)).

### Fixed
- Profile switch dialog: replaced native `AlertWindow` (stuck/crash in hosts) with in-plugin modal
- Live hit marker plots on **current curve** after edits (engine label still shows actual MIDI out)
- Curve preset change syncs engine immediately

### Docs
- README / getting-started: Logic **AU only**; expanded DAW table (Reaper, Bitwig, FL, Cubase, Studio One)
- [RELEASE.md](docs/developer/RELEASE.md): why GitHub Releases was empty; manual workflow_dispatch

## v0.6.8

### Fixed
- Inspector floor/ceiling sliders no longer overwritten by curve-editor merge on live edit
- Profile switch / save / export now commit active pad UI; discard on dirty switch reloads saved snapshot
- Profile MIDI duplicate validation on save, export, and session restore (skips invalid user profiles)
- Calibration section height + histogram panel uses full strip height
- Collapsible section header height consistency (26px)

### Added
- **U4:** Unsaved profile dialog on profile switch (Save / Discard / Cancel)
- **U2:** Drag bottom grip on collapsible sections to resize height
- `scripts/qa-iterate.sh` — build + ctest + smoke checklist gate
- `scripts/validate-plugins-local.sh` — RelWithDebInfo artefact paths
- CI-7: PR jobs skip pluginval; `nightly.yml` full pluginval schedule
- Engine tests: floor/ceiling mapping, profile MIDI key validation
- About panel: developer portrait + azazhang · Studio J
- Curve live-hit crosshair, linear reference, in→out label

## v0.6.7

### Fixed
- **P0:** Curve edits lost when switching pads — `commitInspectorEdits()` saved inspector's stale curve over curve-editor state
- **P0:** Pad switching / curve reset cascade from the same bug + per-drag full engine rebuild
- Pad grid horizontal scrollbar restored (Launchpad 8-column layouts)
- Performance: removed 20Hz unconditional curve repaint; decimate LUT draw to 256 steps; defer engine sync until drag ends
- Buy Me a Coffee URL → `https://buymeacoffee.com/azhang`

### Docs
- `CONTRIBUTING.md`: explains multiple `*_artefacts` folders and RelWithDebInfo vs Release

## v0.6.6

### Fixed
- **Critical:** Curve editor was invisible at default size — layout gave ~0px to the curve after fixed tabs/histogram/inspector heights
- Collapsible sections: histograms, MIDI routing/remap, calibration (collapsed by default); pad settings on the right

### Added
- App icon (`Resources/AppIcon.png`) for Standalone and plugin bundles
- `clap-info` CI smoke for macOS/Windows CLAP (when available)
- Engine test: deterministic MIDI velocity replay

## v0.6.5

### Fixed
- UI: scrollbar track artifacts (transparent background + overflow-only scrollbars)
- UI: clipped curve editor header, pad name field padding, taller Pad Settings panel
- UI: zone routing two-column layout (fixes stray “Keep” text at tab bottom)
- Title/subtitle header labels laid out correctly (BUG-005)
- Curve preset toolbar wraps at min window size (BUG-004)
- Import rejects profiles with duplicate MIDI note+channel (BUG-003)
- Local builds auto-install CLAP to `~/Library/Audio/Plug-Ins/CLAP/`

## v0.6.3 (unreleased)

### Fixed
- **CI root cause:** macOS jobs timed out during compile (~90m), not pluginval — disabled LTO in CI, Ninja + ccache, RelWithDebInfo, cancel-in-progress concurrency
- Windows CLAP pluginval skip retained; artefact paths use `${CMAKE_BUILD_TYPE}`

### Added
- [docs/developer/CI.md](docs/developer/CI.md) — CI architecture and failure audit

## v0.6.2

### Added
- **Add pad** / **Delete pad** on Pad Layout — build kits from Blank Custom or trim factory templates
- Save-in-place for `[My]` profiles; Save on factory template creates a new user copy
- About panel (version, pluginval badge, links)
- Per-group zone routing channel combos; scrollable Routing tab
- Dynamic pad grid columns (8 for Launchpad, 4 for other profiles)
- Engine tests for pad add/remove and duplicate MIDI key rejection
- QA docs: `docs/developer/TESTING.md`, pluginval exception notes

### Fixed
- A/B audition stays in sync after profile/engine updates (BUG-001)
- Pad inspector no longer clears live hit flashes (BUG-002)
- Copy curve no longer breaks pad selection
- Live hits row vs tabs layout overlap; tab panel height
- Launchpad 8×8 grid overlap

## v0.6.0

### Added
- Logic-inspired dark theme + optional light theme (Appearance menu)
- Input gate handles: horizontal input range + vertical output at gates
- User docs: [Install](docs/user/install.md), [Getting started](docs/user/getting-started.md)
- GitHub Releases workflow (tag `v*.*.*`)
- `AGENTS.md` for AI agents; developer docs under `docs/developer/`

### Fixed
- Toolbar layout overlap (Routing / Remap / MIDI meters) — tabbed panel
- Endpoint handles restore vertical drag while keeping input gating
- Standalone `.app` bundle timestamp refreshed on rebuild (CMake)

## v0.5.3
- Windows pluginval: quote VST3 paths containing spaces

## v0.5.0
- Dual VST3/AU builds, CLAP, MIDI 2.0 LUT, factory `.svcp` exports

## v0.4.0
- Full audit backlog: remap UI, histograms, calibration, standalone MIDI I/O
