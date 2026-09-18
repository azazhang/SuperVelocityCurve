#include "../Source/Engine/HitEventFifo.h"
#include "../Source/Engine/MidiUtilities.h"
#include "../Source/Engine/EngineSettings.h"
#include "../Source/Engine/VelocityCurve.h"
#include "../Source/Engine/MidiVelocityTransport.h"
#include "../Source/Engine/VelocityEngine.h"
#include "../Source/Profiles/ControllerProfile.h"
#include "../Source/Profiles/PadTypes.h"
#include "../Source/Profiles/ProfileStore.h"
#include <cmath>
#include <iostream>
#include <thread>

#define EXPECT_TRUE(expr) do { if (! (expr)) { std::cerr << "FAIL: " #expr << " at " << __LINE__ << '\n'; return 1; } } while (0)
#define EXPECT_NEAR(a, b, eps) do { if (std::abs ((a) - (b)) > (eps)) { std::cerr << "FAIL: " #a " vs " #b << " at " << __LINE__ << '\n'; return 1; } } while (0)

static int testVelocityCurveMonotonic()
{
    svc::VelocityCurve curve;
    curve.applyPreset (svc::CurvePreset::soft);
    const auto& lut = curve.getLut();

    for (int i = 1; i < svc::VelocityCurve::lutSize; ++i)
        EXPECT_TRUE (lut[static_cast<size_t> (i)] >= lut[static_cast<size_t> (i - 1)]);

    std::vector<svc::CurveControlPoint> points = {
        { 0.0f, 0.0f }, { 0.3f, 0.8f }, { 0.6f, 0.5f }, { 1.0f, 1.0f }
    };
    curve.setControlPoints (points);
    const auto& lut2 = curve.getLut();
    for (int i = 1; i < svc::VelocityCurve::lutSize; ++i)
        EXPECT_TRUE (lut2[static_cast<size_t> (i)] >= lut2[static_cast<size_t> (i - 1)]);

    return 0;
}

static int testVelocityEnginePerPadRetrigger()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);

    svc::PadSettings pad;
    pad.retriggerGuardMs = 50.0;
    pad.enabled = true;
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.8f), 0);
    engine.processMidiBuffer (buffer, 128);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.9f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (buffer.getNumEvents() == 0);
    return 0;
}

static int testProfileApplyClearsOldPads()
{
    svc::VelocityEngine engine;
    svc::ControllerProfile gm = svc::ControllerProfile::createGMStandard();
    gm.applyToEngine (engine);

    svc::PadSettings gatedPad;
    gatedPad.velocityGate = 0.5f;
    engine.setPadSettings (36, 10, gatedPad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.2f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 0);

    svc::ControllerProfile small = svc::ControllerProfile::createSpdSx();
    small.applyToEngine (engine);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.2f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);
    return 0;
}

static int testNoteRemapAndChannelFilter()
{
    svc::MidiRoutingSettings routing;
    routing.inputChannelFilter = 10;
    routing.outputChannel = 10;
    routing.setRemap (60, 0, 36, 10);

    svc::VelocityEngine engine;
    engine.setMidiRouting (routing);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (9, 60, 0.7f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 0);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOn (10, 60, 0.7f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);

    juce::MidiMessage msg;
    for (const auto metadata : buffer)
        msg = metadata.getMessage();

    EXPECT_TRUE (msg.getNoteNumber() == 36);
    EXPECT_TRUE (msg.getChannel() == 10);

    // Non-channel messages like MIDI clock must not be blocked by inputChannelFilter
    buffer.clear();
    buffer.addEvent (juce::MidiMessage::midiClock(), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);
    return 0;
}

static int testMidi1OutputMode()
{
    svc::VelocityEngine engine;
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.333f), 0);
    engine.processMidiBuffer (buffer, 64);

    juce::MidiMessage msg;
    for (const auto metadata : buffer)
        msg = metadata.getMessage();

    EXPECT_TRUE (msg.getVelocity() == 42);
    return 0;
}

static int testGateClampMode()
{
    svc::VelocityEngine engine;
    svc::PadSettings pad;
    pad.enabled = true;
    pad.velocityGate = 0.2f;
    pad.gateMode = svc::VelocityGateMode::clampToFloor;
  pad.curve.setFloor (0.15f);
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.05f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);
    return 0;
}

