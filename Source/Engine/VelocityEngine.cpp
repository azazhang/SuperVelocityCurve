#include "VelocityEngine.h"
#include "LibraryCompensation.h"

namespace svc
{

VelocityEngine::VelocityEngine()
{
    clearVoiceState();
    activeState = std::make_unique<EngineState> (uiState);
}

VelocityEngine::~VelocityEngine() = default;

void VelocityEngine::setSampleRate (double rate) noexcept
{
    sampleRate = rate > 0.0 ? rate : 44100.0;
}

void VelocityEngine::setOutputMode (VelocityOutputMode mode) noexcept
{
    outputMode = mode;
}

void VelocityEngine::clearAllPads()
{
    uiState.pads.clear();
    uiState.midiRouting.clearAftertouchSettings();

    auto newState = std::make_unique<EngineState> (uiState);
    std::unique_ptr<EngineState> oldState;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        oldState = std::move (activeState);
        activeState = std::move (newState);
    }

    clearVoiceState();
}

void VelocityEngine::setPadSettings (int note, int channel, const PadSettings& settings)
{
    uiState.pads[{ note, channel }] = settings;
    if (settings.aftertouch.enabled)
        uiState.midiRouting.setAftertouchSettings (note, channel, settings.aftertouch);
    else
        uiState.midiRouting.setAftertouchSettings (note, channel, AftertouchPadSettings{});

    auto newState = std::make_unique<EngineState> (uiState);
    std::unique_ptr<EngineState> oldState;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        oldState = std::move (activeState);
        activeState = std::move (newState);
    }
}

void VelocityEngine::applyProfileState (const MidiRoutingSettings& routing,
                                        const EngineProcessingSettings& processing,
                                        const PadMap& newPads,
                                        const bool resetVoices)
{
    uiState.midiRouting.setSettings (routing);
    uiState.processingSettings = processing;
    uiState.pads = newPads;

    uiState.midiRouting.clearAftertouchSettings();
    for (const auto& entry : uiState.pads)
    {
        if (entry.second.aftertouch.enabled)
            uiState.midiRouting.setAftertouchSettings (entry.first.note,
                                                       entry.first.channel,
                                                       entry.second.aftertouch);
    }

    auto newState = std::make_unique<EngineState> (uiState);
    std::unique_ptr<EngineState> oldState;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        oldState = std::move (activeState);
        activeState = std::move (newState);
    }

    if (resetVoices)
        clearVoiceState();
}

PadSettings VelocityEngine::getPadSettings (int note, int channel) const
{
    return resolvePadSettingsState (uiState, note, channel);
}

void VelocityEngine::setMidiRouting (const MidiRoutingSettings& settings)
{
    uiState.midiRouting.setSettings (settings);

    auto newState = std::make_unique<EngineState> (uiState);
    std::unique_ptr<EngineState> oldState;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        oldState = std::move (activeState);
        activeState = std::move (newState);
    }
}

MidiRoutingSettings VelocityEngine::getMidiRouting() const noexcept
{
    return uiState.midiRouting.getSettings();
}

void VelocityEngine::setProcessingSettings (const EngineProcessingSettings& settings)
{
    uiState.processingSettings = settings;

    auto newState = std::make_unique<EngineState> (uiState);
    std::unique_ptr<EngineState> oldState;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        oldState = std::move (activeState);
        activeState = std::move (newState);
    }
}

EngineProcessingSettings VelocityEngine::getProcessingSettings() const noexcept
{
    return uiState.processingSettings;
}

HistogramSnapshot VelocityEngine::getGlobalHistogramSnapshot() const
{
    return histogramBank.getGlobalSnapshot();
}

HistogramSnapshot VelocityEngine::getPadHistogramSnapshot (int note, int channel) const
{
    return histogramBank.getPadSnapshot (note, channel);
}

const PadSettings* VelocityEngine::findPad (int note, int channel) const
{
    const NoteKey key { note, channel };
    const auto it = uiState.pads.find (key);
    return it != uiState.pads.end() ? &it->second : nullptr;
}

PadSettings VelocityEngine::resolvePadSettings (int note, int channel) const
{
    return resolvePadSettingsState (uiState, note, channel);
}

