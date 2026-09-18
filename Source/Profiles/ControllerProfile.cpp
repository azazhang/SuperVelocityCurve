#include "ControllerProfile.h"
#include "../Engine/VelocityEngine.h"
#include "GMDrumMap.h"
#include <array>

namespace svc
{

namespace
{
juce::ValueTree curveToTree (const VelocityCurve& curve)
{
    juce::ValueTree tree ("Curve");
    tree.setProperty ("floor", curve.getFloor(), nullptr);
    tree.setProperty ("ceiling", curve.getCeiling(), nullptr);

    for (const auto& point : curve.getControlPoints())
    {
        juce::ValueTree pointTree ("Point");
        pointTree.setProperty ("input", point.input, nullptr);
        pointTree.setProperty ("output", point.output, nullptr);
        tree.appendChild (pointTree, nullptr);
    }

    return tree;
}

VelocityCurve curveFromTree (const juce::ValueTree& tree)
{
    VelocityCurve curve;
    curve.setFloor (tree.getProperty ("floor", 0.0f));
    curve.setCeiling (tree.getProperty ("ceiling", 1.0f));

    std::vector<CurveControlPoint> points;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto pointTree = tree.getChild (i);
        if (pointTree.hasType ("Point"))
        {
            points.push_back ({
                static_cast<float> (pointTree.getProperty ("input")),
                static_cast<float> (pointTree.getProperty ("output"))
            });
        }
    }

    if (points.size() >= 2)
        curve.setControlPoints (points);

    return curve;
}
} // namespace

ControllerProfile::ControllerProfile (juce::String profileName, ProfileLayout layoutType)
    : name (std::move (profileName)), layout (layoutType)
{
}

void ControllerProfile::getGridDimensions (int& rows, int& cols) const noexcept
{
    rows = 1;
    cols = 1;

    for (const auto& pad : pads)
    {
        rows = std::max (rows, pad.gridRow + 1);
        cols = std::max (cols, pad.gridCol + 1);
    }
}

ControllerProfile ControllerProfile::copy() const
{
    ControllerProfile copy;
    copy.name = name;
    copy.layout = layout;
    copy.pads = pads;
    copy.midiRouting = midiRouting;
    copy.processingSettings = processingSettings;
    return copy;
}

PadSettings ControllerProfile::toEngineSettings (const ProfilePad& pad)
{
    PadSettings settings;
    settings.midiNote = pad.midiNote;
    settings.midiChannel = pad.midiChannel;
    settings.name = pad.label;
    settings.group = pad.group;
    settings.curve = pad.curve;
    settings.enabled = pad.enabled;
    settings.velocityGate = pad.velocityGate;
    settings.gateMode = pad.gateMode;
    settings.retriggerGuardMs = pad.retriggerGuardMs;
    settings.aftertouch = pad.aftertouch;
    return settings;
}

void ControllerProfile::applyToEngine (VelocityEngine& engine) const
{
    VelocityEngine::PadMap padMap;
    padMap.reserve (pads.size());

    for (const auto& pad : pads)
        padMap[{ pad.midiNote, pad.midiChannel }] = toEngineSettings (pad);

    engine.applyProfileState (midiRouting, processingSettings, padMap, true);
}