static int testPerPadHistogram()
{
    svc::VelocityEngine engine;
    svc::PadSettings pad;
    pad.enabled = true;
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 64);

    const auto snap = engine.getPadHistogramSnapshot (36, 10);
    int total = 0;
    for (int i = 0; i < 128; ++i)
        total += snap.inputBins[static_cast<size_t> (i)];
    EXPECT_TRUE (total == 1);
    return 0;
}

static int testLaunchpadProfileSize()
{
    const auto profile = svc::ControllerProfile::createLaunchpadDrumRack();
    EXPECT_TRUE (profile.getPads().size() == 64);
    return 0;
}

static int testMidi2LutMonotonic()
{
    svc::VelocityCurve curve;
    curve.applyPreset (svc::CurvePreset::hard);
    const auto& lut = curve.getMidi2Lut();

    for (int i = 1; i < svc::VelocityCurve::midi2LutSize; ++i)
        EXPECT_TRUE (lut[static_cast<size_t> (i)] >= lut[static_cast<size_t> (i - 1)]);

    const auto mapped = curve.mapMidi2 (8192);
    EXPECT_TRUE (mapped >= 0 && mapped <= svc::midi2Max);
    return 0;
}

static int testInputGateThresholds()
{
    svc::VelocityCurve curve;
    curve.setControlPoints ({ { 0.25f, 0.0f }, { 0.75f, 1.0f } });

    EXPECT_NEAR (curve.mapNormalized (0.199f), 0.0f, 0.0001f);
    EXPECT_NEAR (curve.mapNormalized (0.1f), 0.0f, 0.001f);
    const auto atInputCeil = curve.mapNormalized (0.75f);
    EXPECT_NEAR (curve.mapNormalized (0.9f), atInputCeil, 0.0001f);
    EXPECT_TRUE (curve.mapNormalized (0.5f) > 0.2f && curve.mapNormalized (0.5f) < 0.8f);
    return 0;
}

static int testPolyAftertouchRemap()
{
    svc::MidiRoutingSettings routing;
    routing.setRemap (60, 10, 36, 10);

    svc::MidiRoutingProcessor processor;
    processor.setSettings (routing);

    svc::AftertouchPadSettings pad60;
    pad60.enabled = true;
    pad60.curve.setControlPoints ({ { 0.0f, 0.0f }, { 1.0f, 0.5f } });
    processor.setAftertouchSettings (60, 10, pad60);

    auto message = juce::MidiMessage::aftertouchChange (10, 60, 100);
    EXPECT_TRUE (processor.processMessage (message));
    EXPECT_TRUE (message.getNoteNumber() == 36);
    EXPECT_TRUE (message.getChannel() == 10);
    EXPECT_TRUE (message.getAfterTouchValue() == 50);
    return 0;
}

static int testZoneRoutingPolyAftertouch()
{
    svc::MidiRoutingSettings routing;
    routing.setRemap (60, 1, 36, 1);

    svc::EngineProcessingSettings processing;
    processing.zoneRouting.enabled = true;
    processing.zoneRouting.groupOutputChannel[static_cast<size_t> (svc::PadGroup::kick)] = 5;

    svc::PadSettings pad;
    pad.group = svc::PadGroup::kick;
    pad.aftertouch.enabled = true;
    pad.aftertouch.curve.applyPreset (svc::CurvePreset::linear);

    svc::VelocityEngine engine;
    engine.setProcessingSettings (processing);
    engine.setMidiRouting (routing);
    engine.setPadSettings (60, 1, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::aftertouchChange (1, 60, 80), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);

    for (const auto metadata : buffer)
    {
        const auto msg = metadata.getMessage();
        EXPECT_TRUE (msg.isAftertouch());
        EXPECT_TRUE (msg.getNoteNumber() == 36);
        EXPECT_TRUE (msg.getChannel() == 5);
    }
    return 0;
}

