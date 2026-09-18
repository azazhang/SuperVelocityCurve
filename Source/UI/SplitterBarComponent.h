#pragma once

#include "Theme.h"
#include <JuceHeader.h>
#include <functional>

class SplitterBarComponent : public juce::Component
{
public:
    SplitterBarComponent()
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    std::function<void (int deltaX)> onResizeDelta;
    std::function<void()> onResetToDefault;

    void mouseEnter (const juce::MouseEvent&) override
    {
        isHovered = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        isHovered = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragStartPos = event.getScreenX();
        isDragging = true;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const int currentScreenX = event.getScreenX();
        const int deltaX = currentScreenX - dragStartPos;
        if (deltaX != 0)
        {
            dragStartPos = currentScreenX;
            if (onResizeDelta)
                onResizeDelta (deltaX);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDragging = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (onResetToDefault)
            onResetToDefault();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float midX = bounds.getCentreX();

        // Hover or active dragging background glow
        if (isDragging || isHovered)
        {
            g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (isDragging ? 0.22f : 0.12f));
            g.fillRoundedRectangle (bounds.reduced (1.0f, 4.0f), 3.0f);
        }

        // Subtle vertical central groove
        g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.75f));
        g.drawVerticalLine (static_cast<int> (midX), bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);

        // Center grip pips
        const float midY = bounds.getCentreY();
        const float dotRadius = 1.6f;
        const float dotSpacing = 8.0f;

        g.setColour (isDragging || isHovered ? juce::Colour (svc::ui::Theme::accent())
                                            : juce::Colour (svc::ui::Theme::textMuted()).withAlpha (0.6f));

        for (int i = -1; i <= 1; ++i)
        {
            const float y = midY + static_cast<float> (i) * dotSpacing;
            g.fillEllipse (midX - dotRadius, y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        }
    }

private:
    int dragStartPos = 0;
    bool isHovered = false;
    bool isDragging = false;
};
