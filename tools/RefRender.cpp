// Renders audio offline through a VST3 (a shell's sub-plugin by name), with what pedalboard cannot give a plug-in:
// an enabled side-chain bus and a host tempo.
//   RefRender <plugin.vst3> "<sub-plugin name>[@uid]" <in.f32> <out.f32> [sc=<sc.f32>] [bpm=<tempo>] [fs=<rate>]
//             [block=<samples>] [preroll=<seconds>] [name=text | name:=normalised ...] [at=<seconds>:name=text ...]
// preroll: that much silence is rendered first (not written), the parameters set after its first block: some plug-ins
// (H-Delay) show but do not process values set before their first block. at=: a parameter change at that time of the
// input (sample exact; the block is split there; before 0: in the pre-roll, at a block's start).
// in.f32 / sc.f32: interleaved stereo float32 at fs; out.f32: the main output, interleaved stereo, the plug-in's latency
// removed. Parameters are set by their display text (name=text, converted by the plug-in) or a normalised value
// (name:=0.25). "--list" as the name lists the shell's sub-plugins.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <vector>

using namespace juce;

struct FixedHead : public AudioPlayHead
{
    double bpm = 0.0, fs = 48000.0;
    int64 pos = 0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        if (bpm > 0.0) { p.setBpm (bpm); p.setTimeSignature (TimeSignature { 4, 4 }); }
        p.setTimeInSamples (pos);
        p.setTimeInSeconds ((double) pos / fs);
        if (bpm > 0.0) p.setPpqPosition ((double) pos / fs * bpm / 60.0);
        p.setIsPlaying (true);
        return p;
    }
};

