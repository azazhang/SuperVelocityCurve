#include "PluginProcessor.h"
#include "PluginEditor.h"

SuperVelocityCurveAudioProcessor::SuperVelocityCurveAudioProcessor()
#if JucePlugin_IsMidiEffect
    : AudioProcessor (BusesProperties()),
#else
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    profileStore.applyActiveToEngine (engine);
    apvts.addParameterListener ("outputMode", this);
}

SuperVelocityCurveAudioProcessor::~SuperVelocityCurveAudioProcessor()
{
    apvts.removeParameterListener ("outputMode", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout SuperVelocityCurveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "outputMode",
        "Velocity Output",
        juce::StringArray { "Auto", "MIDI 1.0", "MIDI 2.0" },
        0));

    return { params.begin(), params.end() };
}

void SuperVelocityCurveAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "outputMode")
        syncOutputModeToEngine();
}

void SuperVelocityCurveAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSampleRate (sampleRate);
    syncOutputModeToEngine();
    applyProfileToEngine();
}

void SuperVelocityCurveAudioProcessor::releaseResources()
{
}

bool SuperVelocityCurveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled();
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
#endif
}

void SuperVelocityCurveAudioProcessor::syncOutputModeToEngine()
{
    const auto outputModeIndex = static_cast<int> (*apvts.getRawParameterValue ("outputMode"));
    engine.setOutputMode (static_cast<svc::VelocityOutputMode> (outputModeIndex));
}

void SuperVelocityCurveAudioProcessor::applyProfileToEngine()
{
    profileStore.applyActiveToEngine (engine);
}

void SuperVelocityCurveAudioProcessor::markStateDirty()
{
    updateHostDisplay();
}

void SuperVelocityCurveAudioProcessor::syncPadToEngine (const svc::ProfilePad& pad)
{
    engine.setPadSettings (pad.midiNote, pad.midiChannel, svc::ControllerProfile::toEngineSettings (pad));
}

void SuperVelocityCurveAudioProcessor::syncRoutingToEngine()
{
    const auto& profile = profileStore.getActiveProfile();
    engine.setMidiRouting (profile.getMidiRouting());
    engine.setProcessingSettings (profile.getProcessingSettings());
}

void SuperVelocityCurveAudioProcessor::handleAsyncUpdate()
{
    if (auto* editor = getActiveEditor())
    {
        if (auto* svcEditor = dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (editor))
            svcEditor->handlePendingEngineHits();
    }
}

void SuperVelocityCurveAudioProcessor::injectStandaloneMidi (const juce::MidiMessage& message)
{
    const juce::ScopedLock lock (standaloneMidiLock);
    standaloneMidiQueue.addEvent (message, 0);
}

void SuperVelocityCurveAudioProcessor::setStandaloneMidiOutput (juce::MidiOutput* output) noexcept
{
    standaloneMidiOutput = output;
}

void SuperVelocityCurveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
#if ! JucePlugin_IsMidiEffect
    buffer.clear();
#endif

    {
        const juce::ScopedLock lock (standaloneMidiLock);
        for (const auto metadata : standaloneMidiQueue)
            midiMessages.addEvent (metadata.getMessage(), metadata.samplePosition);
        standaloneMidiQueue.clear();
    }

    engine.processMidiBuffer (midiMessages, buffer.getNumSamples());

    if (engine.getHitFifo().hasPending())
        triggerAsyncUpdate();

    if (wrapperType == wrapperType_Standalone && standaloneMidiOutput != nullptr)
    {
        const juce::ScopedLock lock (standaloneOutputLock);
        for (const auto metadata : midiMessages)
            standaloneMidiOutputQueue.addEvent (metadata.getMessage(), metadata.samplePosition);
    }
}

bool SuperVelocityCurveAudioProcessor::hasPendingStandaloneMidiOutput() const
{
    if (wrapperType != wrapperType_Standalone || standaloneMidiOutput == nullptr)
        return false;

    const juce::ScopedLock lock (standaloneOutputLock);
    return standaloneMidiOutputQueue.getNumEvents() > 0;
}

void SuperVelocityCurveAudioProcessor::flushStandaloneMidiOutput()
{
    if (wrapperType != wrapperType_Standalone || standaloneMidiOutput == nullptr)
        return;

    juce::MidiBuffer toSend;
    {
        const juce::ScopedLock lock (standaloneOutputLock);
        if (standaloneMidiOutputQueue.getNumEvents() == 0)
            return;

        toSend.swapWith (standaloneMidiOutputQueue);
    }

    for (const auto metadata : toSend)
        standaloneMidiOutput->sendMessageNow (metadata.getMessage());
}

juce::AudioProcessorEditor* SuperVelocityCurveAudioProcessor::createEditor()
{
    return new SuperVelocityCurveAudioProcessorEditor (*this);
}

void SuperVelocityCurveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("SuperVelocityCurveState");
    state.setProperty ("version", 2, nullptr);
    state.appendChild (profileStore.toValueTree(), nullptr);
    state.appendChild (apvts.copyState(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SuperVelocityCurveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType ("SuperVelocityCurveState"))
        {
            apvts.removeParameterListener ("outputMode", this);

            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                const auto child = state.getChild (i);
                if (child.hasType ("SuperVelocityCurveProfileStore")
                    || child.hasType ("SuperVelocityCurveProfile"))
                {
                    // Never notify UI from host state restore — editor may be destroyed
                    // (pluginval state tests) or on a non-message thread.
                    profileStore.fromValueTree (child, false);
                }
                else if (child.hasType ("Parameters"))
                    apvts.replaceState (child);
            }

            apvts.addParameterListener ("outputMode", this);
            syncOutputModeToEngine();
            applyProfileToEngine();

            if (auto* editor = getActiveEditor())
            {
                juce::Component::SafePointer<juce::AudioProcessorEditor> safeEditor (editor);
                juce::MessageManager::callAsync ([safeEditor]
                {
                    if (auto* svcEditor = dynamic_cast<SuperVelocityCurveAudioProcessorEditor*> (safeEditor.getComponent()))
                        svcEditor->syncFromProcessorState();
                });
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperVelocityCurveAudioProcessor();
}
