#include "VelocityEngine.h"
#include "LibraryCompensation.h"

namespace svc
{

static void retireState (VelocityEngine::EngineState* oldState)
{
    if (oldState != nullptr)
    {
        if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
            juce::MessageManager::callAsync ([oldState] { delete oldState; });
        else
            delete oldState;
    }
}

VelocityEngine::VelocityEngine()
{
    clearVoiceState();
    auto initialState = std::make_unique<EngineState>();
    activeState.store (initialState.release(), std::memory_order_release);
}

VelocityEngine::~VelocityEngine()
{
    if (auto* state = activeState.load (std::memory_order_acquire))
        delete state;
}

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
    auto newState = std::make_unique<EngineState>();
    if (auto* current = activeState.load (std::memory_order_acquire))
    {
        newState->processingSettings = current->processingSettings;
        newState->midiRouting = current->midiRouting;
    }
    newState->pads.clear();
    newState->midiRouting.clearAftertouchSettings();

    auto* oldState = activeState.exchange (newState.release(), std::memory_order_acq_rel);
    retireState (oldState);

    clearVoiceState();
}

void VelocityEngine::setPadSettings (int note, int channel, const PadSettings& settings)
{
    auto newState = std::make_unique<EngineState>();
    if (auto* current = activeState.load (std::memory_order_acquire))
        *newState = *current;

    newState->pads[{ note, channel }] = settings;
    if (settings.aftertouch.enabled)
        newState->midiRouting.setAftertouchSettings (note, channel, settings.aftertouch);
    else
        newState->midiRouting.setAftertouchSettings (note, channel, AftertouchPadSettings{});

    auto* oldState = activeState.exchange (newState.release(), std::memory_order_acq_rel);
    retireState (oldState);
}

void VelocityEngine::applyProfileState (const MidiRoutingSettings& routing,
                                        const EngineProcessingSettings& processing,
                                        const PadMap& newPads,
                                        const bool resetVoices)
{
    auto newState = std::make_unique<EngineState>();
    newState->midiRouting.setSettings (routing);
    newState->processingSettings = processing;
    newState->pads = newPads;

    newState->midiRouting.clearAftertouchSettings();
    for (const auto& entry : newState->pads)
    {
        if (entry.second.aftertouch.enabled)
            newState->midiRouting.setAftertouchSettings (entry.first.note,
                                                       entry.first.channel,
                                                       entry.second.aftertouch);
    }

    auto* oldState = activeState.exchange (newState.release(), std::memory_order_acq_rel);
    retireState (oldState);

    if (resetVoices)
        clearVoiceState();
}

PadSettings VelocityEngine::getPadSettings (int note, int channel) const
{
    auto* state = activeState.load (std::memory_order_acquire);
    if (state != nullptr)
        return resolvePadSettingsState (*state, note, channel);

    PadSettings defaults;
    defaults.midiNote = note;
    defaults.midiChannel = channel;
    defaults.name = "Note " + juce::String (note);
    return defaults;
}

void VelocityEngine::setMidiRouting (const MidiRoutingSettings& settings)
{
    auto newState = std::make_unique<EngineState>();
    if (auto* current = activeState.load (std::memory_order_acquire))
        *newState = *current;

    newState->midiRouting.setSettings (settings);

    auto* oldState = activeState.exchange (newState.release(), std::memory_order_acq_rel);
    retireState (oldState);
}

MidiRoutingSettings VelocityEngine::getMidiRouting() const noexcept
{
    auto* state = activeState.load (std::memory_order_acquire);
    return state != nullptr ? state->midiRouting.getSettings() : MidiRoutingSettings{};
}

void VelocityEngine::setProcessingSettings (const EngineProcessingSettings& settings)
{
    auto newState = std::make_unique<EngineState>();
    if (auto* current = activeState.load (std::memory_order_acquire))
        *newState = *current;

    newState->processingSettings = settings;

    auto* oldState = activeState.exchange (newState.release(), std::memory_order_acq_rel);
    retireState (oldState);
}

