#pragma once

#include "../Engine/VelocityEngine.h"
#include "../Profiles/ProfileStore.h"
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
    void setStandaloneMidiOutput (juce::MidiOutput* output) noexcept;
    void flushStandaloneMidiOutput();
    bool hasPendingStandaloneMidiOutput() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperVelocityCurveAudioProcessor)
};
