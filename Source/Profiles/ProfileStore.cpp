#include "ProfileStore.h"
#include "../Engine/VelocityEngine.h"

namespace svc
{

ProfileStore::ProfileStore()
{
    rebuildFactoryTemplates();
    activeProfile = factoryTemplates.front().copy();
}

void ProfileStore::rebuildFactoryTemplates()
{
    factoryTemplates = {
        ControllerProfile::createGMStandard(),
        ControllerProfile::createBlank(),
        ControllerProfile::createLaunchpadDrumRack(),
        ControllerProfile::createMaschineGroup(),
        ControllerProfile::createSpdSx(),
        ControllerProfile::createFgdp()
    };
}

void ProfileStore::notifyChanged()
{
    if (onProfileChanged)
        onProfileChanged();
}

std::vector<ProfileListEntry> ProfileStore::getProfileList() const
{
    std::vector<ProfileListEntry> entries;

    for (int i = 0; i < static_cast<int> (factoryTemplates.size()); ++i)
    {
        entries.push_back ({
            ProfileEntryType::factoryTemplate,
            i,
            "[Template] " + factoryTemplates[static_cast<size_t> (i)].getName()
        });
    }

    for (int i = 0; i < static_cast<int> (userProfiles.size()); ++i)
    {
        entries.push_back ({
            ProfileEntryType::userProfile,
            i,
            "[My] " + userProfiles[static_cast<size_t> (i)].getName()
        });
    }

    return entries;
}

void ProfileStore::setActiveFromEntry (ProfileEntryType type, int index)
{
    activeEntryType = type;
    activeEntryIndex = index;

    if (type == ProfileEntryType::factoryTemplate
        && index >= 0
        && index < static_cast<int> (factoryTemplates.size()))
    {
        activeProfile = factoryTemplates[static_cast<size_t> (index)].copy();
    }
    else if (type == ProfileEntryType::userProfile
             && index >= 0
             && index < static_cast<int> (userProfiles.size()))
    {
        activeProfile = userProfiles[static_cast<size_t> (index)].copy();
    }

    notifyChanged();
}

void ProfileStore::loadFactoryTemplate (int index)
{
    setActiveFromEntry (ProfileEntryType::factoryTemplate, index);
}

void ProfileStore::loadUserProfile (int index)
{
    setActiveFromEntry (ProfileEntryType::userProfile, index);
}

bool ProfileStore::validateProfileMidiKeys (const ControllerProfile& profile, juce::String* errorMessage)
{
    for (int i = 0; i < static_cast<int> (profile.getPads().size()); ++i)
    {
        const auto& pad = profile.getPads()[static_cast<size_t> (i)];
        if (profile.hasDuplicateMidiKey (pad.midiNote, pad.midiChannel, i))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = "Pads cannot share the same MIDI note and channel (conflict on note "
                                + juce::String (pad.midiNote) + ", channel " + juce::String (pad.midiChannel) + ").";
            }
            return false;
        }
    }

    return true;
}

bool ProfileStore::saveActiveAsUserProfile (const juce::String& name, juce::String* errorMessage)
{
    if (name.trim().isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = "Enter a profile name to save.";
        return false;
    }

    if (! validateProfileMidiKeys (activeProfile, errorMessage))
        return false;

    auto profile = activeProfile.copy();
    profile.setName (name.trim());
    profile.setLayout (ProfileLayout::custom);
    userProfiles.push_back (profile);
    setActiveFromEntry (ProfileEntryType::userProfile, static_cast<int> (userProfiles.size()) - 1);
    return true;
}

bool ProfileStore::updateActiveUserProfile (const juce::String& name, juce::String* errorMessage)
{
    if (name.trim().isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = "Enter a profile name to save.";
        return false;
    }

    if (! validateProfileMidiKeys (activeProfile, errorMessage))
        return false;

    activeProfile.setName (name.trim());
    activeProfile.setLayout (ProfileLayout::custom);

    if (activeEntryType == ProfileEntryType::userProfile
        && activeEntryIndex >= 0
        && activeEntryIndex < static_cast<int> (userProfiles.size()))
    {
        userProfiles[static_cast<size_t> (activeEntryIndex)] = activeProfile.copy();
        notifyChanged();
        return true;
    }

    return saveActiveAsUserProfile (name, errorMessage);
}

PadMutationResult ProfileStore::addPadToActive (const ProfilePad& pad)
{
    auto newPad = pad.label.isEmpty() ? activeProfile.makeDefaultPad() : pad;
    const auto result = activeProfile.addPad (std::move (newPad));
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();

    return result;
}

PadMutationResult ProfileStore::removePadFromActive (int index)
{
    const auto result = activeProfile.removePad (index);
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();
    return result;
}

PadMutationResult ProfileStore::swapPadsInActive (int indexA, int indexB)
{
    const auto result = activeProfile.swapPads (indexA, indexB);
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();
    return result;
}

PadMutationResult ProfileStore::movePadInActive (int index, int targetRow, int targetCol)
{
    const auto result = activeProfile.movePadToCell (index, targetRow, targetCol);
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();
    return result;
}

PadMutationResult ProfileStore::duplicatePadInActive (int sourceIndex, std::optional<std::pair<int, int>> targetCell)
{
    const auto result = activeProfile.duplicatePad (sourceIndex, targetCell);
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();
    return result;
}

