// Windows check that a Wavy plugin leaves the keyboard with its host:
//   FocusCheck <path to .vst3>
// Hosts the plugin's editor in a window, then sends every window the plugin created the message Windows sends on a
// mouse click (WM_MOUSEACTIVATE). A window that answers MA_NOACTIVATE (3) does not take the keyboard from the host.
// Exit code 0 = all plugin windows refuse activation.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <windows.h>

static BOOL CALLBACK collect (HWND h, LPARAM lp)
{
    reinterpret_cast<std::vector<HWND>*> (lp)->push_back (h);
    return TRUE;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: FocusCheck plugin.vst3\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager fm;
    fm.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, juce::String (argv[1]));
    if (found.isEmpty()) { std::cerr << "no plugin found\n"; return 2; }
    juce::String err;
    auto inst = fm.createPluginInstance (*found[0], 48000.0, 512, err);
    if (inst == nullptr) { std::cerr << "load failed: " << err << "\n"; return 2; }

    juce::DocumentWindow win ("FocusCheck", juce::Colours::black, 0);
    auto* ed = inst->createEditorIfNeeded();
    win.setContentNonOwned (ed, true);
    win.setVisible (true);
    for (int i = 0; i < 20; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

    auto host = (HWND) win.getWindowHandle();
    std::vector<HWND> children;
    EnumChildWindows (host, collect, (LPARAM) &children);
    int refusing = 0, taking = 0;
    for (auto h : children)
    {
        // only judge windows whose class was registered by the plugin's module (not this host's own embedding window)
        char mod[MAX_PATH] = {};
        GetModuleFileNameA ((HMODULE) GetClassLongPtr (h, GCLP_HMODULE), mod, MAX_PATH);
        const bool fromPlugin = juce::String (mod).containsIgnoreCase (".vst3");
        const auto r = SendMessage (h, WM_MOUSEACTIVATE, (WPARAM) host, MAKELPARAM (HTCLIENT, WM_LBUTTONDOWN));
        std::cout << (fromPlugin ? "plugin window" : "host window  ") << " (" << juce::File (mod).getFileName() << "): WM_MOUSEACTIVATE -> "
                  << (long long) r << (r == MA_NOACTIVATE ? " (no activate)" : " (activates)") << "\n";
        if (fromPlugin) (r == MA_NOACTIVATE ? refusing : taking)++;
    }
    win.clearContentComponent();
    delete ed;
    inst = nullptr;
    std::cout << (children.empty() ? "no plugin windows found\n" : taking == 0 ? "PASS: the host keeps the keyboard\n" : "FAIL: a click takes the keyboard\n");
    return (refusing + taking == 0 || taking != 0) ? 1 : 0;
}
