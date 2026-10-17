#include "WavyToolbar.h"
#include "WavyLogo.h"
#include "WavySettings.h"

namespace wavy
{
namespace
{
juce::Font tbFont (float h = 11.0f, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Arial", h, bold ? juce::Font::bold : juce::Font::plain));
}
const juce::Colour kInk (180, 180, 180);
const float kScales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
}

Toolbar::Toolbar (juce::UndoManager& u, juce::AudioProcessorValueTreeState& s, juce::StringArray m,
                  juce::RangedAudioParameter* mp, juce::String name, juce::String ver)
    : undo (u), apvts (s), modes (std::move (m)), modeParam (mp), product (std::move (name)), version (std::move (ver))
{
    loadedValues = currentValues();
    setSize (width, height);
}

// Control rectangles: the reference's ToolbarXML rectangles, with the arrow / copy / setup
// widths narrowed so the Mode drop-down fits next to Skins.
juce::Rectangle<int> Toolbar::rect (Item i) const
{
    switch (i)
    {
        case About:  return { 4, 2, 16, 15 };
        case Skins:  return { 23, 2, 49, 15 };
        case Mode:   return { 74, 2, 42, 15 };
        case Undo:   return { 118, 2, 24, 15 };
        case Redo:   return { 144, 2, 24, 15 };
        case Prev:   return { 170, 2, 24, 15 };
        case Next:   return { 196, 2, 24, 15 };
        case Copy:   return { 222, 2, 42, 15 };
        case Setup:  return { 266, 2, 42, 15 };
        case Save:   return { 310, 2, 38, 15 };
        case Menu:   return { 352, 2, 17, 15 };
        case Preset: return { 2, 19, 367, 15 };
        default:     return {};
    }
}

int Toolbar::hitTest (juce::Point<int> p) const
{
    for (int i = 0; i < numItems; ++i)
        if (rect ((Item) i).contains (p)) return i;
    return -1;
}

void Toolbar::refresh() { repaint(); }

//==============================================================================
// Setup A/B
void Toolbar::switchSetup()
{
    slot[active] = apvts.copyState();
    const int other = 1 - active;
    if (! slot[other].isValid()) slot[other] = slot[active].createCopy();
    active = other;
    const auto w = apvts.state.getProperty ("uiWidth");
    apvts.replaceState (slot[active].createCopy());
    apvts.state.setProperty ("uiWidth", w, nullptr);
}

void Toolbar::copyToOther()
{
    slot[active] = apvts.copyState();
    slot[1 - active] = slot[active].createCopy();
}

//==============================================================================
// Presets
juce::File Toolbar::userDir() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Wavy Suite").getChildFile ("User Presets").getChildFile (product);
}

juce::Array<juce::File> Toolbar::userPresets() const
{
    auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.wavypreset");
    files.sort();
    return files;
}

std::vector<float> Toolbar::currentValues() const
{
    std::vector<float> v;
    for (auto* p : apvts.processor.getParameters())
        v.push_back (p->getValue());
    return v;
}

bool Toolbar::isModified() const
{
    const auto now = currentValues();
    for (size_t i = 0; i < now.size() && i < loadedValues.size(); ++i)
        if (std::abs (now[i] - loadedValues[i]) > 1.0e-5f) return true;
    return false;
}

void Toolbar::loadDefault()
{
    for (auto* p : apvts.processor.getParameters())
    {
        if (p == modeParam) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
    presetName = "Default";
    presetFile = juce::File();
    loadedValues = currentValues();
    repaint();
}

void Toolbar::loadUser (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
    {
        const auto w = apvts.state.getProperty ("uiWidth");
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("uiWidth", w, nullptr);
        presetName = f.getFileNameWithoutExtension();
        presetFile = f;
        loadedValues = currentValues();
        repaint();
    }
}

void Toolbar::step (int delta)
{
    juce::Array<juce::File> list { juce::File() };   // index 0 = factory default
    list.addArray (userPresets());
    int idx = juce::jmax (0, list.indexOf (presetFile));
    idx = (idx + delta + list.size()) % list.size();
    if (idx == 0) loadDefault(); else loadUser (list[idx]);
}

void Toolbar::saveTo (const juce::File& f)
{
    f.getParentDirectory().createDirectory();
    auto state = apvts.copyState();
    state.removeProperty ("uiWidth", nullptr);
    if (auto xml = state.createXml()) xml->writeTo (f);
    presetName = f.getFileNameWithoutExtension();
    presetFile = f;
    loadedValues = currentValues();
    repaint();
}

void Toolbar::saveAs()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&menuLook);
    w->addTextEditor ("name", presetFile.exists() ? presetName : juce::String());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Toolbar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
        if (name.isNotEmpty())
            safe->saveTo (safe->userDir().getChildFile (name + ".wavypreset"));
    }), true);
}

