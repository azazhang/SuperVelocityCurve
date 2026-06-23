#include "CurveEditorComponent.h"

CurveEditorComponent::CurveEditorComponent()
{
    setOpaque (true);
}

svc::VelocityCurve& CurveEditorComponent::activeCurve() noexcept
{
    return editTarget == EditTarget::aftertouch ? currentPad.aftertouch.curve : currentPad.curve;
}

const svc::VelocityCurve& CurveEditorComponent::activeCurve() const noexcept
{
    return editTarget == EditTarget::aftertouch ? currentPad.aftertouch.curve : currentPad.curve;
}

void CurveEditorComponent::setPad (const svc::ProfilePad& pad, bool clearHitMarkers)
{
    currentPad = pad;
    if (clearHitMarkers)
        hitMarkers.clear();
    repaint();
}

void CurveEditorComponent::setEditTarget (EditTarget target)
{
    editTarget = target;
    repaint();
}

void CurveEditorComponent::addHitMarker (int note, int channel, float inputNormalized, float outputNormalized, bool)
{
    if (note != currentPad.midiNote || channel != currentPad.midiChannel)
        return;

    const auto curveOutput = activeCurve().mapNormalized (inputNormalized);
    hitMarkers.push_back ({ inputNormalized, curveOutput, outputNormalized,
                            juce::Time::getMillisecondCounterHiRes() });
    if (hitMarkers.size() > 48)
        hitMarkers.erase (hitMarkers.begin());
    repaint();
}

bool CurveEditorComponent::needsHitVisualRepaint() const noexcept
{
    if (hitMarkers.empty())
        return false;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    for (const auto& hit : hitMarkers)
        if (now - hit.createdMs < 1200.0)
            return true;

    return false;
}

bool CurveEditorComponent::decayHitMarkers() noexcept
{
    if (hitMarkers.empty())
        return false;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    bool removed = false;

    for (auto it = hitMarkers.begin(); it != hitMarkers.end();)
    {
        if (now - it->createdMs > 1200.0)
        {
            it = hitMarkers.erase (it);
            removed = true;
        }
        else
        {
            ++it;
        }
    }

    if (removed)
        repaint();

    return removed;
}

namespace
{
constexpr int kCurveHeaderHeight = 36;
constexpr int kCurveFooterHeight = 22;
}

juce::Rectangle<float> CurveEditorComponent::plotArea() const
{
    return getLocalBounds().reduced (44, 10)
        .withTrimmedTop (kCurveHeaderHeight)
        .withTrimmedBottom (kCurveFooterHeight)
        .toFloat();
}

juce::Point<float> CurveEditorComponent::normalizedToPoint (float input, float output) const
{
    const auto plot = plotArea();
    return { plot.getX() + input * plot.getWidth(),
             plot.getBottom() - output * plot.getHeight() };
}

float CurveEditorComponent::controlOutputToPlot (float controlOutput) const noexcept
{
    const auto& curve = activeCurve();
    return curve.getFloor() + controlOutput * (curve.getCeiling() - curve.getFloor());
}

float CurveEditorComponent::plotOutputToControl (float plotOutput) const noexcept
{
    const auto& curve = activeCurve();
    const auto span = curve.getCeiling() - curve.getFloor();
    if (span <= 0.0001f)
        return plotOutput;

    return (plotOutput - curve.getFloor()) / span;
}

juce::Point<float> CurveEditorComponent::eventToNormalized (juce::Point<float> pos) const
{
    const auto plot = plotArea();
    const auto input = juce::jlimit (0.0f, 1.0f, (pos.x - plot.getX()) / plot.getWidth());
    const auto output = juce::jlimit (0.0f, 1.0f, 1.0f - (pos.y - plot.getY()) / plot.getHeight());
    return { input, output };
}

int CurveEditorComponent::findNearestControlPoint (juce::Point<float> pos) const
{
    const auto& points = activeCurve().getControlPoints();
    int nearest = -1;
    float bestDistance = 16.0f;

    for (int i = 0; i < static_cast<int> (points.size()); ++i)
    {
        const auto screen = normalizedToPoint (points[static_cast<size_t> (i)].input,
                                               controlOutputToPlot (points[static_cast<size_t> (i)].output));
        const auto distance = screen.getDistanceFrom (pos);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = i;
        }
    }

    return nearest;
}

