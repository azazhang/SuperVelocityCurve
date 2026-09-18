#pragma once

#include <JuceHeader.h>

namespace svc::ui::layout
{

constexpr int kMinEditorWidth = 1100;
constexpr int kMinEditorHeight = 760;
constexpr int kMinCurvePlotHeight = 120;
constexpr int kMinCurvePlotWidth = 240;

constexpr int kHeaderHeight = 56;
constexpr int kOuterPadding = 12;
constexpr int kStatusBarTrim = 22;
constexpr int kStandalonePanelHeight = 88;
constexpr int kStandaloneGap = 4;
constexpr int kToolbarHeight = 118;
constexpr int kLiveHitsRowHeight = 20;
constexpr int kBottomSectionsTrailingPad = 8;
constexpr int kSplitterWidth = 6;
constexpr int kMinPadGridWidth = 180;

struct SectionStackHeights
{
    int histogram = 26;
    int midiTools = 26;
    int calibration = 26;
};

struct EditorLayoutInputs
{
    juce::Rectangle<int> editorBounds;
    bool hasStandaloneMidiPanel = false;
    bool padSettingsExpanded = true;
    SectionStackHeights bottomSections;
    std::optional<int> customPadGridWidth;
};

struct EditorLayoutResult
{
    juce::Rectangle<int> curveEditorBounds;
    juce::Rectangle<int> padGridBounds;
    juce::Rectangle<int> splitterBounds;
    juce::Rectangle<int> padSettingsBounds;
    int bottomSectionsTotal = 0;
};

int bottomSectionsTotalHeight (const SectionStackHeights& sections) noexcept;

EditorLayoutResult computeEditorLayout (const EditorLayoutInputs& inputs) noexcept;

} // namespace svc::ui::layout
