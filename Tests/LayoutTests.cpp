#include "../Source/Plugin/PluginEditor.h"
#include "../Source/Plugin/PluginProcessor.h"
#include "../Source/Standalone/StandaloneMidiPanel.h"
#include "../Source/UI/CollapsibleSection.h"
#include "../Source/UI/CurveEditorComponent.h"
#include "../Source/UI/EditorLayout.h"
#include "../Source/UI/PadGridComponent.h"
#include "../Source/UI/MidiNoteNames.h"
#include "../Source/UI/PadUiMerge.h"
#include <JuceHeader.h>
#include <iostream>

#define EXPECT_TRUE(cond) \
    do { \
        if (! (cond)) { \
            std::cerr << "FAIL: " #cond " at " << __LINE__ << '\n'; \
            return 1; \
        } \
    } while (false)

template <typename T>
static T* findChildComponent (juce::Component& root)
{
    if (auto* match = dynamic_cast<T*> (&root))
        return match;

    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        if (auto* found = findChildComponent<T> (*root.getChildComponent (i)))
            return found;
    }

    return nullptr;
}

static int testMinWindowCurveVisible()
{
    svc::ui::layout::EditorLayoutInputs inputs;
    inputs.editorBounds = { 0, 0, svc::ui::layout::kMinEditorWidth, svc::ui::layout::kMinEditorHeight };
    inputs.hasStandaloneMidiPanel = true;
    inputs.padSettingsExpanded = true;
  {
    const auto layout = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (layout.curveEditorBounds.getHeight() >= svc::ui::layout::kMinCurvePlotHeight);
    EXPECT_TRUE (layout.curveEditorBounds.getWidth() >= svc::ui::layout::kMinCurvePlotWidth);
  }

    inputs.hasStandaloneMidiPanel = false;
  {
    const auto layout = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (layout.curveEditorBounds.getHeight() >= svc::ui::layout::kMinCurvePlotHeight);
  }

    return 0;
}

static int testDefaultWindowWithCollapsedBottomSections()
{
    svc::ui::layout::EditorLayoutInputs inputs;
    inputs.editorBounds = { 0, 0, 1280, 860 };
    inputs.hasStandaloneMidiPanel = true;
    inputs.padSettingsExpanded = true;
    inputs.bottomSections.histogram = CollapsibleSection::kHeaderHeight;
    inputs.bottomSections.midiTools = CollapsibleSection::kHeaderHeight;
    inputs.bottomSections.calibration = CollapsibleSection::kHeaderHeight;

    const auto layout = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (layout.curveEditorBounds.getHeight() >= 200);
    EXPECT_TRUE (layout.curveEditorBounds.getWidth() >= svc::ui::layout::kMinCurvePlotWidth);
    return 0;
}

static int testSectionContentHeightClamped()
{
    juce::Component content;
    CollapsibleSection section { "Limits", content, 120, true };
    section.setContentHeightLimits (72, 200);
    section.setContentHeight (500);
    EXPECT_TRUE (section.getContentHeight() == 200);
    EXPECT_TRUE (section.getTotalHeight() == CollapsibleSection::kHeaderHeight + 200);
    return 0;
}

static int testCollapsibleSectionHeights()
{
    juce::Component content;
    CollapsibleSection collapsed { "Test", content, 120, false };
    EXPECT_TRUE (collapsed.getTotalHeight() == CollapsibleSection::kHeaderHeight);

    CollapsibleSection expanded { "Test", content, 120, true };
    EXPECT_TRUE (expanded.getTotalHeight() == CollapsibleSection::kHeaderHeight + 120);

    expanded.setContentHeight (200);
    EXPECT_TRUE (expanded.getTotalHeight() == CollapsibleSection::kHeaderHeight + 200);
    return 0;
}

static int testPadUiMergePreservesCurvePoints()
{
    svc::ProfilePad curvePad;
    curvePad.curve.setControlPoints ({ { 0.0f, 0.0f }, { 0.5f, 0.7f }, { 1.0f, 1.0f } });
    curvePad.curve.setFloor (0.1f);
    curvePad.curve.setCeiling (0.9f);

    svc::ProfilePad inspectorPad;
    inspectorPad.label = "Snare";
    inspectorPad.midiNote = 38;
    inspectorPad.midiChannel = 10;
    inspectorPad.curve.setFloor (0.2f);
    inspectorPad.curve.setCeiling (0.85f);

    const auto merged = svc::ui::mergePadFromCurveAndInspector (curvePad, inspectorPad);
    EXPECT_TRUE (merged.label == "Snare");
    EXPECT_TRUE (merged.midiNote == 38);
    EXPECT_TRUE (std::abs (merged.curve.getFloor() - 0.2f) < 0.001f);
    EXPECT_TRUE (std::abs (merged.curve.getCeiling() - 0.85f) < 0.001f);
    EXPECT_TRUE (merged.curve.getControlPoints().size() == curvePad.curve.getControlPoints().size());
    EXPECT_TRUE (std::abs (merged.curve.getControlPoints()[1].output - 0.7f) < 0.001f);
    return 0;
}

