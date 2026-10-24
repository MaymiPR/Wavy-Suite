// Records a plug-in editor's meters while audio plays (Windows), to measure meter ballistics the host cannot read.
//   MeterCapture <plugin.vst3> "<sub-plugin name>[@uid]" <out.raw> [paramName=value ...] [in=<mono float32 48 kHz file>]
//   ... [design=<width>] drag=x,y,dx,dy[,ctrl|shift|alt|wheel|wheelctrl|midctrl|ctrlup] ...   (drag script instead of the capture, see DragScript)
//   ... gl=1 [crop=x0,y0,x1,y1]   (read the editor's OpenGL frames back instead of grabbing the screen, see GlCapture)
//   ... hostwin=1   (the editor inside a plain Win32 window that keeps the keyboard focus, as FL hosts it)
//   ... away=0   (the drag script leaves the mouse over the editor between drags)
//   paramName=value sets the normalised value, paramName:=text the value the parameter's text parses to.
// Loads the plug-in (a shell's sub-plugin by name), opens its editor, feeds a fixed schedule of 50 Hz tone bursts from
// a real-time paced thread, and grabs the editor's screen area every ~25 ms. out.raw: per frame a double timestamp
// (seconds since the schedule started), then width * height * 3 bytes (RGB); out.raw.txt holds width and height,
// out.raw.pcm the plug-in's output (float32, interleaved stereo, 48 kHz).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <mutex>
#include <vector>

using namespace juce;

static const double fs = 48000.0;
static const int block = 480;
struct Seg { double secs; double db; bool on; };
static const Seg schedule[] = { { 1.0, 0, false }, { 2.0, -6, true }, { 3.0, 0, false }, { 2.0, -20, true }, { 3.0, 0, false },
                                { 0.3, -6, true }, { 3.0, 0, false } };

// a transport that plays from the first block on, at the feeder's position: Waves editors take their meter values
// from the processing only while the host's transport advances (with pedalboard's empty play head the needle stays put)
class FeederPlayHead : public AudioPlayHead
{
public:
    std::atomic<int64> samples { 0 };
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setTimeInSamples (samples.load()); p.setTimeInSeconds ((double) samples.load() / fs);
        p.setBpm (120.0); p.setTimeSignature (TimeSignature { 4, 4 });
        p.setPpqPosition ((double) samples.load() / fs * 2.0);
        p.setIsPlaying (true);
        return p;
    }
};

class Feeder : public Thread
{
public:
    Feeder (AudioPluginInstance& p, std::vector<float> in) : Thread ("feeder"), plugin (p), input (std::move (in)) { plugin.setPlayHead (&playHead); }
    ~Feeder() override { plugin.setPlayHead (nullptr); }
    FeederPlayHead playHead;
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
            playHead.samples = (int64) (i + (size_t) block);
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
        if (feeder.startTime == 0.0 || gl) return;
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
    bool gl = false;     // GlCapture writes the frames; this only ends the run
};

// Reads a Waves editor's frames back from OpenGL: outside FL its frames never reach the window (it stays white, in
// this host and in pedalboard) although it renders, so the shell's import of gdi32!SwapBuffers is pointed at a hook
// that reads the back buffer (crop, in editor pixels from the top left) just before the swap. out.raw: per frame two
// doubles (the audio time processed so far and the wall time since the schedule started, s), then the crop's
// width * height * 3 bytes (RGB, top row first); out.raw.txt holds the crop's width and height.
namespace GlCapture
{
    using SwapFn = BOOL (WINAPI*) (HDC);
    using ReadPixelsFn = void (WINAPI*) (int, int, int, int, unsigned, unsigned, void*);
    using GetIntegervFn = void (WINAPI*) (unsigned, int*);
    using UintFn = void (WINAPI*) (unsigned);
    using PixelStoreFn = void (WINAPI*) (unsigned, int);
    static SwapFn realSwap = nullptr;
    static ReadPixelsFn readPixels; static GetIntegervFn getIntegerv; static UintFn readBuffer; static PixelStoreFn pixelStore;
    static std::mutex lock;
    static std::unique_ptr<FileOutputStream> stream;
    static File out;
    static Feeder* feeder = nullptr;
    static int crop[4] = { 0, 0, 0, 0 };          // x0, y0, x1, y1; zero: the whole viewport
    static bool sizeWritten = false;
    static std::vector<uint8> px;

