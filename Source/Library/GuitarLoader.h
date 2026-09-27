#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Engine/Types.h"
#include <functional>

namespace dgtr
{
// Loads <soundset>/<guitar>/guitar.json (DGL v1) into memory. Runs on a background thread.
class GuitarLoader
{
public:
    // progress: 0..1. Returns nullptr and fills 'error' on failure.
    static std::shared_ptr<GuitarSet> load (const juce::File& guitarDir, juce::String& error,
                                            const std::function<void (float)>& progress,
                                            const std::function<bool()>& shouldAbort)
    {
        const auto manifestFile = guitarDir.getChildFile ("guitar.json");
        const auto manifest = juce::JSON::parse (manifestFile);
        if (! manifest.isObject() || manifest["format"].toString() != "DGL")
        {
            error = "Invalid guitar manifest: " + manifestFile.getFullPathName();
            return nullptr;
        }

        auto g = std::make_shared<GuitarSet>();
        g->id = manifest["id"].toString().toStdString();
        g->name = manifest["name"].toString().toStdString();
        g->releaseSec = (float) (double) manifest.getProperty ("releaseSec", 0.1);

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        const auto samples = manifest["samples"];
        const int numSamples = samples.size();
        g->samples.resize ((size_t) numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            if (shouldAbort()) { error = "aborted"; return nullptr; }
            const auto s = samples[i];
            const auto file = guitarDir.getChildFile (s["file"].toString());
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr) { error = "Cannot read " + file.getFullPathName(); return nullptr; }

            const int len = (int) reader->lengthInSamples;
            const int ch = juce::jlimit (1, 2, (int) reader->numChannels);
            juce::AudioBuffer<float> buf (ch, len);
            reader->read (&buf, 0, len, 0, true, ch > 1);

            auto& d = g->samples[(size_t) i];
            d.sampleRate = reader->sampleRate;
            d.onset = juce::jlimit (0, juce::jmax (0, len - 1), (int) s.getProperty ("onset", 0));
            d.left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
            if (ch > 1) d.right.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
            // 4 samples of guard for the interpolator
            for (int k = 0; k < 4; ++k) { d.left.push_back (0.0f); if (ch > 1) d.right.push_back (0.0f); }

            progress ((float) (i + 1) / (float) juce::jmax (1, numSamples));
        }

        const auto zones = manifest["zones"];
        for (int i = 0; i < zones.size(); ++i)
        {
            const auto z = zones[i];
            Zone zone;
            const auto kind = z["kind"].toString();
            zone.kind = kind == "release" ? ZoneKind::Release : kind == "noise" ? ZoneKind::Noise : ZoneKind::Note;
            zone.sample = (int) z["sample"];
            zone.loKey = (int) z["loKey"];   zone.hiKey = (int) z["hiKey"];  zone.keyCenter = (int) z["key"];
            zone.loVel = (int) z["loVel"];   zone.hiVel = (int) z["hiVel"];  zone.slot = (int) z["slot"];
            zone.tuneCents = (float) (double) z.getProperty ("tune", 0.0);
            zone.gainDb = (float) (double) z.getProperty ("gain", 0.0);
            zone.noiseType = z.getProperty ("noise", "").toString().toStdString();
            g->zones.push_back (zone);
        }
        g->finalize();
        if (g->slots.empty()) { error = "Guitar has no playable zones"; return nullptr; }
        return g;
    }
};
} // namespace dgtr