PadMutationResult ProfileStore::renamePadInActive (int index, const juce::String& newLabel)
{
    const auto result = activeProfile.renamePad (index, newLabel);
    if (result == PadMutationResult::ok)
        syncActiveUserProfileFromEdits();
    return result;
}

bool ProfileStore::deleteUserProfile (int index)
{
    if (index < 0 || index >= static_cast<int> (userProfiles.size()))
        return false;

    userProfiles.erase (userProfiles.begin() + index);

    if (activeEntryType == ProfileEntryType::userProfile)
    {
        if (userProfiles.empty())
            loadFactoryTemplate (0);
        else
            loadUserProfile (juce::jmin (index, static_cast<int> (userProfiles.size()) - 1));
    }

    notifyChanged();
    return true;
}

bool ProfileStore::duplicateActiveAsUserProfile (const juce::String& name)
{
    juce::String error;
    if (! validateProfileMidiKeys (activeProfile, &error))
        return false;

    return saveActiveAsUserProfile (name.isEmpty() ? activeProfile.getName() + " Copy" : name, &error);
}

void ProfileStore::applyActiveToEngine (VelocityEngine& engine) const
{
    activeProfile.applyToEngine (engine);
}

juce::ValueTree ProfileStore::toValueTree() const
{
    juce::ValueTree tree ("SuperVelocityCurveProfileStore");
    tree.setProperty ("version", 2, nullptr);
    tree.setProperty ("activeEntryType", static_cast<int> (activeEntryType), nullptr);
    tree.setProperty ("activeEntryIndex", activeEntryIndex, nullptr);
    tree.appendChild (activeProfile.toValueTree(), nullptr);

    juce::ValueTree users ("UserProfiles");
    for (const auto& profile : userProfiles)
        users.appendChild (profile.toValueTree(), nullptr);

    tree.appendChild (users, nullptr);
    return tree;
}

void ProfileStore::syncActiveUserProfileFromEdits()
{
    if (activeEntryType == ProfileEntryType::userProfile
        && activeEntryIndex >= 0
        && activeEntryIndex < static_cast<int> (userProfiles.size()))
    {
        userProfiles[static_cast<size_t> (activeEntryIndex)] = activeProfile.copy();
    }
}

void ProfileStore::fromValueTree (const juce::ValueTree& tree, bool notifyUpdates)
{
    if (tree.hasType ("SuperVelocityCurveProfile"))
    {
        activeProfile = ControllerProfile::fromValueTree (tree);
        activeEntryType = ProfileEntryType::factoryTemplate;
        activeEntryIndex = static_cast<int> (tree.getProperty ("activeProfileIndex", 0));
        if (notifyUpdates)
            notifyChanged();
        return;
    }

    if (! tree.hasType ("SuperVelocityCurveProfileStore"))
        return;

    userProfiles.clear();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);
        if (child.hasType ("UserProfiles"))
        {
            for (int u = 0; u < child.getNumChildren(); ++u)
            {
                const auto profileTree = child.getChild (u);
                if (profileTree.hasType ("ControllerProfile"))
                {
                    auto profile = ControllerProfile::fromValueTree (profileTree);
                    if (validateProfileMidiKeys (profile))
                        userProfiles.push_back (std::move (profile));
                }
            }
        }
        else if (child.hasType ("ControllerProfile"))
        {
            activeProfile = ControllerProfile::fromValueTree (child);
        }
    }

    activeEntryType = static_cast<ProfileEntryType> (static_cast<int> (tree.getProperty ("activeEntryType", 0)));
    activeEntryIndex = tree.getProperty ("activeEntryIndex", 0);

    if (! validateProfileMidiKeys (activeProfile))
    {
        activeProfile = factoryTemplates.front().copy();
        activeEntryType = ProfileEntryType::factoryTemplate;
        activeEntryIndex = 0;
    }

    if (notifyUpdates)
        notifyChanged();
}

bool ProfileStore::exportActiveProfileToFile (const juce::File& file, juce::String* errorMessage) const
{
    if (! validateProfileMidiKeys (activeProfile, errorMessage))
        return false;

    const auto xml = activeProfile.toValueTree().createXml();
    if (xml == nullptr)
        return false;

    return xml->writeTo (file);
}

bool ProfileStore::importProfileFromFile (const juce::File& file, juce::String* errorMessage)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "Import failed: file is not valid XML.";
        return false;
    }

    auto profile = ControllerProfile::fromValueTree (juce::ValueTree::fromXml (*xml));

    for (int i = 0; i < static_cast<int> (profile.getPads().size()); ++i)
    {
        const auto& pad = profile.getPads()[static_cast<size_t> (i)];
        if (profile.hasDuplicateMidiKey (pad.midiNote, pad.midiChannel, i))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = "Import rejected: pads \"" + pad.label + "\" and another pad share MIDI note "
                                + juce::String (pad.midiNote) + " on channel " + juce::String (pad.midiChannel) + ".";
            }
            return false;
        }
    }

    profile.setLayout (ProfileLayout::custom);
    userProfiles.push_back (profile);
    loadUserProfile (static_cast<int> (userProfiles.size()) - 1);
    return true;
}

} // namespace svc
