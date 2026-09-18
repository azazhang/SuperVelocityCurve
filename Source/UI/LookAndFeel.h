#pragma once

#include "Theme.h"
#include <JuceHeader.h>

namespace svc::ui
{

class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel() { refreshTheme(); }

    void refreshTheme()
    {
        setColourScheme (svc::ui::Theme::getMode() == svc::ui::ThemeMode::light
                             ? getLightColourScheme()
                             : getDarkColourScheme());
        applyThemeColours();
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 0.5f);
        const bool toggled = button.getToggleState();

        juce::Colour base = juce::Colour (Theme::panelRaised());
        if (toggled)
            base = juce::Colour (Theme::accentDim());
        else if (shouldDrawButtonAsDown)
            base = base.darker (0.2f);
        else if (shouldDrawButtonAsHighlighted)
            base = juce::Colour (Theme::padHover());

        juce::ColourGradient grad (base.brighter (0.04f), 0.0f, bounds.getY(),
                                   base.darker (0.04f), 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 6.0f);

        if (Theme::getMode() == ThemeMode::dark)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        }

        if (toggled)
        {
            g.setColour (juce::Colour (Theme::accentGold()).withAlpha (0.6f));
            g.drawRoundedRectangle (bounds.expanded (0.5f), 5.5f, 1.2f);
        }

        g.setColour (juce::Colour (Theme::border()).withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
        const bool active = button.isFrontTab();

        g.setColour (active ? juce::Colour (Theme::panelRaised())
                            : juce::Colour (Theme::panel()).withAlpha (0.6f));
        g.fillRoundedRectangle (area, 5.0f);

        if (active)
        {
            g.setColour (juce::Colour (Theme::accent()).withAlpha (0.85f));
            g.fillRoundedRectangle (area.getX() + 4.0f, area.getBottom() - 2.5f, area.getWidth() - 8.0f, 2.0f, 1.0f);
        }

        g.setColour (juce::Colour (Theme::textPrimary()).withAlpha (active ? 1.0f : 0.75f));
        g.setFont (Theme::smallFont().boldened());
        g.drawFittedText (button.getButtonText(), area.toNearestInt(), juce::Justification::centred, 1);
        juce::ignoreUnused (isMouseOver, isMouseDown);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        g.setColour (button.isEnabled() ? juce::Colour (Theme::textPrimary())
                                        : juce::Colour (Theme::textSecondary()));
        g.setFont (Theme::smallFont());
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0),
                          juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
        Theme::fillPanel (g, bounds, 5.0f);

        if (box.hasKeyboardFocus (false))
        {
            g.setColour (juce::Colour (Theme::accent()).withAlpha (0.45f));
            g.drawRoundedRectangle (bounds.expanded (0.5f), 5.5f, 1.2f);
        }

        const auto arrowZone = bounds.removeFromRight (20.0f).reduced (5.0f, 7.0f);
        juce::Path chevron;
        const float midX = arrowZone.getCentreX();
        const float midY = arrowZone.getCentreY();
        chevron.startNewSubPath (midX - 3.5f, midY - 1.5f);
        chevron.lineTo (midX, midY + 2.0f);
        chevron.lineTo (midX + 3.5f, midY - 1.5f);

        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Label* createComboBoxTextBox (juce::ComboBox& box) override
    {
        auto* label = LookAndFeel_V4::createComboBoxTextBox (box);
        label->setColour (juce::Label::textColourId, box.findColour (juce::ComboBox::textColourId));
        label->setFont (Theme::bodyFont());
        return label;
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (1.0f, 1.0f, static_cast<float> (width) - 2.0f, static_cast<float> (height) - 2.0f, 5.0f);
        g.setColour (editor.findColour (juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (0.5f, 0.5f, static_cast<float> (width) - 1.0f, static_cast<float> (height) - 1.0f, 5.0f, 1.0f);
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.isBeingEdited())
        {
            if (auto* editor = label.getCurrentTextEditor())
            {
                editor->setBounds (label.getLocalBounds());
                editor->setVisible (true);
            }
            return;
        }

        const auto bg = label.findColour (juce::Label::backgroundColourId);
        if (bg.isOpaque())
            g.fillAll (bg);

        const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (label.getFont());
        g.drawFittedText (label.getText(), label.getLocalBounds().reduced (1),
                          label.getJustificationType(), juce::jmax (1, static_cast<int> (static_cast<float> (label.getHeight()) / label.getFont().getHeight())),
                          label.getMinimumHorizontalScale());
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto tickSize = juce::jmin (18.0f, bounds.getHeight() - 2.0f);
        auto tick = bounds.removeFromLeft (tickSize).withSizeKeepingCentre (tickSize, tickSize);

        g.setColour (juce::Colour (Theme::panelRaised()));
        g.fillRoundedRectangle (tick, 3.5f);
        g.setColour (juce::Colour (Theme::border()));
        g.drawRoundedRectangle (tick, 3.5f, 1.0f);

        if (button.getToggleState())
        {
            g.setColour (juce::Colour (Theme::accentGold()));
            g.fillRoundedRectangle (tick.reduced (3.5f), 2.0f);
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (Theme::bodyFont());
        g.drawFittedText (button.getButtonText(), bounds.reduced (4.0f, 0.0f).toNearestInt(),
                          juce::Justification::centredLeft, 2);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos);

        if (slider.isBar())
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                              minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const bool isHorizontal = slider.isHorizontal();
        const auto trackBounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                         static_cast<float> (width), static_cast<float> (height));
        const float trackThickness = juce::jlimit (4.0f, 6.0f, isHorizontal ? trackBounds.getHeight() * 0.22f
                                                                            : trackBounds.getWidth() * 0.22f);

        juce::Rectangle<float> bgTrack;
        if (isHorizontal)
        {
            const float trackY = trackBounds.getCentreY() - trackThickness * 0.5f;
            bgTrack = { trackBounds.getX(), trackY, trackBounds.getWidth(), trackThickness };
        }
        else
        {
            const float trackX = trackBounds.getCentreX() - trackThickness * 0.5f;
            bgTrack = { trackX, trackBounds.getY(), trackThickness, trackBounds.getHeight() };
        }

        g.setColour (slider.findColour (juce::Slider::backgroundColourId).darker (0.2f));
        g.fillRoundedRectangle (bgTrack, trackThickness * 0.5f);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId).brighter (0.12f));
        g.drawRoundedRectangle (bgTrack, trackThickness * 0.5f, 0.8f);

        juce::Point<float> thumbPoint;
        if (isHorizontal)
        {
            thumbPoint = { sliderPos, trackBounds.getCentreY() };
            const float activeW = juce::jmax (0.0f, sliderPos - trackBounds.getX());
            auto activeRect = juce::Rectangle<float> (trackBounds.getX(), bgTrack.getY(), activeW, trackThickness);
            
            juce::ColourGradient activeGrad (juce::Colour (Theme::accentDim()), activeRect.getX(), activeRect.getY(),
                                             juce::Colour (Theme::accent()), activeRect.getRight(), activeRect.getY(), false);
            g.setGradientFill (activeGrad);
            g.fillRoundedRectangle (activeRect, trackThickness * 0.5f);
        }
        else
        {
            thumbPoint = { trackBounds.getCentreX(), sliderPos };
            const float activeH = juce::jmax (0.0f, trackBounds.getBottom() - sliderPos);
            auto activeRect = juce::Rectangle<float> (bgTrack.getX(), sliderPos, trackThickness, activeH);

            juce::ColourGradient activeGrad (juce::Colour (Theme::accent()), activeRect.getX(), activeRect.getY(),
                                             juce::Colour (Theme::accentDim()), activeRect.getX(), activeRect.getBottom(), false);
            g.setGradientFill (activeGrad);
            g.fillRoundedRectangle (activeRect, trackThickness * 0.5f);
        }

        const float thumbRadius = juce::jlimit (6.0f, 9.0f, isHorizontal ? trackBounds.getHeight() * 0.38f
                                                                         : trackBounds.getWidth() * 0.38f);
        auto thumbRect = juce::Rectangle<float> (thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre (thumbPoint);

        g.setColour (juce::Colour (Theme::accent()).withAlpha (0.25f));
        g.fillEllipse (thumbRect.expanded (2.5f));

        g.setColour (juce::Colour (Theme::panelRaised()));
        g.fillEllipse (thumbRect);

        g.setColour (juce::Colour (Theme::accent()));
        g.fillEllipse (thumbRect.reduced (thumbRadius * 0.45f));

        g.setColour (Theme::getMode() == ThemeMode::dark
                         ? juce::Colours::white.withAlpha (0.75f)
                         : juce::Colour (Theme::borderBright()));
        g.drawEllipse (thumbRect, 1.2f);
    }

