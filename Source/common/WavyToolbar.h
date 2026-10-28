#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "WavyMenuLook.h"
#include "WavyPresets.h"

namespace wavy
{
// The WaveSystem-style toolbar of every Wavy plugin, on the reference's grid (371 x 36 px):
//   row 1: mark | Skins | Mode | Undo | Redo | Prev | Next | A>B copy | Setup A/B | Save | menu
//   row 2: the preset bar across the full width (name, "*" when modified).
// Setup A/B follows the reference: one Setup button toggles between two settings, the copy
// button copies the current setup to the other one.
// Style::Flat is the newer single-row WaveSystem bar (800 x 36 px, e.g. MaxxBass): mark | Mode | undo | redo |
// preset box | prev | next | A>B copy | Setup A/B | save icon | menu; same behaviour.
// Style::Hybrid is the Hybrid-line bar (643 x 19 px, e.g. H-Comp): an orange logo block (the mark and the suite name,
// then Mode) | undo | redo | preset bar | prev | next | A>B copy | Setup A/B | Save | menu.
// Style::Classic is the older grey WaveSystem bar (e.g. the signature line's, 738 x 19 px): the Hybrid bar's boxes and
// colours with a grey logo block.
class Toolbar : public juce::Component
{
public:
    enum class Style { Renaissance, Flat, Hybrid, Classic };
    bool hybridLayout() const { return style == Style::Hybrid || style == Style::Classic; }
    Toolbar (juce::UndoManager& undo, juce::AudioProcessorValueTreeState& state, juce::StringArray modeNames,
             juce::RangedAudioParameter* modeParam, juce::String productName, juce::String version,
             Style style = Style::Renaissance);

    std::function<float()> getWindowScale;            // current window magnification (1 = 100 %)
    std::function<void (float)> setWindowScale;       // choose a magnification

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    void setFactoryPresets (const std::vector<FactoryPreset>* list) { factory = list; }
    // parameters the Default and factory presets leave alone (besides Mode)
    void setKeepOnPresetLoad (std::vector<juce::AudioProcessorParameter*> list) { keepOnLoad = std::move (list); }

    static constexpr int width = 371, height = 36, flatWidth = 800, flatHeight = 36, hybridWidth = 643, hybridHeight = 19;
    const Style style;

private:
    void paintFlat (juce::Graphics&);
    enum Item { About, Skins, Mode, Undo, Redo, Prev, Next, Copy, Setup, Save, Menu, Preset, numItems };
    juce::Rectangle<int> rect (Item) const;
    int hitTest (juce::Point<int>) const;

    void switchSetup();
    void copyToOther();

    // presets: factory "Default" and the plugin's factory list, then user presets (.wavypreset files in
    // Documents/Wavy Suite/User Presets/<plugin>)
    juce::File userDir() const;
    juce::Array<juce::File> userPresets() const;
    void loadDefault();
    void loadFactory (int index);
    void loadUser (const juce::File&);
    void step (int delta);
    void saveAs();
    void saveTo (const juce::File&);
    void showPresetMenu();
    void showToolbarMenu();
    void showAboutMenu();
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

    juce::String presetName { "Default Preset" };
    juce::File presetFile;                 // empty for the factory presets
    const std::vector<FactoryPreset>* factory = nullptr;
    std::vector<juce::AudioProcessorParameter*> keepOnLoad;
    int factoryIndex = -1;                 // -1: Default Preset or a user preset
    std::vector<float> loadedValues;
    MenuLook menuLook;
};
}
