// Records a plug-in editor's meters while audio plays (Windows), to measure meter ballistics the host cannot read.
//   MeterCapture <plugin.vst3> "<sub-plugin name>[@uid]" <out.raw> [paramName=value ...] [in=<mono float32 48 kHz file>]
//   ... [design=<width>] drag=x,y,dx,dy[,ctrl|shift|alt] ...   (drag script instead of the capture, see DragScript)
// Loads the plug-in (a shell's sub-plugin by name), opens its editor, feeds a fixed schedule of 50 Hz tone bursts from
// a real-time paced thread, and grabs the editor's screen area every ~25 ms. out.raw: per frame a double timestamp
// (seconds since the schedule started), then width * height * 3 bytes (RGB); out.raw.txt holds width and height,
// out.raw.pcm the plug-in's output (float32, interleaved stereo, 48 kHz).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <windows.h>
#include <cstdio>
#include <vector>

using namespace juce;

static const double fs = 48000.0;
static const int block = 480;
struct Seg { double secs; double db; bool on; };
static const Seg schedule[] = { { 1.0, 0, false }, { 2.0, -6, true }, { 3.0, 0, false }, { 2.0, -20, true }, { 3.0, 0, false },
                                { 0.3, -6, true }, { 3.0, 0, false } };

class Feeder : public Thread
{
public:
    Feeder (AudioPluginInstance& p, std::vector<float> in) : Thread ("feeder"), plugin (p), input (std::move (in)) {}
    std::atomic<double> startTime { 0.0 };
    std::atomic<bool> done { false };
    std::vector<float> recorded;   // the plug-in's output, interleaved stereo; written next to the frames
    void run() override
    {
        AudioBuffer<float> buf (2, block); MidiBuffer midi;
        sleep (2500);
        const double t0 = Time::getMillisecondCounterHiRes() * 0.001;
        startTime = t0;
        for (size_t i = 0; i + (size_t) block <= input.size() && ! threadShouldExit(); i += (size_t) block)
        {
            const double wait = t0 + (double) i / fs - Time::getMillisecondCounterHiRes() * 0.001;
            if (wait > 0) Thread::sleep ((int) (wait * 1000.0));
            for (int k = 0; k < block; ++k) { buf.setSample (0, k, input[i + (size_t) k]); buf.setSample (1, k, input[i + (size_t) k]); }
            plugin.processBlock (buf, midi);
            for (int k = 0; k < block; ++k) { recorded.push_back (buf.getSample (0, k)); recorded.push_back (buf.getSample (1, k)); }
        }
        done = true;
    }
    // the default input: 50 Hz tone bursts (see schedule)
    static std::vector<float> defaultInput()
    {
        std::vector<float> v;
        for (auto& s : schedule)
        {
            const float a = s.on ? (float) std::pow (10.0, s.db / 20.0) : 0.0f;
            for (int64 i = 0; i < (int64) (s.secs * fs); ++i)
                v.push_back (a * (float) std::sin (2.0 * MathConstants<double>::pi * 50.0 * (double) v.size() / fs));
        }
        return v;
    }
    AudioPluginInstance& plugin;
    std::vector<float> input;
};

class Capture : public Timer
{
public:
    Capture (Component& ed, Feeder& f, const String& path) : editor (ed), feeder (f), out (path) { out.deleteFile(); stream = out.createOutputStream(); }
    void timerCallback() override
    {
        if (feeder.done)
        {
            stopTimer(); stream.reset();
            File (out.getFullPathName() + ".pcm").replaceWithData (feeder.recorded.data(), feeder.recorded.size() * sizeof (float));
            JUCEApplicationBase::quit(); return;
        }
        if (feeder.startTime == 0.0) return;
        auto r = editor.getScreenBounds();
        const double scale = Desktop::getInstance().getDisplays().getDisplayForRect (r)->scale;
        const int x = roundToInt (r.getX() * scale), y = roundToInt (r.getY() * scale), w = roundToInt (r.getWidth() * scale), h = roundToInt (r.getHeight() * scale);
        HDC screen = GetDC (nullptr), mem = CreateCompatibleDC (screen);
        HBITMAP bmp = CreateCompatibleBitmap (screen, w, h);
        auto old = SelectObject (mem, bmp);
        BitBlt (mem, 0, 0, w, h, screen, x, y, SRCCOPY);
        BITMAPINFO bi {}; bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
        const int stride = ((w * 3 + 3) / 4) * 4;
        HeapBlock<uint8> px ((size_t) stride * (size_t) h);
        GetDIBits (mem, bmp, 0, (UINT) h, px.get(), &bi, DIB_RGB_COLORS);
        SelectObject (mem, old); DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
        const double t = Time::getMillisecondCounterHiRes() * 0.001 - feeder.startTime;
        stream->write (&t, sizeof (double));
        HeapBlock<uint8> rgb ((size_t) w * 3);
        for (int row = 0; row < h; ++row)
        {
            const uint8* src = px.get() + (size_t) row * (size_t) stride;
            for (int c = 0; c < w; ++c) { rgb[c * 3] = src[c * 3 + 2]; rgb[c * 3 + 1] = src[c * 3 + 1]; rgb[c * 3 + 2] = src[c * 3]; }
            stream->write (rgb.get(), (size_t) w * 3);
        }
        if (! sizeWritten) { File (out.getFullPathName() + ".txt").replaceWithText (String (w) + " " + String (h)); sizeWritten = true; }
    }
    Component& editor; Feeder& feeder; File out; std::unique_ptr<FileOutputStream> stream; bool sizeWritten = false;
};

