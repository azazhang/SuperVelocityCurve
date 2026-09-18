# Pad Layout Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the Pad Layout section of Super Velocity Curve into a modern, seamless canvas with direct drag-and-drop reordering/swapping, inline double-click editing, right-click context menus, and a modernized toolbar.

**Architecture:** Extend `ControllerProfile` and `ProfileStore` with atomic mutation operations (`swapPads`, `movePadToCell`, `duplicatePad`, `renamePad`). Redesign `PadGridComponent::PadCanvas` into an interactive drag-and-drop surface with inline text editing and right-click popups. Connect callbacks in `PluginEditor` to update the audio engine and mark DAW state dirty without any separate "edit mode".

**Tech Stack:** C++20, JUCE 7/8, CMake, Ninja, CTest.

## Global Constraints

- **Single-Window Philosophy**: No separate windows or modal dialogs. All pad layout editing occurs directly on the canvas, while deep parameter tweaks stay in the collapsible `PadInspectorComponent`.
- **Zero Edit Mode Switch**: No mode toggle button. Drag-to-swap, duplicate, rename, and context menu must be directly available during normal operation.
- **Audio Thread Safety**: Realtime processing via `VelocityEngine` lookup tables remains lock-free and allocation-free. Layout mutations trigger atomic table updates via `applyProfileToEngine()`.
- **JUCE Standards**: Modern JUCE painting and component hierarchy; adhere to `svc::ui::Theme`.
- **Mandatory Verification**: After every source change, build with `cmake --build build --parallel` and test with `ctest --test-dir build --output-on-failure`. Never `git commit` without explicit user instruction.

---

### Task 1: Profile & Store Data Model Mutations

**Files:**
- Modify: `Source/Profiles/ControllerProfile.h`
- Modify: `Source/Profiles/ControllerProfile.cpp`
- Modify: `Source/Profiles/ProfileStore.h`
- Modify: `Source/Profiles/ProfileStore.cpp`
- Test: `Tests/EngineTests.cpp`

**Interfaces:**
- Consumes: `svc::ProfilePad`, `svc::ControllerProfile`, `svc::PadMutationResult`
- Produces:
  - `ControllerProfile::swapPads(int indexA, int indexB) -> PadMutationResult`
  - `ControllerProfile::movePadToCell(int index, int targetRow, int targetCol) -> PadMutationResult`
  - `ControllerProfile::duplicatePad(int sourceIndex, std::optional<int> targetIndex = std::nullopt) -> PadMutationResult`
  - `ControllerProfile::renamePad(int index, const juce::String& newLabel) -> PadMutationResult`
  - `ProfileStore::swapPadsInActive(int indexA, int indexB) -> PadMutationResult`
  - `ProfileStore::movePadInActive(int index, int targetRow, int targetCol) -> PadMutationResult`
  - `ProfileStore::duplicatePadInActive(int sourceIndex) -> PadMutationResult`
  - `ProfileStore::renamePadInActive(int index, const juce::String& newLabel) -> PadMutationResult`

- [ ] **Step 1: Write the failing tests in `Tests/EngineTests.cpp`**
Add tests for `swapPads`, `movePadToCell`, `duplicatePad`, and `renamePad`:
```cpp
static int testProfilePadMutations()
{
    svc::ControllerProfile profile("Test", svc::ProfileLayout::custom);
    svc::ProfilePad pad1;
    pad1.midiNote = 36;
    pad1.label = "Kick";
    pad1.gridRow = 0; pad1.gridCol = 0;
    profile.addPad(pad1);

    svc::ProfilePad pad2;
    pad2.midiNote = 38;
    pad2.label = "Snare";
    pad2.gridRow = 0; pad2.gridCol = 1;
    profile.addPad(pad2);

    // Swap pads
    EXPECT_TRUE(profile.swapPads(0, 1) == svc::PadMutationResult::ok);
    EXPECT_TRUE(profile.getPads()[0].label == "Snare");
    EXPECT_TRUE(profile.getPads()[1].label == "Kick");
    // Verify grid coords swapped
    EXPECT_TRUE(profile.getPads()[0].gridRow == 0 && profile.getPads()[0].gridCol == 0);
    EXPECT_TRUE(profile.getPads()[1].gridRow == 0 && profile.getPads()[1].gridCol == 1);

    // Rename pad
    EXPECT_TRUE(profile.renamePad(0, "Snare 2") == svc::PadMutationResult::ok);
    EXPECT_TRUE(profile.getPads()[0].label == "Snare 2");

    // Move pad to empty cell
    EXPECT_TRUE(profile.movePadToCell(0, 1, 2) == svc::PadMutationResult::ok);
    EXPECT_TRUE(profile.getPads()[0].gridRow == 1 && profile.getPads()[0].gridCol == 2);

    // Duplicate pad
    EXPECT_TRUE(profile.duplicatePad(0) == svc::PadMutationResult::ok);
    EXPECT_TRUE(profile.getPads().size() == 3);
    EXPECT_TRUE(profile.getPads()[2].midiNote != pad1.midiNote && profile.getPads()[2].midiNote != pad2.midiNote);

    return 0;
}
```
Register `testProfilePadMutations()` in `main()` of `Tests/EngineTests.cpp`.

