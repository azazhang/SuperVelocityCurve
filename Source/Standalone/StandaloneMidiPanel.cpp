#include "StandaloneMidiPanel.h"
#include "../UI/Theme.h"

StandaloneMidiPanel::StandaloneMidiPanel()
{
    addAndMakeVisible (inputLabel);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (inputDeviceBox);
    addAndMakeVisible (outputDeviceBox);
    addAndMakeVisible (statusLabel);

    inputLabel.setFont (svc::ui::Theme::smallFont().boldened());
    outputLabel.setFont (svc::ui::Theme::smallFont().boldened());
    statusLabel.setFont (svc::ui::Theme::smallFont());
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    inputDeviceBox.setTextWhenNoChoicesAvailable ("Detecting MIDI inputs...");
    outputDeviceBox.setTextWhenNoChoicesAvailable ("Detecting MIDI outputs...");
    statusLabel.setText ("Scanning MIDI devices in background...", juce::dontSendNotification);

    inputDeviceBox.onChange = [this] { connectInput (inputDeviceBox.getSelectedItemIndex()); };
    outputDeviceBox.onChange = [this] { connectOutput (outputDeviceBox.getSelectedItemIndex()); };

    scanDevicesAsync();
}

StandaloneMidiPanel::ScannerThread::ScannerThread (StandaloneMidiPanel& owner)
    : juce::Thread ("MidiDeviceScanner"), panel (owner)
{
}

StandaloneMidiPanel::ScannerThread::~ScannerThread()
{
    stopThread (1000);
}

void StandaloneMidiPanel::ScannerThread::run()
{
    const auto inputs = juce::MidiInput::getAvailableDevices();
    if (threadShouldExit())
        return;

    const auto outputs = juce::MidiOutput::getAvailableDevices();
    if (threadShouldExit())
        return;

    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
    {
        mm->callAsync ([safe = juce::Component::SafePointer<StandaloneMidiPanel> (&panel), inputs, outputs]
        {
            if (safe != nullptr)
            {
                safe->scanning = false;
                safe->populateDeviceLists (inputs, outputs);
            }
        });
    }
}

StandaloneMidiPanel::~StandaloneMidiPanel()
{
    if (scannerThread != nullptr)
    {
        scannerThread->signalThreadShouldExit();
        scannerThread->stopThread (1000);
        scannerThread.reset();
    }
    activeInput.reset();
    activeOutput.reset();
}

void StandaloneMidiPanel::scanDevicesAsync()
{
    if (scanning.exchange (true))
        return;

    scannerThread = std::make_unique<ScannerThread> (*this);
    scannerThread->startThread();
}

void StandaloneMidiPanel::populateDeviceLists (const juce::Array<juce::MidiDeviceInfo>& inputs,
                                              const juce::Array<juce::MidiDeviceInfo>& outputs)
{
    cachedInputs = inputs;
    cachedOutputs = outputs;

    inputDeviceBox.clear();
    outputDeviceBox.clear();

    int id = 1;
    for (const auto& device : cachedInputs)
        inputDeviceBox.addItem (device.name, id++);

    outputDeviceBox.addItem ("(none)", 1);
    id = 2;
    for (const auto& device : cachedOutputs)
        outputDeviceBox.addItem (device.name, id++);

    if (cachedInputs.isEmpty())
    {
        statusLabel.setText ("No MIDI inputs found. Connect a controller or use macOS IAC Driver.", juce::dontSendNotification);
    }
    else
    {
        inputDeviceBox.setSelectedId (1, juce::dontSendNotification);
        connectInput (0);
    }

    outputDeviceBox.setSelectedId (1, juce::dontSendNotification);
}

void StandaloneMidiPanel::connectInput (int index)
{
    activeInput.reset();
    if (index < 0 || index >= cachedInputs.size())
        return;

    const auto devInfo = cachedInputs[index];
    activeInput = juce::MidiInput::openDevice (devInfo.identifier, this);
    if (activeInput != nullptr)
    {
        activeInput->start();
        statusLabel.setText ("Active In: " + devInfo.name + " | Ready", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("Failed to open MIDI input: " + devInfo.name, juce::dontSendNotification);
    }
}

void StandaloneMidiPanel::connectOutput (int index)
{
    activeOutput.reset();
    if (index <= 0)
    {
        if (onOutputDeviceChanged)
            onOutputDeviceChanged (nullptr);
        return;
    }

    const auto devIndex = index - 1;
    if (devIndex < 0 || devIndex >= cachedOutputs.size())
        return;

    const auto devInfo = cachedOutputs[devIndex];
    activeOutput = juce::MidiOutput::openDevice (devInfo.identifier);
    if (onOutputDeviceChanged)
        onOutputDeviceChanged (activeOutput.get());
}

void StandaloneMidiPanel::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (onMidiMessage)
        onMidiMessage (message);
}

void StandaloneMidiPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    svc::ui::Theme::fillPanel (g, bounds, 8.0f);

    // Subtle hardware header badge
    g.setColour (juce::Colour (svc::ui::Theme::textMuted()));
    g.setFont (svc::ui::Theme::smallFont().boldened());
    g.drawText ("STANDALONE MIDI ROUTING", bounds.removeFromTop (16.0f).reduced (10.0f, 2.0f),
                juce::Justification::centredLeft);
}

void StandaloneMidiPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 4);
    area.removeFromTop (14); // Space for header badge

    auto row1 = area.removeFromTop (22);
    inputLabel.setBounds (row1.removeFromLeft (68));
    inputDeviceBox.setBounds (row1);

    area.removeFromTop (3);

    auto row2 = area.removeFromTop (22);
    outputLabel.setBounds (row2.removeFromLeft (68));
    outputDeviceBox.setBounds (row2);

    area.removeFromTop (2);
    statusLabel.setBounds (area);
}
