#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wavy
{
// Popup menus in the look of the Waves toolbar: flat dark grey, 1 px grey border, Arial 11,
// the highlighted row turns black with white text.
class MenuLook : public juce::LookAndFeel_V4
{
public:
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::plain)); }
    int getPopupMenuBorderSize() override { return 1; }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (34, 34, 34));
        g.setColour (juce::Colour (128, 128, 128));
        g.drawRect (0, 0, w, h, 1);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& w, int& h) override
    {
        if (isSeparator) { w = 50; h = 5; return; }
        w = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 34;
        h = 16;
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& a, bool sep, bool active, bool hot, bool ticked,
                            bool sub, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) override
    {
        if (sep)
        {
            g.setColour (juce::Colour (80, 80, 80));
            g.fillRect (a.getX() + 4, a.getCentreY(), a.getWidth() - 8, 1);
            return;
        }
        if (hot && active) { g.setColour (juce::Colours::black); g.fillRect (a); }
        g.setColour ((hot && active ? juce::Colours::white : juce::Colour (190, 190, 190)).withMultipliedAlpha (active ? 1.0f : 0.4f));
        g.setFont (getPopupMenuFont());
        g.drawText (text, a.withTrimmedLeft (18).withTrimmedRight (8), juce::Justification::centredLeft, true);
        if (ticked)
        {
            juce::Path p;
            const float x = (float) a.getX() + 5.0f, y = (float) a.getCentreY();
            p.startNewSubPath (x, y); p.lineTo (x + 2.5f, y + 3.0f); p.lineTo (x + 7.0f, y - 3.5f);
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        if (sub)
        {
            juce::Path p;
            const float x = (float) a.getRight() - 8.0f, y = (float) a.getCentreY();
            p.addTriangle (x, y - 3.0f, x, y + 3.0f, x + 4.0f, y);
            g.fillPath (p);
        }
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& a, const juce::String& s) override
    {
        g.setColour (juce::Colour (140, 140, 140));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::bold)));
        g.drawText (s, a.withTrimmedLeft (8), juce::Justification::centredLeft, true);
        g.setColour (juce::Colour (80, 80, 80));
        g.fillRect (a.getX() + 4, a.getBottom() - 1, a.getWidth() - 8, 1);
    }
};
}
