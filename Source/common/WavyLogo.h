#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/WavySuite";

// The Wavy mark: a "W" drawn as one continuous sine wave, cyan to amber, with a drop riding the crest.
inline juce::Path wavePath (juce::Rectangle<float> r)
{
    juce::Path p;
    constexpr int n = 64;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / n;
        const float pi = juce::MathConstants<float>::pi;
        const float y = 0.5f - 0.42f * std::cos (4.0f * pi * x) + 0.20f * std::pow (std::sin (pi * x), 2.0f) * (1.0f - std::cos (4.0f * pi * x)) * 0.5f;
        const juce::Point<float> pt (r.getX() + x * r.getWidth(), r.getY() + juce::jlimit (0.0f, 1.0f, y) * r.getHeight());
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    return p;
}

inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, float stroke, float alpha = 1.0f)
{
    r = r.reduced (stroke * 0.6f);
    juce::ColourGradient grad (juce::Colour (64, 214, 240).withAlpha (alpha), r.getX(), 0.0f,
                               juce::Colour (255, 170, 30).withAlpha (alpha), r.getRight(), 0.0f, false);
    g.setGradientFill (grad);
    g.strokePath (wavePath (r), juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}
