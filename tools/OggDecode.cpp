// Decodes an Ogg Vorbis stream to raw interleaved float32, for building test material from Ogg-coded samples (such
// as WAV files whose data chunk is an Ogg stream, format 0x674f):
//   OggDecode <in.ogg> <out.f32>
// Prints the sample rate, channel count and length on stdout.
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 3) { std::cerr << "usage: OggDecode in.ogg out.f32\n"; return 1; }
    juce::OggVorbisAudioFormat ogg;
    std::unique_ptr<juce::AudioFormatReader> r (ogg.createReaderFor (new juce::FileInputStream (juce::File (argv[1])), true));
    if (r == nullptr) { std::cerr << "not an Ogg Vorbis stream\n"; return 1; }
    const int ch = (int) r->numChannels, n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> b (ch, n);
    r->read (&b, 0, n, 0, true, true);
    juce::FileOutputStream out { juce::File (argv[2]) };
    if (! out.openedOk()) { std::cerr << "cannot write " << argv[2] << "\n"; return 1; }
    out.setPosition (0); out.truncate();
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c) out.writeFloat (b.getSample (c, i));
    std::cout << r->sampleRate << " " << ch << " " << n << "\n";
    return 0;
}
