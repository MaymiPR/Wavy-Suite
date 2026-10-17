#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
inline const char* const repoUrl = "https://github.com/MaymiPR/WavySuite";

// The Wavy mark: a minimal waveform, a row of rounded bars rising and falling like a sound wave.
inline void drawMark (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    static const float heights[] = { 0.30f, 0.62f, 0.95f, 0.55f, 0.80f, 0.38f, 0.18f };
    constexpr int n = 7;
    const float pitch = r.getWidth() / (float) n, bar = juce::jmax (1.2f, pitch * 0.58f);
    g.setColour (colour);
    for (int i = 0; i < n; ++i)
    {
        const float h = heights[i] * r.getHeight();
        g.fillRoundedRectangle (r.getX() + pitch * ((float) i + 0.5f) - bar * 0.5f, r.getCentreY() - h * 0.5f, bar, h, bar * 0.5f);
    }
}
}