static int testMidiRoutingOutputChannelRemapsAllChannelMessages()
{
    svc::MidiRoutingSettings routing;
    routing.outputChannel = 3;

    svc::MidiRoutingProcessor processor;
    processor.setSettings (routing);

    auto pb = juce::MidiMessage::pitchWheel (1, 8192);
    EXPECT_TRUE (processor.processMessage (pb));
    EXPECT_TRUE (pb.getChannel() == 3);

    auto cc = juce::MidiMessage::controllerEvent (1, 64, 127);
    EXPECT_TRUE (processor.processMessage (cc));
    EXPECT_TRUE (cc.getChannel() == 3);

    auto pc = juce::MidiMessage::programChange (1, 10);
    EXPECT_TRUE (processor.processMessage (pc));
    EXPECT_TRUE (pc.getChannel() == 3);

    return 0;
}

static int testChannelPressureNoNoteZeroCollision()
{
    svc::MidiRoutingProcessor processor;
    svc::AftertouchPadSettings noteZeroPad;
    noteZeroPad.enabled = true;
    noteZeroPad.curve.setControlPoints ({ { 0.0f, 0.0f }, { 1.0f, 0.5f } });
    processor.setAftertouchSettings (0, 10, noteZeroPad);

    auto message = juce::MidiMessage::channelPressureChange (10, 80);
    EXPECT_TRUE (processor.processMessage (message));
    EXPECT_TRUE (message.getChannelPressureValue() == 80);
    return 0;
}

static int testHumanizeWithinBounds()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);

    svc::EngineProcessingSettings settings;
    settings.humanizeAmount = 0.2f;
    engine.setProcessingSettings (settings);

    svc::PadSettings pad;
    pad.enabled = true;
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 128);
    EXPECT_TRUE (buffer.getNumEvents() == 1);

    for (const auto metadata : buffer)
    {
        const auto velocity = metadata.getMessage().getFloatVelocity();
        EXPECT_TRUE (velocity >= 0.0f && velocity <= 1.0f);
    }
    return 0;
}

static int testPadAddRemoveAndDuplicateKey()
{
    svc::ControllerProfile profile = svc::ControllerProfile::createBlank();
    EXPECT_TRUE (profile.getPads().size() == 1);

    const auto pad2 = profile.makeDefaultPad();
    EXPECT_TRUE (profile.addPad (pad2) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads().size() == 2);

    auto duplicate = profile.getPads().front();
    duplicate.midiNote = profile.getPads().back().midiNote;
    duplicate.midiChannel = profile.getPads().back().midiChannel;
    EXPECT_TRUE (profile.setPadAt (0, duplicate) == svc::PadMutationResult::duplicateMidiKey);

    EXPECT_TRUE (profile.removePad (1) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads().size() == 1);
    EXPECT_TRUE (profile.removePad (0) == svc::PadMutationResult::wouldEmptyProfile);

    svc::ControllerProfile gm = svc::ControllerProfile::createGMStandard();
    const auto occupied = gm.suggestNextGridCell();
    bool cellTaken = false;
    for (const auto& pad : gm.getPads())
    {
        const int cols = gm.getDisplayGridColumns();
        const int displayCol = pad.gridCol % cols;
        const int displayRow = pad.gridRow + (pad.gridCol / cols);
        if (displayRow == occupied.first && displayCol == occupied.second)
            cellTaken = true;
    }
    EXPECT_TRUE (! cellTaken);
    return 0;
}

static int testCurveFloorCeilingMapping()
{
    svc::VelocityCurve curve;
    curve.setIdentity();
    curve.setFloor (0.2f);
    curve.setCeiling (0.8f);

    EXPECT_NEAR (curve.mapNormalized (0.0f), 0.2f, 0.02f);
    EXPECT_NEAR (curve.mapNormalized (1.0f), 0.8f, 0.02f);
    return 0;
}

static int testProfileMidiKeyValidation()
{
    EXPECT_TRUE (svc::ProfileStore::validateProfileMidiKeys (svc::ControllerProfile::createBlank()));
    EXPECT_TRUE (svc::ProfileStore::validateProfileMidiKeys (svc::ControllerProfile::createGMStandard()));

    auto tree = svc::ControllerProfile::createBlank().toValueTree();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);
        if (child.hasType ("Pad"))
        {
            tree.appendChild (child.createCopy(), nullptr);
            break;
        }
    }

    const auto broken = svc::ControllerProfile::fromValueTree (tree);
    juce::String error;
    EXPECT_TRUE (! svc::ProfileStore::validateProfileMidiKeys (broken, &error));
    EXPECT_TRUE (error.isNotEmpty());
    return 0;
}

