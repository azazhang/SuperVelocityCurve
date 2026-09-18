#pragma once

#include <JuceHeader.h>

namespace svc::ui
{

enum class ThemeMode
{
    system,
    dark,
    light
};

inline juce::String themeModeToString (ThemeMode mode) noexcept
{
    switch (mode)
    {
        case ThemeMode::system: return "system";
        case ThemeMode::light:  return "light";
        case ThemeMode::dark:
        default:                return "dark";
    }
}

inline ThemeMode themeModeFromString (const juce::String& str) noexcept
{
    if (str.equalsIgnoreCase ("system"))
        return ThemeMode::system;
    if (str.equalsIgnoreCase ("light"))
        return ThemeMode::light;
    return ThemeMode::dark;
}

inline ThemeMode resolveEffectiveMode (ThemeMode mode) noexcept
{
    if (mode == ThemeMode::system)
        return juce::Desktop::getInstance().isDarkModeActive() ? ThemeMode::dark : ThemeMode::light;
    return mode;
}

/** Flagship modern studio palette — deep obsidian & electric cyan glow (dark); alabaster & cobalt (light). */
struct Theme
{
    static void setMode (ThemeMode mode) noexcept
    {
        configuredMode = mode;
        effectiveMode = resolveEffectiveMode (mode);
    }
    static ThemeMode getConfiguredMode() noexcept { return configuredMode; }
    static ThemeMode getMode() noexcept { return effectiveMode; }

    static juce::uint32 background()       { return effectiveMode == ThemeMode::dark ? 0xff0d0f14 : 0xfff1f3f7; }
    static juce::uint32 backgroundTop()    { return effectiveMode == ThemeMode::dark ? 0xff141722 : 0xfff8fafc; }
    static juce::uint32 panel()            { return effectiveMode == ThemeMode::dark ? 0xff161922 : 0xffffffff; }
    static juce::uint32 panelRaised()      { return effectiveMode == ThemeMode::dark ? 0xff1e222e : 0xfff8fafc; }
    static juce::uint32 border()           { return effectiveMode == ThemeMode::dark ? 0xff282f3d : 0xffcbd5e1; }
    static juce::uint32 borderBright()     { return effectiveMode == ThemeMode::dark ? 0xff3b4457 : 0xff94a3b8; }
    static juce::uint32 accent()           { return effectiveMode == ThemeMode::dark ? 0xff38bdf8 : 0xff0284c7; }
    static juce::uint32 accentGold()       { return effectiveMode == ThemeMode::dark ? 0xfff59e0b : 0xffd97706; }
    static juce::uint32 accentSecondary()  { return effectiveMode == ThemeMode::dark ? 0xffa855f7 : 0xff7c3aed; }
    static juce::uint32 accentDim()        { return effectiveMode == ThemeMode::dark ? 0xff16334a : 0xffbae6fd; }
    static juce::uint32 accentWarm()       { return effectiveMode == ThemeMode::dark ? 0xfffb923c : 0xffea580c; }
    static juce::uint32 textPrimary()      { return effectiveMode == ThemeMode::dark ? 0xfff1f5f9 : 0xff0f172a; }
    static juce::uint32 textSecondary()    { return effectiveMode == ThemeMode::dark ? 0xff94a3b8 : 0xff475569; }
    static juce::uint32 textMuted()        { return effectiveMode == ThemeMode::dark ? 0xff64748b : 0xff94a3b8; }
    static juce::uint32 padIdle()          { return effectiveMode == ThemeMode::dark ? 0xff202532 : 0xffe2e8f0; }
    static juce::uint32 padHover()         { return effectiveMode == ThemeMode::dark ? 0xff2b3244 : 0xffcbd5e1; }
    static juce::uint32 padSelected()      { return effectiveMode == ThemeMode::dark ? 0xff1a3c63 : 0xff93c5fd; }
    static juce::uint32 padHit()           { return effectiveMode == ThemeMode::dark ? 0xfff59e0b : 0xfff59e0b; }
    static juce::uint32 padDisabled()      { return effectiveMode == ThemeMode::dark ? 0xff13161f : 0xfff1f5f9; }
    static juce::uint32 curveLine()        { return effectiveMode == ThemeMode::dark ? 0xff38bdf8 : 0xff0284c7; }
    static juce::uint32 curveLineEnd()     { return effectiveMode == ThemeMode::dark ? 0xff7dd3fc : 0xff38bdf8; }
    static juce::uint32 curveGrid()        { return effectiveMode == ThemeMode::dark ? 0xff222836 : 0xffe2e8f0; }
    static juce::uint32 curveHit()         { return effectiveMode == ThemeMode::dark ? 0xfff59e0b : 0xffd97706; }
    static juce::uint32 curveGateMuted()   { return effectiveMode == ThemeMode::dark ? 0xff22141c : 0xfffee2e2; }
    static juce::uint32 curveGateSaturated() { return effectiveMode == ThemeMode::dark ? 0xff101c2e : 0xffe0f2fe; }
    static juce::uint32 plotBackground()   { return effectiveMode == ThemeMode::dark ? 0xff090b10 : 0xffe8edf4; }
    static juce::uint32 success()          { return effectiveMode == ThemeMode::dark ? 0xff10b981 : 0xff16a34a; }
    static juce::uint32 error()            { return effectiveMode == ThemeMode::dark ? 0xffef4444 : 0xffdc2626; }