void CurveEditorComponent::notifyChanged (bool commitToProfile)
{
    if (commitToProfile && onPadChanged)
        onPadChanged (currentPad);

    repaint();
}

void CurveEditorComponent::applyPreset (svc::CurvePreset preset)
{
    activeCurve().applyPreset (preset);
    notifyChanged();
    repaint();
}

void CurveEditorComponent::resetCurve()
{
    activeCurve().setIdentity();
    notifyChanged();
    repaint();
}

void CurveEditorComponent::copyFrom (const svc::VelocityCurve& other)
{
    auto& curve = activeCurve();
    curve.setControlPoints (other.getControlPoints());
    curve.setFloor (other.getFloor());
    curve.setCeiling (other.getCeiling());
    notifyChanged();
    repaint();
}

void CurveEditorComponent::setFloorCeiling (float floor, float ceiling)
{
    auto& curve = activeCurve();
    curve.setFloor (floor);
    curve.setCeiling (ceiling);
    notifyChanged();
    repaint();
}

void CurveEditorComponent::drawGrid (juce::Graphics& g) const
{
    const auto plot = plotArea();
    g.setColour (juce::Colour (svc::ui::Theme::curveGrid()).withAlpha (svc::ui::Theme::curveGridAlpha()));

    for (int i = 0; i <= 4; ++i)
    {
        const auto t = static_cast<float> (i) / 4.0f;
        const auto x = plot.getX() + t * plot.getWidth();
        const auto y = plot.getBottom() - t * plot.getHeight();
        g.drawVerticalLine (static_cast<int> (x), plot.getY(), plot.getBottom());
        g.drawHorizontalLine (static_cast<int> (y), plot.getX(), plot.getRight());
    }

    g.setColour (juce::Colour (svc::ui::Theme::textSecondary()));
    g.setFont (svc::ui::Theme::smallFont());
    g.drawText ("0", static_cast<int> (plot.getX()) - 20, static_cast<int> (plot.getBottom()) - 6, 16, 12, juce::Justification::centred);
    g.drawText ("127", static_cast<int> (plot.getRight()) - 10, static_cast<int> (plot.getY()) - 14, 28, 12, juce::Justification::centred);
    g.drawText ("Input velocity", static_cast<int> (plot.getCentreX()) - 40, static_cast<int> (plot.getBottom()) + 8, 80, 14, juce::Justification::centred);
    g.drawText ("Out", static_cast<int> (plot.getX()) - 38, static_cast<int> (plot.getCentreY()) - 7, 28, 14, juce::Justification::centred);

    g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.15f));
    g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.2f);
}

void CurveEditorComponent::drawGateZones (juce::Graphics& g) const
{
    const auto plot = plotArea();
    const auto& points = activeCurve().getControlPoints();
    if (points.size() < 2)
        return;

    const auto inputGate = points.front().input;
    const auto inputCeil = points.back().input;

    if (inputGate > 0.001f)
    {
        const auto gateRect = juce::Rectangle<float> (plot.getX(), plot.getY(),
                                                      inputGate * plot.getWidth(), plot.getHeight());
        g.setColour (juce::Colour (svc::ui::Theme::curveGateMuted()).withAlpha (0.55f));
        g.fillRect (gateRect);
        const auto gateX = plot.getX() + inputGate * plot.getWidth();
        g.setColour (juce::Colour (svc::ui::Theme::accentWarm()).withAlpha (0.35f));
        g.drawLine (gateX, plot.getY(), gateX, plot.getBottom(), 1.5f);
    }

    if (inputCeil < 0.999f)
    {
        const auto ceilX = plot.getX() + inputCeil * plot.getWidth();
        const auto satRect = juce::Rectangle<float> (ceilX, plot.getY(),
                                                    plot.getRight() - ceilX, plot.getHeight());
        g.setColour (juce::Colour (svc::ui::Theme::curveGateSaturated()).withAlpha (0.55f));
        g.fillRect (satRect);
        g.setColour (juce::Colour (svc::ui::Theme::accentSecondary()).withAlpha (0.4f));
        g.drawLine (ceilX, plot.getY(), ceilX, plot.getBottom(), 1.5f);
    }
}

