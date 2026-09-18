#pragma once

#include <JuceHeader.h>

class StandaloneMidiPanel : public juce::Component,
                            private juce::MidiInputCallback
{
public:
    StandaloneMidiPanel();
    ~StandaloneMidiPanel() override;

    std::function<void (const juce::MidiMessage&)> onMidiMessage;
    std::function<void (juce::MidiOutput*)> onOutputDeviceChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void scanDevicesAsync();
    void populateDeviceLists (const juce::Array<juce::MidiDeviceInfo>& inputs,
                              const juce::Array<juce::MidiDeviceInfo>& outputs);
    bool isDeviceScanning() const noexcept { return scanning.load(); }

private:
    juce::ComboBox inputDeviceBox;
    juce::ComboBox outputDeviceBox;
    juce::Label inputLabel { {}, "MIDI In" };
    juce::Label outputLabel { {}, "MIDI Out" };
    juce::Label statusLabel;
    std::unique_ptr<juce::MidiInput> activeInput;
    std::unique_ptr<juce::MidiOutput> activeOutput;

    juce::Array<juce::MidiDeviceInfo> cachedInputs;
    juce::Array<juce::MidiDeviceInfo> cachedOutputs;
    std::atomic<bool> scanning { false };

    void connectInput (int index);
    void connectOutput (int index);
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;

    class ScannerThread : public juce::Thread
    {
    public:
        ScannerThread (StandaloneMidiPanel& owner);
        ~ScannerThread() override;
        void run() override;

    private:
        StandaloneMidiPanel& panel;
    };

    std::unique_ptr<ScannerThread> scannerThread;
};
