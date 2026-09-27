#include "PluginProcessor.h"
#include "PluginEditor.h"

DaliGTRProcessor::DaliGTRProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("DI", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DaliGTR", dgtr::createLayout())
{
    events.reserve (1024);
    timerCallback();
    startTimerHz (5);
}

DaliGTRProcessor::~DaliGTRProcessor()
{
    stopTimer();
}

void DaliGTRProcessor::timerCallback()
{
    const int idx = juce::roundToInt (apvts.getRawParameterValue (pid::guitar)->load());
    soundSet.setDesiredGuitar (dgtr::guitarIds()[juce::jlimit (0, dgtr::guitarIds().size() - 1, idx)]);
}

void DaliGTRProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    diL.assign ((size_t) std::max (samplesPerBlock, 512), 0.0f);
    diR.assign ((size_t) std::max (samplesPerBlock, 512), 0.0f);
    volume.reset (sampleRate, 0.03);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (pid::volume)->load()));
}

bool DaliGTRProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono()) return false;
    if (layouts.outputBuses.size() > 1)
    {
        const auto di = layouts.outputBuses[1];
        if (! di.isDisabled() && di != juce::AudioChannelSet::stereo() && di != juce::AudioChannelSet::mono()) return false;
    }
    return layouts.getMainInputChannelSet().isDisabled();
}

void DaliGTRProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;

    // newly loaded guitar?
    dgtr::GuitarSetPtr incoming;
    if (soundSet.fetchIfChanged (incoming, seenGuitarVersion) && incoming != nullptr)
        soundSet.retire (engine.setGuitar (std::move (incoming)));

    dgtr::EngineSettings s;
    s.bendRangeSemis = dgtr::bendRangeSemis (juce::roundToInt (apvts.getRawParameterValue (pid::bendRange)->load()));
    s.positionPref   = juce::roundToInt (apvts.getRawParameterValue (pid::position)->load());
    s.customFret     = apvts.getRawParameterValue (pid::customFret)->load();
    s.releaseNoise   = apvts.getRawParameterValue (pid::releaseNoise)->load();
    engine.setSettings (s);

    events.clear();
    for (const auto meta : midi)
    {
        const auto* raw = meta.data;
        if (meta.numBytes < 1) continue;
        dgtr::MidiEvent e;
        e.offset = juce::jlimit (0, n - 1, meta.samplePosition);
        e.status = raw[0];
        e.data1 = meta.numBytes > 1 ? raw[1] : 0;
        e.data2 = meta.numBytes > 2 ? raw[2] : 0;
        events.push_back (e);
    }

    if ((int) diL.size() < n) { diL.resize ((size_t) n); diR.resize ((size_t) n); }
    engine.process (events.data(), (int) events.size(), diL.data(), diR.data(), n);

    // Main = guitar with volume (amp/cab/FX chain arrives in later phases)
    auto main = getBusBuffer (buffer, false, 0);
    volume.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (pid::volume)->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = volume.getNextValue();
        if (main.getNumChannels() > 1)
        {
            main.setSample (0, i, diL[(size_t) i] * g);
            main.setSample (1, i, diR[(size_t) i] * g);
        }
        else if (main.getNumChannels() == 1)
            main.setSample (0, i, 0.5f * (diL[(size_t) i] + diR[(size_t) i]) * g);
    }

    // DI bus = raw guitar, untouched
    if (getBusCount (false) > 1)
    {
        auto di = getBusBuffer (buffer, false, 1);
        if (di.getNumChannels() > 1) { di.copyFrom (0, 0, diL.data(), n); di.copyFrom (1, 0, diR.data(), n); }
        else if (di.getNumChannels() == 1)
        {
            di.copyFrom (0, 0, diL.data(), n, 0.5f);
            di.addFrom (0, 0, diR.data(), n, 0.5f);
        }
    }
}

juce::AudioProcessorEditor* DaliGTRProcessor::createEditor() { return new DaliGTREditor (*this); }

void DaliGTRProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DaliGTRProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DaliGTRProcessor(); }
