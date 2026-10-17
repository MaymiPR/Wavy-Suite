#include "WavyToolbar.h"

namespace wavy
{
static juce::Font tbFont() { return juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::plain)); }

Toolbar::Toolbar (juce::UndoManager& u, juce::StringArray m, juce::RangedAudioParameter* mp)
    : undo (u), modes (std::move (m)), modeParam (mp)
{
    setSize (367, height);
}

juce::Rectangle<int> Toolbar::rect (Item i) const
{
    switch (i)
    {
        case Logo:   return { 0, 0, 18, height };
        case Skins:  return { 21, 0, 49, height };
        case Prev:   return { 73, 0, 26, height };
        case Next:   return { 101, 0, 26, height };
        case Preset: return { 129, 0, 105, height };
        case Menu:   return { 236, 0, 15, height };
        case Undo:   return { 254, 0, 26, height };
        case Redo:   return { 282, 0, 26, height };
        case Mode:   return { 311, 0, 56, height };
        default:     return {};
    }
}

int Toolbar::hitTest (juce::Point<int> p) const
{
    for (int i = 0; i < numItems; ++i)
        if (rect ((Item) i).contains (p)) return i;
    return -1;
}

void Toolbar::paint (juce::Graphics& g)
{
    auto drawBox = [&] (juce::Rectangle<int> r, bool hot, bool enabled = true)
    {
        g.setColour (hot ? juce::Colours::black : juce::Colour (34, 34, 34));
        g.fillRect (r);
        g.setColour (hot ? juce::Colours::white : juce::Colour (128, 128, 128));
        g.drawRect (r, 1);
        g.setColour (juce::Colour (170, 170, 170).withMultipliedAlpha (enabled ? 1.0f : 0.4f));
    };
    auto arrow = [&] (juce::Rectangle<int> r, bool right)
    {
        auto c = r.toFloat().getCentre();
        juce::Path p;
        const float s = right ? 1.0f : -1.0f;
        p.addArrow ({ c.x - 6 * s, c.y, c.x + 6 * s, c.y }, 4.0f, 8.0f, 5.0f);
        g.fillPath (p);
    };
    auto curved = [&] (juce::Rectangle<int> r, bool redo)
    {
        auto c = r.toFloat().getCentre();
        juce::Path p;
        p.addCentredArc (c.x, c.y, 4.5f, 4.0f, 0, redo ? 0.6f : -3.6f, redo ? 4.6f : 0.6f + 0.0f, true);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        const float ax = redo ? c.x + 4.5f : c.x - 4.5f;
        g.fillRect (juce::Rectangle<float> (ax - 2.5f, c.y - 4.5f, 5.0f, 3.0f));
    };

    for (int i = 0; i < numItems; ++i)
    {
        const auto r = rect ((Item) i);
        const bool hot = (i == this->hot);
        switch ((Item) i)
        {
            case Logo:
            {
                g.setColour (juce::Colour (34, 34, 34)); g.fillRect (r);
                g.setColour (hot ? juce::Colours::white : juce::Colour (170, 170, 170));
                g.setFont (juce::Font (juce::FontOptions ("Georgia", 13.0f, juce::Font::bold | juce::Font::italic)));
                g.drawText ("W", r, juce::Justification::centred);
                break;
            }
            case Skins:
                drawBox (r, hot);
                g.setFont (tbFont());
                g.drawText (juce::String::fromUTF8 ("Skins \xe2\x96\xbc"), r, juce::Justification::centred);
                break;
            case Prev: drawBox (r, hot); arrow (r, false); break;
            case Next: drawBox (r, hot); arrow (r, true);  break;
            case Preset:
                drawBox (r, hot);
                g.setFont (tbFont());
                g.drawText (presetName, r.reduced (4, 0), juce::Justification::centredLeft, true);
                break;
            case Menu:
                drawBox (r, hot);
                for (int k = 0; k < 3; ++k)
                    g.fillRect (r.getX() + 3, r.getY() + 4 + k * 3, 9, 1);
                break;
            case Undo: drawBox (r, hot, undo.canUndo()); curved (r, false); break;
            case Redo: drawBox (r, hot, undo.canRedo()); curved (r, true);  break;
            case Mode:
            {
                drawBox (r, hot);
                g.setFont (tbFont());
                const auto name = modeParam != nullptr ? modes[(int) modeParam->convertFrom0to1 (modeParam->getValue())] : juce::String();
                g.drawText (name + juce::String::fromUTF8 (" \xe2\x96\xbc"), r, juce::Justification::centred);
                break;
            }
            default: break;
        }
    }
}

void Toolbar::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTest (e.getPosition());
    if (h != hot) { hot = h; repaint(); }
}

void Toolbar::mouseExit (const juce::MouseEvent&) { hot = -1; repaint(); }

void Toolbar::mouseDown (const juce::MouseEvent& e)
{
    switch ((Item) hitTest (e.getPosition()))
    {
        case Prev: if (onPresetStep) onPresetStep (-1); break;
        case Next: if (onPresetStep) onPresetStep (+1); break;
        case Undo: undo.undo(); break;
        case Redo: undo.redo(); break;
        case Skins:
        {
            juce::PopupMenu m;
            m.addItem (1, "Dark", true, true);   // the other skins of the reference are not part of Wavy
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            break;
        }
        case Preset: case Menu:
        {
            juce::PopupMenu m;
            m.addItem (1, "Default", true, true);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            break;
        }
        case Mode:
            if (modeParam != nullptr)
            {
                juce::PopupMenu m;
                const int cur = (int) modeParam->convertFrom0to1 (modeParam->getValue());
                for (int i = 0; i < modes.size(); ++i)
                    m.addItem (i + 1, modes[i], true, i == cur);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (56),
                                 [this] (int r)
                                 {
                                     if (r <= 0) return;
                                     modeParam->beginChangeGesture();
                                     modeParam->setValueNotifyingHost (modeParam->convertTo0to1 ((float) (r - 1)));
                                     modeParam->endChangeGesture();
                                     repaint();
                                 });
            }
            break;
        default: break;
    }
    repaint();
}
}
