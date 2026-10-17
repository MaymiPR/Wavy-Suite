// Renders the WVox editor to a PNG:  WVoxSnapshot <out.png> [comp gate gain [mono]]
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include "WVoxEditor.h"

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: WVoxSnapshot out.png [comp gate gain [mono]]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    WVoxProcessor proc;
    if (argc >= 5)
    {
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (wvox::comp, (float) atof (argv[2]));
        set (wvox::gate, (float) atof (argv[3]));
        set (wvox::gain, (float) atof (argv[4]));
    }
    if (argc >= 6) { auto* p = proc.apvts.getParameter (wvox::mode); p->setValueNotifyingHost (1.0f); }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
