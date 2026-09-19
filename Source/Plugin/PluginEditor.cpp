#include "PluginEditor.h"
#include "../UI/EditorLayout.h"
#include "../UI/PadUiMerge.h"
#include "../UI/ThemeUi.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace
{
juce::String padMutationMessage (svc::PadMutationResult result)
{
    switch (result)
    {
        case svc::PadMutationResult::duplicateMidiKey:
            return "That MIDI note and channel is already used by another pad.";
        case svc::PadMutationResult::wouldEmptyProfile:
            return "Cannot delete the last pad in a profile.";
        case svc::PadMutationResult::maxPadsReached:
            return "Maximum number of pads reached.";
        case svc::PadMutationResult::invalidMidiKey:
            return "MIDI note must be 0-127 and channel 1-16.";
        case svc::PadMutationResult::indexOutOfRange:
            return "Pad index out of range.";
        case svc::PadMutationResult::ok:
        default:
            return {};
    }
}

int themeModeToId (svc::ui::ThemeMode mode) noexcept
{
    switch (mode)
    {
        case svc::ui::ThemeMode::system: return 1;
        case svc::ui::ThemeMode::dark:   return 2;
        case svc::ui::ThemeMode::light:  return 3;
    }
    return 1;
}

svc::ui::ThemeMode idToThemeMode (int id) noexcept
{
    switch (id)
    {
        case 1: return svc::ui::ThemeMode::system;
        case 2: return svc::ui::ThemeMode::dark;
        case 3: return svc::ui::ThemeMode::light;
    }
    return svc::ui::ThemeMode::system;
}
} // namespace

void SuperVelocityCurveAudioProcessorEditor::syncFromProcessorState()
{
    const auto themeId = themeModeToId (audioProcessor.getTheme());
    if (themeBox.getSelectedId() != themeId)
    {
        themeBox.setSelectedId (themeId, juce::dontSendNotification);
        appLookAndFeel.refreshTheme();
        refreshThemedComponents();
        repaintThemedCanvases();
        sendLookAndFeelChange();
    }

    profileNameEditor.setText (audioProcessor.getProfileStore().getActiveProfile().getName(), juce::dontSendNotification);
    rebuildProfileList();
    refreshPadUI();
    refreshRoutingPanels();
    captureProfileBaseline();
}

SuperVelocityCurveAudioProcessorEditor::~SuperVelocityCurveAudioProcessorEditor()
{
    removeKeyListener (this);
    stopTimer();
    audioProcessor.getProfileStore().onProfileChanged = nullptr;
    setLookAndFeel (nullptr);
}

