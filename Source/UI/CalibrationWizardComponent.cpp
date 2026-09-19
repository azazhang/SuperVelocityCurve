#include "CalibrationWizardComponent.h"

CalibrationWizardComponent::CalibrationWizardComponent()
{
    addAndMakeVisible (instructionLabel);
    addAndMakeVisible (readoutLabel);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (applyButton);

    instructionLabel.setJustificationType (juce::Justification::topLeft);
    instructionLabel.setFont (svc::ui::Theme::bodyFont());
    readoutLabel.setJustificationType (juce::Justification::topLeft);
    readoutLabel.setFont (svc::ui::Theme::smallFont());
    readoutLabel.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textSecondary()));

    resetButton.onClick = [this] { reset(); };
    applyButton.onClick = [this]
    {
        if (onCurveCalibrated && step == Step::done && hasValidHits())
            onCurveCalibrated (buildCurve());
    };

    applyTheme();
    reset();
}

void CalibrationWizardComponent::applyTheme()
{
    instructionLabel.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textPrimary()));
    readoutLabel.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textMuted()));
}

bool CalibrationWizardComponent::hasValidHits() const noexcept
{
    return softHit >= 0.0f && mediumHit >= 0.0f && hardHit >= 0.0f
           && mediumHit > softHit + 0.02f && hardHit > mediumHit + 0.02f;
}

void CalibrationWizardComponent::updateReadout()
{
    juce::String text;
    if (softHit >= 0.0f)
        text << "Soft: " << static_cast<int> (std::lround (softHit * 127.0f)) << "   ";
    if (mediumHit >= 0.0f)
        text << "Medium: " << static_cast<int> (std::lround (mediumHit * 127.0f)) << "   ";
    if (hardHit >= 0.0f)
        text << "Hard: " << static_cast<int> (std::lround (hardHit * 127.0f));

    readoutLabel.setText (text, juce::dontSendNotification);
    applyButton.setEnabled (step == Step::done && hasValidHits());
}

void CalibrationWizardComponent::reset()
{
    step = Step::soft;
    softHit = mediumHit = hardHit = -1.0f;
    instructionLabel.setText (instructionText(), juce::dontSendNotification);
    updateReadout();
    repaint();
}

juce::String CalibrationWizardComponent::instructionText() const
{
    switch (step)
    {
        case Step::soft:
            return "Step 1/3: Select a pad, expand this section, then hit softly (ghost note).";
        case Step::medium:
            return "Step 2/3: Hit the same pad with medium force.";
        case Step::hard:
            return "Step 3/3: Hit with maximum accent force.";
        case Step::done:
            return hasValidHits() ? "Calibration complete. Preview below, then Apply."
                                  : "Hits were too similar - Reset and try again with clearer soft / medium / hard strokes.";
    }
    return {};
}

void CalibrationWizardComponent::captureHit (float velocityNormalized)
{
    const auto v = juce::jlimit (0.0f, 1.0f, velocityNormalized);

    switch (step)
    {
        case Step::soft:
            softHit = v;
            step = Step::medium;
            break;
        case Step::medium:
            if (v <= softHit + 0.02f)
                return;
            mediumHit = v;
            step = Step::hard;
            break;
        case Step::hard:
            if (v <= mediumHit + 0.02f)
                return;
            hardHit = v;
            step = Step::done;
            break;
        case Step::done:
            return;
    }

    instructionLabel.setText (instructionText(), juce::dontSendNotification);
    updateReadout();
    repaint();
}

svc::VelocityCurve CalibrationWizardComponent::buildCurve() const
{
    svc::VelocityCurve curve;
    if (! hasValidHits())
        return curve;

    const auto s = juce::jlimit (0.0f, 1.0f, softHit);
    const auto m = juce::jlimit (s + 0.02f, 1.0f, mediumHit);
    const auto h = juce::jlimit (m + 0.02f, 1.0f, hardHit);

    std::vector<svc::CurveControlPoint> points = {
        { 0.0f, 0.0f },
        { s, 0.25f },
        { m, 0.55f },
        { h, 0.92f },
        { 1.0f, 1.0f }
    };
    curve.setControlPoints (std::move (points));
    return curve;
}

void CalibrationWizardComponent::drawPreviewCurve (juce::Graphics& g, juce::Rectangle<float> area) const
{
    if (step != Step::done || ! hasValidHits())
        return;

    g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.35f));
    for (int i = 1; i < 4; ++i)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / 4.0f;
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    }

    const auto curve = buildCurve();
    const auto& lut = curve.getLut();
    juce::Path path;
    for (int i = 0; i < svc::VelocityCurve::lutSize; ++i)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / static_cast<float> (svc::VelocityCurve::lutSize - 1);
        const auto y = area.getBottom() - lut[static_cast<size_t> (i)] * area.getHeight();
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (juce::Colour (svc::ui::Theme::curveLine()));
    g.strokePath (path, juce::PathStrokeType (2.0f));
}

void CalibrationWizardComponent::paint (juce::Graphics& g)
{
    svc::ui::Theme::fillPanel (g, getLocalBounds().toFloat(), 8.0f);
    g.setColour (juce::Colour (svc::ui::Theme::textPrimary()));
    g.setFont (svc::ui::Theme::sectionFont());
    g.drawText ("Calibration Wizard", getLocalBounds().removeFromTop (22).reduced (10, 0), juce::Justification::centredLeft);

    if (! previewBounds.isEmpty())
    {
        g.setColour (juce::Colour (svc::ui::Theme::background()));
        g.fillRoundedRectangle (previewBounds.toFloat(), 4.0f);
        drawPreviewCurve (g, previewBounds.reduced (4).toFloat());
    }
}

void CalibrationWizardComponent::resized()
{
    auto area = getLocalBounds().reduced (10).withTrimmedTop (24);
    instructionLabel.setBounds (area.removeFromTop (40));
    area.removeFromTop (4);
    readoutLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    auto buttons = area.removeFromBottom (26);
    resetButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2));
    applyButton.setBounds (buttons.reduced (2));

    area.removeFromBottom (4);
    previewBounds = area;
}
