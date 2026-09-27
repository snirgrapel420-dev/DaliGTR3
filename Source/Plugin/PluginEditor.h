#pragma once
#include "PluginProcessor.h"
#include "../UI/DaliLookAndFeel.h"
#include "../UI/NeckView.h"

class DaliGTREditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DaliGTREditor (DaliGTRProcessor&);
    ~DaliGTREditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawReadout (juce::Graphics&, juce::Rectangle<int>, const juce::String& label, const juce::String& value) const;
    void locateZip();

    DaliGTRProcessor& proc;
    DaliLookAndFeel laf;
    NeckView neck;

    juce::ComboBox guitarBox, positionBox, bendBox;
    juce::Slider volumeKnob, fretKnob, releaseKnob;
    juce::TextButton downloadButton { "DOWNLOAD SOUND SET" }, locateButton { "LOCATE ZIP..." };
    std::unique_ptr<juce::FileChooser> chooser;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::unique_ptr<APVTS::ComboBoxAttachment> guitarAtt, positionAtt, bendAtt;
    std::unique_ptr<APVTS::SliderAttachment> volumeAtt, fretAtt, releaseAtt;

    juce::Rectangle<int> headerArea, hudArea, controlsArea, statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaliGTREditor)
};