juce::ValueTree ControllerProfile::toValueTree() const
{
    juce::ValueTree tree ("ControllerProfile");
    tree.setProperty ("name", name, nullptr);
    tree.setProperty ("layout", static_cast<int> (layout), nullptr);
    tree.setProperty ("inputChannel", midiRouting.inputChannelFilter, nullptr);
    tree.setProperty ("outputChannel", midiRouting.outputChannel, nullptr);
    tree.setProperty ("remapEnabled", midiRouting.remapEnabled, nullptr);
    tree.setProperty ("humanizeAmount", processingSettings.humanizeAmount, nullptr);
    tree.setProperty ("libraryPreset", libraryCompensationPresetName (processingSettings.libraryPreset), nullptr);
    tree.setProperty ("libraryBlend", processingSettings.libraryBlend, nullptr);
    tree.setProperty ("zoneRoutingEnabled", processingSettings.zoneRouting.enabled, nullptr);

    juce::ValueTree zoneTree ("ZoneRouting");
    const char* groupNames[] = { "Other", "Kick", "Snare", "Hat", "Tom", "Cymbal", "Percussion" };
    for (size_t i = 0; i < processingSettings.zoneRouting.groupOutputChannel.size(); ++i)
    {
        juce::ValueTree zone ("Group");
        zone.setProperty ("name", groupNames[i], nullptr);
        zone.setProperty ("channel", processingSettings.zoneRouting.groupOutputChannel[i], nullptr);
        zoneTree.appendChild (zone, nullptr);
    }
    tree.appendChild (zoneTree, nullptr);

    juce::ValueTree remaps ("Remaps");
    for (const auto& e : midiRouting.getRemaps())
    {
        juce::ValueTree r ("Remap");
        r.setProperty ("sourceNote", e.sourceNote, nullptr);
        r.setProperty ("sourceChannel", e.sourceChannel, nullptr);
        r.setProperty ("targetNote", e.targetNote, nullptr);
        r.setProperty ("targetChannel", e.targetChannel, nullptr);
        remaps.appendChild (r, nullptr);
    }
    tree.appendChild (remaps, nullptr);

    for (const auto& pad : pads)
    {
        juce::ValueTree padTree ("Pad");
        padTree.setProperty ("midiNote", pad.midiNote, nullptr);
        padTree.setProperty ("midiChannel", pad.midiChannel, nullptr);
        padTree.setProperty ("label", pad.label, nullptr);
        padTree.setProperty ("gridRow", pad.gridRow, nullptr);
        padTree.setProperty ("gridCol", pad.gridCol, nullptr);
        padTree.setProperty ("enabled", pad.enabled, nullptr);
        padTree.setProperty ("velocityGate", pad.velocityGate, nullptr);
        padTree.setProperty ("gateMode", gateModeToString (pad.gateMode), nullptr);
        padTree.setProperty ("padGroup", padGroupToString (pad.group), nullptr);
        padTree.setProperty ("retriggerGuardMs", pad.retriggerGuardMs, nullptr);
        padTree.setProperty ("aftertouchEnabled", pad.aftertouch.enabled, nullptr);
        padTree.appendChild (curveToTree (pad.curve), nullptr);
        if (pad.aftertouch.enabled)
        {
            auto atTree = curveToTree (pad.aftertouch.curve);
            atTree.setProperty ("aftertouch", true, nullptr);
            padTree.appendChild (atTree, nullptr);
        }
        tree.appendChild (padTree, nullptr);
    }

    return tree;
}

