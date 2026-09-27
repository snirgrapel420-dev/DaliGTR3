#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "../Engine/GuitarEngine.h"
#include "../Library/SoundSetManager.h"

class DaliGTRProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    DaliGTRProcessor();
    ~DaliGTRProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DaliGTR"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    dgtr::SoundSetManager soundSet;
    dgtr::GuitarEngine engine;

private:
    void timerCallback() override;   // follows the Guitar parameter on the message thread

    std::vector<dgtr::MidiEvent> events;
    std::vector<float> diL, diR;
    int seenGuitarVersion = 0;
    juce::SmoothedValue<float> volume;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaliGTRProcessor)
};
