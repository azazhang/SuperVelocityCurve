#include "MidiUtilities.h"
#include <algorithm>

namespace svc
{

void MidiRoutingSettings::setRemap (int sourceNote, int sourceChannel, int targetNote, int targetChannel)
{
    if (sourceNote < 0 || sourceNote >= 128)
        return;

    noteRemaps.erase (std::remove_if (noteRemaps.begin(), noteRemaps.end(),
                                      [sourceNote, sourceChannel] (const NoteRemapEntry& entry)
                                      {
                                          return entry.sourceNote == sourceNote
                                              && entry.sourceChannel == sourceChannel;
                                      }),
                      noteRemaps.end());

    noteRemaps.push_back ({ sourceNote, sourceChannel, targetNote, targetChannel });
    remapEnabled = ! noteRemaps.empty();
}

void MidiRoutingSettings::clearRemaps()
{
    noteRemaps.clear();
    remapEnabled = false;
}

bool MidiRoutingSettings::remapNote (int& note, int& channel) const
{
    if (! remapEnabled || note < 0 || note >= 128)
        return false;

    for (const auto& entry : noteRemaps)
    {
        if (entry.sourceNote != note)
            continue;

        if (entry.sourceChannel != 0 && entry.sourceChannel != channel)
            continue;

        note = entry.targetNote;
        if (entry.targetChannel != 0)
            channel = entry.targetChannel;

        return true;
    }

    return false;
}

bool MidiRoutingSettings::passesChannelFilter (int channel) const
{
    return channel == 0 || inputChannelFilter == 0 || inputChannelFilter == channel;
}

int MidiRoutingSettings::transformOutputChannel (int channel) const
{
    return outputChannel != 0 ? outputChannel : channel;
}

void MidiRoutingProcessor::setSettings (const MidiRoutingSettings& settings)
{
    routing = settings;
}

void MidiRoutingProcessor::setAftertouchSettings (int note, int channel, const AftertouchPadSettings& settings)
{
    aftertouchPads[{ note, channel }] = settings;
}

void MidiRoutingProcessor::clearAftertouchSettings()
{
    aftertouchPads.clear();
}

float MidiRoutingProcessor::processAftertouch (int note, int channel, float pressure) const
{
    const AtKey key { note, channel };
    const auto it = aftertouchPads.find (key);
    if (it == aftertouchPads.end() || ! it->second.enabled)
        return pressure;

    return it->second.curve.mapNormalized (pressure);
}

float MidiRoutingProcessor::processChannelPressure (int channel, float pressure) const
{
    const AtKey key { kChannelPressureNote, channel };
    const auto it = aftertouchPads.find (key);
    if (it == aftertouchPads.end() || ! it->second.enabled)
        return pressure;

    return it->second.curve.mapNormalized (pressure);
}

bool MidiRoutingProcessor::processMessage (juce::MidiMessage& message) const
{
    auto channel = message.getChannel();
    if (! routing.passesChannelFilter (channel))
        return false;

    if (message.isNoteOn() || message.isNoteOff())
    {
        auto note = message.getNoteNumber();
        routing.remapNote (note, channel);

        const auto outCh = routing.transformOutputChannel (channel);
        if (message.isNoteOn())
            message = juce::MidiMessage::noteOn (outCh, note, message.getFloatVelocity());
        else
            message = juce::MidiMessage::noteOff (outCh, note, message.getFloatVelocity());

        return true;
    }

    if (message.isAftertouch())
    {
        const auto physicalNote = message.getNoteNumber();
        const auto shaped = processAftertouch (physicalNote, channel, static_cast<float> (message.getAfterTouchValue()) / 127.0f);
        auto note = physicalNote;
        routing.remapNote (note, channel);
        message = juce::MidiMessage::aftertouchChange (routing.transformOutputChannel (channel),
                                                       note,
                                                       juce::jlimit (0, 127, static_cast<int> (std::round (shaped * 127.0f))));
        return true;
    }

    if (message.isChannelPressure())
    {
        const auto shaped = processChannelPressure (channel, static_cast<float> (message.getChannelPressureValue()) / 127.0f);
        message = juce::MidiMessage::channelPressureChange (routing.transformOutputChannel (channel),
                                                            juce::jlimit (0, 127, static_cast<int> (std::round (shaped * 127.0f))));
        return true;
    }

    if (channel > 0)
        message.setChannel (routing.transformOutputChannel (channel));

    return true;
}

void VelocityHistogram::record (float inputNormalized, float outputNormalized) noexcept
{
    const auto inBin = juce::jlimit (0, 127, static_cast<int> (inputNormalized * 127.0f));
    const auto outBin = juce::jlimit (0, 127, static_cast<int> (outputNormalized * 127.0f));
    inputBins[static_cast<size_t> (inBin)].fetch_add (1, std::memory_order_relaxed);
    outputBins[static_cast<size_t> (outBin)].fetch_add (1, std::memory_order_relaxed);
}

void VelocityHistogram::clear() noexcept
{
    for (auto& bin : inputBins)
        bin.store (0, std::memory_order_relaxed);
    for (auto& bin : outputBins)
        bin.store (0, std::memory_order_relaxed);
}

HistogramSnapshot VelocityHistogram::snapshot() const
{
    HistogramSnapshot copy;
    for (int i = 0; i < 128; ++i)
    {
        copy.inputBins[static_cast<size_t> (i)] = inputBins[static_cast<size_t> (i)].load (std::memory_order_relaxed);
        copy.outputBins[static_cast<size_t> (i)] = outputBins[static_cast<size_t> (i)].load (std::memory_order_relaxed);
    }
    return copy;
}

HistogramBank::HistogramBank()
    : perPad (std::make_unique<std::array<VelocityHistogram, kMidiNoteChannelSlots>>())
{
}

void HistogramBank::record (int note, int channel, float inputNormalized, float outputNormalized) noexcept
{
    global.record (inputNormalized, outputNormalized);
    (*perPad)[midiNoteChannelIndex (note, channel)].record (inputNormalized, outputNormalized);
}

void HistogramBank::clear() noexcept
{
    global.clear();
    for (auto& histogram : *perPad)
        histogram.clear();
}

void HistogramBank::clearPad (int note, int channel) noexcept
{
    (*perPad)[midiNoteChannelIndex (note, channel)].clear();
}

HistogramSnapshot HistogramBank::getGlobalSnapshot() const
{
    return global.snapshot();
}

HistogramSnapshot HistogramBank::getPadSnapshot (int note, int channel) const
{
    return (*perPad)[midiNoteChannelIndex (note, channel)].snapshot();
}

} // namespace svc
