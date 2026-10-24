// Renders any Wavy plugin's editor to a PNG:  <Plugin>Snapshot out.png [paramID=value ...] [--scale=2]  (values in real units)
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: Snapshot out.png [paramID=value ...]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    float scale = 1.0f;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.startsWith ("--scale=")) { scale = a.fromFirstOccurrenceOf ("=", false, false).getFloatValue(); continue; }
        for (auto* p : proc->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == a.upToFirstOccurrenceOf ("=", false, false))
                    r->setValueNotifyingHost (r->convertTo0to1 (a.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    auto* ed = proc->createEditorIfNeeded();          // owned by us; its destructor detaches it from the processor
    if (juce::String (argc > 2 ? argv[argc - 1] : "") == "--bench")   // each visible slider: the time to repaint it as it turns
    {
        std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
        {
            for (auto* k : c.getChildren())
            {
                if (auto* s = dynamic_cast<juce::Slider*> (k); s != nullptr && s->isVisible() && s->getWidth() > 20)
                {
                    s->createComponentSnapshot (s->getLocalBounds(), true, scale);
                    const double t0 = juce::Time::getMillisecondCounterHiRes();
                    for (int n = 0; n < 40; ++n)
                    {
                        s->setValue (s->proportionOfLengthToValue ((double) n / 40.0), juce::dontSendNotification);
                        s->createComponentSnapshot (s->getLocalBounds(), true, scale);
                    }
                    std::cout << "slider at " << s->getBounds().toString() << ": "
                              << (juce::Time::getMillisecondCounterHiRes() - t0) / 40.0 << " ms per repaint\n";
                }
                visit (*k);
            }
        };
        visit (*ed);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    delete ed;
    return 0;
}