SuperVelocityCurveAudioProcessorEditor::SuperVelocityCurveAudioProcessorEditor (SuperVelocityCurveAudioProcessor& p)
    : AudioProcessorEditor (p),
      audioProcessor (p)
{
    setLookAndFeel (&appLookAndFeel);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1100, 760, 1900, 1150);
    setSize (1100, 760);
    addKeyListener (this);

    titleLabel.setFont (svc::ui::Theme::titleFont());
    subtitleLabel.setFont (svc::ui::Theme::smallFont());
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textMuted()));

    statusLabel.setInterceptsMouseClicks (false, false);
    statusLabel.setVisible (false);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (statusLabel);

    titleLabel.setInterceptsMouseClicks (false, false);
    subtitleLabel.setInterceptsMouseClicks (false, false);

    for (auto* c : { &titleLabel, &subtitleLabel, &profileLabel, &outputModeLabel, &presetLabel,
                     &liveHitsLabel, &themeLabel })
        addAndMakeVisible (c);

    themeBox.addItem ("System", 1);
    themeBox.addItem ("Dark", 2);
    themeBox.addItem ("Light", 3);
    const auto initialTheme = audioProcessor.getTheme();
    themeBox.setSelectedId (themeModeToId (initialTheme), juce::dontSendNotification);
    themeBox.onChange = [this] { applyThemeFromUI(); };
    addAndMakeVisible (themeBox);

    for (auto* c : { &profileBox, &outputModeBox, &curvePresetBox })
        addAndMakeVisible (c);

    profileNameEditor.setText ("My Profile");
    profileNameEditor.setFont (svc::ui::Theme::bodyFont());
    addAndMakeVisible (profileNameEditor);

    for (auto* b : { &saveProfileButton, &duplicateProfileButton, &deleteProfileButton,
                     &importButton, &exportButton, &resetCurveButton, &undoCurveButton,
                     &copyCurveButton, &pasteCurveButton, &pasteGroupButton, &captureAbButton,
                     &abToggleButton, &clearHistogramButton, &aboutButton })
    {
        addAndMakeVisible (b);
        b->setMouseClickGrabsKeyboardFocus (false);
    }

    undoCurveButton.setTooltip ("Undo last curve change (Cmd+Z / Ctrl+Z)");
    undoCurveButton.setEnabled (false);
    undoCurveButton.onClick = [this] { performCurveUndo(); };

    auditionToggle.setMouseClickGrabsKeyboardFocus (false);
    auditionToggle.setTooltip ("Audition pad note with sound when selected");
    addAndMakeVisible (auditionToggle);

    aboutButton.onClick = [this] { showAboutPanel(); };

    for (auto* c : { &profileBox, &outputModeBox, &curvePresetBox, &themeBox })
        c->setMouseClickGrabsKeyboardFocus (false);

    liveHitsLabel.setInterceptsMouseClicks (false, false);

    outputModeBox.addItemList ({ "Auto (match input)", "MIDI 1.0 (7-bit)", "MIDI 2.0 (high-res)" }, 1);
    curvePresetBox.addItemList ({ "Linear", "Soft", "Hard", "S-Curve", "Exponential", "Logarithmic", "Power" }, 1);
    curvePresetBox.setSelectedId (1, juce::dontSendNotification);

    padHistogram.setTitle ("Pad Histogram");
    globalHistogram.setTitle ("Global Histogram");
    addAndMakeVisible (padGrid);
    addAndMakeVisible (padGridResizer);
    padGridResizer.onResizeDelta = [this] (int deltaX)
    {
        const int currentWidth = audioProcessor.getCustomPadGridWidth().value_or (padGrid.getWidth() + 8);
        const int available = getWidth() - (padSettingsSection.isExpanded() ? 220 : 30) - svc::ui::layout::kMinCurvePlotWidth - 60;
        const int maxW = std::max (svc::ui::layout::kMinPadGridWidth, available);
        const int newWidth = juce::jlimit (svc::ui::layout::kMinPadGridWidth, maxW, currentWidth + deltaX);
        audioProcessor.setCustomPadGridWidth (newWidth, false);
        resized();
    };
    padGridResizer.onResizeEnd = [this]
    {
        audioProcessor.saveGlobalSettings();
    };
    padGridResizer.onResetToDefault = [this]
    {
        audioProcessor.setCustomPadGridWidth (std::nullopt, true);
        resized();
    };
    addAndMakeVisible (curveEditor);
    curveEditor.onBeforeCurveMutated = [this] { pushCurveUndo(); };

    midiToolsTabs.setOutline (0);
    midiToolsTabs.addTab ("Routing", juce::Colour (svc::ui::Theme::panel()), &midiRoutingPanel, false);
    midiToolsTabs.addTab ("Note Remap", juce::Colour (svc::ui::Theme::panel()), &noteRemapEditor, false);

    for (auto* section : { &histogramSection, &midiToolsSection, &calibrationSection, &padSettingsSection })
    {
        addAndMakeVisible (section);
        section->onLayoutChanged = [this] { resized(); };
    }

    histogramSection.setContentHeightLimits (72, 200);
    midiToolsSection.setContentHeightLimits (120, 280);
    calibrationSection.setContentHeightLimits (140, 240);
    padSettingsSection.setContentHeightLimits (160, 360);

    addAndMakeVisible (midiMeters);

    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        standaloneMidiPanel = std::make_unique<StandaloneMidiPanel>();
        addAndMakeVisible (*standaloneMidiPanel);
        standaloneMidiPanel->onMidiMessage = [this] (const juce::MidiMessage& msg)
        {
            audioProcessor.injectStandaloneMidi (msg);
        };
        standaloneMidiPanel->onOutputDeviceChanged = [this] (juce::MidiOutput* out)
        {
            audioProcessor.setStandaloneMidiOutput (out);
        };
    }

    outputModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getApvts(), "outputMode", outputModeBox);

    padGrid.onPadSelected = [this] (int index)
    {
        onPadSelected (index);
        if (auditionToggle.getToggleState())
        {
            const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
            if (index >= 0 && index < static_cast<int> (profile.getPads().size()))
            {
                const auto& pad = profile.getPads()[static_cast<size_t> (index)];
                audioProcessor.injectTestNote (pad.midiNote, pad.midiChannel, 100);
            }
        }
    };

    padGrid.onAddPadRequested = [this]
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto result = store.addPadToActive();
        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        applyProfileToEngine();
        const auto newIndex = static_cast<int> (store.getActiveProfile().getPads().size()) - 1;
        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (newIndex);
        padGrid.scrollPadIntoView (newIndex);
        showStatus ("Pad added.");
        audioProcessor.markStateDirty();
    };

    padGrid.onDeletePadRequested = [this]
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto targetIndex = padGrid.getSelectedPadIndex();
        if (targetIndex < 0 || targetIndex >= static_cast<int> (store.getActiveProfile().getPads().size()))
            return;
        const auto& removed = store.getActiveProfile().getPads()[static_cast<size_t> (targetIndex)];
        const auto removedNote = removed.midiNote;
        const auto removedChannel = removed.midiChannel;
        const auto result = store.removePadFromActive (targetIndex);

        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        audioProcessor.getEngine().clearPadHistogram (removedNote, removedChannel);
        clearAbCompare();
        undoCurveState.reset();
        undoCurveButton.setEnabled (false);
        applyProfileToEngine();
        const auto newIndex = juce::jlimit (0,
                                           static_cast<int> (store.getActiveProfile().getPads().size()) - 1,
                                           targetIndex);
        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (newIndex);
        showStatus ("Pad deleted.");
        audioProcessor.markStateDirty();
    };

    padGrid.onPadSwapRequested = [this] (int fromIndex, int toIndex)
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto result = store.swapPadsInActive (fromIndex, toIndex);
        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        undoCurveState.reset();
        undoCurveButton.setEnabled (false);
        applyProfileToEngine();
        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (toIndex);
        showStatus ("Pads swapped.");
        audioProcessor.markStateDirty();
    };

    padGrid.onPadMoveRequested = [this] (int index, int targetRow, int targetCol)
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto result = store.movePadInActive (index, targetRow, targetCol);
        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        applyProfileToEngine();
        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (index);
        showStatus ("Pad moved.");
        audioProcessor.markStateDirty();
    };

    padGrid.onPadDuplicateRequested = [this] (int sourceIndex, std::optional<std::pair<int, int>> targetCell)
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto result = store.duplicatePadInActive (sourceIndex, targetCell);
        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        applyProfileToEngine();
        const auto newIndex = static_cast<int> (store.getActiveProfile().getPads().size()) - 1;
        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (newIndex);
        padGrid.scrollPadIntoView (newIndex);
        showStatus ("Pad duplicated.");
        audioProcessor.markStateDirty();
    };

    padGrid.onPadRenamed = [this] (int index, const juce::String& newName)
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        const auto result = store.renamePadInActive (index, newName);
        if (result != svc::PadMutationResult::ok)
        {
            showStatus (padMutationMessage (result), true);
            return;
        }

        selectedPadIndex = -1;
        refreshPadUI (false);
        onPadSelected (index);
        showStatus ("Pad renamed.");
        audioProcessor.markStateDirty();
    };

    padGrid.onCopyCurveRequested = [this] (int index)
    {
        const auto& pads = audioProcessor.getProfileStore().getActiveProfile().getPads();
        if (index >= 0 && index < static_cast<int> (pads.size()))
        {
            clipboardCurve = pads[static_cast<size_t> (index)].curve;
            padGrid.setCanPasteCurve (true);
            showStatus ("Curve copied to clipboard.");
        }
    };

    padGrid.onPasteCurveRequested = [this] (int index)
    {
        if (! clipboardCurve.has_value())
            return;

        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        if (index < 0 || index >= static_cast<int> (store.getActiveProfile().getPads().size()))
            return;

        auto pad = store.getActiveProfile().getPads()[static_cast<size_t> (index)];
        pad.curve = *clipboardCurve;
        const auto res = store.getActiveProfile().setPadAt (index, pad);
        if (res == svc::PadMutationResult::ok)
        {
            store.syncActiveUserProfileFromEdits();
            applyProfileToEngine();
            selectedPadIndex = -1;
            refreshPadUI (false);
            onPadSelected (index);
            showStatus ("Curve pasted to pad.");
            audioProcessor.markStateDirty();
        }
        else
        {
            showStatus (padMutationMessage (res), true);
        }
    };

    padGrid.onResetCurveRequested = [this] (int index)
    {
        commitActivePadEdits();
        auto& store = audioProcessor.getProfileStore();
        if (index < 0 || index >= static_cast<int> (store.getActiveProfile().getPads().size()))
            return;

        auto pad = store.getActiveProfile().getPads()[static_cast<size_t> (index)];
        pad.curve = svc::VelocityCurve();
        const auto res = store.getActiveProfile().setPadAt (index, pad);
        if (res == svc::PadMutationResult::ok)
        {
            store.syncActiveUserProfileFromEdits();
            applyProfileToEngine();
            selectedPadIndex = -1;
            refreshPadUI (false);
            onPadSelected (index);
            showStatus ("Curve reset to linear.");
            audioProcessor.markStateDirty();
        }
        else
        {
            showStatus (padMutationMessage (res), true);
        }
    };

    padGrid.onLearnMidiRequested = [this] (int index)
    {
        padAwaitingMidiLearn = index;
        showStatus ("Press a MIDI pad to assign note...");
    };
    curveEditor.onPadChanged = [this] (const svc::ProfilePad& pad)
    {
        juce::ignoreUnused (pad);
        if (selectedPadIndex < 0)
            return;

        const auto merged = mergeActivePadFromUI();
        if (! tryUpdateSelectedPadFromUI (selectedPadIndex, merged, true))
            return;
    };

    curveEditor.onPadEditFinished = nullptr;

    padInspector.onPadChanged = [this] (int index, const svc::ProfilePad& inspectorPad)
    {
        juce::ignoreUnused (inspectorPad);
        auto pad = (index == selectedPadIndex) ? mergeActivePadFromUI() : padInspector.getPad();

        if (! tryUpdateSelectedPadFromUI (index, pad, true))
            return;

        if (index == selectedPadIndex)
        {
            const auto& committed = audioProcessor.getProfileStore().getActiveProfile().getPads()[static_cast<size_t> (index)];
            curveEditor.setPad (committed, false);
            padHistogram.setTitle ("Pad: " + committed.label);
        }
    };

    padInspector.onPadEditFinished = [this]
    {
        if (selectedPadIndex < 0)
            return;

        const auto& pad = audioProcessor.getProfileStore().getActiveProfile().getPads()[static_cast<size_t> (selectedPadIndex)];
        audioProcessor.syncPadToEngine (pad);
        syncAbAuditionIfActive();
    };

    padInspector.onEditAftertouchRequested = [this]
    {
        if (curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch)
            curveEditor.setEditTarget (CurveEditorComponent::EditTarget::velocity);
        else
            curveEditor.setEditTarget (CurveEditorComponent::EditTarget::aftertouch);

        syncCurveEditTargetUI();
        showStatus (curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch
                        ? "Editing aftertouch curve - use the same button to return to velocity."
                        : "Editing velocity curve.");
    };

    calibrationSection.onExpandedChanged = [this] (bool expanded)
    {
        if (expanded)
            calibrationWizard.reset();
    };

    profileBox.onChange = [this]
    {
        if (suppressProfileBoxChange)
            return;

        attemptProfileSwitch (profileBox.getSelectedId());
    };

    curvePresetBox.onChange = [this]
    {
        curveEditor.applyPreset (static_cast<svc::CurvePreset> (curvePresetBox.getSelectedItemIndex()));
        tryUpdateSelectedPadFromUI (selectedPadIndex, curveEditor.getPad());
    };

    resetCurveButton.onClick = [this] { curveEditor.resetCurve(); };

    copyCurveButton.onClick = [this]
    {
        const bool isAt = curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch;
        clipboardCurve = isAt ? curveEditor.getPad().aftertouch.curve : curveEditor.getPad().curve;
        showStatus (isAt ? "Aftertouch curve copied." : "Curve copied.");
    };

    pasteCurveButton.onClick = [this]
    {
        if (! clipboardCurve.has_value())
        {
            showStatus ("Nothing to paste.", true);
            return;
        }

        pushCurveUndo();
        const bool isAt = curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch;
        auto pad = curveEditor.getPad();
        if (isAt)
            pad.aftertouch.curve = *clipboardCurve;
        else
            pad.curve = *clipboardCurve;

        tryUpdateSelectedPadFromUI (selectedPadIndex, pad);
        curveEditor.setPad (pad, false);
        showStatus (isAt ? "Aftertouch curve pasted." : "Curve pasted.");
    };

    pasteGroupButton.onClick = [this]
    {
        if (! clipboardCurve.has_value())
        {
            showStatus ("Copy a curve first.", true);
            return;
        }

        pushCurveUndo();
        auto& profile = audioProcessor.getProfileStore().getActiveProfile();
        if (selectedPadIndex < 0 || selectedPadIndex >= static_cast<int> (profile.getPads().size()))
            return;

        const bool isAt = curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch;
        const auto group = profile.getPads()[static_cast<size_t> (selectedPadIndex)].group;
        int count = 0;
        for (auto& pad : profile.getPads())
        {
            if (pad.group == group)
            {
                if (isAt)
                    pad.aftertouch.curve = *clipboardCurve;
                else
                    pad.curve = *clipboardCurve;
                ++count;
            }
        }

        audioProcessor.getProfileStore().syncActiveUserProfileFromEdits();
        applyProfileToEngine();
        refreshPadUI();
        showStatus ("Pasted " + juce::String (isAt ? "aftertouch " : "") + "curve to " + juce::String (count) + " pads in group " + svc::padGroupToString (group));
        audioProcessor.markStateDirty();
    };

    captureAbButton.onClick = [this]
    {
        const bool isAt = curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch;
        curveA = isAt ? curveEditor.getPad().aftertouch.curve : curveEditor.getPad().curve;
        hearingCurveA = false;
        abToggleButton.setButtonText ("Hear A");
        curveEditor.clearCompareCurve();
        applyProfileToEngine();
        captureAbButton.setColour (juce::TextButton::buttonOnColourId,
                                   juce::Colour (svc::ui::Theme::accentDim()));
        showStatus ("Captured A. Edit the curve, then click Hear A to switch audition.");
    };

    abToggleButton.setTooltip ("Toggle audition between captured curve A and your current edits (B).");
    abToggleButton.onClick = [this] { toggleAbCurve(); };

    clearHistogramButton.onClick = [this]
    {
        audioProcessor.getEngine().clearHistogram();
        updateHistograms();
    };

    calibrationWizard.onCurveCalibrated = [this] (const svc::VelocityCurve& curve)
    {
        pushCurveUndo();
        auto pad = curveEditor.getPad();
        pad.curve = curve;
        tryUpdateSelectedPadFromUI (selectedPadIndex, pad);
        curveEditor.setPad (pad, false);
        showStatus ("Calibration applied.");
    };

    midiRoutingPanel.onRoutingChanged = [this]
    {
        audioProcessor.getProfileStore().syncActiveUserProfileFromEdits();
        audioProcessor.syncRoutingToEngine();
        refreshRoutingPanels();
        audioProcessor.markStateDirty();
    };

    noteRemapEditor.onRemapsChanged = [this]
    {
        audioProcessor.getProfileStore().syncActiveUserProfileFromEdits();
        audioProcessor.syncRoutingToEngine();
        refreshRoutingPanels();
        audioProcessor.markStateDirty();
    };

    saveProfileButton.onClick = [this]
    {
        commitActivePadEdits();
        juce::String saveError;
        if (saveProfileFromUI (&saveError))
        {
            applyProfileToEngine();
            showStatus ("Profile saved.");
        }
        else
            showStatus (saveError.isNotEmpty() ? saveError : "Save failed.", true);
    };

    duplicateProfileButton.onClick = [this]
    {
        commitActivePadEdits();
        juce::String dupError;
        if (! svc::ProfileStore::validateProfileMidiKeys (audioProcessor.getProfileStore().getActiveProfile(), &dupError))
        {
            showStatus (dupError, true);
            return;
        }

        if (audioProcessor.getProfileStore().duplicateActiveAsUserProfile ({ }))
        {
            rebuildProfileList();
            captureProfileBaseline();
            showStatus ("Profile duplicated.");
        }
    };

    deleteProfileButton.onClick = [this]
    {
        auto& store = audioProcessor.getProfileStore();
        if (store.getActiveEntryType() != svc::ProfileEntryType::userProfile)
        {
            showStatus ("Only user profiles can be deleted.", true);
            return;
        }

        if (store.deleteUserProfile (store.getActiveEntryIndex()))
        {
            rebuildProfileList();
            onPadSelected (0);
            applyProfileToEngine();
            showStatus ("Profile deleted.");
        }
    };

    importButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Import profile", juce::File{}, "*.xml;*.svcp");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  juce::String importError;
                                  if (file.existsAsFile()
                                      && audioProcessor.getProfileStore().importProfileFromFile (file, &importError))
                                  {
                                      profileNameEditor.setText (audioProcessor.getProfileStore().getActiveProfile().getName(), juce::dontSendNotification);
                                      rebuildProfileList();
                                      refreshRoutingPanels();
                                      onPadSelected (0);
                                      applyProfileToEngine();
                                      captureProfileBaseline();
                                      showStatus ("Imported " + file.getFileName());
                                  }
                                  else if (file != juce::File())
                                      showStatus (importError.isNotEmpty() ? importError : "Import failed.", true);
                              });
    };

    exportButton.onClick = [this]
    {
        commitActivePadEdits();
        auto chooser = std::make_shared<juce::FileChooser> ("Export profile", juce::File{}, "*.svcp");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& fc)
                              {
                                  auto file = fc.getResult();
                                  if (file == juce::File())
                                      return;

                                  if (! file.hasFileExtension (".svcp") && ! file.hasFileExtension (".xml"))
                                      file = file.withFileExtension (".svcp");

                                  juce::String exportError;
                                  if (audioProcessor.getProfileStore().exportActiveProfileToFile (file, &exportError))
                                      showStatus ("Exported to " + file.getFileName());
                                  else
                                      showStatus (exportError.isNotEmpty() ? exportError : "Export failed.", true);
                              });
    };

    audioProcessor.getProfileStore().onProfileChanged = [this]
    {
        if (suppressProfileStoreNotifications > 0)
            return;

        rebuildProfileList();
        refreshPadUI();
        refreshRoutingPanels();
        audioProcessor.markStateDirty();
    };

    finishEditorStartup();
}