static int testLaunchpadGridHorizontalScroll()
{
    PadGridComponent grid;
    grid.setSize (250, 420);
    grid.setProfile (svc::ControllerProfile::createLaunchpadDrumRack());
    grid.resized();

    EXPECT_TRUE (grid.getDisplayGridColumns() == 8);
    EXPECT_TRUE (grid.getPadCanvasWidth() > grid.getViewportClientWidth());
    EXPECT_TRUE (grid.needsHorizontalScroll());
    return 0;
}

static int testLaunchpadGridVerticalScrollbarWithoutResize()
{
    PadGridComponent grid;
    grid.setSize (400, 320);
    grid.resized();
    grid.setProfile (svc::ControllerProfile::createLaunchpadDrumRack());

    EXPECT_TRUE (grid.needsVerticalScroll());
    EXPECT_TRUE (grid.isVerticalScrollbarShown());
    return 0;
}

static int testMidiNoteDisplayFormat()
{
    const auto text = svc::ui::formatMidiNote (36);
    EXPECT_TRUE (text.contains ("36"));
    EXPECT_TRUE (text.contains ("C"));

    const auto shortName = svc::ui::formatMidiNoteShort (36);
    EXPECT_TRUE (shortName.contains ("C"));
    EXPECT_TRUE (! shortName.contains ("("));
    return 0;
}

static int testInvalidStateDoesNotCrash()
{
    SuperVelocityCurveAudioProcessor processor;
    const char garbage[] = "not-valid-plugin-state";
    processor.setStateInformation (garbage, static_cast<int> (sizeof (garbage)));
    juce::MemoryBlock empty;
    processor.setStateInformation (empty.getData(), 0);
    return 0;
}

static int testProcessorStateRoundtrip()
{
    SuperVelocityCurveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    EXPECT_TRUE (saved.getSize() > 0);

    SuperVelocityCurveAudioProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    restored.prepareToPlay (48000.0, 512);

    juce::MemoryBlock roundTrip;
    restored.getStateInformation (roundTrip);
    EXPECT_TRUE (roundTrip.getSize() > 0);
    return 0;
}

static int testHeadlessEditorMinLayout()
{
    SuperVelocityCurveAudioProcessor processor;
    std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
        dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
    EXPECT_TRUE (editor != nullptr);

    editor->setSize (svc::ui::layout::kMinEditorWidth, svc::ui::layout::kMinEditorHeight);
    editor->resized();

    auto* curve = findChildComponent<CurveEditorComponent> (*editor);
    EXPECT_TRUE (curve != nullptr);
    EXPECT_TRUE (curve->getHeight() >= svc::ui::layout::kMinCurvePlotHeight);
    EXPECT_TRUE (curve->getWidth() >= svc::ui::layout::kMinCurvePlotWidth);
    return 0;
}

static int testStateAfterEditorDestroyed()
{
    SuperVelocityCurveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    {
        std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
            dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
        EXPECT_TRUE (editor != nullptr);
    }

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    return 0;
}

static int testEditorStateRoundtrip()
{
    SuperVelocityCurveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
        dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
    EXPECT_TRUE (editor != nullptr);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    editor->resized();
    return 0;
}

static int testThemePersistenceRoundtrip()
{
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("svc_test_" + juce::String (juce::Time::getMillisecondCounterHiRes()));
    tempDir.createDirectory();
    const auto tempSettings = tempDir.getChildFile ("settings.xml");
    SuperVelocityCurveAudioProcessor::setGlobalSettingsFileOverride (tempSettings);

    struct ScopedCleanup
    {
        juce::File dir;
        ~ScopedCleanup()
        {
            SuperVelocityCurveAudioProcessor::clearGlobalSettingsFileOverride();
            dir.deleteRecursively();
        }
    } cleanup { tempDir };

    SuperVelocityCurveAudioProcessor defaultProc;
    defaultProc.prepareToPlay (48000.0, 512);
    EXPECT_TRUE (defaultProc.getTheme() == svc::ui::ThemeMode::system);

    SuperVelocityCurveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    processor.setTheme (svc::ui::ThemeMode::light);
    EXPECT_TRUE (processor.getTheme() == svc::ui::ThemeMode::light);

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);

    SuperVelocityCurveAudioProcessor restoredProcessor;
    restoredProcessor.prepareToPlay (48000.0, 512);
    restoredProcessor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    EXPECT_TRUE (restoredProcessor.getTheme() == svc::ui::ThemeMode::light);

    // Test system mode persistence
    processor.setTheme (svc::ui::ThemeMode::system);
    EXPECT_TRUE (processor.getTheme() == svc::ui::ThemeMode::system);
    saved.reset();
    processor.getStateInformation (saved);
    restoredProcessor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    EXPECT_TRUE (restoredProcessor.getTheme() == svc::ui::ThemeMode::system);

    EXPECT_TRUE (svc::ui::themeModeToString (svc::ui::ThemeMode::system) == "system");
    EXPECT_TRUE (svc::ui::themeModeToString (svc::ui::ThemeMode::dark) == "dark");
    EXPECT_TRUE (svc::ui::themeModeToString (svc::ui::ThemeMode::light) == "light");
    EXPECT_TRUE (svc::ui::themeModeFromString ("system") == svc::ui::ThemeMode::system);
    EXPECT_TRUE (svc::ui::themeModeFromString ("light") == svc::ui::ThemeMode::light);
    EXPECT_TRUE (svc::ui::themeModeFromString ("dark") == svc::ui::ThemeMode::dark);
    EXPECT_TRUE (svc::ui::themeModeFromString ("invalid") == svc::ui::ThemeMode::dark);

    const auto resolved = svc::ui::resolveEffectiveMode (svc::ui::ThemeMode::system);
    EXPECT_TRUE (resolved == svc::ui::ThemeMode::dark || resolved == svc::ui::ThemeMode::light);

    return 0;
}