ControllerProfile ControllerProfile::fromValueTree (const juce::ValueTree& tree)
{
    ControllerProfile profile;
    profile.name = tree.getProperty ("name", "Custom");
    profile.layout = static_cast<ProfileLayout> (static_cast<int> (tree.getProperty ("layout", 0)));
    profile.midiRouting.inputChannelFilter = tree.getProperty ("inputChannel", 0);
    profile.midiRouting.outputChannel = tree.getProperty ("outputChannel", 0);
    profile.midiRouting.remapEnabled = tree.getProperty ("remapEnabled", false);
    profile.processingSettings.humanizeAmount = tree.getProperty ("humanizeAmount", 0.0f);
    profile.processingSettings.libraryPreset = libraryCompensationPresetFromName (
        tree.getProperty ("libraryPreset", "None").toString().toStdString());
    profile.processingSettings.libraryBlend = tree.getProperty ("libraryBlend", 0.0f);
    profile.processingSettings.zoneRouting.enabled = tree.getProperty ("zoneRoutingEnabled", false);

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);

        if (child.hasType ("ZoneRouting"))
        {
            for (int z = 0; z < child.getNumChildren(); ++z)
            {
                const auto zone = child.getChild (z);
                const auto name = zone.getProperty ("name", juce::String()).toString();
                const auto channel = static_cast<int> (zone.getProperty ("channel", 0));
                if (name == "Other")       profile.processingSettings.zoneRouting.groupOutputChannel[0] = channel;
                else if (name == "Kick")   profile.processingSettings.zoneRouting.groupOutputChannel[1] = channel;
                else if (name == "Snare")  profile.processingSettings.zoneRouting.groupOutputChannel[2] = channel;
                else if (name == "Hat")    profile.processingSettings.zoneRouting.groupOutputChannel[3] = channel;
                else if (name == "Tom")    profile.processingSettings.zoneRouting.groupOutputChannel[4] = channel;
                else if (name == "Cymbal") profile.processingSettings.zoneRouting.groupOutputChannel[5] = channel;
                else if (name == "Percussion") profile.processingSettings.zoneRouting.groupOutputChannel[6] = channel;
            }
            continue;
        }

        if (child.hasType ("Remaps"))
        {
            for (int r = 0; r < child.getNumChildren(); ++r)
            {
                const auto remapTree = child.getChild (r);
                profile.midiRouting.setRemap (
                    remapTree.getProperty ("sourceNote", 0),
                    remapTree.getProperty ("sourceChannel", 0),
                    remapTree.getProperty ("targetNote", 0),
                    remapTree.getProperty ("targetChannel", 0));
            }
            continue;
        }

        if (! child.hasType ("Pad"))
            continue;

        ProfilePad pad;
        pad.midiNote = child.getProperty ("midiNote", 36);
        pad.midiChannel = child.getProperty ("midiChannel", 10);
        pad.label = child.getProperty ("label", juce::String());
        pad.gridRow = child.getProperty ("gridRow", 0);
        pad.gridCol = child.getProperty ("gridCol", 0);
        pad.enabled = child.getProperty ("enabled", true);
        pad.velocityGate = child.getProperty ("velocityGate", 0.0f);
        pad.gateMode = gateModeFromString (child.getProperty ("gateMode", "Drop notes").toString());
        pad.group = padGroupFromString (child.getProperty ("padGroup", "Other").toString());
        pad.retriggerGuardMs = child.getProperty ("retriggerGuardMs", 0.0);
        pad.aftertouch.enabled = child.getProperty ("aftertouchEnabled", false);

        for (int c = 0; c < child.getNumChildren(); ++c)
        {
            const auto curveTree = child.getChild (c);
            if (! curveTree.hasType ("Curve"))
                continue;

            if (static_cast<bool> (curveTree.getProperty ("aftertouch", false)))
                pad.aftertouch.curve = curveFromTree (curveTree);
            else
                pad.curve = curveFromTree (curveTree);
        }

        profile.pads.push_back (pad);
    }

    return profile;
}

namespace
{
PadGroup inferGroupFromName (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    if (lower.contains ("kick") || lower.contains ("bd"))
        return PadGroup::kick;
    if (lower.contains ("snare") || lower.contains ("rim"))
        return PadGroup::snare;
    if (lower.contains ("hh") || lower.contains ("hat"))
        return PadGroup::hat;
    if (lower.contains ("tom"))
        return PadGroup::tom;
    if (lower.contains ("crash") || lower.contains ("ride") || lower.contains ("cym"))
        return PadGroup::cymbal;
    if (lower.contains ("clap") || lower.contains ("tamb") || lower.contains ("cow"))
        return PadGroup::percussion;
    return PadGroup::other;
}
} // namespace

ControllerProfile ControllerProfile::createGMStandard()
{
    ControllerProfile profile ("GM Standard", ProfileLayout::gmStandard);

    for (const auto& def : getGMDrumPads())
    {
        ProfilePad pad;
        pad.midiNote = def.midiNote;
        pad.midiChannel = 10;
        pad.label = def.name;
        pad.gridRow = def.row;
        pad.gridCol = def.col;
        pad.group = inferGroupFromName (def.name);
        profile.pads.push_back (pad);
    }

    return profile;
}