void SuperVelocityCurveAudioProcessorEditor::finishEditorStartup()
{
    if (editorStartupComplete)
        return;

    editorStartupComplete = true;
    applyThemeFromUI();
    profileNameEditor.setText (audioProcessor.getProfileStore().getActiveProfile().getName(), juce::dontSendNotification);
    rebuildProfileList();
    refreshRoutingPanels();
    refreshPadUI (true);
    captureProfileBaseline();
    refreshThemedComponents();

    if (isShowing())
        syncUiTimer();
}

bool SuperVelocityCurveAudioProcessorEditor::needsAnimatedUi() const noexcept
{
    if (padGrid.hasActiveHitVisuals())
        return true;

    if (midiMeters.hasVisibleLevels())
        return true;

    if (audioProcessor.getEngine().getHitFifo().hasPending())
        return true;

    return false;
}

void SuperVelocityCurveAudioProcessorEditor::syncUiTimer() noexcept
{
    if (! isShowing())
    {
        stopTimer();
        uiTimerHz = 0;
        return;
    }

    // Keep polling while visible so MIDI hits are not stranded in the FIFO when idle.
    const auto targetHz = needsAnimatedUi() ? 30 : 15;

    if (! isTimerRunning() || uiTimerHz != targetHz)
    {
        startTimerHz (targetHz);
        uiTimerHz = targetHz;
    }
}

void SuperVelocityCurveAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    juce::AudioProcessorEditor::mouseMove (event);

    if (! isTimerRunning()
        && (audioProcessor.getEngine().getHitFifo().hasPending() || needsAnimatedUi()))
    {
        timerCallback();
        syncUiTimer();
    }
}

void SuperVelocityCurveAudioProcessorEditor::applyThemeFromUI()
{
    const auto mode = idToThemeMode (themeBox.getSelectedId());
    audioProcessor.setTheme (mode);
    appLookAndFeel.refreshTheme();
    refreshThemedComponents();
    repaintThemedCanvases();
    sendLookAndFeelChange();
}

void SuperVelocityCurveAudioProcessorEditor::showAboutPanel()
{
    if (aboutPanel != nullptr)
        return;

    aboutPanel = std::make_unique<AboutPanelComponent>();
    aboutPanel->applyTheme();
    aboutPanel->onDismiss = [this] { hideAboutPanel(); };
    addAndMakeVisible (*aboutPanel);
    aboutPanel->setBounds (getLocalBounds());
    aboutPanel->toFront (true);
}

void SuperVelocityCurveAudioProcessorEditor::hideAboutPanel()
{
    aboutPanel.reset();
}

void SuperVelocityCurveAudioProcessorEditor::refreshThemedComponents()
{
    const auto primary = juce::Colour (svc::ui::Theme::textPrimary());
    const auto secondary = juce::Colour (svc::ui::Theme::textSecondary());
    const auto muted = juce::Colour (svc::ui::Theme::textMuted());

    titleLabel.setColour (juce::Label::textColourId, primary);
    subtitleLabel.setColour (juce::Label::textColourId, muted);
    for (auto* label : { &profileLabel, &outputModeLabel, &presetLabel, &themeLabel })
        label->setColour (juce::Label::textColourId, primary);
    liveHitsLabel.setColour (juce::Label::textColourId, secondary);

    statusLabel.setFont (svc::ui::Theme::smallFont());
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    for (auto* label : { &titleLabel, &subtitleLabel, &profileLabel, &outputModeLabel, &presetLabel,
                         &themeLabel, &liveHitsLabel, &statusLabel })
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    svc::ui::applyTextEditorTheme (profileNameEditor);

    for (auto* box : { &profileBox, &outputModeBox, &curvePresetBox, &themeBox })
        svc::ui::applyComboBoxTheme (*box);

    midiToolsTabs.setTabBackgroundColour (0, juce::Colour (svc::ui::Theme::panel()));
    midiToolsTabs.setTabBackgroundColour (1, juce::Colour (svc::ui::Theme::panel()));
    padInspector.applyTheme();
    midiRoutingPanel.applyTheme();
    calibrationWizard.applyTheme();
    auditionToggle.setColour (juce::ToggleButton::textColourId, primary);

    if (aboutPanel != nullptr)
        aboutPanel->applyTheme();
}

void SuperVelocityCurveAudioProcessorEditor::repaintThemedCanvases()
{
    curveEditor.repaint();
    padGrid.refreshVisualCache();
    padGridResizer.repaint();
    auditionToggle.repaint();
    undoCurveButton.repaint();
    padHistogram.repaint();
    globalHistogram.repaint();

    for (auto* c : { static_cast<juce::Component*> (&midiMeters),
                     static_cast<juce::Component*> (&calibrationWizard),
                     static_cast<juce::Component*> (&noteRemapEditor),
                     static_cast<juce::Component*> (&midiRoutingPanel),
                     static_cast<juce::Component*> (&histogramSection),
                     static_cast<juce::Component*> (&midiToolsSection),
                     static_cast<juce::Component*> (&calibrationSection),
                     static_cast<juce::Component*> (&padSettingsSection) })
        c->repaint();

    padInspector.applyTheme();
    repaint();
}