static int testEditorLayoutBottomSectionsClampMinCurveHeight()
{
    svc::ui::layout::EditorLayoutInputs inputs;
    inputs.editorBounds = { 0, 0, svc::ui::layout::kMinEditorWidth, svc::ui::layout::kMinEditorHeight };
    inputs.hasStandaloneMidiPanel = true;
    inputs.padSettingsExpanded = true;
    inputs.bottomSections.histogram = 300;
    inputs.bottomSections.midiTools = 300;
    inputs.bottomSections.calibration = 300;

    const auto layout = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (layout.curveEditorBounds.getHeight() >= svc::ui::layout::kMinCurvePlotHeight);
    EXPECT_TRUE (layout.curveEditorBounds.getWidth() >= svc::ui::layout::kMinCurvePlotWidth);
    return 0;
}

static int testPadGridColumnsNoOverlap()
{
    PadGridComponent grid;
    grid.setSize (400, 600);
    grid.setProfile (svc::ControllerProfile::createLaunchpadDrumRack());
    grid.setDisplayGridColumns (4);
    grid.resized();

    const int numPads = 64;
    for (int i = 0; i < numPads; ++i)
    {
        const auto b1 = grid.padBoundsForIndex (i);
        EXPECT_TRUE (! b1.isEmpty());
        for (int j = i + 1; j < numPads; ++j)
        {
            const auto b2 = grid.padBoundsForIndex (j);
            EXPECT_TRUE (! b1.intersects (b2));
        }
    }
    return 0;
}

static int testPadGridCellMappingAndClamping()
{
    PadGridComponent grid;
    grid.setSize (400, 500);
    grid.setProfile (svc::ControllerProfile::createGMStandard());
    grid.resized();

    // GM Standard has pads in rows 0-6
    EXPECT_TRUE (grid.padIndexAtCell (0, 0) == 0);
    EXPECT_TRUE (grid.padIndexAtCell (0, 1) == 1);
    EXPECT_TRUE (grid.padIndexAtCell (3, 3) == 15);
    EXPECT_TRUE (grid.padIndexAtCell (6, 1) == -1);
    EXPECT_TRUE (grid.padIndexAtCell (7, 0) == -1);

    // Display to grid round trip
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            const auto [gr, gc] = grid.displayToGridCell (r, c);
            const auto [dr, dc] = grid.gridToDisplayCell (gr, gc);
            EXPECT_TRUE (dr == r && dc == c);
        }
    }

    // Boundary clamping: row 0..5 allowed (ghost pad on row 4), row 50 rejected
    const auto validCell = grid.cellAt (juce::Point<int> (20, 20));
    EXPECT_TRUE (validCell.first == 0 && validCell.second == 0);
    const auto clampedCell = grid.cellAt (juce::Point<int> (20, 5000));
    EXPECT_TRUE (clampedCell.first == -1 && clampedCell.second == -1);

    return 0;
}

static int testPadGridInlineEditing()
{
    PadGridComponent grid;
    grid.setSize (400, 500);
    grid.setProfile (svc::ControllerProfile::createGMStandard());
    grid.resized();

    juce::String renamedTo;
    grid.onPadRenamed = [&] (int idx, const juce::String& name)
    {
        juce::ignoreUnused (idx);
        renamedTo = name;
    };

    grid.startInlineEditing (0);
    grid.cancelInlineEditing();
    EXPECT_TRUE (renamedTo.isEmpty());

    return 0;
}