    static void readBack()
    {
        std::lock_guard<std::mutex> g (lock);
        if (feeder == nullptr || feeder->startTime == 0.0 || feeder->done || stream == nullptr) return;
        int vp[4] {}; getIntegerv (0x0BA2, vp);                          // GL_VIEWPORT
        const int x0 = crop[2] > 0 ? crop[0] : 0, y0 = crop[2] > 0 ? crop[1] : 0;
        const int x1 = crop[2] > 0 ? jmin (crop[2], vp[2]) : vp[2], y1 = crop[2] > 0 ? jmin (crop[3], vp[3]) : vp[3];
        const int w = x1 - x0, h = y1 - y0;
        if (w <= 0 || h <= 0) return;
        px.resize ((size_t) w * (size_t) h * 3);
        readBuffer (0x0405); pixelStore (0x0D05, 1);                    // GL_BACK, GL_PACK_ALIGNMENT
        readPixels (x0, vp[3] - y1, w, h, 0x1907, 0x1401, px.data());  // GL_RGB, GL_UNSIGNED_BYTE; bottom row first
        const double t[2] = { (double) feeder->playHead.samples.load() / fs, Time::getMillisecondCounterHiRes() * 0.001 - feeder->startTime };
        stream->write (t, sizeof (t));
        for (int row = h - 1; row >= 0; --row) stream->write (px.data() + (size_t) row * (size_t) w * 3, (size_t) w * 3);
        if (! sizeWritten) { File (out.getFullPathName() + ".txt").replaceWithText (String (w) + " " + String (h)); sizeWritten = true; }
    }
    static BOOL WINAPI hookedSwap (HDC dc) { readBack(); return realSwap (dc); }

    // points every loaded WaveShell module's SwapBuffers import at the hook
    static int install()
    {
        HMODULE gl = LoadLibraryA ("opengl32.dll");
        readPixels = (ReadPixelsFn) (void*) GetProcAddress (gl, "glReadPixels");
        getIntegerv = (GetIntegervFn) (void*) GetProcAddress (gl, "glGetIntegerv");
        readBuffer = (UintFn) (void*) GetProcAddress (gl, "glReadBuffer");
        pixelStore = (PixelStoreFn) (void*) GetProcAddress (gl, "glPixelStorei");
        int patched = 0;
        HANDLE snap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, GetCurrentProcessId());
        MODULEENTRY32W me { sizeof (MODULEENTRY32W) };
        for (BOOL ok = Module32FirstW (snap, &me); ok; ok = Module32NextW (snap, &me))
        {
            if (! String (me.szModule).startsWithIgnoreCase ("WaveShell")) continue;
            auto* base = (uint8*) me.modBaseAddr;
            auto* nt = (IMAGE_NT_HEADERS*) (base + ((IMAGE_DOS_HEADER*) base)->e_lfanew);
            auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            for (auto* d = (IMAGE_IMPORT_DESCRIPTOR*) (base + dir.VirtualAddress); d->Name != 0; ++d)
            {
                if (! String ((const char*) (base + d->Name)).equalsIgnoreCase ("gdi32.dll")) continue;
                auto* names = (IMAGE_THUNK_DATA*) (base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
                auto* slots = (IMAGE_THUNK_DATA*) (base + d->FirstThunk);
                for (; names->u1.AddressOfData != 0; ++names, ++slots)
                {
                    if (IMAGE_SNAP_BY_ORDINAL (names->u1.Ordinal)) continue;
                    if (strcmp ((const char*) ((IMAGE_IMPORT_BY_NAME*) (base + names->u1.AddressOfData))->Name, "SwapBuffers") != 0) continue;
                    DWORD old = 0;
                    VirtualProtect (&slots->u1.Function, sizeof (void*), PAGE_READWRITE, &old);
                    realSwap = (SwapFn) slots->u1.Function;
                    slots->u1.Function = (ULONG_PTR) &hookedSwap;
                    VirtualProtect (&slots->u1.Function, sizeof (void*), old, &old);
                    ++patched;
                }
            }
        }
        CloseHandle (snap);
        return patched;
    }
}