PadSettings VelocityEngine::resolvePadSettingsState (const EngineState& state, int note, int channel) const
{
    const NoteKey key { note, channel };
    const auto it = state.pads.find (key);
    if (it != state.pads.end())
        return it->second;

    PadSettings defaults;
    defaults.midiNote = note;
    defaults.midiChannel = channel;
    defaults.name = "Note " + juce::String (note);
    return defaults;
}

void VelocityEngine::clearVoiceState() noexcept
{
    for (auto& voice : activeVoices)
        voice = {};

    for (auto& timestamp : retriggerLastTimeUs)
        timestamp.store (-1, std::memory_order_relaxed);
}

void VelocityEngine::markRetriggerTime (int note, int channel, double eventTimeSeconds) noexcept
{
    const auto index = midiNoteChannelIndex (note, channel);
    retriggerLastTimeUs[index].store (static_cast<int64_t> (eventTimeSeconds * 1'000'000.0),
                                      std::memory_order_relaxed);
}

bool VelocityEngine::shouldDropRetrigger (const PadSettings& pad,
                                          int note,
                                          int channel,
                                          double eventTimeSeconds) noexcept
{
    if (pad.retriggerGuardMs <= 0.0)
        return false;

    const auto index = midiNoteChannelIndex (note, channel);
    const auto lastUs = retriggerLastTimeUs[index].load (std::memory_order_relaxed);
    if (lastUs < 0)
        return false;

    const auto eventUs = static_cast<int64_t> (eventTimeSeconds * 1'000'000.0);
    return (eventUs - lastUs) < static_cast<int64_t> (pad.retriggerGuardMs * 1000.0);
}

float VelocityEngine::processNoteVelocity (const PadSettings& pad, float inputNormalized, const EngineProcessingSettings& processing) const
{
    if (! pad.enabled)
        return inputNormalized;

    if (inputNormalized < pad.velocityGate)
    {
        if (pad.gateMode == VelocityGateMode::clampToFloor)
            return pad.curve.mapNormalized (pad.velocityGate);
        return -1.0f;
    }

    auto output = pad.curve.mapNormalized (inputNormalized);
    output = applyLibraryCompensation (output,
                                       processing.libraryPreset,
                                       processing.libraryBlend);
    return applyHumanize (output, processing.humanizeAmount);
}

float VelocityEngine::applyHumanize (float normalized, float humanizeAmount) const
{
    const auto amount = std::clamp (humanizeAmount, 0.0f, 0.25f);
    if (amount <= 0.0f)
        return normalized;

    const auto spread = amount * 0.5f;
    const auto delta = humanizeRandom.nextFloat() * spread * 2.0f - spread;
    return std::clamp (normalized + delta, 0.0f, 1.0f);
}

int VelocityEngine::resolveOutputChannel (const EngineState& state, PadGroup group, int incomingChannel) const
{
    if (! state.processingSettings.zoneRouting.enabled)
        return incomingChannel;

    const auto groupIndex = static_cast<size_t> (group);
    if (groupIndex >= state.processingSettings.zoneRouting.groupOutputChannel.size())
        return incomingChannel;

    const auto overrideChannel = state.processingSettings.zoneRouting.groupOutputChannel[groupIndex];
    return overrideChannel > 0 ? overrideChannel : incomingChannel;
}

VelocityEncoding VelocityEngine::encodeAndApplyOutput (juce::MidiMessage& message,
                                                         float outputNormalized,
                                                         bool inputIsMidi2) const
{
    const auto encoding = encodeOutputVelocity (outputMode, outputNormalized, inputIsMidi2);
    applyEncodingToMidiMessage (message, encoding);
    return encoding;
}