static juce::MouseEvent makeMouseEvent (juce::Component* comp,
                                       juce::Point<int> pos,
                                       juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
{
    const auto p = pos.toFloat();
    return { juce::Desktop::getInstance().getMainMouseSource(), p, mods,
             juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
             juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY,
             comp, comp, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false };
}

static int testHumanUserWorkflowSimulation()
{
    // Step 1: Human launches plugin in DAW / Standalone host
    std::cout << "  [SIM] Step 1: Human launches plugin in DAW / Standalone host" << std::endl;
    SuperVelocityCurveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
        dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
    EXPECT_TRUE (editor != nullptr);

    editor->setSize (1000, 700);
    editor->resized();

    auto* grid = findChildComponent<PadGridComponent> (*editor);
    EXPECT_TRUE (grid != nullptr);
    auto* curve = findChildComponent<CurveEditorComponent> (*editor);
    EXPECT_TRUE (curve != nullptr);
    auto* viewport = findChildComponent<juce::Viewport> (*grid);
    EXPECT_TRUE (viewport != nullptr);
    auto* canvas = viewport->getViewedComponent();
    EXPECT_TRUE (canvas != nullptr);

    // Initial state check
    EXPECT_TRUE (grid->getSelectedPadIndex() == 0);
    EXPECT_TRUE (curve->getPad().label == "Open HH");
    EXPECT_TRUE (curve->getPad().midiNote == 46);

    // Step 2: Human clicks Pad 12 (Kick, Note 36)
    std::cout << "  [SIM] Step 2: Human clicks Pad 12 (Kick, Note 36)" << std::endl;
    const auto kickCenter = grid->padBoundsForIndex (12).getCentre();
    const auto clickKick = makeMouseEvent (canvas, kickCenter);
    canvas->mouseDown (clickKick);
    canvas->mouseUp (clickKick);
    EXPECT_TRUE (grid->getSelectedPadIndex() == 12);
    EXPECT_TRUE (curve->getPad().label == "Kick");
    EXPECT_TRUE (curve->getPad().midiNote == 36);

    // Step 3: Human double-clicks Pad 12 and renames it
    std::cout << "  [SIM] Step 3: Human double-clicks Pad 12 and renames it" << std::endl;
    canvas->mouseDoubleClick (clickKick);
    auto* inlineText = findChildComponent<juce::TextEditor> (*canvas);
    EXPECT_TRUE (inlineText != nullptr);
    inlineText->setText ("Super Kick 808");
    grid->commitInlineEditing();
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[12].label == "Super Kick 808");
    EXPECT_TRUE (curve->getPad().label == "Super Kick 808");

    // Step 4: Human edits velocity curve control points on curveEditor
    std::cout << "  [SIM] Step 4: Human edits velocity curve control points on curveEditor" << std::endl;
    auto editedPad = curve->getPad();
    editedPad.curve.setControlPoints ({ { 0.0f, 0.0f }, { 0.45f, 0.8f }, { 1.0f, 1.0f } });
    curve->setPad (editedPad, false);
    curve->onPadChanged (editedPad);

    // Step 5: Human clicks another pad (Pad 1, Closed HH) and then switches back to Pad 12
    std::cout << "  [SIM] Step 5: Human switches between pads and checks persistence" << std::endl;
    const auto pad1Center = grid->padBoundsForIndex (1).getCentre();
    const auto clickPad1 = makeMouseEvent (canvas, pad1Center);
    canvas->mouseDown (clickPad1);
    canvas->mouseUp (clickPad1);
    EXPECT_TRUE (grid->getSelectedPadIndex() == 1);
    EXPECT_TRUE (curve->getPad().label == "Closed HH");

    canvas->mouseDown (clickKick);
    canvas->mouseUp (clickKick);
    EXPECT_TRUE (grid->getSelectedPadIndex() == 12);
    // Crucial check: control points persisted without being overwritten by inspector
    EXPECT_TRUE (curve->getPad().curve.getControlPoints().size() == 3);
    EXPECT_TRUE (std::abs (curve->getPad().curve.getControlPoints()[1].output - 0.8f) < 0.001f);

    // Step 6: Human uses context menu to copy curve from Pad 12 and paste to Pad 1
    std::cout << "  [SIM] Step 6: Human copies curve and pastes to Pad 1" << std::endl;
    grid->onCopyCurveRequested (12);
    EXPECT_TRUE (grid->getCanPasteCurve());

    grid->onPasteCurveRequested (1);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[1].curve.getControlPoints().size() == 3);
    EXPECT_TRUE (std::abs (processor.getProfileStore().getActiveProfile().getPads()[1].curve.getControlPoints()[1].output - 0.8f) < 0.001f);

    // Step 7: Human resets Pad 1 curve to linear
    std::cout << "  [SIM] Step 7: Human resets Pad 1 curve to linear" << std::endl;
    grid->onResetCurveRequested (1);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[1].curve.getControlPoints().size() == 2);

    // Step 8: Human drags Pad 0 over Pad 1 to swap them
    std::cout << "  [SIM] Step 8: Human drags Pad 0 over Pad 1 to swap them" << std::endl;
    const auto nameBefore0 = processor.getProfileStore().getActiveProfile().getPads()[0].label;
    const auto nameBefore1 = processor.getProfileStore().getActiveProfile().getPads()[1].label;
    const auto noteBefore0 = processor.getProfileStore().getActiveProfile().getPads()[0].midiNote;
    const auto noteBefore1 = processor.getProfileStore().getActiveProfile().getPads()[1].midiNote;

    const auto p0Center = grid->padBoundsForIndex (0).getCentre();
    canvas->mouseDown (makeMouseEvent (canvas, p0Center));
    canvas->mouseDrag (makeMouseEvent (canvas, pad1Center));
    canvas->mouseUp (makeMouseEvent (canvas, pad1Center));

    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].label == nameBefore1);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[1].label == nameBefore0);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].midiNote == noteBefore1);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[1].midiNote == noteBefore0);

    // Step 9: Human moves Pad 0 to empty cell (row 7, col 2)
    std::cout << "  [SIM] Step 9: Human moves Pad 0 to empty cell" << std::endl;
    grid->onPadMoveRequested (0, 7, 2);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].gridRow == 7);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].gridCol == 2);

    // Step 10: Human Alt-drags Pad 0 to duplicate it onto cell (7, 3)
    std::cout << "  [SIM] Step 10: Human duplicates Pad 0 onto empty cell" << std::endl;
    const size_t padCountBeforeDup = processor.getProfileStore().getActiveProfile().getPads().size();
    grid->onPadDuplicateRequested (0, std::make_pair (7, 3));
    const size_t padCountAfterDup = processor.getProfileStore().getActiveProfile().getPads().size();
    EXPECT_TRUE (padCountAfterDup == padCountBeforeDup + 1);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads().back().gridRow == 7);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads().back().gridCol == 3);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads().back().label.contains ("Copy"));

    // Step 11: Human uses Delete key to delete the clone
    std::cout << "  [SIM] Step 11: Human deletes cloned pad" << std::endl;
    const int cloneIndex = static_cast<int> (padCountAfterDup) - 1;
    grid->setSelectedPadIndex (cloneIndex);
    if (grid->onPadSelected)
        grid->onPadSelected (cloneIndex);
    canvas->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads().size() == padCountBeforeDup);

    // Step 12: Human plays live MIDI Note-On into processor
    std::cout << "  [SIM] Step 12: Live MIDI Note-On processing" << std::endl;
    juce::AudioBuffer<float> audioBuffer (2, 512);
    audioBuffer.clear();
    juce::MidiBuffer midiBuffer;
    midiBuffer.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 75), 0);
    processor.processBlock (audioBuffer, midiBuffer);

    EXPECT_TRUE (! midiBuffer.isEmpty());
    for (const auto meta : midiBuffer)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            EXPECT_TRUE (msg.getNoteNumber() == 36);
            EXPECT_TRUE (msg.getVelocity() > 0);
        }
    }

    // Refresh GUI timers
    static_cast<juce::Timer*> (editor.get())->timerCallback();
    EXPECT_TRUE (grid->hasActiveHitVisuals());
    grid->decayHitVisuals();

    // Step 13: Human uses MIDI Learn
    std::cout << "  [SIM] Step 13: MIDI Learn" << std::endl;
    grid->onLearnMidiRequested (0);
    midiBuffer.clear();
    midiBuffer.addEvent (juce::MidiMessage::noteOn (1, 65, (juce::uint8) 85), 0);
    processor.processBlock (audioBuffer, midiBuffer);
    static_cast<juce::Timer*> (editor.get())->timerCallback();

    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].midiNote == 65);
    EXPECT_TRUE (processor.getProfileStore().getActiveProfile().getPads()[0].midiChannel == 1);

    // Step 14: Human toggles theme between Light, Dark, and System
    std::cout << "  [SIM] Step 14: Theme switching" << std::endl;
    processor.setTheme (svc::ui::ThemeMode::light);
    grid->refreshVisualCache();
    processor.setTheme (svc::ui::ThemeMode::dark);
    grid->refreshVisualCache();
    processor.setTheme (svc::ui::ThemeMode::system);
    grid->refreshVisualCache();

    // Step 15: Human toggles column selector
    std::cout << "  [SIM] Step 15: Column selector" << std::endl;
    grid->setDisplayGridColumns (8);
    EXPECT_TRUE (grid->getDisplayGridColumns() == 8);
    grid->setDisplayGridColumns (4);
    EXPECT_TRUE (grid->getDisplayGridColumns() == 4);

    // Step 16: Clean teardown
    std::cout << "  [SIM] Step 16: Clean teardown" << std::endl;
    editor.reset();

    return 0;
}

