// Renders WComp's VU dial alone (Source/common/WavyHybridUI VuMeter, at its place in the WComp editor) to a PNG, for the
// pixel comparison with the reference's filmed frames:   VuRender out.png gr|io <needle degrees> [scale]
#include <juce_gui_basics/juce_gui_basics.h>
#include "WavyHybridUI.h"

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: VuRender out.png gr|io degrees [scale]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    wavy::hybrid::VuMeter vu;
    vu.setBounds (197, 73, 251, 170);                         // WCompEditor's kDial
    vu.pivot = juce::Point<float> (320.7f, 220.9f) - vu.getPosition().toFloat();    // kVuPivot
    vu.setFace (juce::String (argv[2]) == "io" ? wavy::hybrid::VuMeter::Face::io : wavy::hybrid::VuMeter::Face::gr);
    vu.setNeedle ((float) std::atof (argv[3]));
    const float scale = argc > 4 ? (float) std::atof (argv[4]) : 1.0f;
    auto img = vu.createComponentSnapshot (vu.getLocalBounds(), true, scale);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
