#include "PadInspectorComponent.h"
#include "MidiNoteNames.h"
#include "ScrollHelpers.h"
#include "ThemeUi.h"

PadInspectorComponent::PadInspectorComponent()
{
    setOpaque (false);
    content.setOpaque (true);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    svc::ui::configureVerticalViewport (viewport);

    padNameEditor.setFont (svc::ui::Theme::bodyFont());
    padNameEditor.setIndents (8, 4);
    padNameEditor.setMultiLine (false);
    padNameEditor.setTextToShowWhenEmpty ("Pad name", juce::Colour (svc::ui::Theme::textMuted()));
    padNameEditor.onFocusLost = padNameEditor.onReturnKey = [this]
    {
        currentPad.label = padNameEditor.getText().trim();
        if (currentPad.label.isEmpty())
            currentPad.label = "Pad " + juce::String (currentPadIndex + 1);
        notifyChanged();
        finishPadEdit();
    };
    content.addAndMakeVisible (padNameLabel);
    content.addAndMakeVisible (padNameEditor);

    midiNoteSlider.setRange (0.0, 127.0, 1.0);
    midiNoteSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    midiNoteSlider.textFromValueFunction = [] (double v)
    {
        return svc::ui::formatMidiNote (static_cast<int> (v));
    };
    midiNoteSlider.onValueChange = [this]
    {
        currentPad.midiNote = static_cast<int> (midiNoteSlider.getValue());
        notifyChanged();
    };
    midiNoteSlider.onDragEnd = [this]
    {
        finishPadEdit();
    };
    content.addAndMakeVisible (midiNoteLabel);
    content.addAndMakeVisible (midiNoteSlider);

    for (int ch = 1; ch <= 16; ++ch)
        midiChannelBox.addItem ("Ch " + juce::String (ch), ch);
    midiChannelBox.onChange = [this]
    {
        if (suppressNotify)
            return;

        const auto channel = midiChannelBox.getSelectedId();
        if (channel < 1 || channel > 16)
            return;

        currentPad.midiChannel = channel;
        notifyChanged();
        finishPadEdit();
    };
    content.addAndMakeVisible (midiChannelLabel);
    content.addAndMakeVisible (midiChannelBox);

    content.addAndMakeVisible (enabledToggle);
    content.addAndMakeVisible (aftertouchToggle);

    content.addAndMakeVisible (editAftertouchButton);
    editAftertouchButton.onClick = [this]
    {
        if (onEditAftertouchRequested)
            onEditAftertouchRequested();
    };

    for (auto* label : { &gateLabel, &gateModeLabel, &groupLabel, &retriggerLabel, &floorLabel, &ceilingLabel })
        content.addAndMakeVisible (label);

    content.addAndMakeVisible (groupBox);
    for (int i = 0; i < 7; ++i)
        groupBox.addItem (svc::padGroupToString (static_cast<svc::PadGroup> (i)), i + 1);

    content.addAndMakeVisible (gateModeBox);
    gateModeBox.addItem ("Drop notes", 1);
    gateModeBox.addItem ("Clamp to floor", 2);

    setupSlider (velocityGateSlider, "", true);
    setupSlider (retriggerSlider, " ms", false);
    setupSlider (floorSlider, "", true);
    setupSlider (ceilingSlider, "", true);

    velocityGateSlider.setRange (0.0, 127.0, 1.0);
    retriggerSlider.setRange (0.0, 100.0, 1.0);
    floorSlider.setRange (0.0, 127.0, 1.0);
    ceilingSlider.setRange (0.0, 127.0, 1.0);

    velocityGateSlider.onValueChange = [this]
    {
        currentPad.velocityGate = static_cast<float> (velocityGateSlider.getValue() / 127.0);
        notifyChanged();
    };

    retriggerSlider.onValueChange = [this]
    {
        currentPad.retriggerGuardMs = retriggerSlider.getValue();
        notifyChanged();
    };

    floorSlider.onValueChange = [this]
    {
        if (suppressNotify)
            return;

        auto& curve = editingAftertouch ? currentPad.aftertouch.curve : currentPad.curve;
        curve.setFloor (static_cast<float> (floorSlider.getValue() / 127.0));
        ceilingSlider.setValue (curve.getCeiling() * 127.0, juce::dontSendNotification);
        notifyChanged();
    };

    ceilingSlider.onValueChange = [this]
    {
        if (suppressNotify)
            return;

        auto& curve = editingAftertouch ? currentPad.aftertouch.curve : currentPad.curve;
        curve.setCeiling (static_cast<float> (ceilingSlider.getValue() / 127.0));
        floorSlider.setValue (curve.getFloor() * 127.0, juce::dontSendNotification);
        notifyChanged();
    };

    for (auto* slider : { &velocityGateSlider, &retriggerSlider, &floorSlider, &ceilingSlider })
    {
        content.addAndMakeVisible (slider);
        slider->onDragEnd = [this] { finishPadEdit(); };
    }

    enabledToggle.onClick = [this]
    {
        currentPad.enabled = enabledToggle.getToggleState();
        notifyChanged();
        finishPadEdit();
    };

    aftertouchToggle.onClick = [this]
    {
        currentPad.aftertouch.enabled = aftertouchToggle.getToggleState();
        notifyChanged();
        finishPadEdit();
    };

    groupBox.onChange = [this]
    {
        if (suppressNotify)
            return;

        currentPad.group = static_cast<svc::PadGroup> (groupBox.getSelectedItemIndex());
        notifyChanged();
        finishPadEdit();
    };

    gateModeBox.onChange = [this]
    {
        if (suppressNotify)
            return;

        currentPad.gateMode = gateModeBox.getSelectedId() == 2 ? svc::VelocityGateMode::clampToFloor
                                                               : svc::VelocityGateMode::drop;
        notifyChanged();
        finishPadEdit();
    };

    applyTheme();
}

