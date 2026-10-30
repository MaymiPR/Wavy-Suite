#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/Wavy-Suite";

// The Wavy mark (docs/branding/wavy-icon.svg): three identical sine swells, stepped evenly down
// and across, cyan into amber, each fainter than the last, on a black tile (or on whatever is behind it).
// With an opaque `mono` colour the swells are drawn in that one colour (a print on a panel or a coloured badge).
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, bool bright, bool tile = true,
                      juce::Colour mono = juce::Colours::transparentBlack, float strokeScale = 1.0f)
{
    if (tile)
    {
        g.setColour (juce::Colours::black);
        g.fillRect (r);
    }

    // design space: the three waves span x 28..172, y 31.5..168.5 (200 x 200 icon)
    constexpr float x0 = 28.0f, x1 = 172.0f, y0 = 31.5f, y1 = 168.5f;
    const float s = juce::jmin ((r.getWidth() - 2.0f) / (x1 - x0), (r.getHeight() - 2.0f) / (y1 - y0));
    const auto t = juce::AffineTransform::translation (-(x0 + x1) * 0.5f, -(y0 + y1) * 0.5f)
                       .scaled (s).translated (r.getCentreX(), r.getCentreY());

    juce::Path base;
    base.startNewSubPath (20.0f, 0.0f);
    base.cubicTo (38.0f, -22.0f, 60.0f, -22.0f, 78.0f, 0.0f);
    base.cubicTo (96.0f, 22.0f, 118.0f, 22.0f, 136.0f, 0.0f);

    const auto cyan = bright ? juce::Colour (150, 235, 255) : juce::Colour (64, 214, 240);
    const auto amber = bright ? juce::Colour (255, 210, 110) : juce::Colour (255, 170, 30);
    const float stroke = juce::jmax (1.2f, 16.0f * s) * strokeScale;   // a little bolder than the icon so it holds at 16 px
    struct Wave { float dx, dy, alpha; };
    for (auto w : { Wave { 8.0f, 54.0f, 1.0f }, Wave { 22.0f, 100.0f, 0.6f }, Wave { 36.0f, 146.0f, 0.3f } })
    {
        auto p = base;
        p.applyTransform (juce::AffineTransform::translation (w.dx, w.dy).followedBy (t));
        if (! mono.isTransparent()) g.setColour (mono.withMultipliedAlpha (0.55f + 0.45f * w.alpha));
        else g.setGradientFill (juce::ColourGradient (cyan.withAlpha (w.alpha), r.getX() + 1.0f, 0.0f,
                                                      amber.withAlpha (w.alpha), r.getRight() - 1.0f, 0.0f, false));
        g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
}