ControllerProfile ControllerProfile::createLaunchpadDrumRack()
{
    ControllerProfile profile ("Launchpad Drum Rack", ProfileLayout::launchpadDrumRack);

    for (int row = 7; row >= 0; --row)
    {
        for (int col = 0; col < 8; ++col)
        {
            const int note = 36 + (7 - row) * 8 + col;
            ProfilePad pad;
            pad.midiNote = note;
            pad.midiChannel = 1;
            pad.label = "N" + juce::String (note);
            pad.gridRow = row;
            pad.gridCol = col;
            pad.group = inferGroupFromName (getGMDrumName (note));
            profile.pads.push_back (pad);
        }
    }

    return profile;
}

ControllerProfile ControllerProfile::createMaschineGroup()
{
    ControllerProfile profile ("Maschine Group", ProfileLayout::maschineGroup);

    int slot = 1;
    for (int row = 3; row >= 0; --row)
    {
        for (int col = 0; col < 4; ++col)
        {
            ProfilePad pad;
            pad.midiNote = 35 + slot;
            pad.midiChannel = 1;
            pad.label = "Slot " + juce::String (slot);
            pad.gridRow = row;
            pad.gridCol = col;
            pad.group = PadGroup::other;
            profile.pads.push_back (pad);
            ++slot;
        }
    }

    return profile;
}

ControllerProfile ControllerProfile::createSpdSx()
{
    ControllerProfile profile ("Roland SPD-SX", ProfileLayout::spdSx);

    int note = 60;
    for (int i = 0; i < 12; ++i)
    {
        ProfilePad pad;
        pad.midiNote = note + i;
        pad.midiChannel = 10;
        pad.label = "Pad " + juce::String (i + 1);
        pad.gridRow = i / 4;
        pad.gridCol = i % 4;
        profile.pads.push_back (pad);
    }

    static const int gmTargets[] = { 36, 38, 42, 46, 41, 43, 45, 47, 49, 51, 37, 40 };
    for (int i = 0; i < 12; ++i)
        profile.midiRouting.setRemap (60 + i, 10, gmTargets[i], 10);

    return profile;
}

ControllerProfile ControllerProfile::createFgdp()
{
    ControllerProfile profile ("Yamaha FGDP", ProfileLayout::fgdp);
    profile.midiRouting.outputChannel = 3;

    const int notes[] = { 36, 38, 42, 46, 41, 43, 45, 47, 49, 51, 37, 40 };
    const char* labels[] = {
        "Kick", "Snare", "Closed HH", "Open HH",
        "Low Tom", "Mid Tom", "High Tom", "Floor Tom",
        "Crash", "Ride", "Rim", "Clap"
    };

    for (int i = 0; i < 12; ++i)
    {
        ProfilePad pad;
        pad.midiNote = notes[i];
        pad.midiChannel = 3;
        pad.label = labels[i];
        pad.gridRow = i / 4;
        pad.gridCol = i % 4;
        pad.group = inferGroupFromName (labels[i]);
        profile.pads.push_back (pad);
    }

    for (int i = 0; i < 8; ++i)
    {
        ProfilePad pad;
        pad.midiNote = 82 + i;
        pad.midiChannel = 3;
        pad.label = "Rx " + juce::String (i + 1);
        pad.gridRow = 4;
        pad.gridCol = i;
        pad.group = PadGroup::percussion;
        profile.pads.push_back (pad);
    }

    return profile;
}

int ControllerProfile::getDisplayGridColumns() const noexcept
{
    return layout == ProfileLayout::launchpadDrumRack ? 8 : 4;
}

int ControllerProfile::findPadIndex (int midiNote, int midiChannel) const
{
    for (int i = 0; i < static_cast<int> (pads.size()); ++i)
    {
        const auto& pad = pads[static_cast<size_t> (i)];
        if (pad.midiNote == midiNote && pad.midiChannel == midiChannel)
            return i;
    }
    return -1;
}

