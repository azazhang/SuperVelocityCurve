# Design Specification: Pad Layout Redesign (Seamless Canvas Manipulation)

**Date**: 2026-09-18  
**Topic**: Pad Layout Redesign — Direct Canvas Manipulation & Seamless Inline Editing  
**Target Files**:
- `Source/Profiles/ControllerProfile.h`, `Source/Profiles/ControllerProfile.cpp`
- `Source/Profiles/ProfileStore.h`, `Source/Profiles/ProfileStore.cpp`
- `Source/UI/PadGridComponent.h`, `Source/UI/PadGridComponent.cpp`
- `Source/UI/PadInspectorComponent.h`, `Source/UI/PadInspectorComponent.cpp`
- `Source/Plugin/PluginEditor.h`, `Source/Plugin/PluginEditor.cpp`
- `Tests/ProfileStoreTests.cpp`

---

## 1. Overview & Objectives

The current Pad Layout section in Super Velocity Curve displays pads in a grid with basic selection, but lacks direct manipulation capabilities:
- Editing is bifurcated into a side panel without in-place editing.
- Reordering or swapping pads is not supported.
- Adding and deleting pads relies on prominent legacy buttons (`Add pad`, `Delete pad`) in a separate toolbar rather than natural canvas interactions.

This redesign implements modern pro-audio UX principles (matching Ableton Drum Rack and Logic Drum Machine Designer):
1. **Zero "Edit Mode" friction**: No toggling between "Performance" and "Edit" modes. All editing actions (drag-to-swap, duplicate, rename, context menu) are directly accessible on the canvas.
2. **Single-window philosophy**: All layout adjustments occur in the existing main window canvas. Deep parameter tweaking remains in the collapsible `PadInspectorComponent`.
3. **Direct canvas manipulation**:
   - Drag & drop to swap pad positions or move into empty slots.
   - Option/Alt + Drag to duplicate a pad.
   - Double-click inline label editing directly on the pad.
   - Right-click context menu for quick actions (Duplicate, Delete, MIDI Learn, Copy/Paste Curve).
   - Ghost `+` slot at grid end for one-click pad creation.
   - Clean, compact header with pad count badge, column selector, and minimal `+` button.

---

## 2. User Interactions & Canvas Behaviors

### 2.1 Drag-and-Drop Reorder & Move
- **Drag Threshold**: A mouse drag begins when cursor movement exceeds 4 pixels from the initial `mouseDown` position.
- **Visual Feedback during Drag**:
  - The dragged pad is rendered as a semi-transparent floating card following the mouse pointer.
  - Hovering over another existing pad displays an accent-bordered **Swap Target** indicator.
  - Hovering over an empty grid cell displays a dashed **Move Target** bounding box.
- **Drop Action**:
  - Releasing over an existing pad swaps their grid coordinates (`gridRow`, `gridCol`) and their ordering in the profile.
  - Releasing over an empty grid cell moves the dragged pad to that `(gridRow, gridCol)` location.
  - Releasing outside the grid cancels the drag with no change.
- **Duplicate Modifier**:
  - Holding <kbd>Option</kbd> (macOS) / <kbd>Alt</kbd> (Windows) while dragging creates a clone of the source pad at the target cell.
  - If the target cell is occupied, the clone inserts there and shifts or swaps depending on grid configuration.
  - The duplicated pad automatically inherits the source pad's velocity curve, velocity gate, retrigger guard, and group, but is assigned the next available MIDI note on the same channel.

### 2.2 Inline Quick-Editing
- **Double-Click on Pad Title**:
  - Creates a borderless `juce::TextEditor` directly over the pad's label area.
  - Highlights existing text for immediate overtyping.
  - Pressing <kbd>Enter</kbd> commits the rename.
  - Pressing <kbd>Esc</kbd> or clicking outside cancels without modifying the pad name.
- **Visual Integration**: Uses existing `Theme::textPrimary()`, `Theme::padSelected()`, and `Theme::bodyFont()`, blending seamlessly into the UI.

### 2.3 Right-Click Context Menu
Right-clicking any pad opens a native JUCE popup menu:
- **Rename** (<kbd>↵</kbd>): Activates inline text editing.
- **Duplicate Pad** (<kbd>⌥Drag</kbd> / <kbd>⌘D</kbd>): Clones the pad to the next available slot.
- **MIDI Learn Note**: Puts the selected pad into MIDI Learn mode, capturing the next incoming note.
- *Separator*
- **Copy Velocity Curve**: Copies the pad's curve to an internal editor clipboard.
- **Paste Velocity Curve**: Applies the copied curve to the target pad (disabled if clipboard is empty).
- **Reset to Linear Curve**: Sets the pad curve to default 1:1 linear.
- *Separator*
- **Delete Pad** (<kbd>⌫</kbd>): Deletes the pad (disabled if profile has only 1 pad remaining).

### 2.4 Ghost Add Slot & Header Toolbar
- **Ghost Add Slot**: A dashed card with a subtle `+` is painted at `suggestNextGridCell()`. Clicking it immediately adds a pad and selects it.
- **Header Bar**:
  - Left: Title `Pad Layout` + badge showing total pads (e.g. `16 Pads`).
  - Right: Column selector (`Auto`, `4 cols`, `8 cols`) + minimal `+` icon button.
  - The legacy `Delete pad` button is removed in favor of the context menu and <kbd>Backspace</kbd> shortcut.