void VelocityEngine::processMidiBuffer (juce::MidiBuffer& buffer, int numSamples)
{
    midi2OutputWords.clear();
    juce::MidiBuffer processed;
    const auto blockDurationSeconds = static_cast<double> (numSamples) / sampleRate;

    const juce::SpinLock::ScopedLockType lock (stateLock);
    if (activeState == nullptr)
        return;

    const auto& state = *activeState;

    for (const auto metadata : buffer)
    {
        auto message = metadata.getMessage();
        const auto sampleOffset = metadata.samplePosition;
        const auto eventTime = runningTimeSeconds + (static_cast<double> (sampleOffset) / sampleRate);

        int physicalNote = -1;
        int physicalChannel = 0;
        if (message.isNoteOn() || message.isNoteOff() || message.isAftertouch())
        {
            physicalNote = message.getNoteNumber();
            physicalChannel = message.getChannel();
        }

        if (! state.midiRouting.processMessage (message))
            continue;

        const auto note = message.getNoteNumber();
        const auto channel = message.getChannel();
        const auto slot = midiNoteChannelIndex (note, channel);

        if (message.isNoteOn())
        {
            const auto inputNormalized = decodeInputFromMidi1 (message.getVelocity());
            const bool inputIsMidi2 = false;
            const auto settings = resolvePadSettingsState (state, physicalNote, physicalChannel);

            if (shouldDropRetrigger (settings, physicalNote, physicalChannel, eventTime))
                continue;

            const auto outputNormalized = processNoteVelocity (settings, inputNormalized, state.processingSettings);
            if (outputNormalized < 0.0f)
            {
                if (! activeVoices[slot].sounding)
                    activeVoices[slot].suppressNextNoteOff = true;
                continue;
            }

            const auto encoding = encodeAndApplyOutput (message, outputNormalized, inputIsMidi2);

            if (encoding.emitMidi2Ump)
            {
                appendMidi2NoteOnUmp (midi2OutputWords,
                                      message.getChannel(),
                                      message.getNoteNumber(),
                                      encoding.midi2);
            }

            auto& voice = activeVoices[slot];
            voice.outputNote = note;
            voice.outputChannel = channel;

            if (state.processingSettings.zoneRouting.enabled)
            {
                voice.outputChannel = resolveOutputChannel (state, settings.group, channel);
                message.setChannel (voice.outputChannel);
            }

            voice.sounding = true;
            voice.suppressNextNoteOff = false;

            histogramBank.record (physicalNote, physicalChannel, inputNormalized, outputNormalized);

            HitEvent hit;
            hit.note = physicalNote;
            hit.channel = physicalChannel;
            hit.inputVelocity = inputNormalized;
            hit.outputVelocity = outputNormalized;
            hit.outputMidi2 = encoding.emitMidi2Ump ? encoding.midi2 : -1;
            hit.isMidi2 = encoding.emitMidi2Ump;
            hit.timestamp = static_cast<std::uint64_t> (eventTime * 1000.0);
            hitFifo.push (hit);

            markRetriggerTime (physicalNote, physicalChannel, eventTime);
            processed.addEvent (message, sampleOffset);
            continue;
        }

        if (message.isNoteOff())
        {
            auto& voice = activeVoices[slot];

            if (voice.suppressNextNoteOff && ! voice.sounding)
            {
                voice.suppressNextNoteOff = false;
                continue;
            }

            if (voice.sounding)
            {
                message.setChannel (voice.outputChannel);
                message.setNoteNumber (voice.outputNote);
                voice.sounding = false;
            }
            else if (state.processingSettings.zoneRouting.enabled)
            {
                const auto settings = resolvePadSettingsState (state, physicalNote, physicalChannel);
                message.setChannel (resolveOutputChannel (state, settings.group, channel));
            }

            if (voice.suppressNextNoteOff)
                voice.suppressNextNoteOff = false;

            processed.addEvent (message, sampleOffset);
            continue;
        }

        if (message.isAftertouch() && state.processingSettings.zoneRouting.enabled)
        {
            auto& voice = activeVoices[slot];
            if (voice.sounding)
                message.setChannel (voice.outputChannel);
            else
            {
                const auto atNote = physicalNote >= 0 ? physicalNote : note;
                const auto atChannel = physicalNote >= 0 ? physicalChannel : channel;
                const auto settings = resolvePadSettingsState (state, atNote, atChannel);
                message.setChannel (resolveOutputChannel (state, settings.group, channel));
            }
        }

        processed.addEvent (message, sampleOffset);
    }

    buffer.swapWith (processed);
    runningTimeSeconds += blockDurationSeconds;
}

} // namespace svc
