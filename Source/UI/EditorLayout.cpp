#include "EditorLayout.h"

namespace svc::ui::layout
{

int bottomSectionsTotalHeight (const SectionStackHeights& sections) noexcept
{
    return sections.histogram + sections.midiTools + sections.calibration + kBottomSectionsTrailingPad;
}

EditorLayoutResult computeEditorLayout (const EditorLayoutInputs& inputs) noexcept
{
    EditorLayoutResult result;
    auto area = inputs.editorBounds;
    area.removeFromTop (kHeaderHeight);

    auto bounds = area.reduced (kOuterPadding).withTrimmedBottom (kStatusBarTrim);

    if (inputs.hasStandaloneMidiPanel)
    {
        bounds.removeFromTop (kStandalonePanelHeight);
        bounds.removeFromTop (kStandaloneGap);
    }

    bounds.removeFromTop (kToolbarHeight);
    bounds.removeFromTop (kLiveHitsRowHeight);

    const int maxBottomHeight = std::max (0, bounds.getHeight() - kMinCurvePlotHeight - 8);
    result.bottomSectionsTotal = std::min (bottomSectionsTotalHeight (inputs.bottomSections), maxBottomHeight);
    bounds.removeFromBottom (result.bottomSectionsTotal);

    const int settingsWidth = inputs.padSettingsExpanded
                                  ? juce::jlimit (220, 280, bounds.getWidth() / 3)
                                  : 30;
    result.padSettingsBounds = bounds.removeFromRight (settingsWidth).reduced (4);

    // Remaining width is partitioned between [PadGrid | Splitter | CurveEditor]
    const int availableContentWidth = bounds.getWidth();
    const int maxPadGridWidth = std::max (kMinPadGridWidth,
                                          availableContentWidth - kMinCurvePlotWidth - kSplitterWidth - 16);

    int padColWidth = 0;
    if (inputs.customPadGridWidth.has_value())
    {
        padColWidth = juce::jlimit (kMinPadGridWidth, maxPadGridWidth, *inputs.customPadGridWidth);
    }
    else
    {
        const int defaultWidth = static_cast<int> (bounds.getWidth() * 0.26f);
        padColWidth = juce::jlimit (kMinPadGridWidth, maxPadGridWidth, juce::jlimit (220, 300, defaultWidth));
    }

    result.padGridBounds = bounds.removeFromLeft (padColWidth).reduced (4);
    result.splitterBounds = bounds.removeFromLeft (kSplitterWidth);
    result.curveEditorBounds = bounds.reduced (4);
    return result;
}

} // namespace svc::ui::layout
