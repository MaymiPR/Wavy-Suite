#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/WavySuite";

// The Wavy mark: two beamed eighth notes whose beam is a sound wave, cyan to amber.
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, float alpha = 1.0f)
{
    const float w = r.getWidth(), h = r.getHeight();
    juce::ColourGradient grad (juce::Colour (64, 214, 240).withAlpha (alpha), r.getX(), 0.0f,
                               juce::Colour (255, 170, 30).withAlpha (alpha), r.getRight(), 0.0f, false);
    g.setGradientFill (grad);

    const float rad = 0.15f * w;
    const juce::Point<float> head1 (r.getX() + 0.27f * w, r.getY() + 0.80f * h);
    const juce::Point<float> head2 (r.getX() + 0.77f * w, r.getY() + 0.66f * h);
    const float stem1 = head1.x + rad * 0.85f, stem2 = head2.x + rad * 0.85f;
    const float sw = juce::jmax (1.0f, 0.09f * w);

    // noteheads (tilted ovals)
    for (auto c : { head1, head2 })
    {
        juce::Path p;
        p.addEllipse (c.x - rad * 1.15f, c.y - rad * 0.8f, rad * 2.3f, rad * 1.6f);
        g.fillPath (p, juce::AffineTransform::rotation (-0.35f, c.x, c.y));
    }
    // the wavy beam, with the stems hanging from it
    juce::Path beam;
    const float y0 = r.getY() + 0.22f * h, y1 = r.getY() + 0.10f * h, amp = 0.09f * h;
    constexpr int n = 24;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / n;
        const juce::Point<float> pt (stem1 + (stem2 - stem1) * t,
                                     y0 + (y1 - y0) * t + amp * std::sin (t * juce::MathConstants<float>::twoPi));
        if (i == 0) beam.startNewSubPath (pt); else beam.lineTo (pt);
    }
    g.strokePath (beam, juce::PathStrokeType (juce::jmax (1.6f, 0.17f * h), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRect (stem1 - sw * 0.5f, y0, sw, head1.y - y0);
    g.fillRect (stem2 - sw * 0.5f, y1, sw, head2.y - y1);
}
}