static int testRemapPerChannelEntries()
{
    svc::MidiRoutingSettings routing;
    routing.setRemap (36, 1, 40, 1);
    routing.setRemap (36, 10, 50, 10);
    EXPECT_TRUE (routing.getRemaps().size() == 2);

    int note = 36;
    int channel = 1;
    EXPECT_TRUE (routing.remapNote (note, channel));
    EXPECT_TRUE (note == 40 && channel == 1);

    note = 36;
    channel = 10;
    EXPECT_TRUE (routing.remapNote (note, channel));
    EXPECT_TRUE (note == 50 && channel == 10);
    return 0;
}

static int testRemapUsesPhysicalPadSettings()
{
    svc::MidiRoutingSettings routing;
    routing.setRemap (60, 10, 36, 10);

    svc::VelocityEngine engine;
    engine.setMidiRouting (routing);
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.setControlPoints ({ { 0.0f, 0.0f }, { 1.0f, 0.25f } });
    engine.setPadSettings (60, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 60, 1.0f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);

    juce::MidiMessage msg;
    for (const auto metadata : buffer)
        msg = metadata.getMessage();

    EXPECT_TRUE (msg.getNoteNumber() == 36);
    EXPECT_TRUE (msg.getChannel() == 10);
    EXPECT_TRUE (msg.getVelocity() >= 28 && msg.getVelocity() <= 36);
    return 0;
}

static int testMidi2FullScaleVelocity()
{
    EXPECT_TRUE (svc::normalizedToMidi2 (1.0f) == svc::midi2Max);
    EXPECT_TRUE (std::abs (svc::normalizedToMidi2 (0.5f) - svc::midi2Max / 2) <= 1);
    return 0;
}

static int testDeterministicVelocityReplay()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.setIdentity();
    engine.setPadSettings (36, 10, pad);

    auto runOnce = [&engine] (float inputVel)
    {
        juce::MidiBuffer buffer;
        buffer.addEvent (juce::MidiMessage::noteOn (10, 36, inputVel), 0);
        engine.processMidiBuffer (buffer, 128);
        if (buffer.getNumEvents() != 1)
            return -1;

        return static_cast<int> ((*buffer.begin()).getMessage().getVelocity());
    };

    const auto first = runOnce (0.5f);
    const auto second = runOnce (0.5f);
    EXPECT_TRUE (first == second);
    EXPECT_TRUE (first >= 0);
    return 0;
}

static int testMidi2OutputEncoding()
{
    const auto encoding = svc::encodeOutputVelocity (svc::VelocityOutputMode::midi2, 0.5f, false);
    EXPECT_TRUE (encoding.emitMidi2Ump);
    EXPECT_TRUE (encoding.midi2 == svc::normalizedToMidi2 (0.5f));
    EXPECT_TRUE (encoding.midi1 == svc::downgradeToMidi1 (encoding.midi2));

    std::vector<std::uint32_t> words;
    svc::appendMidi2NoteOnUmp (words, 10, 36, encoding.midi2);
    EXPECT_TRUE (svc::readMidi2VelocityFromUmpWords (words) == encoding.midi2);
    return 0;
}

static int testMidi2EngineUmpPackets()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);
    engine.setOutputMode (svc::VelocityOutputMode::midi2);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.setIdentity();
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (engine.getMidi2OutputWords().size() >= 2);
    const auto wireMidi2 = svc::readMidi2VelocityFromUmpWords (engine.getMidi2OutputWords());
    EXPECT_TRUE (wireMidi2 > 0 && wireMidi2 <= svc::midi2Max);

    svc::HitEvent hit;
    EXPECT_TRUE (engine.getHitFifo().pop (hit));
    EXPECT_TRUE (hit.outputMidi2 == wireMidi2);
    EXPECT_TRUE (hit.isMidi2);
    return 0;
}

