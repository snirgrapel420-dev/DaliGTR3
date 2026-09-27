#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace pid
{
inline constexpr const char* guitar       = "guitar";
inline constexpr const char* volume       = "volume";
inline constexpr const char* bendRange    = "bendRange";
inline constexpr const char* position     = "position";
inline constexpr const char* customFret   = "customFret";
inline constexpr const char* releaseNoise = "releaseNoise";
}

namespace dgtr
{
// Guitar ids in the same order as the "guitar" choice parameter
inline const juce::StringArray& guitarIds()   { static const juce::StringArray a { "sg", "hollow", "archtop" }; return a; }
inline const juce::StringArray& guitarNames() { static const juce::StringArray a { "SG", "Gretsch / Hofner", "Archtop" }; return a; }

inline float bendRangeSemis (int index)
{
    static constexpr float t[] = { 1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 12.0f };
    return t[juce::jlimit (0, 5, index)];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::guitar, 1 }, "Guitar", guitarNames(), 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::volume, 1 }, "Volume",
                                                        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::bendRange, 1 }, "Bend Range",
                                                         StringArray { "1/2 tone", "1 tone", "1 1/2 tones", "2 1/2 tones", "3 1/2 tones", "12 st" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::position, 1 }, "Position",
                                                         StringArray { "Auto", "Low", "Mid", "High", "Custom" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::customFret, 1 }, "Custom Fret",
                                                        NormalisableRange<float> (0.0f, 19.0f, 1.0f), 7.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::releaseNoise, 1 }, "Release Noise",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f));
    return { p.begin(), p.end() };
}
} // namespace dgtr
