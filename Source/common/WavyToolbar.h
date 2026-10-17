#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace wavy
{
// The strip across the top of every Wavy plugin (same 15 px strip and grey button
// style as the reference, with the Wavy mark): Skins, preset arrows, preset name,
// preset menu, undo / redo and the Mode switch (mono / stereo / ... components).
class Toolbar : public juce::Component
{
public:
    Toolbar (juce::UndoManager& undo, juce::StringArray modeNames, juce::RangedAudioParameter* modeParam);

    // Called when the preset buttons are used (+1 / -1 / 0 = menu item chosen by name).
    std::function<void (int delta)> onPresetStep;
    juce::String presetName { "Default" };

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh() { repaint(); }

    static constexpr int height = 15;

private:
    enum Item { Logo, Skins, Prev, Next, Preset, Menu, Undo, Redo, Mode, numItems };
    juce::Rectangle<int> rect (Item) const;
    int hitTest (juce::Point<int>) const;

    juce::UndoManager& undo;
    juce::StringArray modes;
    juce::RangedAudioParameter* modeParam;
    int hot = -1;
};
}