bool ControllerProfile::hasDuplicateMidiKey (int midiNote, int midiChannel, int ignoreIndex) const
{
    for (int i = 0; i < static_cast<int> (pads.size()); ++i)
    {
        if (i == ignoreIndex)
            continue;

        const auto& pad = pads[static_cast<size_t> (i)];
        if (pad.midiNote == midiNote && pad.midiChannel == midiChannel)
            return true;
    }
    return false;
}

std::pair<int, int> ControllerProfile::suggestNextGridCell (std::optional<int> columnsOverride) const
{
    const auto cols = columnsOverride.value_or (getDisplayGridColumns());
    constexpr int kMaxCells = 256;
    std::array<bool, kMaxCells> occupied {};

    for (const auto& pad : pads)
    {
        const int displayCol = pad.gridCol % cols;
        const int displayRow = pad.gridRow + (pad.gridCol / cols);
        const int cell = displayRow * cols + displayCol;
        if (cell >= 0 && cell < kMaxCells)
            occupied[static_cast<size_t> (cell)] = true;
    }

    for (int cell = 0; cell < kMaxCells; ++cell)
    {
        if (! occupied[static_cast<size_t> (cell)])
            return { cell / cols, cell % cols };
    }

    return { 0, 0 };
}

ProfilePad ControllerProfile::makeDefaultPad() const
{
    const auto [row, col] = suggestNextGridCell();
    const int defaultChannel = pads.empty() ? 10 : pads.back().midiChannel;

    int note = 36;
    for (int candidate = 0; candidate <= 127; ++candidate)
    {
        if (! hasDuplicateMidiKey (candidate, defaultChannel))
        {
            note = candidate;
            break;
        }
    }

    ProfilePad pad;
    pad.midiNote = note;
    pad.midiChannel = defaultChannel;
    pad.label = "Pad " + juce::String (static_cast<int> (pads.size()) + 1);
    pad.gridRow = row;
    pad.gridCol = col;
    pad.group = PadGroup::other;
    return pad;
}