void SuperVelocityCurveAudioProcessorEditor::paint (juce::Graphics& g)
{
    svc::ui::Theme::fillBackground (g, getLocalBounds());

    // Sleek 1px divider below the top header ribbon
    g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.45f));
    g.drawHorizontalLine (50, 0.0f, static_cast<float> (getWidth()));

    // Hardware surface card behind Profile section
    if (! toolbarProfileBounds.isEmpty())
    {
        svc::ui::Theme::fillPanel (g, toolbarProfileBounds.toFloat(), 6.0f);
        g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.65f));
        g.drawRoundedRectangle (toolbarProfileBounds.toFloat(), 6.0f, 1.0f);
    }

    // Hardware surface card behind Preset/Performance section
    if (! toolbarRightBounds.isEmpty())
    {
        svc::ui::Theme::fillPanel (g, toolbarRightBounds.toFloat(), 6.0f);
        g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.65f));
        g.drawRoundedRectangle (toolbarRightBounds.toFloat(), 6.0f, 1.0f);
    }
}

void SuperVelocityCurveAudioProcessorEditor::clearStatus()
{
    statusClearTicksRemaining = 0;
    statusMessage.clear();
    statusIsError = false;
    statusLabel.setVisible (false);
}

void SuperVelocityCurveAudioProcessorEditor::scheduleStatusClear()
{
    statusClearTicksRemaining = 180; // ~6 seconds at 30Hz editor timer
}

int SuperVelocityCurveAudioProcessorEditor::bottomSectionsHeight() const noexcept
{
    svc::ui::layout::SectionStackHeights sections;
    sections.histogram = histogramSection.getTotalHeight();
    sections.midiTools = midiToolsSection.getTotalHeight();
    sections.calibration = calibrationSection.getTotalHeight();
    return svc::ui::layout::bottomSectionsTotalHeight (sections);
}

void SuperVelocityCurveAudioProcessorEditor::layoutBottomSections (juce::Rectangle<int> area)
{
    area = area.reduced (4, 0);
    histogramSection.setBounds (area.removeFromTop (histogramSection.getTotalHeight()));
    area.removeFromTop (2);
    midiToolsSection.setBounds (area.removeFromTop (midiToolsSection.getTotalHeight()));
    area.removeFromTop (2);
    calibrationSection.setBounds (area.removeFromTop (calibrationSection.getTotalHeight()));
}

void SuperVelocityCurveAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (50).reduced (16, 4);

    // Right header controls: About and Theme
    auto rightHeader = header.removeFromRight (210);
    aboutButton.setBounds (rightHeader.removeFromRight (64).withSizeKeepingCentre (64, 24));
    rightHeader.removeFromRight (8);
    themeBox.setBounds (rightHeader.removeFromRight (86).withSizeKeepingCentre (86, 24));
    rightHeader.removeFromRight (6);
    themeLabel.setBounds (rightHeader.withSizeKeepingCentre (rightHeader.getWidth(), 24));

    // Left header branding
    titleLabel.setBounds (header.removeFromTop (24));
    subtitleLabel.setBounds (header.removeFromTop (18));

    auto bounds = area.reduced (12).withTrimmedBottom (22);

    if (standaloneMidiPanel != nullptr)
    {
        standaloneMidiPanel->setBounds (bounds.removeFromTop (88));
        bounds.removeFromTop (4);
    }

    auto toolbar = bounds.removeFromTop (118);
    toolbarProfileBounds = toolbar.removeFromLeft (toolbar.getWidth() / 2).reduced (2);
    toolbarRightBounds = toolbar.reduced (2);

    // Profile card contents
    auto col = toolbarProfileBounds.reduced (6, 4);
    profileLabel.setBounds (col.removeFromTop (14));
    profileBox.setBounds (col.removeFromTop (22));
    col.removeFromTop (3);
    profileNameEditor.setBounds (col.removeFromTop (22));
    col.removeFromTop (3);
    auto profileButtons = col.removeFromTop (24);
    const int btnW = profileButtons.getWidth() / 5;
    saveProfileButton.setBounds (profileButtons.removeFromLeft (btnW).reduced (1));
    duplicateProfileButton.setBounds (profileButtons.removeFromLeft (btnW).reduced (1));
    deleteProfileButton.setBounds (profileButtons.removeFromLeft (btnW).reduced (1));
    importButton.setBounds (profileButtons.removeFromLeft (btnW).reduced (1));
    exportButton.setBounds (profileButtons.reduced (1));

    // Performance / Preset card contents
    auto rightToolbar = toolbarRightBounds.reduced (6, 4);
    auto topRow = rightToolbar.removeFromTop (40);
    auto outCol = topRow.removeFromLeft (topRow.getWidth() / 2).reduced (0, 0);
    outputModeLabel.setBounds (outCol.removeFromTop (14));
    outputModeBox.setBounds (outCol.removeFromTop (22));
    auto auditionCol = topRow.removeFromRight (86);
    auditionToggle.setBounds (auditionCol.removeFromBottom (24).reduced (1, 0));
    midiMeters.setBounds (topRow.reduced (2, 0));

    auto presetCol = rightToolbar;
    presetLabel.setBounds (presetCol.removeFromTop (14));
    auto presetRow1 = presetCol.removeFromTop (22);
    curvePresetBox.setBounds (presetRow1.removeFromLeft (presetRow1.getWidth() / 2).reduced (0, 0));
    undoCurveButton.setBounds (presetRow1.removeFromLeft (48).reduced (1));
    clearHistogramButton.setBounds (presetRow1.reduced (1));
    auto presetRow2 = presetCol;
    const int smallBtn = juce::jmax (38, presetRow2.getWidth() / 7);
    resetCurveButton.setBounds (presetRow2.removeFromLeft (smallBtn).reduced (1));
    copyCurveButton.setBounds (presetRow2.removeFromLeft (smallBtn).reduced (1));
    pasteCurveButton.setBounds (presetRow2.removeFromLeft (smallBtn).reduced (1));
    pasteGroupButton.setBounds (presetRow2.removeFromLeft (smallBtn + 6).reduced (1));
    captureAbButton.setBounds (presetRow2.removeFromLeft (smallBtn + 6).reduced (1));
    abToggleButton.setBounds (presetRow2.reduced (1));

    auto liveRow = bounds.removeFromTop (20);
    liveHitsLabel.setBounds (liveRow.reduced (4, 0));

    const auto bottomHeight = bottomSectionsHeight();
    layoutBottomSections (bounds.removeFromBottom (bottomHeight));

    svc::ui::layout::EditorLayoutInputs layoutInputs;
    layoutInputs.editorBounds = getLocalBounds();
    layoutInputs.hasStandaloneMidiPanel = standaloneMidiPanel != nullptr;
    layoutInputs.padSettingsExpanded = padSettingsSection.isExpanded();
    layoutInputs.bottomSections.histogram = histogramSection.getTotalHeight();
    layoutInputs.bottomSections.midiTools = midiToolsSection.getTotalHeight();
    layoutInputs.bottomSections.calibration = calibrationSection.getTotalHeight();
    layoutInputs.customPadGridWidth = audioProcessor.getCustomPadGridWidth();

    const auto layout = svc::ui::layout::computeEditorLayout (layoutInputs);
    padGrid.setBounds (layout.padGridBounds);
    padGridResizer.setBounds (layout.splitterBounds);
    padSettingsSection.setBounds (layout.padSettingsBounds);
    curveEditor.setBounds (layout.curveEditorBounds);

    if (aboutPanel != nullptr)
    {
        aboutPanel->setBounds (getLocalBounds());
        aboutPanel->toFront (true);
    }

    if (unsavedProfileDialog != nullptr)
    {
        unsavedProfileDialog->setBounds (getLocalBounds());
        unsavedProfileDialog->toFront (true);
    }

    statusLabel.setBounds (getLocalBounds().removeFromBottom (22).reduced (16, 2));
}

