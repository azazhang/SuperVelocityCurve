#pragma once

#include "Theme.h"
#include <JuceHeader.h>

namespace svc::ui
{

inline void styleScrollbar (juce::ScrollBar& bar)
{
    bar.setAutoHide (true);
    bar.setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    bar.setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    bar.setColour (juce::ScrollBar::thumbColourId, juce::Colour (Theme::border()).withAlpha (0.55f));
}

/** Inspector / routing panels — vertical scroll only. */
inline void configureVerticalViewport (juce::Viewport& viewport)
{
    viewport.setScrollBarsShown (false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    styleScrollbar (viewport.getVerticalScrollBar());
}

inline void updateVerticalScrollbarVisibility (juce::Viewport& viewport, const juce::Component& content)
{
    const bool needsScroll = content.getHeight() > viewport.getHeight() + 1;
    viewport.setScrollBarsShown (needsScroll, false);
}

/** Pad grid — both axes (Launchpad 8-wide layouts need horizontal scroll). */
inline void configurePadGridViewport (juce::Viewport& viewport)
{
    viewport.setScrollBarsShown (false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    styleScrollbar (viewport.getVerticalScrollBar());
    styleScrollbar (viewport.getHorizontalScrollBar());
}

inline void updatePadGridScrollbars (juce::Viewport& viewport, const juce::Component& content)
{
    const bool needsVertical = content.getHeight() > viewport.getHeight() + 1;
    const bool needsHorizontal = content.getWidth() > viewport.getWidth() + 1;
    viewport.setScrollBarsShown (needsVertical, needsHorizontal);
}

} // namespace svc::ui
