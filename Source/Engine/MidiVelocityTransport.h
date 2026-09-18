#pragma once

#include "MidiVelocity.h"
#include <JuceHeader.h>
#include <cstdint>
#include <vector>

namespace svc
{

// Separates curve mapping (normalized float) from wire encoding (MIDI 1 bytes vs MIDI 2 UMP).
struct VelocityEncoding
{
    float normalized = 0.0f;
    int midi1 = 0;
    int midi2 = 0;
    bool emitMidi2Ump = false;
};

inline float decodeInputFromMidi1 (int midi1Velocity) noexcept
{
    return midi1ToNormalized (midi1Velocity);
}

inline float decodeInputFromMidi2 (int midi2Velocity) noexcept
{
    return midi2ToNormalized (midi2Velocity);
}

inline VelocityEncoding encodeOutputVelocity (VelocityOutputMode mode,
                                              float normalized,
                                              bool inputIsMidi2) noexcept
{
    VelocityEncoding encoding;
    encoding.normalized = std::clamp (normalized, 0.0f, 1.0f);
    encoding.midi2 = normalizedToMidi2 (encoding.normalized);

    switch (mode)
    {
        case VelocityOutputMode::midi1:
            encoding.midi1 = std::clamp (normalizedToMidi1 (encoding.normalized), 1, midi1Max);
            encoding.emitMidi2Ump = false;
            break;

        case VelocityOutputMode::midi2:
            encoding.midi1 = std::clamp (downgradeToMidi1 (encoding.midi2), 1, midi1Max);
            encoding.emitMidi2Ump = true;
            break;

        case VelocityOutputMode::autoDetect:
        default:
            if (inputIsMidi2)
            {
                encoding.midi1 = std::clamp (downgradeToMidi1 (encoding.midi2), 1, midi1Max);
                encoding.emitMidi2Ump = true;
            }
            else
            {
                encoding.midi1 = std::clamp (normalizedToMidi1 (encoding.normalized), 1, midi1Max);
                encoding.emitMidi2Ump = false;
            }
            break;
    }

    return encoding;
}

inline void applyEncodingToMidiMessage (juce::MidiMessage& message, const VelocityEncoding& encoding) noexcept
{
    // In MIDI 1.0, Note-On with velocity 0 is defined as Note-Off.
    // Sounding Note-On messages must always carry velocity in [1, 127].
    const auto vel1 = message.isNoteOn() ? std::clamp (encoding.midi1, 1, midi1Max)
                                         : std::clamp (encoding.midi1, 0, midi1Max);
    message.setVelocity (midi1ToNormalized (vel1));
}

void appendMidi2NoteOnUmp (std::vector<std::uint32_t>& storage,
                           int channel,
                           int note,
                           int midi2Velocity) noexcept;

int readMidi2VelocityFromUmpWords (const std::vector<std::uint32_t>& storage,
                                   std::size_t wordOffset = 0) noexcept;

} // namespace svc
