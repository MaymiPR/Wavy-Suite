#pragma once
// The mouse wheel on a Waves control, as measured on H-Comp, MaxxBass, RVox and RBass (MeterCapture wheel scripts,
// MEASUREMENTS.md): 1/72 of the control's range per notch, a stepped control one step per notch; Ctrl changes nothing.
// JUCE's own wheel moves 15 % of the range per unit, 3.5 % per notch: 2.5 times as far. JUCE gives a Windows notch as
// 60 / 256 of wheel delta.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace wavy
{
// acc: notches not yet turned into steps (stepped controls); scale: a control whose range is not the reference's (measured)
inline void wavesWheel (juce::Slider& s, const juce::MouseEvent& e, const juce::MouseWheelDetails& w, double& acc, bool stepped,
                        double scale = 1.0)
{
    if (e.mods.isAnyMouseButtonDown() || ! s.isEnabled() || s.getMaximum() <= s.getMinimum()) return;
    const double notches = (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY) * (w.isReversed ? -1.0 : 1.0) / (60.0 / 256.0);
    double v = s.getValue();
    if (stepped && s.getInterval() > 0.0)
    {
        acc += notches;
        const double whole = std::trunc (acc);
        acc -= whole;
        v += whole * s.getInterval();
    }
    else v = s.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, s.valueToProportionOfLength (v) + notches * scale / 72.0));
    v = juce::jlimit (s.getMinimum(), s.getMaximum(), v);
    if (v == s.getValue()) return;
    juce::Slider::ScopedDragNotification drag (s);
    s.setValue (v, juce::sendNotificationSync);
}

// a wheel event that reached a parent because the control under the mouse only takes clicks on its thumb (hitTest):
// the reference's controls take the wheel over their whole area
inline bool forwardWheel (juce::Component& parent, const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    for (auto* c : parent.getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (c))
            if (s->isVisible() && s->getBounds().contains (e.getEventRelativeTo (&parent).getPosition()))
            {
                s->mouseWheelMove (e.getEventRelativeTo (s), w);
                return true;
            }
    return false;
}
}
