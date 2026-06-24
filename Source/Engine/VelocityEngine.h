#pragma once

#include "../Profiles/PadTypes.h"
#include "EngineSettings.h"
#include "HitEventFifo.h"
#include "MidiUtilities.h"
#include "MidiVelocity.h"
#include "MidiVelocityTransport.h"
#include "VelocityCurve.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace svc
{

struct PadSettings
{
    int midiNote = 36;
    int midiChannel = 10;
    juce::String name;
    PadGroup group = PadGroup::other;
    VelocityCurve curve;
    bool enabled = true;
    float velocityGate = 0.0f;
    VelocityGateMode gateMode = VelocityGateMode::drop;
    double retriggerGuardMs = 0.0;
    AftertouchPadSettings aftertouch;
};

class VelocityEngine
{
public:
    struct NoteKey
    {
        int note;
        int channel;
        bool operator== (const NoteKey& o) const noexcept { return note == o.note && channel == o.channel; }
    };

    struct NoteKeyHash
    {
        std::size_t operator() (const NoteKey& key) const noexcept
        {
            return static_cast<std::size_t> (key.note) * 17u + static_cast<std::size_t> (key.channel);
        }
    };

    using PadMap = std::unordered_map<NoteKey, PadSettings, NoteKeyHash>;

    struct EngineState
    {
        PadMap pads;
        MidiRoutingProcessor midiRouting;
        EngineProcessingSettings processingSettings;
    };

    VelocityEngine();
    ~VelocityEngine();

    void setSampleRate (double rate) noexcept;
    void setOutputMode (VelocityOutputMode mode) noexcept;
    VelocityOutputMode getOutputMode() const noexcept { return outputMode; }

    void clearAllPads();
    void setPadSettings (int note, int channel, const PadSettings& settings);
    void applyProfileState (const MidiRoutingSettings& routing,
                            const EngineProcessingSettings& processing,
                            const PadMap& newPads,
                            bool resetVoices);
    PadSettings getPadSettings (int note, int channel) const;

    void setMidiRouting (const MidiRoutingSettings& settings);
    MidiRoutingSettings getMidiRouting() const noexcept;

    void setProcessingSettings (const EngineProcessingSettings& settings);
    EngineProcessingSettings getProcessingSettings() const noexcept;

    void processMidiBuffer (juce::MidiBuffer& buffer, int numSamples);
    const std::vector<std::uint32_t>& getMidi2OutputWords() const noexcept { return midi2OutputWords; }
    void clearMidi2OutputWords() noexcept { midi2OutputWords.clear(); }
    HitEventFifo& getHitFifo() noexcept { return hitFifo; }
    HistogramBank& getHistogramBank() noexcept { return histogramBank; }
    const HistogramBank& getHistogramBank() const noexcept { return histogramBank; }
    HistogramSnapshot getGlobalHistogramSnapshot() const;
    HistogramSnapshot getPadHistogramSnapshot (int note, int channel) const;
    void clearHistogram() noexcept { histogramBank.clear(); }
    void clearPadHistogram (int note, int channel) noexcept { histogramBank.clearPad (note, channel); }

private:
    PadSettings resolvePadSettingsState (const EngineState& state, int note, int channel) const;

    struct ActiveVoice
    {
        bool sounding = false;
        bool suppressNextNoteOff = false;
        int outputNote = 0;
        int outputChannel = 1;
    };

    std::atomic<EngineState*> activeState { nullptr };

    std::array<ActiveVoice, kMidiNoteChannelSlots> activeVoices {};
    std::array<std::atomic<int64_t>, kMidiNoteChannelSlots> retriggerLastTimeUs {};
    mutable juce::Random humanizeRandom;
    VelocityOutputMode outputMode = VelocityOutputMode::autoDetect;
    HitEventFifo hitFifo;
    HistogramBank histogramBank;
    std::vector<std::uint32_t> midi2OutputWords;
    double sampleRate = 44100.0;
    double runningTimeSeconds = 0.0;

    const PadSettings* findPad (int note, int channel) const;
    float processNoteVelocity (const PadSettings& pad, float inputNormalized, const EngineProcessingSettings& processing) const;
    float applyHumanize (float normalized, float humanizeAmount) const;
    int resolveOutputChannel (PadGroup group, int incomingChannel) const;
    VelocityEncoding encodeAndApplyOutput (juce::MidiMessage& message,
                                           float outputNormalized,
                                           bool inputIsMidi2) const;
    bool shouldDropRetrigger (const PadSettings& pad, int note, int channel, double eventTimeSeconds) noexcept;
    PadSettings resolvePadSettings (int note, int channel) const;
    void clearVoiceState() noexcept;
    void markRetriggerTime (int note, int channel, double eventTimeSeconds) noexcept;
};

} // namespace svc