- [ ] **Step 2: Run test to verify it fails compilation**
Run: `cmake --build build --target SuperVelocityCurveTests`
Expected: FAIL with "no member named 'swapPads'"

- [ ] **Step 3: Implement `swapPads`, `movePadToCell`, `duplicatePad`, `renamePad` in `ControllerProfile` and `ProfileStore`**
In `Source/Profiles/ControllerProfile.h` and `.cpp`:
```cpp
PadMutationResult ControllerProfile::swapPads(int indexA, int indexB)
{
    if (indexA < 0 || indexA >= static_cast<int>(pads.size()) ||
        indexB < 0 || indexB >= static_cast<int>(pads.size()))
        return PadMutationResult::indexOutOfRange;
    if (indexA == indexB)
        return PadMutationResult::ok;

    // Swap row/col to maintain visual slot stability
    std::swap(pads[static_cast<size_t>(indexA)].gridRow, pads[static_cast<size_t>(indexB)].gridRow);
    std::swap(pads[static_cast<size_t>(indexA)].gridCol, pads[static_cast<size_t>(indexB)].gridCol);
    std::swap(pads[static_cast<size_t>(indexA)], pads[static_cast<size_t>(indexB)]);
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::movePadToCell(int index, int targetRow, int targetCol)
{
    if (index < 0 || index >= static_cast<int>(pads.size()))
        return PadMutationResult::indexOutOfRange;

    pads[static_cast<size_t>(index)].gridRow = juce::jmax(0, targetRow);
    pads[static_cast<size_t>(index)].gridCol = juce::jmax(0, targetCol);
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::duplicatePad(int sourceIndex, std::optional<int> targetIndex)
{
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(pads.size()))
        return PadMutationResult::indexOutOfRange;
    if (pads.size() >= static_cast<size_t>(kMaxProfilePads))
        return PadMutationResult::maxPadsReached;

    ProfilePad clone = pads[static_cast<size_t>(sourceIndex)];
    clone.label = clone.label + " (Copy)";

    // Find next available MIDI note on same channel
    int candidateNote = (clone.midiNote + 1) % 128;
    int tries = 0;
    while (hasDuplicateMidiKey(candidateNote, clone.midiChannel) && tries < 128)
    {
        candidateNote = (candidateNote + 1) % 128;
        tries++;
    }
    if (tries >= 128)
        return PadMutationResult::duplicateMidiKey;

    clone.midiNote = candidateNote;
    const auto [nextRow, nextCol] = suggestNextGridCell();
    clone.gridRow = nextRow;
    clone.gridCol = nextCol;

    return addPad(clone, targetIndex);
}

PadMutationResult ControllerProfile::renamePad(int index, const juce::String& newLabel)
{
    if (index < 0 || index >= static_cast<int>(pads.size()))
        return PadMutationResult::indexOutOfRange;

    pads[static_cast<size_t>(index)].label = newLabel.trim();
    return PadMutationResult::ok;
}
```
Expose corresponding methods in `Source/Profiles/ProfileStore.h` and `.cpp` modifying `activeProfile` and calling `syncActiveUserProfileFromEdits()` & `notifyChanged()`.

- [ ] **Step 4: Run test to verify it passes**
Run: `cmake --build build --target SuperVelocityCurveTests && ctest --test-dir build -R EngineTests --output-on-failure`
Expected: PASS