    static float curveGridAlpha() noexcept { return effectiveMode == ThemeMode::dark ? 0.6f : 0.9f; }

    static juce::Font titleFont()       { return juce::Font (juce::FontOptions (20.0f)).boldened(); }
    static juce::Font sectionFont()     { return juce::Font (juce::FontOptions (11.5f)).boldened(); }
    static juce::Font bodyFont()        { return juce::Font (juce::FontOptions (12.0f)); }
    static juce::Font smallFont()       { return juce::Font (juce::FontOptions (10.5f)); }
    static juce::Font sectionHeaderFont() { return juce::Font (juce::FontOptions (10.0f)).boldened(); }

    static void fillBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (effectiveMode == ThemeMode::dark)
        {
            juce::ColourGradient grad (juce::Colour (backgroundTop()), 0.0f, 0.0f,
                                       juce::Colour (background()), 0.0f, static_cast<float> (bounds.getHeight()), false);
            g.setGradientFill (grad);
            g.fillAll();
        }
        else
        {
            g.fillAll (juce::Colour (background()));
        }
    }

    static void fillPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 8.0f)
    {
        g.setColour (juce::Colour (panel()));
        g.fillRoundedRectangle (bounds, radius);

        // Micro-bevel top lighting for tactile hardware depth
        if (effectiveMode == ThemeMode::dark)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        }

        g.setColour (juce::Colour (border()).withAlpha (effectiveMode == ThemeMode::dark ? 0.85f : 0.9f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }

    static void fillPlot (juce::Graphics& g, juce::Rectangle<float> plot, float radius = 6.0f)
    {
        g.setColour (juce::Colour (plotBackground()));
        g.fillRoundedRectangle (plot, radius);

        if (effectiveMode == ThemeMode::dark)
        {
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawRoundedRectangle (plot, radius, 1.5f);
        }

        g.setColour (juce::Colour (border()).withAlpha (effectiveMode == ThemeMode::dark ? 0.6f : 0.7f));
        g.drawRoundedRectangle (plot, radius, 1.0f);
    }

    static void drawSectionHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        g.setFont (sectionHeaderFont());
        g.setColour (juce::Colour (textSecondary()));
        g.drawText (text.toUpperCase(), area, juce::Justification::centredLeft);
    }

    /** Readable label colour on top of a pad/button fill colour. */
    static juce::Colour textOnBackground (juce::uint32 backgroundArgb) noexcept
    {
        const auto bg = juce::Colour (backgroundArgb);
        return bg.getPerceivedBrightness() > 0.58f ? juce::Colour (0xff0f172a)
                                                   : juce::Colour (textPrimary());
    }

private:
    static inline ThemeMode configuredMode = ThemeMode::system;
    static inline ThemeMode effectiveMode = ThemeMode::dark;
};

} // namespace svc::ui
