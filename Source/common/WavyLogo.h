#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/WavySuite";

// The Wavy mark: a black tile with one smooth sine swell, cyan into amber, and an amber drop
// resting on the crest.
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, bool bright)
{
    g.setColour (juce::Colours::black);
    g.fillRect (r);

    const auto a = r.reduced (1.2f, 0.0f);
    const float mid = a.getCentreY() + 1.2f, amp = a.getHeight() * 0.30f;
    juce::Path wave;
    constexpr int n = 40;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / n;
        // 1.25 periods, damped at the ends so the swell grows and fades
        const float env = std::sin (juce::MathConstants<float>::pi * t);
        const juce::Point<float> p (a.getX() + t * a.getWidth(),
                                    mid - amp * (0.55f + 0.45f * env) * std::sin (t * juce::MathConstants<float>::twoPi * 1.25f - 0.6f));
        if (i == 0) wave.startNewSubPath (p); else wave.lineTo (p);
    }
    const auto cyan = bright ? juce::Colour (150, 235, 255) : juce::Colour (64, 214, 240);
    const auto amber = bright ? juce::Colour (255, 210, 110) : juce::Colour (255, 170, 30);
    g.setGradientFill (juce::ColourGradient (cyan, a.getX(), 0.0f, amber, a.getRight(), 0.0f, false));
    g.strokePath (wave, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // the drop, on the first crest
    const float t = (juce::MathConstants<float>::pi * 0.5f + 0.6f) / (juce::MathConstants<float>::twoPi * 1.25f);
    const float env = std::sin (juce::MathConstants<float>::pi * t);
    const float cx = a.getX() + t * a.getWidth(), cy = mid - amp * (0.55f + 0.45f * env) - 2.6f;
    g.setColour (amber);
    juce::Path drop;
    drop.addEllipse (cx - 1.4f, cy - 1.0f, 2.8f, 2.8f);
    g.fillPath (drop);
}
}
