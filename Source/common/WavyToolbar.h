#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "WavyMenuLook.h"

namespace wavy
{
// The strip across the top of every Wavy plugin: mark, skins, undo / redo, A/B compare,
// preset arrows + name, menu (version, repo link) and the Mode switch (mono / stereo / ...).
class Toolbar : public juce::Component
{
public:
    Toolbar (juce::UndoManager& undo, juce::AudioProcessorValueTreeState& state, juce::StringArray modeNames,
             juce::RangedAudioParameter* modeParam, juce::String productName, juce::String version);

    std::function<void (int delta)> onPresetStep;   // preset arrows (+1 / -1)
    juce::String presetName { "Default" };

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh() { repaint(); }

    static constexpr int height = 15;

private:
    enum Item { Logo, Skins, Undo, Redo, SlotA, SlotB, Copy, Prev, Preset, Next, Menu, Mode, numItems };
    juce::Rectangle<int> rect (Item) const;
    int hitTest (juce::Point<int>) const;
    void selectSlot (int);
    void copyToOther();

    juce::UndoManager& undo;
    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray modes;
    juce::RangedAudioParameter* modeParam;
    juce::String product, version;
    juce::ValueTree slot[2];
    int active = 0;
    int hot = -1;
    MenuLook menuLook;
};
}
