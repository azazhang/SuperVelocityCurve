#include "MidiRoutingPanel.h"
#include "../Engine/EngineSettings.h"
#include "../Profiles/PadTypes.h"
#include "ScrollHelpers.h"

namespace
{
constexpr const char* groupLabels[] = { "Other", "Kick", "Snare", "Hat", "Tom", "Cymbal", "Percussion" };
}

MidiRoutingPanel::MidiRoutingPanel()
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    svc::ui::configureVerticalViewport (viewport);

    content.addAndMakeVisible (inputLabel);
    content.addAndMakeVisible (outputLabel);
    content.addAndMakeVisible (inputChannelBox);
    content.addAndMakeVisible (outputChannelBox);
    content.addAndMakeVisible (remapToggle);
    content.addAndMakeVisible (humanizeLabel);
    content.addAndMakeVisible (humanizeSlider);
    content.addAndMakeVisible (libraryLabel);
    content.addAndMakeVisible (libraryBlendLabel);
    content.addAndMakeVisible (libraryPresetBox);
    content.addAndMakeVisible (libraryBlendSlider);
    content.addAndMakeVisible (zoneRoutingToggle);
    content.addAndMakeVisible (zoneChannelsLabel);

    humanizeLabel.setText ("Humanize amount", juce::dontSendNotification);
    libraryLabel.setText ("Sample-library compensation", juce::dontSendNotification);
    libraryBlendLabel.setTooltip ("Blends a generic sample-library velocity curve in after your per-pad curves (0 = off, 1 = full). "
                                  "Shapes for acoustic, electronic, or compressed VIs - not tied to any specific library.");
    humanizeSlider.setRange (0.0, 0.25, 0.001);
    libraryBlendSlider.setRange (0.0, 1.0, 0.01);

    for (int ch = 0; ch <= 16; ++ch)
    {
        inputChannelBox.addItem (ch == 0 ? "Any" : "Ch " + juce::String (ch), ch + 1);
        outputChannelBox.addItem (ch == 0 ? "Keep original" : "Ch " + juce::String (ch), ch + 1);
    }

    libraryPresetBox.addItem (svc::libraryCompensationPresetName (svc::LibraryCompensationPreset::none), 1);
    libraryPresetBox.addItem (svc::libraryCompensationPresetName (svc::LibraryCompensationPreset::acoustic), 2);
    libraryPresetBox.addItem (svc::libraryCompensationPresetName (svc::LibraryCompensationPreset::electronic), 3);
    libraryPresetBox.addItem (svc::libraryCompensationPresetName (svc::LibraryCompensationPreset::compressed), 4);

    for (int i = 0; i < 7; ++i)
    {
        zoneGroupLabels[static_cast<size_t> (i)].setText (groupLabels[i], juce::dontSendNotification);
        content.addAndMakeVisible (zoneGroupLabels[static_cast<size_t> (i)]);
        content.addAndMakeVisible (zoneGroupChannelBoxes[static_cast<size_t> (i)]);

        for (int ch = 0; ch <= 16; ++ch)
            zoneGroupChannelBoxes[static_cast<size_t> (i)].addItem (ch == 0 ? "Keep" : "Ch " + juce::String (ch), ch + 1);

        zoneGroupChannelBoxes[static_cast<size_t> (i)].onChange = [this] { notifyChanged(); };
    }

    inputChannelBox.onChange = [this] { notifyChanged(); };
    outputChannelBox.onChange = [this] { notifyChanged(); };
    remapToggle.onClick = [this] { notifyChanged(); };
    humanizeSlider.onValueChange = [this] { notifyChanged(); };
    libraryPresetBox.onChange = [this] { notifyChanged(); };
    libraryBlendSlider.onValueChange = [this] { notifyChanged(); };
    zoneRoutingToggle.onClick = [this] { notifyChanged(); };
}

void MidiRoutingPanel::applyTheme()
{
    const auto labelColour = juce::Colour (svc::ui::Theme::textPrimary());

    for (auto* label : { &inputLabel, &outputLabel, &humanizeLabel, &libraryLabel,
                         &libraryBlendLabel, &zoneChannelsLabel })
        label->setColour (juce::Label::textColourId, labelColour);

    for (auto& label : zoneGroupLabels)
        label.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textSecondary()));
}