void Toolbar::showPresetMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    m.addSectionHeader ("Factory Presets");
    m.addItem (1, "Default", true, ! presetFile.exists());
    const auto users = userPresets();
    if (! users.isEmpty())
    {
        m.addSeparator();
        m.addSectionHeader ("User Presets");
        for (int i = 0; i < users.size(); ++i)
            m.addItem (100 + i, users[i].getFileNameWithoutExtension(), true, users[i] == presetFile);
    }
    m.addSeparator();
    m.addItem (2, "Save As...");
    if (presetFile.exists()) m.addItem (3, "Save " + presetName);
    m.addItem (4, "Show User Presets Folder");
    juce::Component::SafePointer<Toolbar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rect (Preset))).withMinimumWidth (rect (Preset).getWidth()).withStandardItemHeight (16),
                     [safe, users] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1) safe->loadDefault();
        else if (r == 2) safe->saveAs();
        else if (r == 3) safe->saveTo (safe->presetFile);
        else if (r == 4) { safe->userDir().createDirectory(); safe->userDir().revealToUser(); }
        else if (r >= 100 && r - 100 < users.size()) safe->loadUser (users[r - 100]);
    });
}

void Toolbar::showAboutMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    m.addSectionHeader (product + "  v" + version);
    m.addItem (1, "About " + product);
    m.addSeparator();
    m.addItem (2, "Wavy Suite on GitHub");
    m.addItem (3, "Report an Issue");
    m.addItem (4, "License (AGPL-3.0)");
    juce::Component::SafePointer<Toolbar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rect (About))).withMinimumWidth (150).withStandardItemHeight (16),
                     [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1)
        {
            auto* w = new juce::AlertWindow (safe->product, safe->product + "  v" + safe->version + "\n\nOpen-source (AGPL-3.0)\n"
                                             + repoUrl + "\n\nNot affiliated with or endorsed by Waves Audio.",
                                             juce::MessageBoxIconType::NoIcon, safe.getComponent());
            w->setLookAndFeel (&safe->menuLook);
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->enterModalState (true, nullptr, true);
        }
        else if (r == 2) juce::URL (repoUrl).launchInDefaultBrowser();
        else if (r == 3) juce::URL (juce::String (repoUrl) + "/issues").launchInDefaultBrowser();
        else if (r == 4) juce::URL (juce::String (repoUrl) + "/blob/main/LICENSE").launchInDefaultBrowser();
    });
}

void Toolbar::showToolbarMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    const float cur = getWindowScale ? getWindowScale() : 1.0f;
    m.addSectionHeader ("Window Size");
    for (int i = 0; i < 5; ++i)
        m.addItem (10 + i, juce::String (juce::roundToInt (kScales[i] * 100.0f)) + "%", true, std::abs (cur - kScales[i]) < 0.02f);
    m.addSeparator();
    m.addItem (20, "Apply as Default to All");
    juce::Component::SafePointer<Toolbar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rect (Menu))).withStandardItemHeight (16),
                     [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 10 && r < 15) { if (safe->setWindowScale) safe->setWindowScale (kScales[r - 10]); }
        else if (r == 20 && safe->getWindowScale) setDefaultWindowScale (safe->getWindowScale());
    });
}