---

### Task 2: Drag & Drop Reordering & Duplication in `PadGridComponent`

**Files:**
- Modify: `Source/UI/PadGridComponent.h`
- Modify: `Source/UI/PadGridComponent.cpp`

**Interfaces:**
- Consumes: `PadGridComponent`, `svc::ui::Theme`, `svc::ProfilePad`
- Produces:
  - `PadGridComponent::onPadSwapRequested(int fromIndex, int toIndex)`
  - `PadGridComponent::onPadMoveRequested(int index, int targetRow, int targetCol)`
  - `PadGridComponent::onPadDuplicateRequested(int sourceIndex)`
  - Real-time visual feedback during drag: drag ghost card, swap target glow, empty cell snap box

- [ ] **Step 1: Define drag state tracking and public callbacks in `PadGridComponent.h`**
```cpp
enum class DragMode { none, potentialDrag, dragging };

struct DragState
{
    DragMode mode = DragMode::none;
    int sourceIndex = -1;
    juce::Point<int> startPos;
    juce::Point<int> currentPos;
    int hoverTargetIndex = -1;
    std::pair<int, int> hoverTargetCell { -1, -1 };
    bool isAltDuplicate = false;
};

std::function<void(int fromIndex, int toIndex)> onPadSwapRequested;
std::function<void(int index, int targetRow, int targetCol)> onPadMoveRequested;
std::function<void(int sourceIndex)> onPadDuplicateRequested;
```

- [ ] **Step 2: Implement mouse event handlers in `PadCanvas`**
In `PadCanvas`:
- `mouseDown`: Check if left button. Record `startPos` and `sourceIndex = owner.padIndexAt(pos)`. Set `DragMode::potentialDrag`.
- `mouseDrag`: If `mode == DragMode::potentialDrag` and distance > 4px, switch to `DragMode::dragging`. Update `currentPos` and `isAltDuplicate = event.mods.isAltDown()`. Calculate `hoverTargetIndex = owner.padIndexAt(pos)` and if -1, calculate grid cell `(row, col)` from cursor position. Request `repaint()`.
- `mouseUp`: If `mode == DragMode::dragging`:
  - If `hoverTargetIndex >= 0 && hoverTargetIndex != sourceIndex`:
    - If `isAltDuplicate`: call `onPadDuplicateRequested(sourceIndex)`.
    - Else: call `onPadSwapRequested(sourceIndex, hoverTargetIndex)`.
  - Else if `hoverTargetCell.first >= 0`:
    - Call `onPadMoveRequested(sourceIndex, hoverTargetCell.first, hoverTargetCell.second)`.
  - Reset `dragState`. Repaint canvas.

- [ ] **Step 3: Render Drag Ghost & Drop Indicators in `PadCanvas::paint`**
- When `dragState.mode == DragMode::dragging`:
  - Draw semi-transparent ghost rectangle of source pad under mouse cursor.
  - If `hoverTargetIndex >= 0`: draw bright accent outline with "SWAP" badge over destination pad.
  - If hovering empty cell: draw dashed rectangle highlighting the target cell.
  - If `isAltDuplicate`: show "+" badge beside the dragged ghost.

- [ ] **Step 4: Build and test compilation**
Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: All targets compile and all existing tests pass.

---

### Task 3: Inline Text Editing & Context Menu in `PadGridComponent`

**Files:**
- Modify: `Source/UI/PadGridComponent.h`
- Modify: `Source/UI/PadGridComponent.cpp`

**Interfaces:**
- Consumes: `juce::TextEditor`, `juce::PopupMenu`
- Produces:
  - `PadGridComponent::onPadRenamed(int index, const juce::String& newName)`
  - `PadGridComponent::onLearnMidiRequested(int index)`
  - `PadGridComponent::onCopyCurveRequested(int index)`
  - `PadGridComponent::onPasteCurveRequested(int index)`
  - Inline label text editor on double-click
  - Native popup menu on right-click

- [ ] **Step 1: Implement inline `TextEditor` in `PadGridComponent`**
Add `std::unique_ptr<juce::TextEditor> inlineEditor` to `PadGridComponent`.
- Implement `startInlineEditing(int padIndex)`:
  - Position `inlineEditor` over `padBoundsForIndex(padIndex).reduced(6).removeFromTop(...)`.
  - Populate with `pad.label`, select all text, give keyboard focus.
  - `onReturnKey`: commit text via `onPadRenamed(padIndex, inlineEditor->getText())` and remove editor.
  - `onEscapeKey`: cancel editing and remove editor.
  - `onFocusLost`: commit text and remove editor.
