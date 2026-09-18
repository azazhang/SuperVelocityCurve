#pragma once

#include "../Engine/VelocityEngine.h"
#include "../Profiles/ProfileStore.h"
#include "../UI/Theme.h"
#include <JuceHeader.h>

class SuperVelocityCurveAudioProcessor : public juce::AudioProcessor,
                                         private juce::AudioProcessorValueTreeState::Listener,
                                         private juce::AsyncUpdater
{
public:
    SuperVelocityCurveAudioProcessor();
    ~SuperVelocityCurveAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override
    {
       #if JucePlugin_IsMidiEffect
        return true;
       #else
        return false;
       #endif
    }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    svc::VelocityEngine& getEngine() noexcept { return engine; }
    svc::ProfileStore& getProfileStore() noexcept { return profileStore; }
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    void applyProfileToEngine();
    void markStateDirty();
    void syncPadToEngine (const svc::ProfilePad& pad);
    void syncRoutingToEngine();
    void syncOutputModeToEngine();
    void injectStandaloneMidi (const juce::MidiMessage& message);
    void injectTestNote (int note, int channel, int velocity = 100);
    void setStandaloneMidiOutput (juce::MidiOutput* output) noexcept;
    void flushStandaloneMidiOutput();
    bool hasPendingStandaloneMidiOutput() const;

    std::optional<int> getCustomPadGridWidth() const noexcept { return customPadGridWidth; }
    void setCustomPadGridWidth (std::optional<int> width);

    svc::ui::ThemeMode getTheme() const noexcept { return currentTheme; }
    void setTheme (svc::ui::ThemeMode mode);

    static void setGlobalSettingsFileOverride (juce::File file) noexcept;
    static void clearGlobalSettingsFileOverride() noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    svc::ui::ThemeMode currentTheme = svc::ui::ThemeMode::system;
    std::optional<int> customPadGridWidth;
    void loadGlobalSettings();
    void saveGlobalSettings();

    svc::VelocityEngine engine;
    svc::ProfileStore profileStore;
    juce::AudioProcessorValueTreeState apvts;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::CriticalSection standaloneMidiLock;
    juce::MidiBuffer standaloneMidiQueue;
    juce::CriticalSection standaloneOutputLock;
    juce::MidiBuffer standaloneMidiOutputQueue;
    juce::MidiOutput* standaloneMidiOutput = nullptr;
    std::shared_ptr<std::atomic<bool>> isAlive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int> testNoteOffSamplesRemaining { 0 };
    std::atomic<int> testNoteOffChannel { 1 };
    std::atomic<int> testNoteOffNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperVelocityCurveAudioProcessor)
};