static int testPadDragAndScrollIndependence()
{
    PadGridComponent grid;
    grid.setSize (400, 300);
    grid.setProfile (svc::ControllerProfile::createLaunchpadDrumRack()); // 64 pads, 16 rows
    grid.resized();

    juce::Component* padCanvas = nullptr;
    for (int i = 0; i < grid.getNumChildComponents(); ++i)
    {
        if (auto* vp = dynamic_cast<juce::Viewport*> (grid.getChildComponent (i)))
        {
            padCanvas = vp->getViewedComponent();
            break;
        }
    }
    EXPECT_TRUE (padCanvas != nullptr);

    // Initial view position must be (0, 0)
    EXPECT_TRUE (grid.getViewportViewPosition() == juce::Point<int> (0, 0));

    // 1. Pad Drag must NOT drag-scroll the viewport
    const auto topPadIdx = grid.padIndexAtCell (0, 0);
    EXPECT_TRUE (topPadIdx >= 0);
    const auto topCenter = grid.padBoundsForIndex (topPadIdx).getCentre();
    padCanvas->mouseDown (makeMouseEvent (padCanvas, topCenter));
    // Drag down by 40px (well past 4px threshold, reaching y ~ 79px, well within viewport interior)
    padCanvas->mouseDrag (makeMouseEvent (padCanvas, topCenter.translated (0, 40)));
    // Viewport position must remain (0, 0) - NOT hijacked by viewport drag-to-scroll!
    EXPECT_TRUE (grid.getViewportViewPosition() == juce::Point<int> (0, 0));
    padCanvas->mouseUp (makeMouseEvent (padCanvas, topCenter.translated (0, 40)));

    // 2. Empty space drag DOES pan the viewport
    const auto emptyMargin = juce::Point<int> (grid.getPadCanvasWidth() - 2, 50);
    const auto startViewPos = grid.getViewportViewPosition();
    padCanvas->mouseDown (makeMouseEvent (padCanvas, emptyMargin));
    padCanvas->mouseDrag (makeMouseEvent (padCanvas, emptyMargin.translated (0, -30)));
    EXPECT_TRUE (grid.getViewportViewPosition().y > startViewPos.y); // Viewport panned down
    padCanvas->mouseUp (makeMouseEvent (padCanvas, emptyMargin.translated (0, -30)));

    // 3. scrollPadIntoView behavior
    // Reset view position to (0, 0)
    grid.setViewportViewPosition (0, 0);
    // topPadIdx (at cell 0, 0) is already visible: scrollPadIntoView should not change position
    grid.scrollPadIntoView (topPadIdx);
    EXPECT_TRUE (grid.getViewportViewPosition() == juce::Point<int> (0, 0));

    // Pad 0 is at bottom row 7: scrollPadIntoView should scroll view down
    grid.scrollPadIntoView (0);
    EXPECT_TRUE (grid.getViewportViewPosition().y > 0);

    // 4. Edge auto-scroll when dragging pad near viewport bottom edge
    grid.setViewportViewPosition (0, 0);
    padCanvas->mouseDown (makeMouseEvent (padCanvas, topCenter));
    // Drag pad to y = 245 (within 28px of viewport height 252)
    const auto bottomNearEdge = juce::Point<int> (topCenter.x, 245);
    padCanvas->mouseDrag (makeMouseEvent (padCanvas, bottomNearEdge));
    EXPECT_TRUE (grid.getViewportViewPosition().y > 0);
    padCanvas->mouseUp (makeMouseEvent (padCanvas, bottomNearEdge));

    return 0;
}