//==============================================================================
void Toolbar::paint (juce::Graphics& g)
{
    auto box = [&] (juce::Rectangle<int> r, bool hot)
    {
        g.setColour (hot ? juce::Colours::black : juce::Colour (34, 34, 34));
        g.fillRect (r);
        g.setColour (hot ? juce::Colours::white : juce::Colour (128, 128, 128));
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
        g.setColour (kInk.withMultipliedAlpha (enabled ? 1.0f : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        const auto tip = pt (a1);
        const juce::Point<float> tan (dir * std::sin (a1), -std::cos (a1));
        const juce::Point<float> nrm (-tan.y, tan.x);
        juce::Path head;
        head.addTriangle (tip + tan * 2.8f, tip - tan * 0.6f + nrm * 2.6f, tip - tan * 0.6f - nrm * 2.6f);
        g.fillPath (head);
    };
    auto arrow = [&] (juce::Rectangle<int> r, bool right)
    {
        const auto c = r.toFloat().getCentre();
        const float s = right ? 1.0f : -1.0f, len = 5.5f;
        g.setColour (kInk);
        g.drawLine (c.x - s * len, c.y, c.x + s * len, c.y, 1.4f);
        juce::Path h;
        h.addTriangle (c.x + s * (len + 2.2f), c.y, c.x + s * (len - 1.2f), c.y - 3.0f, c.x + s * (len - 1.2f), c.y + 3.0f);
        g.fillPath (h);
    };
    auto label = [&] (juce::Rectangle<int> r, const juce::String& t, float size = 11.0f)
    {
        g.setColour (kInk);
        g.setFont (tbFont (size));
        g.drawText (t, r, juce::Justification::centred);
    };

    for (int i = 0; i < numItems; ++i)
    {
        const auto r = rect ((Item) i);
        const bool hot = (i == this->hot);
        switch ((Item) i)
        {
            case About:
                drawMark (g, r.toFloat(), hot);
                break;
            case Skins: box (r, hot); label (r, juce::String::fromUTF8 ("Skins \xe2\x96\xbc")); break;
            case Mode:
            {
                box (r, hot);
                const auto name = modeParam != nullptr ? modes[(int) modeParam->convertFrom0to1 (modeParam->getValue())] : juce::String();
                label (r, name + juce::String::fromUTF8 (" \xe2\x96\xbc"), 10.0f);
                break;
            }
            case Undo: box (r, hot && undo.canUndo()); curved (r, false, undo.canUndo()); break;
            case Redo: box (r, hot && undo.canRedo()); curved (r, true,  undo.canRedo()); break;
            case Prev: box (r, hot); arrow (r, false); break;
            case Next: box (r, hot); arrow (r, true);  break;
            case Copy:
                box (r, hot);
                label (r, juce::String::fromUTF8 (active == 0 ? "A \xe2\x86\x92 B" : "B \xe2\x86\x92 A"));
                break;
            case Setup:
                box (r, hot);
                label (r, active == 0 ? "Setup A" : "Setup B");
                break;
            case Save: box (r, hot); label (r, "Save"); break;
            case Menu:
                box (r, hot);
                g.setColour (kInk);
                for (int k = 0; k < 3; ++k) g.fillRect (r.getX() + 4, r.getY() + 4 + k * 3, 9, 1);
                break;
            case Preset:
                box (r, hot);
                label (r, presetName + (isModified() ? "*" : ""));
                break;
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
    const auto item = (Item) hitTest (e.getPosition());
    const auto target = juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rect (item))).withStandardItemHeight (16);
    switch (item)
    {
        case Prev:   step (-1); break;
        case Next:   step (+1); break;
        case Undo:   undo.undo(); break;
        case Redo:   undo.redo(); break;
        case Copy:   copyToOther(); break;
        case Setup:  switchSetup(); break;
        case Save:   if (presetFile.exists()) saveTo (presetFile); else saveAs(); break;
        case Preset: showPresetMenu(); break;
        case About:  showAboutMenu(); break;
        case Menu:   showToolbarMenu(); break;
        case Skins:
        {
            juce::PopupMenu m; m.setLookAndFeel (&menuLook);
            m.addItem (1, "Dark", true, true);   // the other skins of the reference are not part of Wavy
            m.showMenuAsync (target);
            break;
        }
        case Mode:
            if (modeParam != nullptr)
            {
                juce::PopupMenu m; m.setLookAndFeel (&menuLook);
                const int cur = (int) modeParam->convertFrom0to1 (modeParam->getValue());
                for (int i = 0; i < modes.size(); ++i) m.addItem (i + 1, modes[i], true, i == cur);
                m.showMenuAsync (target.withMinimumWidth (rect (Mode).getWidth()), [this] (int r)
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