static int testZoneRoutingNoteOffChannel()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);

    svc::EngineProcessingSettings processing;
    processing.zoneRouting.enabled = true;
    processing.zoneRouting.groupOutputChannel[static_cast<size_t> (svc::PadGroup::kick)] = 2;
    engine.setProcessingSettings (processing);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.group = svc::PadGroup::kick;
    pad.curve.setIdentity();
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.8f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (buffer.getNumEvents() == 1);
    for (const auto metadata : buffer)
        EXPECT_TRUE (metadata.getMessage().getChannel() == 2);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOff (10, 36, 0.0f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (buffer.getNumEvents() == 1);
    for (const auto metadata : buffer)
    {
        const auto& message = metadata.getMessage();
        EXPECT_TRUE (message.isNoteOff());
        EXPECT_TRUE (message.getChannel() == 2);
        EXPECT_TRUE (message.getNoteNumber() == 36);
    }

    return 0;
}

static int testGatedNoteOffSuppressed()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.velocityGate = 0.5f;
    pad.gateMode = svc::VelocityGateMode::drop;
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.2f), 0);
    buffer.addEvent (juce::MidiMessage::noteOff (10, 36, 0.0f), 64);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (buffer.getNumEvents() == 0);
    return 0;
}

static int testRetriggerDroppedNoteOffStillPasses()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.retriggerGuardMs = 50.0;
    pad.curve.setIdentity();
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.8f), 0);
    engine.processMidiBuffer (buffer, 128);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.9f), 0);
    buffer.addEvent (juce::MidiMessage::noteOff (10, 36, 0.0f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (buffer.getNumEvents() == 1);
    for (const auto metadata : buffer)
        EXPECT_TRUE (metadata.getMessage().isNoteOff());

    return 0;
}

static int testMidi1WireQuantize()
{
    svc::VelocityEngine engine;
    engine.setSampleRate (48000.0);
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.setIdentity();
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 128);

    EXPECT_TRUE (engine.getMidi2OutputWords().size() == 0);

    for (const auto metadata : buffer)
        EXPECT_TRUE (metadata.getMessage().getVelocity() == 64);

    return 0;
}

static int testHitEventFifoDropsOldestWhenFull()
{
    svc::HitEventFifo fifo;
    constexpr int total = svc::HitEventFifo::capacity + 5;

    for (int i = 0; i < total; ++i)
    {
        svc::HitEvent event;
        event.note = i;
        EXPECT_TRUE (fifo.push (event));
    }

    svc::HitEvent last;
    while (fifo.pop (last)) {}

    EXPECT_TRUE (last.note == total - 1);
    return 0;
}

static int testNoteOnVelocityZeroClampedToMin()
{
    svc::VelocityEngine engine;
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.setFloor (0.0f);
    pad.curve.setCeiling (0.0f);
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.1f), 0);
    engine.processMidiBuffer (buffer, 64);

    EXPECT_TRUE (buffer.getNumEvents() == 1);
    for (const auto metadata : buffer)
    {
        const auto msg = metadata.getMessage();
        EXPECT_TRUE (msg.isNoteOn());
        // In MIDI 1.0, Note-On with velocity 0 is treated as Note-Off.
        // It must be clamped to at least 1.
        EXPECT_TRUE (msg.getVelocity() >= 1);
    }
    return 0;
}

static int testLivePadCurveSyncImmediate()
{
    svc::VelocityEngine engine;
    engine.setOutputMode (svc::VelocityOutputMode::midi1);

    svc::PadSettings pad;
    pad.enabled = true;
    pad.curve.applyPreset (svc::CurvePreset::hard);
    engine.setPadSettings (36, 10, pad);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);
    int hardVel = 0;
    for (const auto metadata : buffer)
        hardVel = metadata.getMessage().getVelocity();

    // Now switch curve to soft immediately without delay
    pad.curve.applyPreset (svc::CurvePreset::soft);
    engine.setPadSettings (36, 10, pad);

    buffer.clear();
    buffer.addEvent (juce::MidiMessage::noteOn (10, 36, 0.5f), 0);
    engine.processMidiBuffer (buffer, 64);
    EXPECT_TRUE (buffer.getNumEvents() == 1);
    int softVel = 0;
    for (const auto metadata : buffer)
        softVel = metadata.getMessage().getVelocity();

    // The new curve takes effect on the very next note without lag or buffering:
    EXPECT_TRUE (softVel > hardVel);
    return 0;
}