// grabs the component's screen area (also native child windows, e.g. a Waves editor) into a PNG
static void grabPng (Component& c, const File& f)
{
    juce::Rectangle<int> r; double scale = 1.0;
    {
        MessageManagerLock mml;
        r = c.getScreenBounds();
        scale = Desktop::getInstance().getDisplays().getDisplayForRect (r)->scale;
    }
    const int x = roundToInt (r.getX() * scale), y = roundToInt (r.getY() * scale), w = roundToInt (r.getWidth() * scale), h = roundToInt (r.getHeight() * scale);
    HDC screen = GetDC (nullptr), mem = CreateCompatibleDC (screen);
    HBITMAP bmp = CreateCompatibleBitmap (screen, w, h);
    auto old = SelectObject (mem, bmp);
    BitBlt (mem, 0, 0, w, h, screen, x, y, SRCCOPY);
    BITMAPINFO bi {}; bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    Image img (Image::ARGB, w, h, true);
    {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        HeapBlock<uint8> px ((size_t) w * (size_t) h * 4);
        GetDIBits (mem, bmp, 0, (UINT) h, px.get(), &bi, DIB_RGB_COLORS);
        for (int row = 0; row < h; ++row)
            for (int col = 0; col < w; ++col)
            {
                const uint8* q = px.get() + ((size_t) row * (size_t) w + (size_t) col) * 4;
                bd.setPixelColour (col, row, Colour (q[2], q[1], q[0]));
            }
    }
    SelectObject (mem, old); DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat().writeImageToStream (img, os);
}