void PadInspectorComponent::setupSlider (juce::Slider& slider, const juce::String& suffix, bool midiScale)
{
    slider.setTextValueSuffix (suffix);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    if (midiScale)
    {
        slider.textFromValueFunction = [] (double value) { return juce::String (static_cast<int> (value)); };
        slider.valueFromTextFunction = [] (const juce::String& text) { return text.getDoubleValue(); };
    }
}

svc::ProfilePad PadInspectorComponent::getPad() const
{
    auto pad = currentPad;
    pad.label = padNameEditor.getText().trim();
    if (pad.label.isEmpty())
        pad.label = "Pad " + juce::String (currentPadIndex + 1);
    return pad;
}

void PadInspectorComponent::commitEdits()
{
    if (suppressNotify)
        return;

    currentPad = getPad();
    notifyChanged();
    finishPadEdit();
}

void PadInspectorComponent::setAftertouchEditMode (bool editingAt)
{
    editingAftertouch = editingAt;
    editAftertouchButton.setButtonText (editingAftertouch ? "Edit velocity curve" : "Edit AT curve");

    suppressNotify = true;
    const auto& curve = editingAftertouch ? currentPad.aftertouch.curve : currentPad.curve;
    floorSlider.setValue (curve.getFloor() * 127.0, juce::dontSendNotification);
    ceilingSlider.setValue (curve.getCeiling() * 127.0, juce::dontSendNotification);
    suppressNotify = false;
}

void PadInspectorComponent::applyTheme()
{
    const auto text = juce::Colour (svc::ui::Theme::textPrimary());

    for (auto* label : { &padNameLabel, &midiNoteLabel, &midiChannelLabel, &gateLabel, &gateModeLabel,
                         &groupLabel, &retriggerLabel, &floorLabel, &ceilingLabel })
    {
        label->setColour (juce::Label::textColourId, text);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    }

    padNameEditor.setTextToShowWhenEmpty ("Pad name", juce::Colour (svc::ui::Theme::textMuted()));
    svc::ui::applyTextEditorTheme (padNameEditor);

    for (auto* slider : { &midiNoteSlider, &velocityGateSlider, &retriggerSlider, &floorSlider, &ceilingSlider })
        svc::ui::applySliderTheme (*slider);

    for (auto* toggle : { &enabledToggle, &aftertouchToggle })
        svc::ui::applyToggleTheme (*toggle);

    svc::ui::applyTextButtonTheme (editAftertouchButton);

    for (auto* box : { &midiChannelBox, &groupBox, &gateModeBox })
        svc::ui::applyComboBoxTheme (*box);
}