void SuperVelocityCurveAudioProcessorEditor::rebuildProfileList()
{
    const juce::ScopedValueSetter<bool> guard (suppressProfileBoxChange, true);
    profileBox.clear (juce::dontSendNotification);

    const auto entries = audioProcessor.getProfileStore().getProfileList();

    int selectedId = 1;
    int id = 1;
    for (const auto& entry : entries)
    {
        profileBox.addItem (entry.displayName, id);
        if (entry.type == audioProcessor.getProfileStore().getActiveEntryType()
            && entry.index == audioProcessor.getProfileStore().getActiveEntryIndex())
            selectedId = id;
        ++id;
    }

    profileBox.setSelectedId (selectedId, juce::dontSendNotification);
}

void SuperVelocityCurveAudioProcessorEditor::refreshRoutingPanels()
{
    auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    midiRoutingPanel.setProfile (profile);
    noteRemapEditor.setProfile (profile);
}

bool SuperVelocityCurveAudioProcessorEditor::isPadMapped (int note, int channel) const
{
    return audioProcessor.getProfileStore().getActiveProfile().findPadIndex (note, channel) >= 0;
}

void SuperVelocityCurveAudioProcessorEditor::captureProfileBaseline()
{
    profileBaseline = audioProcessor.getProfileStore().getActiveProfile().copy();
    profileBaselineName = profileNameEditor.getText().trim();
}

bool SuperVelocityCurveAudioProcessorEditor::isProfileDirty() const
{
    const auto& store = audioProcessor.getProfileStore();
    const auto currentTree = store.getActiveProfile().toValueTree();
    const auto baselineTree = profileBaseline.toValueTree();
    const bool padsOrRoutingDirty = ! currentTree.isEquivalentTo (baselineTree);

    if (store.getActiveEntryType() == svc::ProfileEntryType::factoryTemplate)
    {
        // Profile name field is the "Save as" name for templates, not template identity.
        return padsOrRoutingDirty;
    }

    if (profileNameEditor.getText().trim() != profileBaselineName)
        return true;

    return padsOrRoutingDirty;
}

bool SuperVelocityCurveAudioProcessorEditor::saveProfileFromUI (juce::String* errorMessage)
{
    commitActivePadEdits();
    const auto name = profileNameEditor.getText().trim();
    auto& store = audioProcessor.getProfileStore();
    const bool saved = store.getActiveEntryType() == svc::ProfileEntryType::userProfile
                           ? store.updateActiveUserProfile (name, errorMessage)
                           : store.saveActiveAsUserProfile (name, errorMessage);

    if (saved)
    {
        rebuildProfileList();
        captureProfileBaseline();
    }

    return saved;
}

void SuperVelocityCurveAudioProcessorEditor::revertActiveProfileFromStore()
{
    auto& store = audioProcessor.getProfileStore();
    if (store.getActiveEntryType() == svc::ProfileEntryType::userProfile)
        store.loadUserProfile (store.getActiveEntryIndex());
    else
        store.loadFactoryTemplate (store.getActiveEntryIndex());
}

void SuperVelocityCurveAudioProcessorEditor::attemptProfileSwitch (int profileBoxId)
{
    if (profileBoxId <= 0)
        return;

    commitActivePadEdits();

    if (! isProfileDirty())
    {
        performProfileSwitch (profileBoxId);
        return;
    }

    pendingProfileBoxId = profileBoxId;
    rebuildProfileList();
    showUnsavedProfileDialog();
}

void SuperVelocityCurveAudioProcessorEditor::showUnsavedProfileDialog()
{
    if (unsavedProfileDialog != nullptr)
        return;

    unsavedProfileDialog = std::make_unique<UnsavedProfileDialogComponent>();
    unsavedProfileDialog->onChoice = [this] (UnsavedProfileDialogComponent::Choice choice)
    {
        handleUnsavedProfileChoice (choice);
    };
    addAndMakeVisible (*unsavedProfileDialog);
    unsavedProfileDialog->setBounds (getLocalBounds());
    unsavedProfileDialog->toFront (true);
}

void SuperVelocityCurveAudioProcessorEditor::hideUnsavedProfileDialog()
{
    unsavedProfileDialog.reset();
}

void SuperVelocityCurveAudioProcessorEditor::handleUnsavedProfileChoice (
    UnsavedProfileDialogComponent::Choice choice)
{
    const auto targetId = pendingProfileBoxId;
    hideUnsavedProfileDialog();

    if (choice == UnsavedProfileDialogComponent::Choice::cancel)
    {
        pendingProfileBoxId = 0;
        return;
    }

    if (choice == UnsavedProfileDialogComponent::Choice::save)
    {
        commitActivePadEdits();
        juce::String saveError;
        if (! saveProfileFromUI (&saveError))
        {
            showStatus (saveError.isNotEmpty() ? saveError : "Save failed.", true);
            return;
        }
        performProfileSwitch (targetId);
        return;
    }

    revertActiveProfileFromStore();
    performProfileSwitch (targetId);
}

void SuperVelocityCurveAudioProcessorEditor::performProfileSwitch (int profileBoxId)
{
    if (profileBoxId <= 0)
        return;

    const auto entries = audioProcessor.getProfileStore().getProfileList();
    const auto index = profileBoxId - 1;
    if (index < 0 || index >= static_cast<int> (entries.size()))
        return;

    const juce::ScopedValueSetter<int> profileNotifyGuard (suppressProfileStoreNotifications,
                                                           suppressProfileStoreNotifications + 1);

    const auto& entry = entries[static_cast<size_t> (index)];
    if (entry.type == svc::ProfileEntryType::factoryTemplate)
        audioProcessor.getProfileStore().loadFactoryTemplate (entry.index);
    else
        audioProcessor.getProfileStore().loadUserProfile (entry.index);

    profileNameEditor.setText (audioProcessor.getProfileStore().getActiveProfile().getName(),
                               juce::dontSendNotification);
    svc::ui::applyTextEditorTheme (profileNameEditor);
    clearAbCompare();
    curveEditor.setEditTarget (CurveEditorComponent::EditTarget::velocity);
    undoCurveState.reset();
    undoCurveButton.setEnabled (false);

    padAwaitingMidiLearn = -1;
    rebuildProfileList();
    refreshPadUI (true);
    applyProfileToEngine();
    refreshRoutingPanels();
    captureProfileBaseline();
    pendingProfileBoxId = 0;
}

void SuperVelocityCurveAudioProcessorEditor::onProfileSelected()
{
    attemptProfileSwitch (profileBox.getSelectedId());
}

svc::ProfilePad SuperVelocityCurveAudioProcessorEditor::mergeActivePadFromUI() const
{
    return svc::ui::mergePadFromCurveAndInspector (curveEditor.getPad(), padInspector.getPad());
}

void SuperVelocityCurveAudioProcessorEditor::commitActivePadEdits()
{
    if (selectedPadIndex < 0)
        return;

    padInspector.commitEdits();
    tryUpdateSelectedPadFromUI (selectedPadIndex, mergeActivePadFromUI(), true);
}