static int testStandaloneMidiPanelAsyncInit()
{
    StandaloneMidiPanel panel;
    panel.setSize (400, 88);
    panel.resized();

    // Check that panel begins scanning without blocking constructor
    EXPECT_TRUE (panel.isDeviceScanning());

    // Provide mock device lists to populateDeviceLists
    juce::Array<juce::MidiDeviceInfo> mockInputs;
    juce::MidiDeviceInfo inDev;
    inDev.name = "Mock Controller";
    inDev.identifier = "mock_in_1";
    mockInputs.add (inDev);

    juce::Array<juce::MidiDeviceInfo> mockOutputs;
    juce::MidiDeviceInfo outDev;
    outDev.name = "Mock Synth";
    outDev.identifier = "mock_out_1";
    mockOutputs.add (outDev);

    panel.populateDeviceLists (mockInputs, mockOutputs);

    auto* status = findChildComponent<juce::Label> (panel);
    EXPECT_TRUE (status != nullptr);
    EXPECT_TRUE (status->getHeight() > 10);

    return 0;
}

static int testHeaderAndToolbarLayoutConsistency()
{
    SuperVelocityCurveAudioProcessor processor;
    std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
        dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
    EXPECT_TRUE (editor != nullptr);

    editor->setSize (1100, 760);
    editor->resized();

    // Find about button
    juce::TextButton* aboutBtn = nullptr;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
    {
        if (auto* btn = dynamic_cast<juce::TextButton*> (editor->getChildComponent (i)))
        {
            if (btn->getButtonText() == "About")
            {
                aboutBtn = btn;
                break;
            }
        }
    }
    EXPECT_TRUE (aboutBtn != nullptr);
    EXPECT_TRUE (aboutBtn->getHeight() >= 20); // Must not be squished to 12px

    // Find theme box
    juce::ComboBox* themeBox = nullptr;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
    {
        if (auto* cb = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (i)))
        {
            if (cb->getNumItems() == 3 && cb->getItemText (0) == "System")
            {
                themeBox = cb;
                break;
            }
        }
    }
    EXPECT_TRUE (themeBox != nullptr);
    EXPECT_TRUE (themeBox->getHeight() >= 20);

    return 0;
}