EngineProcessingSettings VelocityEngine::getProcessingSettings() const noexcept
{
    auto* state = activeState.load (std::memory_order_acquire);
    return state != nullptr ? state->processingSettings : EngineProcessingSettings{};
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
    auto* state = activeState.load (std::memory_order_acquire);
    if (state == nullptr)
        return nullptr;
    const NoteKey key { note, channel };
    const auto it = state->pads.find (key);
    return it != state->pads.end() ? &it->second : nullptr;
}

PadSettings VelocityEngine::resolvePadSettings (int note, int channel) const
{
    auto* state = activeState.load (std::memory_order_acquire);
    if (state != nullptr)
        return resolvePadSettingsState (*state, note, channel);

    PadSettings defaults;
    defaults.midiNote = note;
    defaults.midiChannel = channel;
    defaults.name = "Note " + juce::String (note);
    return defaults;
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

int VelocityEngine::resolveOutputChannel (PadGroup group, int incomingChannel) const
{
    auto* state = activeState.load (std::memory_order_acquire);
    if (state == nullptr || ! state->processingSettings.zoneRouting.enabled)
        return incomingChannel;

    const auto groupIndex = static_cast<size_t> (group);
    if (groupIndex >= state->processingSettings.zoneRouting.groupOutputChannel.size())
        return incomingChannel;

    const auto overrideChannel = state->processingSettings.zoneRouting.groupOutputChannel[groupIndex];
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

    auto* state = activeState.load (std::memory_order_acquire);
    if (state == nullptr)
        return;

    for (const auto metadata : buffer)
    {
        auto message = metadata.getMessage();
        const auto sampleOffset = metadata.samplePosition;
        const auto eventTime = runningTimeSeconds + (static_cast<double> (sampleOffset) / sampleRate);

        int physicalNote = -1;
        int physicalChannel = 0;
        if (message.isNoteOn() || message.isNoteOff())
        {
            physicalNote = message.getNoteNumber();
            physicalChannel = message.getChannel();
        }

        if (! state->midiRouting.processMessage (message))
            continue;

        const auto note = message.getNoteNumber();
        const auto channel = message.getChannel();
        const auto slot = midiNoteChannelIndex (note, channel);

        if (message.isNoteOn())
        {
            const auto inputNormalized = decodeInputFromMidi1 (message.getVelocity());
            const bool inputIsMidi2 = false;
            const auto settings = resolvePadSettingsState (*state, physicalNote, physicalChannel);

            if (shouldDropRetrigger (settings, physicalNote, physicalChannel, eventTime))
                continue;

            const auto outputNormalized = processNoteVelocity (settings, inputNormalized, state->processingSettings);
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

            if (state->processingSettings.zoneRouting.enabled)
            {
                voice.outputChannel = resolveOutputChannel (settings.group, channel);
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
            else if (state->processingSettings.zoneRouting.enabled)
            {
                const auto settings = resolvePadSettingsState (*state, physicalNote, physicalChannel);
                message.setChannel (resolveOutputChannel (settings.group, channel));
            }

            if (voice.suppressNextNoteOff)
                voice.suppressNextNoteOff = false;

            processed.addEvent (message, sampleOffset);
            continue;
        }

        if (message.isAftertouch() && state->processingSettings.zoneRouting.enabled)
        {
            auto& voice = activeVoices[slot];
            if (voice.sounding)
                message.setChannel (voice.outputChannel);
            else
            {
                const auto atNote = physicalNote >= 0 ? physicalNote : note;
                const auto atChannel = physicalNote >= 0 ? physicalChannel : channel;
                const auto settings = resolvePadSettingsState (*state, atNote, atChannel);
                message.setChannel (resolveOutputChannel (settings.group, channel));
            }
        }

        processed.addEvent (message, sampleOffset);
    }

    buffer.swapWith (processed);
    runningTimeSeconds += blockDurationSeconds;
}

} // namespace svc