void SuperVelocityCurveAudioProcessorEditor::onPadSelected (int padIndex)
{
    const auto& pads = audioProcessor.getProfileStore().getActiveProfile().getPads();
    if (pads.empty())
        return;

    padIndex = juce::jlimit (0, static_cast<int> (pads.size()) - 1, padIndex);
    if (padIndex == selectedPadIndex)
        return;

    if (padAwaitingMidiLearn >= 0 && padAwaitingMidiLearn != padIndex)
        padAwaitingMidiLearn = -1;

    commitActivePadEdits();
    selectedPadIndex = padIndex;
    padGrid.setSelectedPadIndex (padIndex);
    calibrationWizard.reset();
    curveEditor.setEditTarget (CurveEditorComponent::EditTarget::velocity);
    clearAbCompare();
    showPadAtIndex (padIndex);
}

void SuperVelocityCurveAudioProcessorEditor::refreshPadUI (bool resetPadSelection)
{
    const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    padGrid.setProfile (profile, resetPadSelection);

    if (resetPadSelection)
        selectedPadIndex = profile.getPads().empty() ? -1 : 0;

    if (selectedPadIndex >= 0 && selectedPadIndex < static_cast<int> (profile.getPads().size()))
        showPadAtIndex (selectedPadIndex);
}

void SuperVelocityCurveAudioProcessorEditor::syncCurveEditTargetUI()
{
    padInspector.setAftertouchEditMode (curveEditor.getEditTarget() == CurveEditorComponent::EditTarget::aftertouch);
}

void SuperVelocityCurveAudioProcessorEditor::showPadAtIndex (int padIndex)
{
    const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    if (padIndex < 0 || padIndex >= static_cast<int> (profile.getPads().size()))
        return;

    selectedPadIndex = padIndex;
    padGrid.setSelectedPadIndex (padIndex);

    const auto& pad = profile.getPads()[static_cast<size_t> (padIndex)];
    curveEditor.setPad (pad, false);
    padInspector.setPad (pad, padIndex);
    padHistogram.setTitle ("Pad: " + pad.label);
    syncCurveEditTargetUI();
}

bool SuperVelocityCurveAudioProcessorEditor::tryUpdateSelectedPadFromUI (int padIndex,
                                                                          const svc::ProfilePad& pad,
                                                                          bool syncEngine)
{
    auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    if (padIndex < 0 || padIndex >= static_cast<int> (profile.getPads().size()))
        return false;

    const auto result = profile.setPadAt (padIndex, pad);
    if (result != svc::PadMutationResult::ok)
    {
        showStatus (padMutationMessage (result), true);
        showPadAtIndex (padIndex);
        return false;
    }

    audioProcessor.getProfileStore().syncActiveUserProfileFromEdits();
    padGrid.updatePad (padIndex, pad);
    audioProcessor.markStateDirty();

    if (syncEngine)
    {
        audioProcessor.syncPadToEngine (pad);
        syncAbAuditionIfActive();
    }

    return true;
}

void SuperVelocityCurveAudioProcessorEditor::applyProfileToEngine()
{
    audioProcessor.applyProfileToEngine();
    syncAbAuditionIfActive();
}

void SuperVelocityCurveAudioProcessorEditor::syncAbAuditionIfActive()
{
    if (! curveA.has_value() || ! abToggleButton.getButtonText().contains ("Hearing"))
        return;

    if (hearingCurveA)
        applyListenCurveToEngine (*curveA);
    else
        applyListenCurveToEngine (curveEditor.getPad().curve);
}

void SuperVelocityCurveAudioProcessorEditor::applyListenCurveToEngine (const svc::VelocityCurve& curve)
{
    const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    if (selectedPadIndex < 0 || selectedPadIndex >= static_cast<int> (profile.getPads().size()))
        return;

    auto pad = profile.getPads()[static_cast<size_t> (selectedPadIndex)];
    pad.curve = curve;
    audioProcessor.syncPadToEngine (pad);
}

void SuperVelocityCurveAudioProcessorEditor::clearAbCompare()
{
    curveA.reset();
    hearingCurveA = false;
    abToggleButton.setButtonText ("Hear A");
    curveEditor.clearCompareCurve();
    curveEditor.clearDisplayCurve();
    captureAbButton.removeColour (juce::TextButton::buttonOnColourId);
    applyProfileToEngine();
}

void SuperVelocityCurveAudioProcessorEditor::toggleAbCurve()
{
    if (! curveA.has_value())
    {
        showStatus ("Capture A first.", true);
        return;
    }

    const auto working = curveEditor.getPad().curve;
    hearingCurveA = ! hearingCurveA;

    if (hearingCurveA)
    {
        applyListenCurveToEngine (*curveA);
        curveEditor.setDisplayCurve (&*curveA);
        curveEditor.setCompareCurve (&working);
        curveEditor.setIsAuditioningCompare (true);
        abToggleButton.setButtonText ("Hearing A");
        showStatus ("Auditioning captured A (blue = A, gold = your edits). Play pads to hear.");
    }
    else
    {
        applyListenCurveToEngine (working);
        curveEditor.clearDisplayCurve();
        curveEditor.setCompareCurve (&*curveA);
        curveEditor.setIsAuditioningCompare (false);
        abToggleButton.setButtonText ("Hearing B");
        showStatus ("Auditioning your edits (blue = edits, gold = captured A). Play pads to hear.");
    }
}

void SuperVelocityCurveAudioProcessorEditor::showStatus (const juce::String& message, bool isError)
{
    statusMessage = message;
    statusIsError = isError;
    statusLabel.setText (message, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           juce::Colour (isError ? svc::ui::Theme::error() : svc::ui::Theme::success()));
    statusLabel.setVisible (message.isNotEmpty());
    statusLabel.toFront (false);
    scheduleStatusClear();
}

void SuperVelocityCurveAudioProcessorEditor::updateHistograms()
{
    const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    if (selectedPadIndex >= 0 && selectedPadIndex < static_cast<int> (profile.getPads().size()))
    {
        const auto& pad = profile.getPads()[static_cast<size_t> (selectedPadIndex)];
        padHistogram.setHistogram (audioProcessor.getEngine().getPadHistogramSnapshot (pad.midiNote, pad.midiChannel));
    }

    globalHistogram.setHistogram (audioProcessor.getEngine().getGlobalHistogramSnapshot());
}