private:
    void applyThemeColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (Theme::background()));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (Theme::panelRaised()));
        setColour (juce::ComboBox::textColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (Theme::border()));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (Theme::accent()));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (Theme::panelRaised()));
        setColour (juce::PopupMenu::textColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (Theme::accentDim()));
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (Theme::panelRaised()));
        setColour (juce::TextEditor::textColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::TextEditor::outlineColourId, juce::Colour (Theme::border()));
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (Theme::accent()).withAlpha (0.55f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (Theme::panelRaised()));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (Theme::accentDim()));
        setColour (juce::TextButton::textColourOffId, juce::Colour (Theme::textPrimary()));
        setColour (juce::TextButton::textColourOnId, juce::Colour (Theme::textPrimary()));
        setColour (juce::ToggleButton::textColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (Theme::accentGold()));
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (Theme::border()));
        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, juce::Colour (Theme::accent()));
        setColour (juce::Slider::trackColourId, juce::Colour (Theme::accentDim()));
        setColour (juce::Slider::backgroundColourId, juce::Colour (Theme::padIdle()));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (Theme::panelRaised()));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (Theme::border()));
        setColour (juce::Slider::textBoxHighlightColourId, juce::Colour (Theme::accent()).withAlpha (0.25f));
        setColour (juce::Label::textColourId, juce::Colour (Theme::textPrimary()));
        setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (Theme::border()).withAlpha (0.55f));
        setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    }

    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar& scrollbar,
                        int x,
                        int y,
                        int width,
                        int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition,
                        int thumbSize,
                        bool isMouseOver,
                        bool isMouseDown) override
    {
        juce::ignoreUnused (scrollbar, isMouseOver, isMouseDown);

        if (thumbSize <= 0)
            return;

        const int trackLength = isScrollbarVertical ? height : width;
        if (thumbSize >= trackLength - 2)
            return;

        g.setColour (juce::Colour (Theme::border()).withAlpha (0.45f));
        const int thickness = juce::jmin (isScrollbarVertical ? width : height, 6);

        if (isScrollbarVertical)
            g.fillRoundedRectangle (static_cast<float> (x + (width - thickness) / 2),
                                    static_cast<float> (thumbStartPosition),
                                    static_cast<float> (thickness),
                                    static_cast<float> (thumbSize),
                                    3.0f);
        else
            g.fillRoundedRectangle (static_cast<float> (thumbStartPosition),
                                    static_cast<float> (y + (height - thickness) / 2),
                                    static_cast<float> (thumbSize),
                                    static_cast<float> (thickness),
                                    3.0f);
    }
};

} // namespace svc::ui