// Drag script (drag=x,y,dx,dy[,ctrl|shift|alt] in the editor's design pixels, design=<design width>): moves the real
// mouse over the editor, prints every parameter before and after each drag, and whether the cursor stayed visible
// in the middle of the drag. Used to measure how far a control moves per pixel of mouse movement.
class DragScript : public Thread
{
public:
    DragScript (AudioPluginInstance& p, Component& ed, StringArray d, float designW, File shotBase)
        : Thread ("drag"), plugin (p), editor (ed), drags (std::move (d)), design (designW), shots (shotBase) {}
    std::atomic<bool> done { false };
    bool moveAway = true;                             // away=0: the mouse stays over the editor between drags
    void run() override
    {
        sleep (2000);
        int n = 0;
        auto shot = [this, &n] (const char* when)
        {
            if (shots != File()) grabPng (editor, shots.getSiblingFile (shots.getFileNameWithoutExtension() + "_" + String (n) + "_" + when + ".png"));
        };
        for (auto& d : drags)
        {
            ++n;
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
            // midctrl: Ctrl pressed half way through the drag; ctrlup: Ctrl held from the start, released half way
            const bool midCtrl = mod == "midctrl", ctrlUp = mod == "ctrlup";
            const WORD vk = mod == "ctrl" || ctrlUp ? VK_CONTROL : mod == "shift" ? VK_SHIFT : mod == "alt" ? VK_MENU : 0;
            SetCursorPos (start.x, start.y); sleep (150);
            if (mod.startsWith ("wheel"))                         // dx mouse-wheel notches (+ up), one every 80 ms; "wheelctrl": Ctrl held
            {
                const bool ctl = mod == "wheelctrl";
                if (ctl) { keybd_event (VK_CONTROL, 0, 0, 0); sleep (50); }
                for (int i = 0; i < (int) std::abs (dx); ++i)
                {
                    mouse_event (MOUSEEVENTF_WHEEL, 0, 0, (DWORD) (dx > 0 ? WHEEL_DELTA : -WHEEL_DELTA), 0);
                    sleep (80);
                }
                if (ctl) keybd_event (VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                sleep (300);
                dump ("after"); continue;
            }
            if (mod == "hover")                                   // no click: what does hovering show?
            {
                sleep (400); shot ("hover");
                std::printf ("hover\n"); continue;
            }
            if (mod == "click" || mod == "dbl" || mod.endsWith ("click"))   // click / double-click (+ modifier), then type t[6] + Enter
            {
                const WORD kv = mod.startsWith ("ctrl") ? VK_CONTROL : mod.startsWith ("alt") ? VK_MENU : mod.startsWith ("shift") ? VK_SHIFT : 0;
                if (kv) { keybd_event ((BYTE) kv, 0, 0, 0); sleep (50); }
                for (int c = 0; c < (mod == "dbl" ? 2 : 1); ++c)
                {
                    mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (30);
                    mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); sleep (60);
                }
                if (kv) keybd_event ((BYTE) kv, 0, KEYEVENTF_KEYUP, 0);
                sleep (500); shot ("edit");
                dump ("clicked");
                if (t.size() > 6)
                {
                    for (auto ch : t[6])
                    {
                        const SHORT k = VkKeyScanA ((char) ch);
                        keybd_event ((BYTE) (k & 0xff), 0, 0, 0); sleep (20); keybd_event ((BYTE) (k & 0xff), 0, KEYEVENTF_KEYUP, 0); sleep (40);
                    }
                    sleep (200); shot ("typed");
                    keybd_event (VK_RETURN, 0, 0, 0); sleep (20); keybd_event (VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                }
                sleep (500); shot ("after");
                dump ("after"); continue;
            }
            if (vk) { keybd_event ((BYTE) vk, 0, 0, 0); sleep (50); }
            mouse_event (MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); sleep (80);
            shot ("down");
            bool hidden = false;
            for (int i = 1; i <= steps; ++i)
            {
                SetCursorPos (start.x + roundToInt (dx * stepScale * (float) i / steps), start.y + roundToInt (dy * stepScale * (float) i / steps));
                sleep (12);
                if (i == steps / 2 && midCtrl) { keybd_event (VK_CONTROL, 0, 0, 0); sleep (30); }
                if (i == steps / 2 && ctrlUp) { keybd_event (VK_CONTROL, 0, KEYEVENTF_KEYUP, 0); sleep (30); }
                if (i == steps / 2)
                {
                    shot ("mid");
                    CURSORINFO ci { sizeof (CURSORINFO) };
                    GetCursorInfo (&ci);
                    hidden = (ci.flags & CURSOR_SHOWING) == 0 || ci.hCursor == nullptr;
                }
            }
            sleep (80);
            mouse_event (MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); sleep (50);
            if (vk && ! ctrlUp) keybd_event ((BYTE) vk, 0, KEYEVENTF_KEYUP, 0);
            if (midCtrl) keybd_event (VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            sleep (300);
            shot ("after");
            {
                POINT cp; GetCursorPos (&cp);                     // where the cursor is put back, in design pixels
                std::printf ("cursor after release: %.1f,%.1f (drag ended at %.1f,%.1f)\n", x + (cp.x - start.x) / stepScale,
                             y + (cp.y - start.y) / stepScale, x + dx, y + dy);
            }
            if (moveAway)
            {
                SetCursorPos (start.x + 400, start.y + 300); sleep (600);   // mouse away: does the highlight stay?
                shot ("away");
            }
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
    File shots;
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
        for (int io = 0; io < 2; ++io)                       // bus layout (main / side-chain)
            for (int b = 0; b < plugin->getBusCount (io == 0); ++b)
                if (auto* bus = plugin->getBus (io == 0, b))
                    std::printf ("  %s bus %d '%s': %d ch, enabled %d\n", io == 0 ? "input" : "output", b, bus->getName().toRawUTF8(),
                                 bus->getNumberOfChannels(), (int) bus->isEnabled());
        plugin->setPlayConfigDetails (2, 2, fs, block);
        plugin->prepareToPlay (fs, block);
        for (int i = 3; i < args.size(); ++i)
        {
            const bool text = args[i].contains (":=");
            const auto name = args[i].upToFirstOccurrenceOf (text ? ":=" : "=", false, false);
            const auto value = args[i].fromFirstOccurrenceOf (text ? ":=" : "=", false, false);
            for (auto* p : plugin->getParameters())
                if (p->getName (64) == name)
                {
                    p->setValueNotifyingHost (text ? p->getValueForText (value) : value.getFloatValue());
                    std::printf ("  %s = %s\n", name.toRawUTF8(), p->getText (p->getValue(), 32).toRawUTF8());
                }
        }
        for (auto* p : plugin->getParameters())
            if (p->getName (64).isNotEmpty() && ! p->getName (64).startsWith ("MIDI") && p->getName (64).length() < 24)
                std::printf (" [%s=%s]", p->getName (64).toRawUTF8(), p->getText (p->getValue(), 32).trim().toRawUTF8());
        std::printf ("\n");
        std::fflush (stdout);
        const bool gl = args.contains ("gl=1");
        if (gl)
        {
            for (auto& a : args)
                if (a.startsWith ("crop="))
                {
                    auto c = StringArray::fromTokens (a.substring (5), ",", "");
                    for (int k = 0; k < 4; ++k) GlCapture::crop[k] = c[k].getIntValue();
                }
            std::printf ("SwapBuffers imports patched: %d\n", GlCapture::install());
        }
        if (args.contains ("hostwin=1"))
        {
            // as FL hosts it: the editor a child of the host's own (non-JUCE) window, which keeps the keyboard focus,
            // so the plug-in sees no key messages (a modifier pressed during a drag reaches it only by its key state)
            auto* ed = plugin->createEditorIfNeeded();
            WNDCLASSW wc {};
            wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW (nullptr); wc.lpszClassName = L"MeterCaptureHost";
            RegisterClassW (&wc);
            RECT r { 0, 0, ed->getWidth(), ed->getHeight() };
            AdjustWindowRect (&r, WS_OVERLAPPEDWINDOW, FALSE);
            hostHwnd = CreateWindowExW (WS_EX_TOPMOST, L"MeterCaptureHost", L"MeterCapture host", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                        100, 100, r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
            ed->setTopLeftPosition (0, 0);
            ed->addToDesktop (0, hostHwnd);
            ed->setVisible (true);
            SetFocus (hostHwnd);
            editorComp = ed;
        }
        else
        {
            window = std::make_unique<DocumentWindow> ("MeterCapture", Colours::black, DocumentWindow::closeButton);
            window->setUsingNativeTitleBar (true);
            window->setContentNonOwned (plugin->createEditorIfNeeded(), true);
            window->setTopLeftPosition (100, 100);
            window->setVisible (true);
            window->setAlwaysOnTop (true);
            editorComp = window->getContentComponent();
        }
        StringArray drags; float design = 800.0f;
        for (int i = 3; i < args.size(); ++i)
        {
            if (args[i].startsWith ("drag=")) drags.add (args[i].substring (5));
            if (args[i].startsWith ("design=")) design = args[i].substring (7).getFloatValue();
        }
        if (! drags.isEmpty())
        {
            dragScript = std::make_unique<DragScript> (*plugin, *editorComp, drags, design, File (args[2]));
            dragScript->moveAway = ! args.contains ("away=0");
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
        capture = std::make_unique<Capture> (*editorComp, *feeder, args[2]);
        capture->gl = gl;
        if (gl)
        {
            std::lock_guard<std::mutex> g (GlCapture::lock);
            capture->stream.reset();
            GlCapture::out = File (args[2]); GlCapture::out.deleteFile();
            GlCapture::stream = GlCapture::out.createOutputStream();
            GlCapture::feeder = feeder.get();
        }
        feeder->startThread (Thread::Priority::high);
        capture->startTimer (25);
    }
    void shutdown() override
    {
        if (dragScript) dragScript->stopThread (3000);
        if (feeder) feeder->stopThread (3000);
        {
            std::lock_guard<std::mutex> g (GlCapture::lock);
            GlCapture::feeder = nullptr; GlCapture::stream.reset();
        }
        capture.reset();
        feeder.reset();                                  // before the plug-in: it clears the play head it set
        if (window) window->setContentNonOwned (nullptr, false);
        window.reset();
        if (hostHwnd != nullptr) { if (editorComp) editorComp->removeFromDesktop(); DestroyWindow (hostHwnd); }
        plugin.reset();
    }
private:
    AudioPluginFormatManager fm;
    std::vector<std::unique_ptr<AudioPluginInstance>> keepLoaded;
    std::unique_ptr<AudioPluginInstance> plugin;
    std::unique_ptr<DocumentWindow> window;
    HWND hostHwnd = nullptr;                          // hostwin=1: the plain Win32 window the editor sits in
    Component* editorComp = nullptr;
    std::unique_ptr<Feeder> feeder;
    std::unique_ptr<DragScript> dragScript;
    std::unique_ptr<DragWatch> dragWatch;
    std::unique_ptr<Capture> capture;
};

START_JUCE_APPLICATION (App)