---

## 3. Architecture & Component Changes

```
┌─────────────────────────────────────────────────────────────────┐
│ PluginEditor                                                    │
│  ├─ ProfileStore (active profile mutation & serialization)      │
│  ├─ VelocityEngine (realtime DSP note mapping & curves)         │
│  │                                                              │
│  ├─ PadGridComponent                                            │
│  │   ├─ Header Bar (Pad Count Badge, Col Selector, [+] Button)  │
│  │   ├─ Viewport                                                │
│  │   │   └─ PadCanvas                                           │
│  │   │       ├─ Drag Engine (Ghost card, Snap targets, Swap)   │
│  │   │       ├─ Inline TextEditor (Dynamic overlay)             │
│  │   │       └─ Ghost [+] Cell (One-click create)               │
│  │   └─ Context Menu Dispatcher                                 │
│  │                                                              │
│  └─ PadInspectorComponent (Collapsible parameter panel)         │
└─────────────────────────────────────────────────────────────────┘
```

### 3.1 `ControllerProfile` & `ProfileStore`
Add pad layout mutation methods:
- `PadMutationResult swapPads(int indexA, int indexB)`:
  - Swaps pad positions, grid coordinates, and indices in `pads` vector.
- `PadMutationResult movePadToCell(int index, int targetRow, int targetCol)`:
  - Updates `gridRow` and `gridCol` for pad at `index`.
- `PadMutationResult duplicatePad(int sourceIndex, std::optional<int> targetIndex = std::nullopt)`:
  - Clones source pad.
  - Auto-allocates next available MIDI note (skipping existing notes on that channel).
  - Inserts at `targetIndex` or end of profile.
- `PadMutationResult renamePad(int index, const juce::String& newLabel)`:
  - Updates label for pad at `index`.

### 3.2 `PadGridComponent`
- **Drag State Tracking**:
  - `enum class DragState { idle, potentialDrag, dragging };`
  - Tracks `dragSourceIndex`, `dragCurrentPoint`, `hoverDropTargetIndex`, and `hoverDropTargetCell`.
- **Inline Editing**:
  - Owns `std::unique_ptr<juce::TextEditor> inlineEditor`.
  - Positioned via `padBoundsForIndex(index).reduced(8).removeFromTop(...)`.
- **Callbacks**:
  - `std::function<void(int fromIndex, int toIndex)> onPadSwapRequested;`
  - `std::function<void(int sourceIndex)> onPadDuplicateRequested;`
  - `std::function<void(int padIndex, const juce::String& newName)> onPadRenamed;`
  - `std::function<void(int padIndex)> onLearnMidiRequested;`
  - `std::function<void(int padIndex)> onCopyCurveRequested;`
  - `std::function<void(int padIndex)> onPasteCurveRequested;`

### 3.3 `PluginEditor`
- Connects `PadGridComponent` callbacks to `ProfileStore`.
- Implements `copiedCurve` clipboard:
  - `std::optional<svc::VelocityCurve> clipboardCurve;`
- Manages MIDI Learn state:
  - When active on a pad, the next MIDI Note On event from standalone device or DAW input assigns that note to the pad and ends Learn mode.
- Synchronizes audio engine via `applyProfileToEngine()`.
- Marks DAW state dirty via `audioProcessor.markStateDirty()`.

---

## 4. Audio Thread Safety & Concurrency

- **Realtime Independence**: The audio thread processes MIDI via `VelocityEngine` using pre-compiled lookup tables keyed by `(midiNote, midiChannel)`.
- **Safe Profile Updates**:
  - Dragging, reordering, and renaming do not alter existing note curves until dropped or committed.
  - When swapped or mutated, `applyProfileToEngine()` updates the engine lookup tables atomically without allocating memory or holding locks in the audio callback.
  - Zero clicks, pops, or dropped notes during active playback.

---

## 5. Verification & Testing

### 5.1 Automated Unit Tests (`Tests/ProfileStoreTests.cpp`)
- Test `swapPads`: Verify indices, grid coordinates, and data integrity swap cleanly.
- Test `duplicatePad`: Verify curve clone, settings inheritance, and unique MIDI note assignment.
- Test `movePadToCell`: Verify grid coordinates update without affecting other pads.
- Test `renamePad`: Verify string update and XML serialization round-trip.

### 5.2 Build & Pre-Ship Verification Gate
1. Rebuild all targets with `cmake --build build --parallel`.
2. Run test suite: `ctest --test-dir build --output-on-failure`.
3. Launch standalone binary:
   `build/SuperVelocityCurve_artefacts/RelWithDebInfo/Standalone/Super Velocity Curve.app`
4. Smoke test interactions:
   - Drag pad A onto pad B -> pads swap cleanly.
   - Option-drag pad A -> new duplicated pad appears with unique note.
   - Double-click pad name -> editor appears, typing new name and pressing Enter updates pad and inspector.
   - Right-click pad -> context menu opens, Copy/Paste curve copies settings.
   - Click ghost `+` slot -> adds new pad at grid end.
   - Test undo/redo and DAW project reload.
