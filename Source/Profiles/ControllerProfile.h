#pragma once

#include "../Engine/EngineSettings.h"
#include "../Engine/MidiUtilities.h"
#include "../Engine/VelocityCurve.h"
#include "PadTypes.h"
#include <JuceHeader.h>
#include <optional>
#include <vector>

namespace svc
{

struct PadSettings;

enum class ProfileLayout
{
    gmStandard,
    launchpadDrumRack,
    maschineGroup,
    spdSx,
    fgdp,
    custom
};

enum class PadMutationResult
{
    ok,
    indexOutOfRange,
    wouldEmptyProfile,
    duplicateMidiKey,
    invalidMidiKey,
    maxPadsReached
};

inline constexpr int kMinProfilePads = 1;
inline constexpr int kMaxProfilePads = 128;

struct ProfilePad
{
    int midiNote = 36;
    int midiChannel = 10;
    juce::String label;
    int gridRow = 0;
    int gridCol = 0;
    PadGroup group = PadGroup::other;
    VelocityCurve curve;
    bool enabled = true;
    float velocityGate = 0.0f;
    VelocityGateMode gateMode = VelocityGateMode::drop;
    double retriggerGuardMs = 0.0;
    AftertouchPadSettings aftertouch;
};

class ControllerProfile
{
public:
    ControllerProfile() = default;
    explicit ControllerProfile (juce::String profileName, ProfileLayout layoutType);

    const juce::String& getName() const noexcept { return name; }
    void setName (const juce::String& newName) { name = newName; }
    ProfileLayout getLayout() const noexcept { return layout; }
    void setLayout (ProfileLayout newLayout) { layout = newLayout; }

    const std::vector<ProfilePad>& getPads() const noexcept { return pads; }
    std::vector<ProfilePad>& getPads() noexcept { return pads; }

    void getGridDimensions (int& rows, int& cols) const noexcept;
    int getDisplayGridColumns() const noexcept;
    int findPadIndex (int midiNote, int midiChannel) const;
    bool hasDuplicateMidiKey (int midiNote, int midiChannel, int ignoreIndex = -1) const;
    std::pair<int, int> suggestNextGridCell (std::optional<int> columnsOverride = std::nullopt) const;
    ProfilePad makeDefaultPad() const;
    PadMutationResult addPad (ProfilePad pad, std::optional<int> insertIndex = std::nullopt);
    PadMutationResult removePad (int index);
    PadMutationResult setPadAt (int index, const ProfilePad& pad);
    PadMutationResult swapPads (int indexA, int indexB);
    PadMutationResult movePadToCell (int index, int targetRow, int targetCol);
    PadMutationResult duplicatePad (int sourceIndex, std::optional<std::pair<int, int>> targetCell = std::nullopt);
    PadMutationResult renamePad (int index, const juce::String& newLabel);
    void applyToEngine (class VelocityEngine& engine) const;

    static PadSettings toEngineSettings (const ProfilePad& pad);

    MidiRoutingSettings& getMidiRouting() noexcept { return midiRouting; }
    const MidiRoutingSettings& getMidiRouting() const noexcept { return midiRouting; }
    EngineProcessingSettings& getProcessingSettings() noexcept { return processingSettings; }
    const EngineProcessingSettings& getProcessingSettings() const noexcept { return processingSettings; }
    ControllerProfile copy() const;

    juce::ValueTree toValueTree() const;
    static ControllerProfile fromValueTree (const juce::ValueTree& tree);

    static ControllerProfile createGMStandard();
    static ControllerProfile createLaunchpadDrumRack();
    static ControllerProfile createMaschineGroup();
    static ControllerProfile createSpdSx();
    static ControllerProfile createFgdp();
    static ControllerProfile createBlank();

private:
    juce::String name { "GM Standard" };
    ProfileLayout layout = ProfileLayout::gmStandard;
    std::vector<ProfilePad> pads;
    MidiRoutingSettings midiRouting;
    EngineProcessingSettings processingSettings;
};

} // namespace svc
