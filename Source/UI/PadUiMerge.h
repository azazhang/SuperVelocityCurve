#pragma once

#include "../Profiles/ControllerProfile.h"

namespace svc::ui
{

/** Curve editor owns control points; inspector owns metadata and floor/ceiling. */
inline svc::ProfilePad mergePadFromCurveAndInspector (const svc::ProfilePad& curvePad,
                                                      const svc::ProfilePad& inspectorPad) noexcept
{
    auto pad = curvePad;
    pad.label = inspectorPad.label;
    pad.midiNote = inspectorPad.midiNote;
    pad.midiChannel = inspectorPad.midiChannel;
    pad.enabled = inspectorPad.enabled;
    pad.group = inspectorPad.group;
    pad.velocityGate = inspectorPad.velocityGate;
    pad.gateMode = inspectorPad.gateMode;
    pad.retriggerGuardMs = inspectorPad.retriggerGuardMs;
    pad.aftertouch.enabled = inspectorPad.aftertouch.enabled;
    pad.curve.setFloor (inspectorPad.curve.getFloor());
    pad.curve.setCeiling (inspectorPad.curve.getCeiling());
    pad.aftertouch.curve.setFloor (inspectorPad.aftertouch.curve.getFloor());
    pad.aftertouch.curve.setCeiling (inspectorPad.aftertouch.curve.getCeiling());
    return pad;
}

} // namespace svc::ui
