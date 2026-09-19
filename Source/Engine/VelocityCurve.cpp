#include "VelocityCurve.h"
#include <algorithm>
#include <cmath>

namespace svc
{

VelocityCurve::VelocityCurve()
{
    setIdentity();
}

void VelocityCurve::setIdentity()
{
    controlPoints = makePresetPoints (CurvePreset::linear);
    rebuildLut();
}

void VelocityCurve::applyPreset (CurvePreset preset)
{
    controlPoints = makePresetPoints (preset);
    rebuildLut();
}

std::vector<CurveControlPoint> VelocityCurve::makePresetPoints (CurvePreset preset)
{
    switch (preset)
    {
        case CurvePreset::soft:
            return { { 0.0f, 0.0f }, { 0.25f, 0.45f }, { 0.5f, 0.68f }, { 0.75f, 0.85f }, { 1.0f, 1.0f } };
        case CurvePreset::hard:
            return { { 0.0f, 0.0f }, { 0.35f, 0.12f }, { 0.65f, 0.55f }, { 1.0f, 1.0f } };
        case CurvePreset::sCurve:
            return { { 0.0f, 0.0f }, { 0.2f, 0.08f }, { 0.5f, 0.5f }, { 0.8f, 0.92f }, { 1.0f, 1.0f } };
        case CurvePreset::exponential:
            return { { 0.0f, 0.0f }, { 0.15f, 0.03f }, { 0.4f, 0.18f }, { 0.7f, 0.52f }, { 1.0f, 1.0f } };
        case CurvePreset::logarithmic:
            return { { 0.0f, 0.0f }, { 0.3f, 0.48f }, { 0.6f, 0.78f }, { 0.85f, 0.93f }, { 1.0f, 1.0f } };
        case CurvePreset::power:
            return { { 0.0f, 0.0f }, { 0.2f, 0.04f }, { 0.45f, 0.2f }, { 0.7f, 0.55f }, { 1.0f, 1.0f } };
        case CurvePreset::linear:
        default:
            return { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
    }
}

void VelocityCurve::enforceMonotonicOutputs (std::vector<CurveControlPoint>& points)
{
    if (points.empty())
        return;

    points.front().output = std::clamp (points.front().output, 0.0f, 1.0f);
    for (size_t i = 1; i < points.size(); ++i)
        points[i].output = std::clamp (points[i].output, points[i - 1].output, 1.0f);
}

void VelocityCurve::setControlPoints (std::vector<CurveControlPoint> points)
{
    if (points.size() < 2)
    {
        setIdentity();
        return;
    }

    sortControlPoints (points);
    enforceMonotonicOutputs (points);
    controlPoints = std::move (points);
    rebuildLut();
}

void VelocityCurve::sortControlPoints (std::vector<CurveControlPoint>& points)
{
    std::sort (points.begin(), points.end(), [] (const CurveControlPoint& a, const CurveControlPoint& b)
    {
        return a.input < b.input;
    });

    constexpr float minSpan = 0.02f;
    points.front().input = std::clamp (points.front().input, 0.0f, 1.0f - minSpan);
    points.back().input = std::clamp (points.back().input, points.front().input + minSpan, 1.0f);
    points.front().output = std::clamp (points.front().output, 0.0f, 1.0f);
    points.back().output = std::clamp (points.back().output, points.front().output, 1.0f);
}

void VelocityCurve::setFloor (float normalizedFloor) noexcept
{
    floor = std::clamp (normalizedFloor, 0.0f, 1.0f);
    if (floor > ceiling)
        ceiling = floor;
    rebuildLut();
}

void VelocityCurve::setCeiling (float normalizedCeiling) noexcept
{
    ceiling = std::clamp (normalizedCeiling, 0.0f, 1.0f);
    if (ceiling < floor)
        floor = ceiling;
    rebuildLut();
}

float VelocityCurve::interpolateControlPoints (const CurveControlPoint* points, size_t numPoints, float input)
{
    const auto clampedInput = std::clamp (input, 0.0f, 1.0f);

    if (points == nullptr || numPoints == 0)
        return clampedInput;

    if (clampedInput <= points[0].input)
        return points[0].output;

    if (clampedInput >= points[numPoints - 1].input)
        return points[numPoints - 1].output;

    for (size_t i = 1; i < numPoints; ++i)
    {
        const auto& a = points[i - 1];
        const auto& b = points[i];

        if (clampedInput <= b.input)
        {
            const auto span = b.input - a.input;
            if (span <= 0.0f)
                return b.output;

            const auto t = (clampedInput - a.input) / span;
            return a.output + t * (b.output - a.output);
        }
    }

    return points[numPoints - 1].output;
}

float VelocityCurve::interpolateControlPoints (const std::vector<CurveControlPoint>& points, float input)
{
    return interpolateControlPoints (points.data(), points.size(), input);
}

float VelocityCurve::evaluateMappedOutput (float input) const noexcept
{
    if (controlPoints.size() < 2)
        return std::clamp (input, 0.0f, 1.0f);

    const auto clamped = std::clamp (input, 0.0f, 1.0f);
    const auto inputGate = controlPoints.front().input;
    const auto inputCeil = controlPoints.back().input;

    if (clamped < inputGate)
        return 0.0f;

    const auto shaped = interpolateControlPoints (controlPoints, std::min (clamped, inputCeil));
    return std::clamp (floor + shaped * (ceiling - floor), 0.0f, 1.0f);
}

void VelocityCurve::rebuildLut()
{
    float previous = 0.0f;
    for (int i = 0; i < midi1LutSize; ++i)
    {
        const auto input = static_cast<float> (i) / static_cast<float> (midi1LutSize - 1);
        const auto output = std::max (previous, evaluateMappedOutput (input));
        midi1Lut[static_cast<size_t> (i)] = output;
        previous = output;
    }

    previous = 0.0f;
    midi2Lut.resize (static_cast<size_t> (midi2LutSize));
    for (int i = 0; i < midi2LutSize; ++i)
    {
        const auto input = static_cast<float> (i) / static_cast<float> (midi2LutSize - 1);
        const auto output = std::max (previous, evaluateMappedOutput (input));
        midi2Lut[static_cast<size_t> (i)] = output;
        previous = output;
    }
}

float VelocityCurve::mapNormalized (float input) const noexcept
{
    return evaluateMappedOutput (input);
}

int VelocityCurve::mapMidi1 (int input) const noexcept
{
    return normalizedToMidi1 (mapNormalized (midi1ToNormalized (input)));
}

int VelocityCurve::mapMidi2 (int input) const noexcept
{
    const auto normalized = static_cast<float> (std::clamp (input, 0, midi2Max))
                          / static_cast<float> (midi2Max);
    return normalizedToMidi2 (evaluateMappedOutput (normalized));
}

} // namespace svc
