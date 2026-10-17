#include "WavyToolbar.h"
#include "WavyLogo.h"

namespace wavy
{
static juce::Font tbFont() { return juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::plain)); }

Toolbar::Toolbar (juce::UndoManager& u, juce::AudioProcessorValueTreeState& s, juce::StringArray m,
                  juce::RangedAudioParameter* mp, juce::String name, juce::String ver)
    : undo (u), apvts (s), modes (std::move (m)), modeParam (mp), product (std::move (name)), version (std::move (ver))
{
    setSize (367, height);
}

juce::Rectangle<int> Toolbar::rect (Item i) const
{
    switch (i)
    {
        case Logo:   return { 0, 0, 17, height };
        case Skins:  return { 19, 0, 44, height };
        case Undo:   return { 66, 0, 22, height };
        case Redo:   return { 90, 0, 22, height };
        case SlotA:  return { 115, 0, 15, height };
        case SlotB:  return { 132, 0, 15, height };
        case Copy:   return { 149, 0, 20, height };
        case Prev:   return { 173, 0, 20, height };
        case Preset: return { 195, 0, 72, height };
        case Next:   return { 269, 0, 20, height };
        case Menu:   return { 292, 0, 15, height };
        case Mode:   return { 309, 0, 58, height };
        default:     return {};
    }
}

int Toolbar::hitTest (juce::Point<int> p) const
{
    for (int i = 0; i < numItems; ++i)
        if (rect ((Item) i).contains (p)) return i;
    return -1;
}

void Toolbar::selectSlot (int s)
{
    if (s == active) return;
    slot[active] = apvts.copyState();
    if (! slot[s].isValid()) slot[s] = slot[active].createCopy();   // B starts as a copy of A
    active = s;
    apvts.replaceState (slot[s].createCopy());
}

void Toolbar::copyToOther()
{
    slot[active] = apvts.copyState();
    slot[1 - active] = slot[active].createCopy();
}