void SuperVelocityCurveAudioProcessorEditor::updateLiveHits()
{
    svc::HitEvent hit;
    std::vector<svc::HitEvent> drained;
    drained.reserve (32);

    while (audioProcessor.getEngine().getHitFifo().pop (hit))
        drained.push_back (hit);

    constexpr int maxDisplayHits = 12;
    const int displayStart = juce::jmax (0, static_cast<int> (drained.size()) - maxDisplayHits);

    juce::String text;
    bool sawUnmapped = false;
    int count = 0;

    for (int i = displayStart; i < static_cast<int> (drained.size()); ++i)
    {
        const auto& displayed = drained[static_cast<size_t> (i)];
        const auto inVel = juce::String (static_cast<int> (std::lround (displayed.inputVelocity * 127.0f)));
        const auto outVel = juce::String (static_cast<int> (std::lround (displayed.outputVelocity * 127.0f)));
        const auto protocol = displayed.isMidi2 ? "M2" : "M1";
        text += "N" + juce::String (displayed.note) + " " + protocol + " " + inVel + "->" + outVel + "   ";

        curveEditor.addHitMarker (displayed.note, displayed.channel,
                                  displayed.inputVelocity, displayed.outputVelocity, displayed.isMidi2);
        padGrid.flashPadHit (displayed.note, displayed.channel, displayed.outputVelocity);

        if (padAwaitingMidiLearn >= 0)
        {
            const int targetPad = padAwaitingMidiLearn;
            padAwaitingMidiLearn = -1;
            commitActivePadEdits();
            auto& store = audioProcessor.getProfileStore();
            if (targetPad >= 0 && targetPad < static_cast<int> (store.getActiveProfile().getPads().size()))
            {
                auto pad = store.getActiveProfile().getPads()[static_cast<size_t> (targetPad)];
                pad.midiNote = displayed.note;
                pad.midiChannel = displayed.channel;
                const auto res = store.getActiveProfile().setPadAt (targetPad, pad);
                if (res == svc::PadMutationResult::ok)
                {
                    store.syncActiveUserProfileFromEdits();
                    applyProfileToEngine();
                    selectedPadIndex = -1;
                    refreshPadUI (false);
                    onPadSelected (targetPad);
                    showStatus ("MIDI note " + juce::String (displayed.note) + " assigned to pad.");
                    audioProcessor.markStateDirty();
                }
                else
                {
                    showStatus (padMutationMessage (res), true);
                }
            }
        }

        if (calibrationSection.isExpanded() && selectedPadIndex >= 0)
        {
            const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
            const auto& pad = profile.getPads()[static_cast<size_t> (selectedPadIndex)];
            if (displayed.note == pad.midiNote && displayed.channel == pad.midiChannel)
                calibrationWizard.captureHit (displayed.inputVelocity);
        }

        midiMeters.pushInputLevel (displayed.inputVelocity);
        midiMeters.pushOutputLevel (displayed.outputVelocity);

        if (! isPadMapped (displayed.note, displayed.channel))
            sawUnmapped = true;

        ++count;
    }

    if (count > 0)
    {
        if (sawUnmapped)
            text += "  [unmapped note]";
        persistedLiveHitsText = text;
        liveHitsDisplayTicksRemaining = 60;
    }
    else if (liveHitsDisplayTicksRemaining > 0)
    {
        --liveHitsDisplayTicksRemaining;
        text = persistedLiveHitsText;
    }
    else
    {
        text = "Play your controller - live input->output velocity appears here";
    }

    if (text != lastLiveHitsText)
    {
        lastLiveHitsText = text;
        liveHitsLabel.setText (text, juce::dontSendNotification);
    }

    if (count > 0)
        updateHistograms();
}

void SuperVelocityCurveAudioProcessorEditor::parentHierarchyChanged()
{
    juce::AudioProcessorEditor::parentHierarchyChanged();

    if (isShowing())
    {
        refreshThemedComponents();
        syncUiTimer();
    }
}

void SuperVelocityCurveAudioProcessorEditor::visibilityChanged()
{
    juce::AudioProcessorEditor::visibilityChanged();

    if (isShowing())
    {
        refreshThemedComponents();
        timerCallback();
        syncUiTimer();
    }
    else
    {
        stopTimer();
        uiTimerHz = 0;
    }
}

void SuperVelocityCurveAudioProcessorEditor::setScaleFactor (float newScale)
{
    juce::AudioProcessorEditor::setScaleFactor (newScale);
    resized();
    refreshThemedComponents();
    repaintThemedCanvases();
}

void SuperVelocityCurveAudioProcessorEditor::handlePendingEngineHits()
{
    if (! isShowing())
        return;

    updateLiveHits();
    padGrid.decayHitVisuals();
    midiMeters.decay();
    curveEditor.decayHitMarkers();
    syncUiTimer();
}

void SuperVelocityCurveAudioProcessorEditor::timerCallback()
{
    if (audioProcessor.getTheme() == svc::ui::ThemeMode::system)
    {
        if (++themeCheckCounter >= 15)
        {
            themeCheckCounter = 0;
            const auto resolved = svc::ui::resolveEffectiveMode (svc::ui::ThemeMode::system);
            if (resolved != svc::ui::Theme::getMode())
            {
                svc::ui::Theme::setMode (svc::ui::ThemeMode::system);
                appLookAndFeel.refreshTheme();
                refreshThemedComponents();
                repaintThemedCanvases();
                sendLookAndFeelChange();
            }
        }
    }

    if (statusClearTicksRemaining > 0)
    {
        if (--statusClearTicksRemaining == 0)
            clearStatus();
    }

    updateLiveHits();
    padGrid.decayHitVisuals();
    midiMeters.decay();
    curveEditor.decayHitMarkers();
    audioProcessor.flushStandaloneMidiOutput();
    syncUiTimer();
}

void SuperVelocityCurveAudioProcessorEditor::pushCurveUndo()
{
    if (selectedPadIndex < 0)
        return;

    const auto target = curveEditor.getEditTarget();
    const bool isAt = target == CurveEditorComponent::EditTarget::aftertouch;
    const auto& pad = curveEditor.getPad();
    undoCurveState = CurveUndoState { selectedPadIndex, target, isAt ? pad.aftertouch.curve : pad.curve };
    undoCurveButton.setEnabled (true);
}

void SuperVelocityCurveAudioProcessorEditor::performCurveUndo()
{
    if (! undoCurveState.has_value())
    {
        showStatus ("Nothing to undo.", true);
        return;
    }

    const auto undoState = *undoCurveState;
    const auto& profile = audioProcessor.getProfileStore().getActiveProfile();
    if (undoState.padIndex < 0 || undoState.padIndex >= static_cast<int> (profile.getPads().size()))
    {
        undoCurveState.reset();
        undoCurveButton.setEnabled (false);
        showStatus ("Cannot undo: pad no longer exists.", true);
        return;
    }

    if (undoState.padIndex != selectedPadIndex)
    {
        onPadSelected (undoState.padIndex);
        padGrid.setSelectedPadIndex (undoState.padIndex);
    }

    if (curveEditor.getEditTarget() != undoState.target)
        curveEditor.setEditTarget (undoState.target);

    const bool isAt = undoState.target == CurveEditorComponent::EditTarget::aftertouch;
    auto pad = curveEditor.getPad();
    const auto currentCurve = isAt ? pad.aftertouch.curve : pad.curve;

    undoCurveState = CurveUndoState { undoState.padIndex, undoState.target, currentCurve };

    if (isAt)
        pad.aftertouch.curve = undoState.curve;
    else
        pad.curve = undoState.curve;

    tryUpdateSelectedPadFromUI (undoState.padIndex, pad);
    curveEditor.setPad (pad, false);
    showStatus (isAt ? "Undid aftertouch curve edit." : "Undid curve edit.");
}

bool SuperVelocityCurveAudioProcessorEditor::keyPressed (const juce::KeyPress& key, juce::Component* /*originatingComponent*/)
{
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        performCurveUndo();
        return true;
    }
    return false;
}