static int testPadGridSplitterResize()
{
    svc::ui::layout::EditorLayoutInputs inputs;
    inputs.editorBounds = { 0, 0, 1100, 760 };

    // Default width
    inputs.customPadGridWidth = std::nullopt;
    const auto resDefault = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (resDefault.padGridBounds.getWidth() > 0);
    EXPECT_TRUE (resDefault.splitterBounds.getWidth() == svc::ui::layout::kSplitterWidth);
    EXPECT_TRUE (resDefault.splitterBounds.getX() == resDefault.padGridBounds.getRight() + 4);
    EXPECT_TRUE (resDefault.curveEditorBounds.getX() == resDefault.splitterBounds.getRight() + 4);

    // Narrow width
    inputs.customPadGridWidth = 200;
    const auto resNarrow = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (resNarrow.padGridBounds.getWidth() == 192); // 200 reduced(4) = 192

    // Wide width
    inputs.customPadGridWidth = 400;
    const auto resWide = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (resWide.padGridBounds.getWidth() == 392);

    // Below minimum clamp
    inputs.customPadGridWidth = 100;
    const auto resMin = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (resMin.padGridBounds.getWidth() == svc::ui::layout::kMinPadGridWidth - 8);

    // Above maximum clamp
    inputs.customPadGridWidth = 2000;
    const auto resMax = svc::ui::layout::computeEditorLayout (inputs);
    EXPECT_TRUE (resMax.curveEditorBounds.getWidth() >= svc::ui::layout::kMinCurvePlotWidth - 8);

    return 0;
}

static int testPadGridSplitterPersistenceAndReset()
{
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("svc_test_splitter_" + juce::String (juce::Time::getMillisecondCounterHiRes()));
    tempDir.createDirectory();
    const auto tempSettings = tempDir.getChildFile ("settings.xml");
    SuperVelocityCurveAudioProcessor::setGlobalSettingsFileOverride (tempSettings);

    struct ScopedCleanup
    {
        juce::File dir;
        ~ScopedCleanup()
        {
            SuperVelocityCurveAudioProcessor::clearGlobalSettingsFileOverride();
            dir.deleteRecursively();
        }
    } cleanup { tempDir };

    SuperVelocityCurveAudioProcessor processor;
    EXPECT_TRUE (! processor.getCustomPadGridWidth().has_value());

    processor.setCustomPadGridWidth (320);
    EXPECT_TRUE (processor.getCustomPadGridWidth().has_value());
    EXPECT_TRUE (*processor.getCustomPadGridWidth() == 320);

    juce::MemoryBlock block;
    processor.getStateInformation (block);

    SuperVelocityCurveAudioProcessor restoredProc;
    restoredProc.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    EXPECT_TRUE (restoredProc.getCustomPadGridWidth().has_value());
    EXPECT_TRUE (*restoredProc.getCustomPadGridWidth() == 320);

    processor.setCustomPadGridWidth (std::nullopt);
    EXPECT_TRUE (! processor.getCustomPadGridWidth().has_value());

    return 0;
}

static int testAuditionTestNoteInjection()
{
    SuperVelocityCurveAudioProcessor processor;
    processor.injectTestNote (42, 1, 95);

    juce::AudioBuffer<float> audioBuffer (2, 128);
    audioBuffer.clear();
    juce::MidiBuffer midiBuffer;

    processor.processBlock (audioBuffer, midiBuffer);

    bool foundTestNote = false;
    for (const auto meta : midiBuffer)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && msg.getNoteNumber() == 42 && msg.getChannel() == 1)
        {
            foundTestNote = true;
            EXPECT_TRUE (msg.getVelocity() > 0);
        }
    }
    EXPECT_TRUE (foundTestNote);

    return 0;
}