void Toolbar::paint (juce::Graphics& g)
{
    const juce::Colour ink (176, 182, 188);
    auto box = [&] (juce::Rectangle<int> r, bool hot, bool lit = false)
    {
        g.setColour (hot ? juce::Colours::black : juce::Colour (34, 34, 34));
        g.fillRect (r);
        g.setColour (hot || lit ? juce::Colours::white : juce::Colour (128, 128, 128));
        g.drawRect (r, 1);
    };

    // circular arrow: the arc runs over the top, the head sits at the open end
    auto curved = [&] (juce::Rectangle<int> r, bool redo, bool enabled)
    {
        const auto c = r.toFloat().getCentre();
        const float rad = 4.2f, dir = redo ? -1.0f : 1.0f;
        const float a0 = juce::degreesToRadians (25.0f), a1 = juce::degreesToRadians (-215.0f);
        auto pt = [&] (float a) { return juce::Point<float> (c.x + dir * rad * std::cos (a), c.y + 0.5f + rad * std::sin (a)); };
        juce::Path arc;
        for (int i = 0; i <= 24; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 24.0f;
            if (i == 0) arc.startNewSubPath (pt (a)); else arc.lineTo (pt (a));
        }
        g.setColour (ink.withMultipliedAlpha (enabled ? 1.0f : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        const auto tip = pt (a1);
        const juce::Point<float> tan (dir * std::sin (a1), -std::cos (a1));   // direction of travel at the end
        const juce::Point<float> nrm (-tan.y, tan.x);
        juce::Path head;
        head.addTriangle (tip + tan * 2.8f, tip - tan * 0.6f + nrm * 2.6f, tip - tan * 0.6f - nrm * 2.6f);
        g.fillPath (head);
    };
    auto arrow = [&] (juce::Rectangle<int> r, bool right, float len = 5.5f)
    {
        const auto c = r.toFloat().getCentre();
        const float s = right ? 1.0f : -1.0f;
        g.setColour (ink);
        g.drawLine (c.x - s * len, c.y, c.x + s * len, c.y, 1.4f);
        juce::Path h;
        h.addTriangle (c.x + s * (len + 2.2f), c.y, c.x + s * (len - 1.2f), c.y - 3.0f, c.x + s * (len - 1.2f), c.y + 3.0f);
        g.fillPath (h);
    };

    for (int i = 0; i < numItems; ++i)
    {
        const auto r = rect ((Item) i);
        const bool hot = (i == this->hot);
        switch ((Item) i)
        {
            case Logo:
                g.setColour (juce::Colour (34, 34, 34)); g.fillRect (r);
                drawMark (g, r.toFloat().reduced (1.0f, 3.0f), 1.6f, hot ? 1.0f : 0.9f);
                break;
            case Skins:
                box (r, hot);
                g.setColour (ink);
                g.setFont (tbFont());
                g.drawText (juce::String::fromUTF8 ("Skins \xe2\x96\xbc"), r, juce::Justification::centred);
                break;
            case Undo: box (r, hot && undo.canUndo()); curved (r, false, undo.canUndo()); break;
            case Redo: box (r, hot && undo.canRedo()); curved (r, true,  undo.canRedo()); break;
            case SlotA: case SlotB:
            {
                const int s = i == SlotA ? 0 : 1;
                box (r, hot, active == s);
                g.setColour (active == s ? juce::Colours::white : ink);
                g.setFont (juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::bold)));
                g.drawText (s == 0 ? "A" : "B", r, juce::Justification::centred);
                break;
            }
            case Copy:
                box (r, hot);
                g.setColour (ink);
                g.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::bold)));
                g.drawText (active == 0 ? "A>B" : "B>A", r, juce::Justification::centred);
                break;
            case Prev: box (r, hot); arrow (r, false); break;
            case Next: box (r, hot); arrow (r, true);  break;
            case Preset:
                box (r, hot);
                g.setColour (ink);
                g.setFont (tbFont());
                g.drawText (presetName, r.reduced (4, 0), juce::Justification::centredLeft, true);
                break;
            case Menu:
                box (r, hot);
                g.setColour (ink);
                for (int k = 0; k < 3; ++k) g.fillRect (r.getX() + 3, r.getY() + 4 + k * 3, 9, 1);
                break;
            case Mode:
            {
                box (r, hot);
                g.setColour (ink);
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
    const auto target = juce::PopupMenu::Options().withTargetComponent (this);
    switch ((Item) hitTest (e.getPosition()))
    {
        case Prev:  if (onPresetStep) onPresetStep (-1); break;
        case Next:  if (onPresetStep) onPresetStep (+1); break;
        case Undo:  undo.undo(); break;
        case Redo:  undo.redo(); break;
        case SlotA: selectSlot (0); break;
        case SlotB: selectSlot (1); break;
        case Copy:  copyToOther(); break;
        case Skins:
        {
            juce::PopupMenu m;
            m.addItem (1, "Dark", true, true);   // the other skins of the reference are not part of Wavy
            m.showMenuAsync (target);
            break;
        }
        case Preset:
        {
            juce::PopupMenu m;
            m.addItem (1, "Default", true, true);
            m.showMenuAsync (target);
            break;
        }
        case Menu:
        {
            juce::PopupMenu m;
            m.addSectionHeader (product + "  v" + version);
            m.addItem (1, "Wavy Suite on GitHub");
            m.addSeparator();
            m.addItem (2, "Default preset", true, true);
            m.showMenuAsync (target, [] (int r) { if (r == 1) juce::URL (repoUrl).launchInDefaultBrowser(); });
            break;
        }
        case Mode:
            if (modeParam != nullptr)
            {
                juce::PopupMenu m;
                const int cur = (int) modeParam->convertFrom0to1 (modeParam->getValue());
                for (int i = 0; i < modes.size(); ++i) m.addItem (i + 1, modes[i], true, i == cur);
                m.showMenuAsync (target.withMinimumWidth (58), [this] (int r)
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