void MidiRoutingPanel::setProfile (svc::ControllerProfile& p)
{
    profile = &p;
    const auto& routing = profile->getMidiRouting();
    const auto& processing = profile->getProcessingSettings();

    inputChannelBox.setSelectedId (routing.inputChannelFilter + 1, juce::dontSendNotification);
    outputChannelBox.setSelectedId (routing.outputChannel + 1, juce::dontSendNotification);
    remapToggle.setToggleState (routing.remapEnabled, juce::dontSendNotification);
    humanizeSlider.setValue (processing.humanizeAmount, juce::dontSendNotification);
    libraryBlendSlider.setValue (processing.libraryBlend, juce::dontSendNotification);
    zoneRoutingToggle.setToggleState (processing.zoneRouting.enabled, juce::dontSendNotification);

    for (int i = 0; i < 7; ++i)
    {
        const auto ch = processing.zoneRouting.groupOutputChannel[static_cast<size_t> (i)];
        zoneGroupChannelBoxes[static_cast<size_t> (i)].setSelectedId (ch + 1, juce::dontSendNotification);
    }

    int presetId = 1;
    switch (processing.libraryPreset)
    {
        case svc::LibraryCompensationPreset::acoustic:    presetId = 2; break;
        case svc::LibraryCompensationPreset::electronic:  presetId = 3; break;
        case svc::LibraryCompensationPreset::compressed:  presetId = 4; break;
        case svc::LibraryCompensationPreset::none:      presetId = 1; break;
    }
    libraryPresetBox.setSelectedId (presetId, juce::dontSendNotification);
}

void MidiRoutingPanel::notifyChanged()
{
    if (profile == nullptr)
        return;

    auto& routing = profile->getMidiRouting();
    routing.inputChannelFilter = inputChannelBox.getSelectedId() - 1;
    routing.outputChannel = outputChannelBox.getSelectedId() - 1;
    routing.remapEnabled = remapToggle.getToggleState();

    auto& processing = profile->getProcessingSettings();
    processing.humanizeAmount = static_cast<float> (humanizeSlider.getValue());
    processing.libraryBlend = static_cast<float> (libraryBlendSlider.getValue());
    processing.zoneRouting.enabled = zoneRoutingToggle.getToggleState();

    for (int i = 0; i < 7; ++i)
        processing.zoneRouting.groupOutputChannel[static_cast<size_t> (i)] =
            zoneGroupChannelBoxes[static_cast<size_t> (i)].getSelectedId() - 1;

    switch (libraryPresetBox.getSelectedId())
    {
        case 2:  processing.libraryPreset = svc::LibraryCompensationPreset::acoustic; break;
        case 3:  processing.libraryPreset = svc::LibraryCompensationPreset::electronic; break;
        case 4:  processing.libraryPreset = svc::LibraryCompensationPreset::compressed; break;
        default: processing.libraryPreset = svc::LibraryCompensationPreset::none; break;
    }

    if (onRoutingChanged)
        onRoutingChanged();
}

void MidiRoutingPanel::layoutContent()
{
    const int width = juce::jmax (viewport.getWidth() - 8, 272);
    int y = 8;

    auto labelRow = [&y, width] (juce::Label& label, juce::Component& comp, int h = 40)
    {
        label.setBounds (8, y, width - 16, 14);
        comp.setBounds (8, y + 14, width - 16, h - 14);
        y += h + 4;
    };

    auto row = [&y, width] (juce::Component& comp, int h)
    {
        comp.setBounds (8, y, width - 16, h);
        y += h + 4;
    };

    labelRow (inputLabel, inputChannelBox);
    labelRow (outputLabel, outputChannelBox);
    row (remapToggle, 22);
    labelRow (humanizeLabel, humanizeSlider);
    labelRow (libraryLabel, libraryPresetBox);
    labelRow (libraryBlendLabel, libraryBlendSlider);
    row (zoneRoutingToggle, 22);
    row (zoneChannelsLabel, 16);

    const int colW = (width - 20) / 2;
    for (int i = 0; i < 7; ++i)
    {
        const int col = i % 2;
        const int rowIdx = i / 2;
        const int x = 8 + col * (colW + 4);
        const int rowY = y + rowIdx * 36;

        zoneGroupLabels[static_cast<size_t> (i)].setBounds (x, rowY, colW, 12);
        zoneGroupChannelBoxes[static_cast<size_t> (i)].setBounds (x, rowY + 12, colW, 22);
    }

    y += 4 * 36;

    content.setSize (width, y + 8);
}

void MidiRoutingPanel::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void MidiRoutingPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutContent();
    svc::ui::updateVerticalScrollbarVisibility (viewport, content);
}
