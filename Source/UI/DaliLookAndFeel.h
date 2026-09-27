#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Dali Audio visual language: black / dark grey, restrained neon purple accent, clean type.
namespace dali
{
inline const juce::Colour bg       { 0xff0b0b0e };
inline const juce::Colour panel    { 0xff141418 };
inline const juce::Colour panelHi  { 0xff1b1b21 };
inline const juce::Colour line     { 0xff26262e };
inline const juce::Colour text     { 0xffe4e0ec };
inline const juce::Colour dim      { 0xff7d7a8a };
inline const juce::Colour accent   { 0xffa06bff };   // subtle neon purple
inline const juce::Colour accentLo { 0xff5b3a99 };

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// soft glow: two faint halos + crisp core
inline void softGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
{
    using PST = juce::PathStrokeType;
    g.setColour (c.withMultipliedAlpha (0.10f)); g.strokePath (p, PST (w + 6.0f, PST::curved, PST::rounded));
    g.setColour (c.withMultipliedAlpha (0.22f)); g.strokePath (p, PST (w + 2.5f, PST::curved, PST::rounded));
    g.setColour (c);                              g.strokePath (p, PST (w, PST::curved, PST::rounded));
}
} // namespace dali

class DaliLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DaliLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, dali::bg);
        setColour (Label::textColourId, dali::text);
        setColour (Slider::textBoxTextColourId, dali::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (ComboBox::backgroundColourId, dali::panelHi);
        setColour (ComboBox::textColourId, dali::text);
        setColour (ComboBox::outlineColourId, dali::line);
        setColour (ComboBox::arrowColourId, dali::accent);
        setColour (PopupMenu::backgroundColourId, dali::panel);
        setColour (PopupMenu::textColourId, dali::text);
        setColour (PopupMenu::highlightedBackgroundColourId, dali::accentLo.withAlpha (0.5f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (TextButton::buttonColourId, dali::panelHi);
        setColour (TextButton::textColourOffId, dali::text);
        setColour (TextButton::textColourOnId, Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);

        Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
        g.setColour (dali::line);
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        if (pos > 0.001f)
        {
            Path v; v.addCentredArc (c.x, c.y, r, r, 0.0f, a0, ang, true);
            dali::softGlow (g, v, dali::accent, 3.0f);
        }
        const float kr = r * 0.66f;
        g.setGradientFill (ColourGradient (Colour (0xff24242b), c.x, c.y - kr, Colour (0xff111114), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (dali::line);
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        Path ptr;
        ptr.startNewSubPath (c.x + std::sin (ang) * kr * 0.35f, c.y - std::cos (ang) * kr * 0.35f);
        ptr.lineTo (c.x + std::sin (ang) * kr * 0.85f, c.y - std::cos (ang) * kr * 0.85f);
        g.setColour (dali::text);
        g.strokePath (ptr, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto b = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (dali::panelHi);
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (box.isMouseOver (true) ? dali::accentLo : dali::line);
        g.drawRoundedRectangle (b, 5.0f, 1.0f);
        Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (dali::accent);
        g.fillPath (arrow);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? dali::accentLo : hi ? dali::panelHi.brighter (0.08f) : dali::panelHi);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (hi ? dali::accentLo : dali::line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override            { return dali::font (13.0f); }
    juce::Font getPopupMenuFont() override                            { return dali::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override    { return dali::font (12.5f, true); }
    juce::Font getLabelFont (juce::Label&) override                   { return dali::font (12.0f); }
};