// Drag script (drag=x,y,dx,dy[,ctrl|shift|alt] in the editor's design pixels, design=<design width>): moves the real
// mouse over the editor, prints every parameter before and after each drag, and whether the cursor stayed visible
// in the middle of the drag. Used to measure how far a control moves per pixel of mouse movement.
class DragScript : public Thread
{
public:
    DragScript (AudioPluginInstance& p, Component& ed, StringArray d, float designW)
        : Thread ("drag"), plugin (p), editor (ed), drags (std::move (d)), design (designW) {}
    std::atomic<bool> done { false };
    void run() override
    {
        sleep (2000);
        for (auto& d : drags)
        {
            auto t = StringArray::fromTokens (d, ",", "");
            const float x = t[0].getFloatValue(), y = t[1].getFloatValue(), dx = t[2].getFloatValue(), dy = t[3].getFloatValue();
            const String mod = t.size() > 4 ? t[4] : String();
            const int steps = t.size() > 5 ? jmax (1, t[5].getIntValue()) : 50;     // 12 ms apart
            std::printf ("== drag %s\n", d.toRawUTF8());
            dump ("before");
            Point<int> start;
            {
                MessageManagerLock mml;
                const auto pos = editor.getScreenPosition();
                const float s = (float) editor.getWidth() / design;
                const double scale = Desktop::getInstance().getDisplays().getDisplayForRect (editor.getScreenBounds())->scale;
                start = { roundToInt ((pos.x + x * s) * scale), roundToInt ((pos.y + y * s) * scale) };
                stepScale = s * (float) scale;
            }
            const WORD vk = mod == "ctrl" ? VK_CONTROL : mod == "shift" ? VK_SHIFT : mod == "alt" ? VK_MENU : 0;
            SetCursorPos (start.x, start.y); sleep (150);
            if (vk) { keybd_event ((BYTE) vk, 0, 0, 0); sleep (50); }
            mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (80);
            bool hidden = false;
            for (int i = 1; i <= steps; ++i)
            {
                SetCursorPos (start.x + roundToInt (dx * stepScale * (float) i / steps), start.y + roundToInt (dy * stepScale * (float) i / steps));
                sleep (12);
                if (i == steps / 2)
                {
                    CURSORINFO ci { sizeof (CURSORINFO) };
                    GetCursorInfo (&ci);
                    hidden = (ci.flags & CURSOR_SHOWING) == 0 || ci.hCursor == nullptr;
                }
            }
            sleep (80);
            mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); sleep (50);
            if (vk) keybd_event ((BYTE) vk, 0, KEYEVENTF_KEYUP, 0);
            sleep (300);
            std::printf ("cursor during drag: %s\n", hidden ? "HIDDEN" : "visible");
            dump ("after");
        }
        done = true;
    }
    void dump (const char* when)
    {
        std::printf ("%s:", when);
        for (auto* p : plugin.getParameters())
            if (p->getName (64).isNotEmpty() && ! p->getName (64).startsWith ("MIDI") && p->getName (64).length() < 24)
                std::printf (" [%s=%s]", p->getName (64).toRawUTF8(), p->getText (p->getValue(), 32).trim().toRawUTF8());
        std::printf ("\n");
        std::fflush (stdout);
    }
    AudioPluginInstance& plugin;
    Component& editor;
    StringArray drags;
    float design, stepScale = 1.0f;
};

class DragWatch : public Timer
{
public:
    explicit DragWatch (DragScript& d) : script (d) {}
    void timerCallback() override { if (script.done) { stopTimer(); JUCEApplicationBase::quit(); } }
    DragScript& script;
};

