// Renders any Wavy plugin's editor to a PNG:  <Plugin>Snapshot out.png [paramID=value ...]  (values in real units)
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: Snapshot out.png [paramID=value ...]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        for (auto* p : proc->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == a.upToFirstOccurrenceOf ("=", false, false))
                    r->setValueNotifyingHost (r->convertTo0to1 (a.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    auto* ed = proc->createEditorIfNeeded();          // owned by us; its destructor detaches it from the processor
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    delete ed;
    return 0;
}