- In `PadCanvas::mouseDoubleClick`:
  - If clicked on pad label, call `owner.startInlineEditing(index)`.

- [ ] **Step 2: Implement right-click context menu in `PadCanvas::mouseDown`**
- When `event.mods.isPopupMenu()`:
  - Select clicked pad.
  - Construct `juce::PopupMenu`:
    - "Rename" (Item ID 1) -> triggers `startInlineEditing(index)`
    - "Duplicate Pad" (Item ID 2) -> triggers `onPadDuplicateRequested(index)`
    - "MIDI Learn Note" (Item ID 3) -> triggers `onLearnMidiRequested(index)`
    - Separator
    - "Copy Curve" (Item ID 4) -> triggers `onCopyCurveRequested(index)`
    - "Paste Curve" (Item ID 5, enabled only if clipboard has curve) -> triggers `onPasteCurveRequested(index)`
    - "Reset to Linear Curve" (Item ID 6)
    - Separator
    - "Delete Pad" (Item ID 7, enabled if pads > 1) -> triggers `onDeletePadRequested()`
  - Show popup asynchronously via `showMenuAsync`.

- [ ] **Step 3: Implement Ghost `+` slot on `PadCanvas`**
- In `PadCanvas::paint`:
  - Calculate `const auto [ghostRow, ghostCol] = owner.currentProfile.suggestNextGridCell()`.
  - Draw dashed card with `+ Add Pad` in secondary text color.
- In `PadCanvas::mouseDown`:
  - If click hit ghost slot bounds, trigger `onAddPadRequested()`.

- [ ] **Step 4: Build and test compilation**
Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: PASS

---

### Task 4: Modernize Header Toolbar in `PadGridComponent`

**Files:**
- Modify: `Source/UI/PadGridComponent.h`
- Modify: `Source/UI/PadGridComponent.cpp`
- Modify: `Tests/LayoutTests.cpp`

**Interfaces:**
- Consumes: `juce::Label`, `juce::ComboBox`, `juce::TextButton`
- Produces:
  - Sleek top toolbar replacing legacy `addPadButton` / `deletePadButton`.
  - Pad counter badge label (`"16 Pads"`).
  - Columns selector (`Auto`, `4`, `8`).
  - Compact `+` icon button.

- [ ] **Step 1: Replace buttons in `PadGridComponent.h`**
Replace `addPadButton` and `deletePadButton` with:
```cpp
juce::Label padCountBadge;
juce::ComboBox columnSelector;
juce::TextButton quickAddButton { "+" };
```

- [ ] **Step 2: Initialize and layout toolbar in `PadGridComponent.cpp`**
- Set badge text dynamically in `setProfile()`: `padCountBadge.setText(juce::String(pads.size()) + " Pads", juce::dontSendNotification)`.
- Configure `columnSelector`:
  - Options: `Auto (4 or 8)`, `4 Columns`, `8 Columns`.
  - Set selected from `displayGridColumns`.
  - `onChange`: call `setDisplayGridColumns()`.
- Style `quickAddButton`: compact accent button triggering `onAddPadRequested()`.
- In `resized()`:
  - Header row height: 28px.
  - Left: "Pad Layout" title + `padCountBadge`.
  - Right: `columnSelector` (width 70px) + `quickAddButton` (width 26px).
  - Viewport takes remaining area.

- [ ] **Step 3: Update `LayoutTests.cpp` if it checks child component hierarchy**
Run `cmake --build build --target SuperVelocityCurveLayoutTests && ctest --test-dir build -R LayoutTests --output-on-failure`.
Expected: PASS

---

### Task 5: Wiring in `PluginEditor` & Clipboard / MIDI Learn Support

**Files:**
- Modify: `Source/Plugin/PluginEditor.h`
- Modify: `Source/Plugin/PluginEditor.cpp`

**Interfaces:**
- Consumes: `PadGridComponent` callbacks, `ProfileStore`, `VelocityEngine`
- Produces:
  - Curve clipboard: `std::optional<svc::VelocityCurve> clipboardCurve`
  - Active MIDI Learn handling on pads
  - Reorder, duplicate, rename synchronization with `audioProcessor.markStateDirty()`

