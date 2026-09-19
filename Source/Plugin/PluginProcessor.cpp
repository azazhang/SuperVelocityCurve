#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::File globalSettingsOverrideFile;

static juce::File getGlobalSettingsFile()
{
    if (globalSettingsOverrideFile != juce::File())
        return globalSettingsOverrideFile;

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("SuperVelocityCurve")
        .getChildFile ("settings.xml");
}

void SuperVelocityCurveAudioProcessor::setGlobalSettingsFileOverride (juce::File file) noexcept
{
    globalSettingsOverrideFile = file;
}

void SuperVelocityCurveAudioProcessor::clearGlobalSettingsFileOverride() noexcept
{
    globalSettingsOverrideFile = juce::File();
}

void SuperVelocityCurveAudioProcessor::loadGlobalSettings()
{
    const auto file = getGlobalSettingsFile();
    if (file.existsAsFile())
    {
        if (auto xml = juce::parseXML (file))
        {
            if (xml->hasTagName ("SuperVelocityCurveGlobalSettings"))
            {
                const auto themeStr = xml->getStringAttribute ("theme", "system");
                currentTheme = svc::ui::themeModeFromString (themeStr);
                svc::ui::Theme::setMode (currentTheme);

                if (xml->hasAttribute ("padGridWidth"))
                {
                    const int w = xml->getIntAttribute ("padGridWidth", -1);
                    customPadGridWidth = (w > 0) ? std::make_optional (w) : std::nullopt;
                }
                return;
            }
        }
    }
    svc::ui::Theme::setMode (currentTheme);
}

void SuperVelocityCurveAudioProcessor::saveGlobalSettings()
{
    const auto file = getGlobalSettingsFile();
    file.getParentDirectory().createDirectory();

    juce::XmlElement xml ("SuperVelocityCurveGlobalSettings");
    xml.setAttribute ("theme", svc::ui::themeModeToString (currentTheme));
    if (customPadGridWidth.has_value())
        xml.setAttribute ("padGridWidth", *customPadGridWidth);
    xml.writeTo (file);
}

void SuperVelocityCurveAudioProcessor::setTheme (svc::ui::ThemeMode mode)
{
    if (currentTheme != mode)
    {
        currentTheme = mode;
        svc::ui::Theme::setMode (mode);
        saveGlobalSettings();
        markStateDirty();
    }
}

void SuperVelocityCurveAudioProcessor::setCustomPadGridWidth (std::optional<int> width, bool saveSettings)
{
    const auto clamped = width.has_value() ? std::make_optional (std::max (180, *width)) : std::nullopt;
    if (customPadGridWidth != clamped)
    {
        customPadGridWidth = clamped;
        if (saveSettings)
            saveGlobalSettings();
        markStateDirty();
    }
}

void SuperVelocityCurveAudioProcessor::injectTestNote (int note, int channel, int velocity)
{
    const int ch = juce::jlimit (1, 16, channel);
    const int n = juce::jlimit (0, 127, note);
    const int vel = juce::jlimit (1, 127, velocity);

    injectStandaloneMidi (juce::MidiMessage::noteOn (ch, n, static_cast<juce::uint8> (vel)));

    const double sr = getSampleRate();
    testNoteOffSamplesRemaining.store (static_cast<int> (sr > 0.0 ? sr * 0.12 : 5760.0));
    testNoteOffChannel.store (ch);
    testNoteOffNote.store (n);
}


SuperVelocityCurveAudioProcessor::SuperVelocityCurveAudioProcessor()
#if JucePlugin_IsMidiEffect
    : AudioProcessor (BusesProperties()),
#else
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    loadGlobalSettings();
    profileStore.applyActiveToEngine (engine);
    apvts.addParameterListener ("outputMode", this);
}

SuperVelocityCurveAudioProcessor::~SuperVelocityCurveAudioProcessor()
{
    if (isAlive)
        *isAlive = false;
    testNoteOffSamplesRemaining.store (0);
    testNoteOffNote.store (-1);
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
    const juce::ScopedLock lock (standaloneOutputLock);
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

    const int remaining = testNoteOffSamplesRemaining.load();
    if (remaining > 0)
    {
        const int numSamples = buffer.getNumSamples();
        if (remaining <= numSamples)
        {
            testNoteOffSamplesRemaining.store (0);
            const int note = testNoteOffNote.exchange (-1);
            const int ch = testNoteOffChannel.load();
            if (note >= 0)
                midiMessages.addEvent (juce::MidiMessage::noteOff (ch, note, static_cast<juce::uint8> (0)),
                                       std::max (0, remaining - 1));
        }
        else
        {
            testNoteOffSamplesRemaining.store (remaining - numSamples);
        }
    }

    engine.processMidiBuffer (midiMessages, buffer.getNumSamples());

    if (engine.getHitFifo().hasPending() && getActiveEditor() != nullptr)
        triggerAsyncUpdate();

    if (wrapperType == wrapperType_Standalone && standaloneMidiOutput != nullptr)
    {
        const juce::ScopedLock lock (standaloneOutputLock);
        if (standaloneMidiOutput != nullptr)
        {
            for (const auto metadata : midiMessages)
                standaloneMidiOutput->sendMessageNow (metadata.getMessage());
        }
    }
}

bool SuperVelocityCurveAudioProcessor::hasPendingStandaloneMidiOutput() const
{
    return false;
}

void SuperVelocityCurveAudioProcessor::flushStandaloneMidiOutput()
{
}

juce::AudioProcessorEditor* SuperVelocityCurveAudioProcessor::createEditor()
{
    return new SuperVelocityCurveAudioProcessorEditor (*this);
}

void SuperVelocityCurveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("SuperVelocityCurveState");
    state.setProperty ("version", 2, nullptr);
    state.setProperty ("theme", svc::ui::themeModeToString (currentTheme), nullptr);
    state.setProperty ("padGridWidth", customPadGridWidth.has_value() ? *customPadGridWidth : -1, nullptr);
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
            if (state.hasProperty ("theme"))
            {
                currentTheme = svc::ui::themeModeFromString (state.getProperty ("theme").toString());
                svc::ui::Theme::setMode (currentTheme);
            }

            if (state.hasProperty ("padGridWidth"))
            {
                const int w = state.getProperty ("padGridWidth");
                customPadGridWidth = (w > 0) ? std::make_optional (w) : std::nullopt;
            }

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
