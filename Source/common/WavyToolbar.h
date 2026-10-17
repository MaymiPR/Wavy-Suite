#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "WavyMenuLook.h"

namespace wavy
{
// The WaveSystem-style toolbar of every Wavy plugin, on the reference's grid (371 x 36 px):
//   row 1: mark | Skins | Mode | Undo | Redo | Prev | Next | A>B copy | Setup A/B | Save | menu
//   row 2: the preset bar across the full width (name, "*" when modified).
// Setup A/B follows the reference: one Setup button toggles between two settings, the copy
// button copies the current setup to the other one.
class Toolbar : public juce::Component
{
public:
    Toolbar (juce::UndoManager& undo, juce::AudioProcessorValueTreeState& state, juce::StringArray modeNames,
             juce::RangedAudioParameter* modeParam, juce::String productName, juce::String version);

    std::function<float()> getWindowScale;            // current window magnification (1 = 100 %)
    std::function<void (float)> setWindowScale;       // choose a magnification

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

    static constexpr int width = 371, height = 36;

private:
    enum Item { About, Skins, Mode, Undo, Redo, Prev, Next, Copy, Setup, Save, Menu, Preset, numItems };
    juce::Rectangle<int> rect (Item) const;
    int hitTest (juce::Point<int>) const;

    void switchSetup();
    void copyToOther();

    // presets: factory "Default" + user presets (.wavypreset files in Documents/Wavy Suite/User Presets/<plugin>)
    juce::File userDir() const;
    juce::Array<juce::File> userPresets() const;
    void loadDefault();
    void loadUser (const juce::File&);
    void step (int delta);
    void saveAs();
    void saveTo (const juce::File&);
    void showPresetMenu();
    void showToolbarMenu();
    std::vector<float> currentValues() const;
    bool isModified() const;

    juce::UndoManager& undo;
    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray modes;
    juce::RangedAudioParameter* modeParam;
    juce::String product, version;
    juce::ValueTree slot[2];
    int active = 0;
    int hot = -1;

    juce::String presetName { "Default" };
    juce::File presetFile;                 // empty for the factory default
    std::vector<float> loadedValues;
    MenuLook menuLook;
};
}