void PadInspectorComponent::setPad (const svc::ProfilePad& pad, int padIndex)
{
    suppressNotify = true;
    currentPad = pad;
    currentPadIndex = padIndex;

    padNameEditor.setText (pad.label, juce::dontSendNotification);
    svc::ui::applyTextEditorTheme (padNameEditor);
    midiNoteSlider.setValue (pad.midiNote, juce::dontSendNotification);
    midiChannelBox.setSelectedId (juce::jlimit (1, 16, pad.midiChannel), juce::dontSendNotification);
    enabledToggle.setToggleState (pad.enabled, juce::dontSendNotification);
    aftertouchToggle.setToggleState (pad.aftertouch.enabled, juce::dontSendNotification);
    groupBox.setSelectedItemIndex (static_cast<int> (pad.group), juce::dontSendNotification);
    gateModeBox.setSelectedId (pad.gateMode == svc::VelocityGateMode::clampToFloor ? 2 : 1, juce::dontSendNotification);
    velocityGateSlider.setValue (pad.velocityGate * 127.0, juce::dontSendNotification);
    retriggerSlider.setValue (pad.retriggerGuardMs, juce::dontSendNotification);
    const auto& curve = editingAftertouch ? currentPad.aftertouch.curve : currentPad.curve;
    floorSlider.setValue (curve.getFloor() * 127.0, juce::dontSendNotification);
    ceilingSlider.setValue (curve.getCeiling() * 127.0, juce::dontSendNotification);
    suppressNotify = false;
    refreshScrollbar();
}

void PadInspectorComponent::notifyChanged()
{
    if (suppressNotify || ! onPadChanged)
        return;

    onPadChanged (currentPadIndex, getPad());
}

void PadInspectorComponent::finishPadEdit()
{
    if (suppressNotify || ! onPadEditFinished)
        return;

    onPadEditFinished();
}

void PadInspectorComponent::refreshScrollbar()
{
    svc::ui::updateVerticalScrollbarVisibility (viewport, content);
}

void PadInspectorComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    viewport.setBounds (area);
    layoutContent();
    svc::ui::updateVerticalScrollbarVisibility (viewport, content);
}

void PadInspectorComponent::layoutContent()
{
    const int width = juce::jmax (220, viewport.getWidth() - 4);
    int y = 8;

    auto row = [&y, width] (juce::Label& label, juce::Component& comp, int h = 44)
    {
        label.setBounds (8, y, width - 16, 14);
        comp.setBounds (8, y + 14, width - 16, h - 14);
        y += h + 4;
    };

    row (padNameLabel, padNameEditor, 46);
    row (midiNoteLabel, midiNoteSlider, 52);
    row (midiChannelLabel, midiChannelBox, 44);
    enabledToggle.setBounds (8, y, width - 16, 26);
    y += 28;
    aftertouchToggle.setBounds (8, y, width - 16, 26);
    y += 28;
    editAftertouchButton.setBounds (8, y, width - 16, 24);
    y += 30;

    row (groupLabel, groupBox, 40);
    row (gateLabel, velocityGateSlider);
    row (gateModeLabel, gateModeBox, 40);
    row (retriggerLabel, retriggerSlider);

    auto pairY = y;
    floorLabel.setBounds (8, pairY, (width - 20) / 2, 14);
    ceilingLabel.setBounds (8 + (width - 20) / 2 + 4, pairY, (width - 20) / 2, 14);
    floorSlider.setBounds (8, pairY + 14, (width - 20) / 2, 40);
    ceilingSlider.setBounds (8 + (width - 20) / 2 + 4, pairY + 14, (width - 20) / 2, 40);
    y = pairY + 58;

    content.setSize (width, y + 8);
    refreshScrollbar();
}
