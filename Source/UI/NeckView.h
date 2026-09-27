#pragma once
#include "DaliLookAndFeel.h"
#include "../Engine/GuitarEngine.h"

// Real-time neck: which string / fret each note is on, and where the fretting hand is.
class NeckView : public juce::Component
{
public:
    explicit NeckView (dgtr::GuitarEngine& e) : engine (e) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (dali::panel);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (dali::line);
        g.drawRoundedRectangle (b, 10.0f, 1.0f);

        auto inner = b.reduced (26.0f, 22.0f);
        const auto head = inner.removeFromLeft (26.0f);
        const auto neck = inner;

        g.setGradientFill (ColourGradient (Colour (0xff18161d), neck.getX(), neck.getY(), Colour (0xff0f0e12), neck.getX(), neck.getBottom(), false));
        g.fillRect (neck);

        // inlays
        g.setColour (dali::line.brighter (0.15f));
        for (int f : { 3, 5, 7, 9, 15, 17, 19, 21 })
            g.fillEllipse (mid (f, neck) - 3.5f, neck.getCentreY() - 3.5f, 7.0f, 7.0f);
        for (float fy : { 0.3f, 0.7f })
            g.fillEllipse (mid (12, neck) - 3.5f, neck.getY() + neck.getHeight() * fy - 3.5f, 7.0f, 7.0f);

        // hand window
        const float hp = engine.hud.hand.load();
        {
            const float x0 = fretX (jmax (0.0f, hp - 1.5f), neck), x1 = fretX (jmin (22.0f, hp + 2.5f), neck);
            g.setColour (dali::accent.withAlpha (0.06f));
            g.fillRect (Rectangle<float> (x0, neck.getY(), x1 - x0, neck.getHeight()));
        }

        // nut + frets
        g.setColour (dali::text.withAlpha (0.7f));
        g.fillRect (neck.getX() - 2.5f, neck.getY(), 3.0f, neck.getHeight());
        g.setColour (Colour (0xff3a3842));
        for (int f = 1; f <= dgtr::kNumFrets; ++f)
        {
            const float x = fretX ((float) f, neck);
            g.drawLine (x, neck.getY(), x, neck.getBottom(), 1.2f);
        }
        g.setColour (dali::dim);
        g.setFont (dali::font (9.5f));
        for (int f : { 3, 5, 7, 9, 12, 15, 17, 19, 21 })
            g.drawText (String (f), Rectangle<float> (mid (f, neck) - 10.0f, neck.getBottom() + 3.0f, 20.0f, 12.0f), Justification::centred);

        // strings + markers (low E at the bottom, like tab)
        for (int s = 0; s < dgtr::kNumStrings; ++s)
        {
            const float y = neck.getBottom() - ((float) s + 0.5f) * neck.getHeight() / (float) dgtr::kNumStrings;
            const int fret = engine.hud.stringFret[(size_t) s].load();
            const float lvl = engine.hud.stringLevel[(size_t) s].load();
            g.setColour (fret >= 0 ? dali::text.withAlpha (0.35f + jmin (0.6f, lvl)) : Colour (0xff6a6675));
            g.drawLine (head.getX(), y, neck.getRight(), y, 2.2f - 0.25f * (float) s);

            if (fret >= 0)
            {
                const float cx = fret == 0 ? head.getCentreX() : mid (fret, neck);
                Path ring; ring.addEllipse (cx - 8.0f, y - 8.0f, 16.0f, 16.0f);
                g.setColour (dali::accentLo);
                g.fillPath (ring);
                dali::softGlow (g, ring, dali::accent, 1.2f);
                g.setColour (Colours::white);
                g.setFont (dali::font (9.0f, true));
                g.drawText (String (fret), Rectangle<float> (cx - 8.0f, y - 8.0f, 16.0f, 16.0f), Justification::centred);
            }
            g.setColour (dali::dim);
            g.setFont (dali::font (9.5f, true));
            g.drawText (String (6 - s), Rectangle<float> (b.getX() + 6.0f, y - 6.0f, 12.0f, 12.0f), Justification::centred);
        }
    }

private:
    static float fretX (float f, juce::Rectangle<float> neck)
    {
        const float full = 1.0f - std::pow (2.0f, -22.0f / 12.0f);
        return neck.getX() + neck.getWidth() * (1.0f - std::pow (2.0f, -f / 12.0f)) / full;
    }
    static float mid (int f, juce::Rectangle<float> neck) { return 0.5f * (fretX ((float) (f - 1), neck) + fretX ((float) f, neck)); }

    dgtr::GuitarEngine& engine;
};