PadMutationResult ControllerProfile::addPad (ProfilePad pad, std::optional<int> insertIndex)
{
    if (static_cast<int> (pads.size()) >= kMaxProfilePads)
        return PadMutationResult::maxPadsReached;

    if (pad.midiNote < 0 || pad.midiNote > 127 || pad.midiChannel < 1 || pad.midiChannel > 16)
        return PadMutationResult::invalidMidiKey;

    if (hasDuplicateMidiKey (pad.midiNote, pad.midiChannel))
        return PadMutationResult::duplicateMidiKey;

    layout = ProfileLayout::custom;

    if (insertIndex.has_value())
    {
        const auto idx = *insertIndex;
        if (idx < 0 || idx > static_cast<int> (pads.size()))
            return PadMutationResult::indexOutOfRange;
        pads.insert (pads.begin() + idx, std::move (pad));
    }
    else
    {
        pads.push_back (std::move (pad));
    }

    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::removePad (int index)
{
    if (index < 0 || index >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    if (static_cast<int> (pads.size()) <= kMinProfilePads)
        return PadMutationResult::wouldEmptyProfile;

    pads.erase (pads.begin() + index);
    layout = ProfileLayout::custom;
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::setPadAt (int index, const ProfilePad& pad)
{
    if (index < 0 || index >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    if (pad.midiNote < 0 || pad.midiNote > 127 || pad.midiChannel < 1 || pad.midiChannel > 16)
        return PadMutationResult::invalidMidiKey;

    if (hasDuplicateMidiKey (pad.midiNote, pad.midiChannel, index))
        return PadMutationResult::duplicateMidiKey;

    pads[static_cast<size_t> (index)] = pad;
    layout = ProfileLayout::custom;
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::swapPads (int indexA, int indexB)
{
    if (indexA < 0 || indexA >= static_cast<int> (pads.size()) ||
        indexB < 0 || indexB >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    if (indexA == indexB)
        return PadMutationResult::ok;

    const auto rowA = pads[static_cast<size_t> (indexA)].gridRow;
    const auto colA = pads[static_cast<size_t> (indexA)].gridCol;
    const auto rowB = pads[static_cast<size_t> (indexB)].gridRow;
    const auto colB = pads[static_cast<size_t> (indexB)].gridCol;

    std::swap (pads[static_cast<size_t> (indexA)], pads[static_cast<size_t> (indexB)]);
    pads[static_cast<size_t> (indexA)].gridRow = rowA;
    pads[static_cast<size_t> (indexA)].gridCol = colA;
    pads[static_cast<size_t> (indexB)].gridRow = rowB;
    pads[static_cast<size_t> (indexB)].gridCol = colB;
    layout = ProfileLayout::custom;
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::movePadToCell (int index, int targetRow, int targetCol)
{
    if (index < 0 || index >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    targetRow = juce::jmax (0, targetRow);
    targetCol = juce::jmax (0, targetCol);

    for (size_t i = 0; i < pads.size(); ++i)
    {
        if (static_cast<int> (i) != index && pads[i].gridRow == targetRow && pads[i].gridCol == targetCol)
        {
            pads[i].gridRow = pads[static_cast<size_t> (index)].gridRow;
            pads[i].gridCol = pads[static_cast<size_t> (index)].gridCol;
            break;
        }
    }

    pads[static_cast<size_t> (index)].gridRow = targetRow;
    pads[static_cast<size_t> (index)].gridCol = targetCol;
    layout = ProfileLayout::custom;
    return PadMutationResult::ok;
}

PadMutationResult ControllerProfile::duplicatePad (int sourceIndex, std::optional<std::pair<int, int>> targetCell)
{
    if (sourceIndex < 0 || sourceIndex >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    if (static_cast<int> (pads.size()) >= kMaxProfilePads)
        return PadMutationResult::maxPadsReached;

    ProfilePad clone = pads[static_cast<size_t> (sourceIndex)];
    clone.label = clone.label + " (Copy)";

    int candidateNote = (clone.midiNote + 1) % 128;
    int candidateChannel = clone.midiChannel;
    int tries = 0;
    while (hasDuplicateMidiKey (candidateNote, candidateChannel) && tries < 128 * 16)
    {
        candidateNote = (candidateNote + 1) % 128;
        if (candidateNote == 0)
            candidateChannel = (candidateChannel % 16) + 1;
        ++tries;
    }

    if (tries >= 128 * 16)
        return PadMutationResult::duplicateMidiKey;

    clone.midiNote = candidateNote;
    clone.midiChannel = candidateChannel;

    if (targetCell.has_value())
    {
        bool occupied = false;
        for (const auto& p : pads)
        {
            if (p.gridRow == targetCell->first && p.gridCol == targetCell->second)
            {
                occupied = true;
                break;
            }
        }

        if (! occupied)
        {
            clone.gridRow = targetCell->first;
            clone.gridCol = targetCell->second;
        }
        else
        {
            const auto nextCell = suggestNextGridCell();
            clone.gridRow = nextCell.first;
            clone.gridCol = nextCell.second;
        }
    }
    else
    {
        const auto nextCell = suggestNextGridCell();
        clone.gridRow = nextCell.first;
        clone.gridCol = nextCell.second;
    }

    return addPad (std::move (clone));
}

PadMutationResult ControllerProfile::renamePad (int index, const juce::String& newLabel)
{
    if (index < 0 || index >= static_cast<int> (pads.size()))
        return PadMutationResult::indexOutOfRange;

    pads[static_cast<size_t> (index)].label = newLabel.trim();
    layout = ProfileLayout::custom;
    return PadMutationResult::ok;
}

ControllerProfile ControllerProfile::createBlank()
{
    ControllerProfile profile ("Blank Custom", ProfileLayout::custom);
    ProfilePad pad;
    pad.midiNote = 36;
    pad.midiChannel = 10;
    pad.label = "Pad 1";
    pad.gridRow = 0;
    pad.gridCol = 0;
    pad.group = PadGroup::other;
    profile.pads.push_back (pad);
    return profile;
}

} // namespace svc