- [ ] **Step 1: Add clipboard and MIDI Learn state to `PluginEditor.h`**
```cpp
std::optional<svc::VelocityCurve> clipboardCurve;
int padAwaitingMidiLearn = -1;
```

- [ ] **Step 2: Wire up new callbacks in `PluginEditor.cpp`**
- Wire `padGrid.onPadSwapRequested`:
  ```cpp
  padGrid.onPadSwapRequested = [this] (int from, int to)
  {
      auto& store = audioProcessor.getProfileStore();
      store.swapPadsInActive(from, to);
      applyProfileToEngine();
      refreshPadUI(false);
      onPadSelected(to);
      audioProcessor.markStateDirty();
  };
  ```
- Wire `padGrid.onPadMoveRequested`:
  ```cpp
  padGrid.onPadMoveRequested = [this] (int index, int row, int col)
  {
      auto& store = audioProcessor.getProfileStore();
      store.movePadInActive(index, row, col);
      applyProfileToEngine();
      refreshPadUI(false);
      audioProcessor.markStateDirty();
  };
  ```
- Wire `padGrid.onPadDuplicateRequested`:
  ```cpp
  padGrid.onPadDuplicateRequested = [this] (int sourceIndex)
  {
      auto& store = audioProcessor.getProfileStore();
      store.duplicatePadInActive(sourceIndex);
      applyProfileToEngine();
      const auto newIndex = static_cast<int>(store.getActiveProfile().getPads().size()) - 1;
      refreshPadUI(false);
      onPadSelected(newIndex);
      padGrid.scrollPadIntoView(newIndex);
      showStatus("Pad duplicated.");
      audioProcessor.markStateDirty();
  };
  ```
- Wire `padGrid.onPadRenamed`:
  ```cpp
  padGrid.onPadRenamed = [this] (int index, const juce::String& newName)
  {
      auto& store = audioProcessor.getProfileStore();
      store.renamePadInActive(index, newName);
      refreshPadUI(false);
      audioProcessor.markStateDirty();
  };
  ```
- Wire `padGrid.onCopyCurveRequested` / `onPasteCurveRequested`:
  ```cpp
  padGrid.onCopyCurveRequested = [this] (int index)
  {
      const auto& pads = audioProcessor.getProfileStore().getActiveProfile().getPads();
      if (index >= 0 && index < static_cast<int>(pads.size()))
      {
          clipboardCurve = pads[static_cast<size_t>(index)].curve;
          showStatus("Velocity curve copied.");
      }
  };
  padGrid.onPasteCurveRequested = [this] (int index)
  {
      if (! clipboardCurve.has_value()) return;
      auto& store = audioProcessor.getProfileStore();
      auto pad = store.getActiveProfile().getPads()[static_cast<size_t>(index)];
      pad.curve = *clipboardCurve;
      store.getActiveProfile().setPadAt(index, pad);
      store.syncActiveUserProfileFromEdits();
      applyProfileToEngine();
      refreshPadUI(false);
      showStatus("Velocity curve pasted.");
      audioProcessor.markStateDirty();
  };
  ```
- Wire `padGrid.onLearnMidiRequested`:
  Set `padAwaitingMidiLearn = index; showStatus("Press a MIDI pad to assign note...");`.
  In `PluginEditor::timerCallback()` or incoming note listener, when a note arrives and `padAwaitingMidiLearn >= 0`, assign note and clear learn state.

- [ ] **Step 3: Build all plugin targets and verify compilation**
Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: 100% build clean, all tests pass.

---

### Task 6: Full Verification & QA Gate

**Files:**
- None (Verification only)

**Verification Steps:**
- [ ] **Step 1: Run complete automated test suite**
Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass.

- [ ] **Step 2: Build full Standalone and plugin artefacts**
Run: `cmake --build build --parallel`
Expected: Standalone `.app`, VST3, AU, and CLAP built successfully.

- [ ] **Step 3: Verify Standalone binary timestamp and location**
Check: `ls -la "build/SuperVelocityCurve_artefacts/RelWithDebInfo/Standalone/Super Velocity Curve.app"`
Record timestamp and exact path for the user.