static int testCurveUndoRestore()
{
    SuperVelocityCurveAudioProcessor processor;
    std::unique_ptr<SuperVelocityCurveAudioProcessorEditor> editor (
        dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (processor.createEditor()));
    EXPECT_TRUE (editor != nullptr);

    editor->setSize (1100, 760);
    editor->resized();

    const auto origPoints = processor.getProfileStore().getActiveProfile().getPads()[0].curve.getControlPoints();

    // Find curve preset box
    juce::ComboBox* presetBox = nullptr;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
    {
        if (auto* cb = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (i)))
        {
            if (cb->getNumItems() >= 7 && cb->getItemText (0) == "Linear")
            {
                presetBox = cb;
                break;
            }
        }
    }
    EXPECT_TRUE (presetBox != nullptr);

    // Apply Soft preset (id 2)
    presetBox->setSelectedId (2, juce::sendNotificationSync);

    const auto modifiedPoints = processor.getProfileStore().getActiveProfile().getPads()[0].curve.getControlPoints();
    EXPECT_TRUE (modifiedPoints.size() != origPoints.size() || std::abs (modifiedPoints[1].output - origPoints[1].output) > 0.001f);

    // Human presses Cmd+Z / triggers undo
    const juce::KeyPress cmdZ ('z', juce::ModifierKeys::commandModifier, 0);
    const bool handled = editor->keyPressed (cmdZ, editor.get());
    EXPECT_TRUE (handled);

    const auto restoredPoints = processor.getProfileStore().getActiveProfile().getPads()[0].curve.getControlPoints();
    EXPECT_TRUE (restoredPoints.size() == origPoints.size());
    EXPECT_TRUE (std::abs (restoredPoints.front().output - origPoints.front().output) < 0.001f);
    EXPECT_TRUE (std::abs (restoredPoints.back().output - origPoints.back().output) < 0.001f);

    // Test Redo: pressing Cmd+Z again restores the modified curve
    editor->keyPressed (cmdZ, editor.get());
    const auto redoPoints = processor.getProfileStore().getActiveProfile().getPads()[0].curve.getControlPoints();
    EXPECT_TRUE (redoPoints.size() == modifiedPoints.size());

    // Test Cross-Pad Undo: user edits Pad 0, switches to Pad 1, then presses Cmd+Z
    // First undo back to original linear
    editor->keyPressed (cmdZ, editor.get());
    // Apply Hard preset to Pad 0
    presetBox->setSelectedId (3, juce::sendNotificationSync);
    // Find pad grid and select pad 1
    auto* padGrid = findChildComponent<PadGridComponent> (*editor);
    EXPECT_TRUE (padGrid != nullptr);
    padGrid->setSelectedPadIndex (1);
    if (padGrid->onPadSelected)
        padGrid->onPadSelected (1);

    // Human presses Cmd+Z: should switch back to Pad 0 and restore Pad 0's curve!
    editor->keyPressed (cmdZ, editor.get());
    EXPECT_TRUE (padGrid->getSelectedPadIndex() == 0);
    const auto crossPadRestored = processor.getProfileStore().getActiveProfile().getPads()[0].curve.getControlPoints();
    EXPECT_TRUE (crossPadRestored.size() == origPoints.size());

    return 0;
}

static int testAuditionCompareFloatingBannerState()
{
    CurveEditorComponent curveEditor;
    EXPECT_TRUE (! curveEditor.getIsAuditioningCompare());

    curveEditor.setIsAuditioningCompare (true);
    EXPECT_TRUE (curveEditor.getIsAuditioningCompare());

    curveEditor.clearCompareCurve();
    EXPECT_TRUE (! curveEditor.getIsAuditioningCompare());

    return 0;
}

#define RUN_TEST(fn) \
    do { \
        std::cout << "[ RUN      ] " #fn << std::endl; \
        const int res = fn(); \
        if (res != 0) { \
            std::cerr << "[  FAILED  ] " #fn " returned " << res << std::endl; \
            return res; \
        } \
        std::cout << "[       OK ] " #fn << std::endl; \
    } while (false)

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    RUN_TEST (testMinWindowCurveVisible);
    RUN_TEST (testDefaultWindowWithCollapsedBottomSections);
    RUN_TEST (testSectionContentHeightClamped);
    RUN_TEST (testCollapsibleSectionHeights);
    RUN_TEST (testPadUiMergePreservesCurvePoints);
    RUN_TEST (testLaunchpadGridHorizontalScroll);
    RUN_TEST (testLaunchpadGridVerticalScrollbarWithoutResize);
    RUN_TEST (testMidiNoteDisplayFormat);
    RUN_TEST (testInvalidStateDoesNotCrash);
    RUN_TEST (testProcessorStateRoundtrip);
    RUN_TEST (testHeadlessEditorMinLayout);
    RUN_TEST (testStateAfterEditorDestroyed);
    RUN_TEST (testEditorStateRoundtrip);
    RUN_TEST (testThemePersistenceRoundtrip);
    RUN_TEST (testEditorLayoutBottomSectionsClampMinCurveHeight);
    RUN_TEST (testPadGridColumnsNoOverlap);
    RUN_TEST (testPadGridCellMappingAndClamping);
    RUN_TEST (testPadGridInlineEditing);
    RUN_TEST (testHumanUserWorkflowSimulation);
    RUN_TEST (testPadDragAndScrollIndependence);
    RUN_TEST (testStandaloneMidiPanelAsyncInit);
    RUN_TEST (testHeaderAndToolbarLayoutConsistency);
    RUN_TEST (testPadGridSplitterResize);
    RUN_TEST (testPadGridSplitterPersistenceAndReset);
    RUN_TEST (testAuditionTestNoteInjection);
    RUN_TEST (testCurveUndoRestore);
    RUN_TEST (testAuditionCompareFloatingBannerState);

    std::cout << "All layout tests passed." << std::endl;
    return 0;
}