class App : public JUCEApplication
{
public:
    const String getApplicationName() override { return "MeterCapture"; }
    const String getApplicationVersion() override { return "1.0"; }
    void initialise (const String& cmd) override
    {
        auto args = StringArray::fromTokens (cmd, true);
        for (auto& a : args) a = a.unquoted();
        if (args.size() < 3) { std::printf ("usage: MeterCapture plugin.vst3 \"name\" out.raw [param=value ...]\n"); quit(); return; }
        fm.addFormat (new VST3PluginFormat());
        // "name@uid": build the description directly; enumerating a shell first leaves JUCE's module cache pointing
        // at the shell's first class, so the wrong sub-plugin would load
        if (args[1].contains ("@"))
        {
            static PluginDescription direct;
            direct.pluginFormatName = "VST3"; direct.fileOrIdentifier = args[0];
            direct.name = args[1].upToFirstOccurrenceOf ("@", false, false);
            direct.uniqueId = args[1].fromFirstOccurrenceOf ("@", false, false).getIntValue();
            direct.numInputChannels = direct.numOutputChannels = 2;
            String e2;
            // the shell changes its class list once it has a host context, so the index JUCE looked up before that
            // points at another sub-plugin; keeping the first instance loaded, the second creation looks it up again
            // in the final list
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                plugin = fm.createPluginInstance (direct, fs, block, e2);
                if (plugin == nullptr || plugin->getName() == direct.name) break;
                std::printf ("got %s, retrying\n", plugin->getName().toRawUTF8());
                keepLoaded.push_back (std::move (plugin));
            }
            if (plugin == nullptr) { std::printf ("load failed: %s\n", e2.toRawUTF8()); quit(); return; }
            std::printf ("loaded %s\n", plugin->getName().toRawUTF8());
            setUp (args);
            return;
        }
        OwnedArray<PluginDescription> types;
        for (auto* f : fm.getFormats()) if (f->getName() == "VST3") f->findAllTypesForFile (types, args[0]);
        PluginDescription* d = nullptr;
        for (auto* t : types)
        {
            if (args[1] == "--list") std::printf ("%s | uid %d | id %s\n", t->name.toRawUTF8(), t->uniqueId, t->createIdentifierString().toRawUTF8());
            if (t->name == args[1]) d = t;
        }
        if (d != nullptr) std::printf ("loading %s (uid %d)\n", d->name.toRawUTF8(), d->uniqueId);
        if (d == nullptr) { std::printf ("plug-in not found\n"); quit(); return; }
        String err;
        plugin = fm.createPluginInstance (*d, fs, block, err);
        if (plugin == nullptr) { std::printf ("load failed: %s\n", err.toRawUTF8()); quit(); return; }
        std::printf ("loaded %s\n", plugin->getName().toRawUTF8());
        setUp (args);
    }
    void setUp (const StringArray& args)
    {
        plugin->setPlayConfigDetails (2, 2, fs, block);
        plugin->prepareToPlay (fs, block);
        for (int i = 3; i < args.size(); ++i)
        {
            const auto name = args[i].upToFirstOccurrenceOf ("=", false, false);
            const float v = args[i].fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            for (auto* p : plugin->getParameters())
                if (p->getName (64) == name) p->setValueNotifyingHost (v);
        }
        window = std::make_unique<DocumentWindow> ("MeterCapture", Colours::black, DocumentWindow::closeButton);
        window->setUsingNativeTitleBar (true);
        window->setContentNonOwned (plugin->createEditorIfNeeded(), true);
        window->setTopLeftPosition (100, 100);
        window->setVisible (true);
        window->setAlwaysOnTop (true);
        StringArray drags; float design = 800.0f;
        for (int i = 3; i < args.size(); ++i)
        {
            if (args[i].startsWith ("drag=")) drags.add (args[i].substring (5));
            if (args[i].startsWith ("design=")) design = args[i].substring (7).getFloatValue();
        }
        if (! drags.isEmpty())
        {
            dragScript = std::make_unique<DragScript> (*plugin, *window->getContentComponent(), drags, design);
            dragWatch = std::make_unique<DragWatch> (*dragScript);
            dragScript->startThread();
            dragWatch->startTimer (100);
            return;
        }
        std::vector<float> in = Feeder::defaultInput();
        for (int i = 3; i < args.size(); ++i)
            if (args[i].startsWith ("in="))      // a mono float32 48 kHz file instead of the default schedule
            {
                MemoryBlock mb; File (args[i].substring (3)).loadFileAsData (mb);
                in.assign ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / sizeof (float));
            }
        feeder = std::make_unique<Feeder> (*plugin, std::move (in));
        capture = std::make_unique<Capture> (*window->getContentComponent(), *feeder, args[2]);
        feeder->startThread (Thread::Priority::high);
        capture->startTimer (25);
    }
    void shutdown() override
    {
        if (dragScript) dragScript->stopThread (3000);
        if (feeder) feeder->stopThread (3000);
        capture.reset();
        if (window) window->setContentNonOwned (nullptr, false);
        window.reset();
        plugin.reset();
    }
private:
    AudioPluginFormatManager fm;
    std::vector<std::unique_ptr<AudioPluginInstance>> keepLoaded;
    std::unique_ptr<AudioPluginInstance> plugin;
    std::unique_ptr<DocumentWindow> window;
    std::unique_ptr<Feeder> feeder;
    std::unique_ptr<DragScript> dragScript;
    std::unique_ptr<DragWatch> dragWatch;
    std::unique_ptr<Capture> capture;
};

START_JUCE_APPLICATION (App)
