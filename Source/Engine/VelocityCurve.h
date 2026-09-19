#pragma once

#include "MidiVelocity.h"
#include <array>
#include <vector>

namespace svc
{

enum class CurvePreset
{
    linear,
    soft,
    hard,
    sCurve,
    exponential,
    logarithmic,
    power
};

struct CurveControlPoint
{
    float input = 0.0f;
    float output = 0.0f;
};

class VelocityCurve
{
public:
    static constexpr int midi1LutSize = 128;
    static constexpr int midi2LutSize = 16384;
    static constexpr int lutSize = midi1LutSize;

    VelocityCurve();

    void setIdentity();
    void applyPreset (CurvePreset preset);
    void setControlPoints (std::vector<CurveControlPoint> points);
    const std::vector<CurveControlPoint>& getControlPoints() const noexcept { return controlPoints; }

    void setFloor (float normalizedFloor) noexcept;
    void setCeiling (float normalizedCeiling) noexcept;
    float getFloor() const noexcept { return floor; }
    float getCeiling() const noexcept { return ceiling; }

    void rebuildLut();
    float mapNormalized (float input) const noexcept;
    int mapMidi1 (int input) const noexcept;
    int mapMidi2 (int input) const noexcept;

    const std::array<float, midi1LutSize>& getLut() const noexcept { return midi1Lut; }
    const std::vector<float>& getMidi2Lut() const noexcept { return midi2Lut; }

    static std::vector<CurveControlPoint> makePresetPoints (CurvePreset preset);
    static void enforceMonotonicOutputs (std::vector<CurveControlPoint>& points);
    static float interpolateControlPoints (const CurveControlPoint* points, size_t numPoints, float input);

private:
    std::vector<CurveControlPoint> controlPoints;
    std::array<float, midi1LutSize> midi1Lut {};
    std::vector<float> midi2Lut;
    float floor = 0.0f;
    float ceiling = 1.0f;

    static void sortControlPoints (std::vector<CurveControlPoint>& points);
    static float interpolateControlPoints (const std::vector<CurveControlPoint>& points, float input);
    float evaluateMappedOutput (float input) const noexcept;
};

} // namespace svc
