// Renders any Wavy plugin's editor to a PNG:  <Plugin>Snapshot out.png [paramID=value ...] [--click=x,y ...] [--scale=2]  (values in real units)
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
    // --click=x,y (editor pixels, in order): a mouse click on whatever component is there, for states only a click sets
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (! a.startsWith ("--click=")) continue;
        const auto xy = juce::StringArray::fromTokens (a.fromFirstOccurrenceOf ("=", false, false), ",", "");
        const juce::Point<float> pt (xy[0].getFloatValue(), xy[1].getFloatValue());
        ed->setVisible (true);                        // (off screen; hit-testing skips hidden components)
        auto* target = ed->getComponentAt (pt.roundToInt());
        if (target == nullptr) continue;
        std::cerr << "click " << pt.toString() << " -> " << typeid (*target).name() << " at " << target->getScreenBounds().toString() << "\n";
        const auto local = target->getLocalPoint (ed, pt);
        const auto now = juce::Time::getCurrentTime();
        auto ev = [&] (juce::ModifierKeys mods)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), local, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     target, target, now, local, now, 1, false);
        };
        target->mouseDown (ev (juce::ModifierKeys::leftButtonModifier));
        target->mouseUp (ev ({}));
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    delete ed;
    return 0;
}