void CurveEditorComponent::drawCurvePath (juce::Graphics& g, const svc::VelocityCurve& curve,
                                          juce::Colour colour, float strokeWidth) const
{
    const auto& lut = curve.getLut();
    juce::Path curvePath;
    constexpr int displaySteps = 64;
    const int step = juce::jmax (1, svc::VelocityCurve::lutSize / displaySteps);

    for (int i = 0; i < svc::VelocityCurve::lutSize; i += step)
    {
        const auto input = static_cast<float> (i) / static_cast<float> (svc::VelocityCurve::lutSize - 1);
        const auto point = normalizedToPoint (input, lut[static_cast<size_t> (i)]);

        if (i == 0)
            curvePath.startNewSubPath (point);
        else
            curvePath.lineTo (point);
    }

    const auto lastInput = 1.0f;
    curvePath.lineTo (normalizedToPoint (lastInput, lut.back()));

    g.setColour (colour);
    g.strokePath (curvePath, juce::PathStrokeType (strokeWidth));
}

void CurveEditorComponent::drawCurve (juce::Graphics& g) const
{
    const auto plot = plotArea();
    const auto& mainCurve = displayCurve != nullptr ? *displayCurve : activeCurve();
    const auto& lut = mainCurve.getLut();

    if (compareCurve != nullptr)
        drawCurvePath (g, *compareCurve, juce::Colour (svc::ui::Theme::accentGold()).withAlpha (0.55f), 2.0f);

    juce::Path curvePath;
    juce::Path fillPath;

    constexpr int displaySteps = 64;
    const int step = juce::jmax (1, svc::VelocityCurve::lutSize / displaySteps);

    for (int i = 0; i < svc::VelocityCurve::lutSize; i += step)
    {
        const auto input = static_cast<float> (i) / static_cast<float> (svc::VelocityCurve::lutSize - 1);
        const auto output = lut[static_cast<size_t> (i)];
        const auto point = normalizedToPoint (input, output);

        if (i == 0)
        {
            curvePath.startNewSubPath (point);
            fillPath.startNewSubPath (point.x, plot.getBottom());
            fillPath.lineTo (point);
        }
        else
        {
            curvePath.lineTo (point);
            fillPath.lineTo (point);
        }
    }

    {
        const auto point = normalizedToPoint (1.0f, lut.back());
        curvePath.lineTo (point);
        fillPath.lineTo (point);
    }

    fillPath.lineTo (normalizedToPoint (1.0f, lut.back()).x, plot.getBottom());
    fillPath.closeSubPath();

    g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.14f));
    g.fillPath (fillPath);

    g.setColour (juce::Colour (svc::ui::Theme::curveLine()));
    g.strokePath (curvePath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto& points = activeCurve().getControlPoints();
    for (int i = 0; i < static_cast<int> (points.size()); ++i)
    {
        const auto& point = points[static_cast<size_t> (i)];
        const auto p = normalizedToPoint (point.input, controlOutputToPlot (point.output));
        const auto isLeftGate = i == 0;
        const auto isRightGate = i == static_cast<int> (points.size()) - 1;
        const auto isEndpoint = isLeftGate || isRightGate;

        if (isEndpoint)
        {
            const auto handleW = 12.0f;
            const auto handleH = 14.0f;
            juce::Rectangle<float> handle (p.x - handleW * 0.5f, p.y - handleH * 0.5f, handleW, handleH);
            g.setColour (isLeftGate ? juce::Colour (svc::ui::Theme::accentGold())
                                    : juce::Colour (svc::ui::Theme::accentSecondary()));
            g.fillRoundedRectangle (handle, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (handle, 3.0f, 1.2f);
        }
        else
        {
            g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.25f));
            g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colour (svc::ui::Theme::border()));
            g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.0f);
        }
    }

    juce::ignoreUnused (plot);
}