static int testHitEventFifoMultithreadedStress()
{
    svc::HitEventFifo fifo;
    std::atomic<bool> start { false };
    std::atomic<bool> producerDone { false };
    std::atomic<bool> failed { false };
    constexpr int numEvents = 50000;

    std::thread producer ([&]
    {
        while (! start.load (std::memory_order_acquire)) {}
        for (int i = 0; i < numEvents; ++i)
        {
            svc::HitEvent ev;
            ev.note = i % 128;
            ev.channel = (i % 16) + 1;
            ev.inputVelocity = static_cast<float> (i % 127) / 127.0f;
            ev.outputVelocity = ev.inputVelocity;
            ev.timestamp = static_cast<uint64_t> (i);
            fifo.push (ev);
        }
        producerDone.store (true, std::memory_order_release);
    });

    std::thread consumer ([&]
    {
        while (! start.load (std::memory_order_acquire)) {}
        svc::HitEvent ev;
        while (! producerDone.load (std::memory_order_acquire) || fifo.hasPending())
        {
            if (fifo.pop (ev))
            {
                if (ev.note < 0 || ev.note >= 128 || ev.channel < 1 || ev.channel > 16)
                    failed.store (true, std::memory_order_relaxed);
            }
        }
    });

    start.store (true, std::memory_order_release);
    producer.join();
    consumer.join();
    EXPECT_TRUE (! failed.load (std::memory_order_relaxed));
    return 0;
}

static int testProfilePadMutations()
{
    svc::ControllerProfile profile ("Test", svc::ProfileLayout::custom);
    svc::ProfilePad pad1;
    pad1.midiNote = 36;
    pad1.label = "Kick";
    pad1.gridRow = 0; pad1.gridCol = 0;
    EXPECT_TRUE (profile.addPad (pad1) == svc::PadMutationResult::ok);

    svc::ProfilePad pad2;
    pad2.midiNote = 38;
    pad2.label = "Snare";
    pad2.gridRow = 0; pad2.gridCol = 1;
    EXPECT_TRUE (profile.addPad (pad2) == svc::PadMutationResult::ok);

    // Swap pads
    EXPECT_TRUE (profile.swapPads (0, 1) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads()[0].label == "Snare");
    EXPECT_TRUE (profile.getPads()[1].label == "Kick");
    EXPECT_TRUE (profile.getPads()[0].gridRow == 0 && profile.getPads()[0].gridCol == 0);
    EXPECT_TRUE (profile.getPads()[1].gridRow == 0 && profile.getPads()[1].gridCol == 1);

    // Rename pad
    EXPECT_TRUE (profile.renamePad (0, "Snare 2") == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads()[0].label == "Snare 2");

    // Move pad to cell
    EXPECT_TRUE (profile.movePadToCell (0, 1, 2) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads()[0].gridRow == 1 && profile.getPads()[0].gridCol == 2);

    // Duplicate pad
    EXPECT_TRUE (profile.duplicatePad (0) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads().size() == 3);
    EXPECT_TRUE (profile.getPads()[2].midiNote != pad1.midiNote && profile.getPads()[2].midiNote != pad2.midiNote);

    // Duplicate pad with target cell
    EXPECT_TRUE (profile.duplicatePad (0, std::make_pair (3, 2)) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads().size() == 4);
    EXPECT_TRUE (profile.getPads()[3].gridRow == 3 && profile.getPads()[3].gridCol == 2);

    // Duplicate pad with already occupied target cell (should safely fallback without overlapping)
    EXPECT_TRUE (profile.duplicatePad (0, std::make_pair (3, 2)) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads().size() == 5);
    EXPECT_TRUE (profile.getPads()[4].gridRow != 3 || profile.getPads()[4].gridCol != 2);

    // Move pad to already occupied cell should swap coordinates, preventing stacking
    const auto oldRow0 = profile.getPads()[0].gridRow;
    const auto oldCol0 = profile.getPads()[0].gridCol;
    const auto destRow = profile.getPads()[3].gridRow;
    const auto destCol = profile.getPads()[3].gridCol;
    EXPECT_TRUE (profile.movePadToCell (0, destRow, destCol) == svc::PadMutationResult::ok);
    EXPECT_TRUE (profile.getPads()[0].gridRow == destRow && profile.getPads()[0].gridCol == destCol);
    EXPECT_TRUE (profile.getPads()[3].gridRow == oldRow0 && profile.getPads()[3].gridCol == oldCol0);

    // Test suggestNextGridCell with columns override
    const auto nextCell4 = profile.suggestNextGridCell (4);
    const auto nextCell8 = profile.suggestNextGridCell (8);
    EXPECT_TRUE (nextCell4.second < 4);
    EXPECT_TRUE (nextCell8.second < 8);

    // Test in ProfileStore
    svc::ProfileStore store;
    store.addPadToActive (pad1);
    const int initialCount = static_cast<int> (store.getActiveProfile().getPads().size());
    EXPECT_TRUE (store.duplicatePadInActive (0) == svc::PadMutationResult::ok);
    EXPECT_TRUE (static_cast<int> (store.getActiveProfile().getPads().size()) == initialCount + 1);
    EXPECT_TRUE (store.renamePadInActive (0, "Renamed Pad") == svc::PadMutationResult::ok);
    EXPECT_TRUE (store.getActiveProfile().getPads()[0].label == "Renamed Pad");
    EXPECT_TRUE (store.swapPadsInActive (0, 1) == svc::PadMutationResult::ok);
    EXPECT_TRUE (store.getActiveProfile().getPads()[1].label == "Renamed Pad");

    return 0;
}