static std::vector<float> readF32 (const String& path)
{
    MemoryBlock mb;
    File (File::getCurrentWorkingDirectory().getChildFile (path)).loadFileAsData (mb);
    std::vector<float> v (mb.getSize() / sizeof (float));
    std::memcpy (v.data(), mb.getData(), v.size() * sizeof (float));
    return v;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (String (argv[i]).unquoted());
    if (args.size() < 2) { std::printf ("usage: RefRender plugin.vst3 \"name[@uid]\" in.f32 out.f32 [sc=..] [bpm=..] [fs=..] [param=text ...]\n"); return 2; }

    double fs = 48000.0, bpm = 0.0, preroll = 0.0;
    int block = 512;
    String scPath;
    for (int i = 4; i < args.size(); ++i)
    {
        if (args[i].startsWith ("fs=")) fs = args[i].substring (3).getDoubleValue();
        else if (args[i].startsWith ("bpm=")) bpm = args[i].substring (4).getDoubleValue();
        else if (args[i].startsWith ("block=")) block = args[i].substring (6).getIntValue();
        else if (args[i].startsWith ("sc=")) scPath = args[i].substring (3);
        else if (args[i].startsWith ("preroll=")) preroll = args[i].substring (8).getDoubleValue();
    }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    std::unique_ptr<AudioPluginInstance> plugin;
    std::vector<std::unique_ptr<AudioPluginInstance>> keepLoaded;
    String err;
    if (args[1].contains ("@"))
    {
        // as MeterCapture: a shell changes its class list once it has a host context, so the first instance may be
        // another sub-plugin; keeping it loaded, the next creation finds the right one
        PluginDescription d;
        d.pluginFormatName = "VST3"; d.fileOrIdentifier = args[0];
        d.name = args[1].upToFirstOccurrenceOf ("@", false, false);
        d.uniqueId = args[1].fromFirstOccurrenceOf ("@", false, false).getIntValue();
        d.numInputChannels = d.numOutputChannels = 2;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            plugin = fm.createPluginInstance (d, fs, block, err);
            if (plugin == nullptr || plugin->getName() == d.name) break;
            keepLoaded.push_back (std::move (plugin));
        }
    }
    else
    {
        OwnedArray<PluginDescription> types;
        VST3PluginFormat vst3;
        vst3.findAllTypesForFile (types, args[0]);
        for (auto* t : types)
        {
            if (args[1] == "--list") std::printf ("%s@%d\n", t->name.toRawUTF8(), t->uniqueId);
            else if (t->name == args[1]) { plugin = fm.createPluginInstance (*t, fs, block, err); break; }
        }
        if (args[1] == "--list") return 0;
    }
    if (plugin == nullptr || (args[1].contains ("@") && plugin->getName() != args[1].upToFirstOccurrenceOf ("@", false, false)))
    {
        std::printf ("load failed: %s\n", err.toRawUTF8());
        return 1;
    }

    // main stereo in / out, the side-chain bus (if any) enabled in stereo
    auto layout = plugin->getBusesLayout();
    for (int b = 1; b < layout.inputBuses.size(); ++b) layout.inputBuses.getReference (b) = AudioChannelSet::stereo();
    if (! plugin->setBusesLayout (layout)) std::printf ("could not enable the side-chain bus\n");
    const int nIn = plugin->getTotalNumInputChannels(), nOut = plugin->getTotalNumOutputChannels();
    plugin->setPlayConfigDetails (nIn, nOut, fs, block);
    FixedHead head;
    head.bpm = bpm; head.fs = fs;
    plugin->setPlayHead (&head);
    plugin->prepareToPlay (fs, block);
    auto setParam = [&plugin] (const String& a)
    {
        const bool normalised = a.contains (":=");
        const auto name = a.upToFirstOccurrenceOf (normalised ? ":=" : "=", false, false);
        const auto val = a.fromFirstOccurrenceOf (normalised ? ":=" : "=", false, false);
        bool found = false;
        for (auto* p : plugin->getParameters())
            if (p->getName (64).equalsIgnoreCase (name))
            {
                const float v = normalised ? val.getFloatValue() : p->getValueForText (val);
                p->setValueNotifyingHost (v);
                std::printf ("  %s = %s (%.6f)\n", name.toRawUTF8(), p->getText (p->getValue(), 64).toRawUTF8(), p->getValue());
                found = true;
            }
        if (! found) std::printf ("  no parameter '%s'\n", name.toRawUTF8());
    };
    StringArray settings;
    std::vector<std::pair<int, String>> changes;                  // (sample of the input, name=value)
    for (int i = 4; i < args.size(); ++i)
    {
        const auto& a = args[i];
        if (a.startsWith ("fs=") || a.startsWith ("bpm=") || a.startsWith ("block=") || a.startsWith ("sc=") || a.startsWith ("preroll=")) continue;
        if (a.startsWith ("at="))
            changes.push_back ({ (int) std::llround (a.substring (3).upToFirstOccurrenceOf (":", false, false).getDoubleValue() * fs),
                                 a.fromFirstOccurrenceOf (":", false, false) });
        else settings.add (a);
    }
    std::sort (changes.begin(), changes.end(), [] (auto& x, auto& y) { return x.first < y.first; });
    AudioBuffer<float> pre (jmax (plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels()), block);
    MidiBuffer preMidi;
    const int preN = preroll > 0.0 ? jmax (2 * block, (int) std::llround (preroll * fs)) : 0;
    size_t nextChange = 0;
    for (int pos = 0; pos < preN; pos += block)                   // a first block of silence, the settings, the rest
    {
        if (pos == block) for (auto& a : settings) setParam (a);
        while (pos >= block && nextChange < changes.size() && changes[nextChange].first <= pos - preN)   // at= < 0
            setParam (changes[nextChange++].second);
        pre.clear();
        head.pos = pos - preN;
        plugin->processBlock (pre, preMidi);
    }
    if (preN == 0) for (auto& a : settings) setParam (a);

    const auto in = readF32 (args[2]);
    const auto sc = scPath.isNotEmpty() ? readF32 (scPath) : std::vector<float>();
    const int latency = plugin->getLatencySamples();
    const int n = (int) (in.size() / 2);
    std::printf ("in %d ch, out %d ch, latency %d, %d frames, side-chain %s, bpm %g\n", nIn, nOut, latency, n,
                 sc.empty() ? "off" : "fed", bpm);
    std::vector<float> out ((size_t) n * 2, 0.0f);
    AudioBuffer<float> buf (jmax (nIn, nOut), block);
    MidiBuffer midi;
    for (int pos = 0; pos < n + latency; )
    {
        while (nextChange < changes.size() && changes[nextChange].first <= pos) setParam (changes[nextChange++].second);
        // a block up to the next change
        const int len = nextChange < changes.size() ? jmin (block, changes[nextChange].first - pos) : block;
        buf.setSize (buf.getNumChannels(), len, false, false, true);
        buf.clear();
        for (int i = 0; i < len; ++i)
        {
            const int k = pos + i;
            if (k < n)
            {
                buf.setSample (0, i, in[(size_t) k * 2]);
                if (nIn > 1) buf.setSample (1, i, in[(size_t) k * 2 + 1]);
                if (nIn > 3 && (size_t) k * 2 + 1 < sc.size()) { buf.setSample (2, i, sc[(size_t) k * 2]); buf.setSample (3, i, sc[(size_t) k * 2 + 1]); }
            }
        }
        head.pos = pos;
        plugin->processBlock (buf, midi);
        for (int i = 0; i < len; ++i)
        {
            const int k = pos + i - latency;
            if (k >= 0 && k < n)
            {
                out[(size_t) k * 2] = buf.getSample (0, i);
                out[(size_t) k * 2 + 1] = buf.getSample (nOut > 1 ? 1 : 0, i);
            }
        }
        pos += len;
    }
    plugin->releaseResources();
    File f (File::getCurrentWorkingDirectory().getChildFile (args[3]));
    f.deleteFile();
    FileOutputStream os (f);
    os.write (out.data(), out.size() * sizeof (float));
    return 0;
}
