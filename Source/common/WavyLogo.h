#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/WavySuite";

// The Wavy mark: a mock of the reference's about-button, a black block holding a "W" built from
// tapered waveform bars (up, down, up, down, up), here in cyan.
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    g.setColour (juce::Colours::black);
    g.fillRect (r);

    const float sx = r.getWidth() / 16.0f, sy = r.getHeight() / 15.0f, mid = r.getY() + 7.0f * sy;
    struct Bar { float x, tip; };   // tip: distance from the mid line (negative = up)
    static const Bar bars[] = { { 1.8f, -6.8f }, { 4.7f, 6.0f }, { 8.0f, -5.8f }, { 11.3f, 6.0f }, { 14.2f, -6.8f } };
    g.setColour (colour);
    for (auto& b : bars)
    {
        const float cx = r.getX() + b.x * sx, wm = 1.7f * sx, wt = 0.7f * sx, ty = mid + b.tip * sy;
        juce::Path p;
        p.startNewSubPath (cx - wm, mid);
        p.lineTo (cx - wt, ty);
        p.lineTo (cx + wt, ty);
        p.lineTo (cx + wm, mid);
        p.closeSubPath();
        g.fillPath (p);
    }
}
}