int main()
{
    if (testProfilePadMutations() != 0) return 1;
    if (testVelocityCurveMonotonic() != 0) return 1;
    if (testVelocityEnginePerPadRetrigger() != 0) return 1;
    if (testProfileApplyClearsOldPads() != 0) return 1;
    if (testNoteRemapAndChannelFilter() != 0) return 1;
    if (testRemapPerChannelEntries() != 0) return 1;
    if (testRemapUsesPhysicalPadSettings() != 0) return 1;
    if (testMidi2FullScaleVelocity() != 0) return 1;
    if (testMidi1OutputMode() != 0) return 1;
    if (testGateClampMode() != 0) return 1;
    if (testPerPadHistogram() != 0) return 1;
    if (testLaunchpadProfileSize() != 0) return 1;
    if (testMidi2LutMonotonic() != 0) return 1;
    if (testInputGateThresholds() != 0) return 1;
    if (testPolyAftertouchRemap() != 0) return 1;
    if (testZoneRoutingPolyAftertouch() != 0) return 1;
    if (testMidiRoutingOutputChannelRemapsAllChannelMessages() != 0) return 1;
    if (testChannelPressureNoNoteZeroCollision() != 0) return 1;
    if (testHumanizeWithinBounds() != 0) return 1;
    if (testPadAddRemoveAndDuplicateKey() != 0) return 1;
    if (testCurveFloorCeilingMapping() != 0) return 1;
    if (testProfileMidiKeyValidation() != 0) return 1;
    if (testDeterministicVelocityReplay() != 0) return 1;
    if (testMidi2OutputEncoding() != 0) return 1;
    if (testMidi2EngineUmpPackets() != 0) return 1;
    if (testMidi1WireQuantize() != 0) return 1;
    if (testZoneRoutingNoteOffChannel() != 0) return 1;
    if (testGatedNoteOffSuppressed() != 0) return 1;
    if (testRetriggerDroppedNoteOffStillPasses() != 0) return 1;
    if (testHitEventFifoDropsOldestWhenFull() != 0) return 1;
    if (testNoteOnVelocityZeroClampedToMin() != 0) return 1;
    if (testLivePadCurveSyncImmediate() != 0) return 1;
    if (testHitEventFifoMultithreadedStress() != 0) return 1;
    std::cout << "All engine tests passed.\n";
    return 0;
}