void CurveEditorComponent::drawLiveHits (juce::Graphics& g) const
{
    if (hitMarkers.empty())
        return;

    const auto plot = plotArea();
    const auto now = juce::Time::getMillisecondCounterHiRes();

    for (size_t i = 0; i < hitMarkers.size(); ++i)
    {
        const auto& hit = hitMarkers[i];
        const auto age = now - hit.createdMs;
        const auto alpha = juce::jlimit (0.0f, 1.0f, 1.0f - static_cast<float> (age / 1200.0));
        if (alpha <= 0.01f)
            continue;

        const auto isLatest = i + 1 == hitMarkers.size();
        const auto p = normalizedToPoint (hit.input, hit.curveOutput);

        if (isLatest)
        {
            const auto linear = normalizedToPoint (hit.input, hit.input);
            const float dash[] = { 4.0f, 4.0f };

            g.setColour (juce::Colour (svc::ui::Theme::textSecondary()).withAlpha (0.45f * alpha));
            g.drawDashedLine ({ plot.getX(), p.y, p.x, p.y }, dash, 2, 1.0f);
            g.drawDashedLine ({ p.x, p.y, p.x, plot.getBottom() }, dash, 2, 1.0f);

            g.setColour (juce::Colour (svc::ui::Theme::textSecondary()).withAlpha (0.55f * alpha));
            g.drawEllipse (linear.x - 4.0f, linear.y - 4.0f, 8.0f, 8.0f, 1.2f);

            g.setColour (juce::Colour (svc::ui::Theme::accentGold()).withAlpha (0.75f * alpha));
            g.drawLine (linear.x, linear.y, p.x, p.y, 1.8f);

            g.setColour (juce::Colour (svc::ui::Theme::curveHit()).withAlpha (alpha * 0.35f));
            g.fillEllipse (p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f);
            g.setColour (juce::Colour (svc::ui::Theme::curveHit()).withAlpha (alpha));
            g.fillEllipse (p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
            g.drawEllipse (p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f, 1.4f);

            const auto inVel = juce::String (static_cast<int> (std::lround (hit.input * 127.0f)));
            const auto outVel = juce::String (static_cast<int> (std::lround (hit.engineOutput * 127.0f)));
            const auto label = inVel + juce::String::charToString (0x2192) + outVel;

            g.setFont (svc::ui::Theme::smallFont().boldened());
            g.setColour (juce::Colours::black.withAlpha (0.55f * alpha));
            const auto labelW = 52.0f;
            const auto labelH = 16.0f;
            auto labelX = p.x + 10.0f;
            if (labelX + labelW > plot.getRight())
                labelX = p.x - labelW - 10.0f;
            auto labelY = p.y - labelH - 8.0f;
            if (labelY < plot.getY())
                labelY = p.y + 10.0f;
            g.fillRoundedRectangle (labelX, labelY, labelW, labelH, 4.0f);
            g.setColour (juce::Colour (svc::ui::Theme::curveHit()).withAlpha (alpha));
            g.drawRoundedRectangle (labelX, labelY, labelW, labelH, 4.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawText (label, static_cast<int> (labelX), static_cast<int> (labelY),
                        static_cast<int> (labelW), static_cast<int> (labelH),
                        juce::Justification::centred);
        }
        else
        {
            g.setColour (juce::Colour (svc::ui::Theme::curveHit()).withAlpha (alpha * 0.25f));
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colour (svc::ui::Theme::curveHit()).withAlpha (alpha * 0.7f));
            g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
        }
    }

    juce::ignoreUnused (plot);
}

void CurveEditorComponent::paint (juce::Graphics& g)
{
    svc::ui::Theme::fillPanel (g, getLocalBounds().toFloat(), 12.0f);

    g.setColour (juce::Colour (svc::ui::Theme::textPrimary()));
    g.setFont (svc::ui::Theme::sectionFont());
    const juce::String mode = editTarget == EditTarget::aftertouch ? "Aftertouch" : "Velocity";
    const auto header = mode + " curve — " + currentPad.label;
    auto headerArea = getLocalBounds().removeFromTop (kCurveHeaderHeight).reduced (14, 6);
    g.drawFittedText (header, headerArea,
                      juce::Justification::centredLeft, 1);

    const auto plot = plotArea();
    svc::ui::Theme::fillPlot (g, plot, 8.0f);

    drawGrid (g);
    drawGateZones (g);
    drawCurve (g);
    drawLiveHits (g);

    g.setFont (svc::ui::Theme::smallFont());
    g.setColour (juce::Colour (svc::ui::Theme::textMuted()));
    g.drawText ("Gate handles: left/right = input range, up/down = output at gate | dbl-click add | right-click remove",
                getLocalBounds().removeFromBottom (kCurveFooterHeight).reduced (12, 2),
                juce::Justification::centred);
}

void CurveEditorComponent::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        const auto index = findNearestControlPoint (event.position);
        auto points = activeCurve().getControlPoints();
        if (index > 0 && index < static_cast<int> (points.size()) - 1)
        {
            points.erase (points.begin() + index);
            activeCurve().setControlPoints (points);
            notifyChanged();
            repaint();
        }
        return;
    }

    draggedPointIndex = findNearestControlPoint (event.position);
}

void CurveEditorComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedPointIndex < 0)
        return;

    auto points = activeCurve().getControlPoints();
    if (draggedPointIndex >= static_cast<int> (points.size()))
        return;

    const auto normalized = eventToNormalized (event.position);
    const auto plotOutput = plotOutputToControl (normalized.y);
    auto& point = points[static_cast<size_t> (draggedPointIndex)];
    const auto lastIndex = static_cast<int> (points.size()) - 1;

    if (draggedPointIndex == 0)
    {
        // Left gate: X = input threshold (below → 0); Y = output at gate entry.
        point.input = juce::jlimit (0.0f, points[1].input - 0.02f, normalized.x);
        point.output = juce::jlimit (0.0f, points[1].output - 0.001f, plotOutput);
    }
    else if (draggedPointIndex == lastIndex)
    {
        // Right gate: X = input ceiling (above → max); Y = output at ceiling.
        point.input = juce::jlimit (points[static_cast<size_t> (lastIndex - 1)].input + 0.02f,
                                    1.0f, normalized.x);
        point.output = juce::jlimit (points[static_cast<size_t> (lastIndex - 1)].output + 0.001f,
                                     1.0f, plotOutput);
    }
    else
    {
        point.input = juce::jlimit (points[static_cast<size_t> (draggedPointIndex - 1)].input + 0.02f,
                                    points[static_cast<size_t> (draggedPointIndex + 1)].input - 0.02f,
                                    normalized.x);

        const auto minOut = points[static_cast<size_t> (draggedPointIndex - 1)].output + 0.001f;
        const auto maxOut = points[static_cast<size_t> (draggedPointIndex + 1)].output - 0.001f;
        point.output = juce::jlimit (minOut, maxOut, plotOutput);
    }

    activeCurve().setControlPoints (points);
    notifyChanged (false);
}

void CurveEditorComponent::mouseUp (const juce::MouseEvent&)
{
    const bool wasDragging = draggedPointIndex >= 0;
    draggedPointIndex = -1;

    if (wasDragging)
    {
        notifyChanged (true);

        if (onPadEditFinished)
            onPadEditFinished();
    }
}

void CurveEditorComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (findNearestControlPoint (event.position) >= 0)
        return;

    auto points = activeCurve().getControlPoints();
    const auto normalized = eventToNormalized (event.position);
    const auto inputGate = points.front().input;
    const auto inputCeil = points.back().input;

    if (normalized.x <= inputGate + 0.01f || normalized.x >= inputCeil - 0.01f)
        return;

    points.push_back ({ normalized.x, plotOutputToControl (normalized.y) });
    activeCurve().setControlPoints (points);
    notifyChanged();
    repaint();
}
