#include "PluginEditor.h"

DaliGTREditor::DaliGTREditor (DaliGTRProcessor& p)
    : AudioProcessorEditor (&p), proc (p), neck (p.engine)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (neck);

    auto setupCombo = [this] (juce::ComboBox& box, const char* id, std::unique_ptr<APVTS::ComboBoxAttachment>& att)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (c->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box);
    };
    setupCombo (guitarBox, pid::guitar, guitarAtt);
    setupCombo (positionBox, pid::position, positionAtt);
    setupCombo (bendBox, pid::bendRange, bendAtt);

    auto setupKnob = [this] (juce::Slider& s, const char* id, std::unique_ptr<APVTS::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible (s);
        att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, s);
    };
    setupKnob (volumeKnob, pid::volume, volumeAtt);
    setupKnob (fretKnob, pid::customFret, fretAtt);
    setupKnob (releaseKnob, pid::releaseNoise, releaseAtt);
    volumeKnob.setTextValueSuffix (" dB");

    addAndMakeVisible (downloadButton);
    addAndMakeVisible (locateButton);
    downloadButton.onClick = [this] { proc.soundSet.requestDownload(); };
    locateButton.onClick = [this] { locateZip(); };

    // first time the window opens without a Sound Set: fetch it automatically
    // (not on plugin load - Ableton instantiates every plugin while scanning)
    if (! proc.soundSet.isInstalled() && dgtr::SoundSetManager::downloadUrl().isNotEmpty()
        && proc.soundSet.getState() == dgtr::SoundSetManager::State::Missing)
        proc.soundSet.requestDownload();

    setSize (1000, 620);
    startTimerHz (30);
}

DaliGTREditor::~DaliGTREditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DaliGTREditor::locateZip()
{
    chooser = std::make_unique<juce::FileChooser> ("Locate DaliGTR-SoundSet.zip", juce::File(), "*.zip");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.soundSet.requestInstallZip (f);
                          });
}

void DaliGTREditor::timerCallback()
{
    using S = dgtr::SoundSetManager::State;
    const auto st = proc.soundSet.getState();
    const bool needsInstall = st == S::Missing || (st == S::Error && ! proc.soundSet.isInstalled());
    downloadButton.setVisible (needsInstall);
    locateButton.setVisible (needsInstall);
    neck.repaint();
    repaint (hudArea);
    repaint (statusArea);
}

void DaliGTREditor::drawReadout (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, const juce::String& value) const
{
    g.setColour (dali::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (dali::line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (dali::dim);
    g.setFont (dali::font (10.0f, true).withExtraKerningFactor (0.15f));
    g.drawText (label, r.removeFromTop (24).reduced (12, 0), juce::Justification::bottomLeft);
    g.setColour (dali::text);
    g.setFont (dali::font (22.0f));
    g.drawText (value, r.reduced (12, 4), juce::Justification::centredLeft);
}

void DaliGTREditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (dali::bg);

    // ---- header: DaliGTR / Dali Audio
    {
        auto h = headerArea.reduced (28, 14);
        GlyphArrangement ga;
        ga.addLineOfText (dali::font (40.0f, true), "DaliGTR", (float) h.getX(), (float) h.getY() + 38.0f);
        Path logo; ga.createPath (logo);
        DropShadow (dali::accent.withAlpha (0.35f), 14, {}).drawForPath (g, logo);
        g.setGradientFill (ColourGradient (Colours::white, (float) h.getX(), (float) h.getY(),
                                           dali::accent.brighter (0.4f), (float) h.getX() + 190.0f, (float) h.getY() + 44.0f, false));
        g.fillPath (logo);
        g.setColour (dali::dim);
        g.setFont (dali::font (11.0f, true).withExtraKerningFactor (0.35f));
        g.drawText ("DALI AUDIO", h.getX() + 2, h.getY() + 46, 200, 14, Justification::centredLeft);

        g.setColour (dali::dim);
        g.setFont (dali::font (10.0f, true).withExtraKerningFactor (0.15f));
        g.drawText ("GUITAR", guitarBox.getBounds().translated (0, -16).withHeight (14), Justification::centredLeft);
        g.drawText ("VOLUME", volumeKnob.getBounds().translated (0, -12).withHeight (12), Justification::centred);
        g.setColour (dali::line);
        g.drawHorizontalLine (headerArea.getBottom() - 1, 20.0f, (float) getWidth() - 20.0f);
    }

    // ---- live performance readout
    {
        auto& hud = proc.engine.hud;
        const int note = hud.note.load();
        const int s = hud.string.load();
        const int f = hud.fret.load();
        const float bend = hud.bend.load();
        auto r = hudArea;
        const int w = (r.getWidth() - 5 * 10) / 6;
        auto cell = [&] { auto c = r.removeFromLeft (w); r.removeFromLeft (10); return c; };
        drawReadout (g, cell(), "NOTE", note >= 0 ? MidiMessage::getMidiNoteName (note, true, true, 4) : "-");
        drawReadout (g, cell(), "STRING", s >= 0 ? String (6 - s) : "-");
        drawReadout (g, cell(), "FRET", f >= 0 ? String (f) : "-");
        drawReadout (g, cell(), "PICK", note >= 0 ? (hud.dir.load() == 0 ? "DOWN" : "UP") : "-");
        drawReadout (g, cell(), "VELOCITY", note >= 0 ? String (hud.velocity.load()) : "-");
        drawReadout (g, cell(), "BEND", (bend >= 0 ? "+" : "") + String (bend, 1) + " st");
    }

    // ---- control labels
    g.setColour (dali::dim);
    g.setFont (dali::font (10.0f, true).withExtraKerningFactor (0.15f));
    g.drawText ("POSITION", positionBox.getBounds().translated (0, -16).withHeight (14), Justification::centredLeft);
    g.drawText ("BEND RANGE", bendBox.getBounds().translated (0, -16).withHeight (14), Justification::centredLeft);
    g.drawText ("CUSTOM FRET", fretKnob.getBounds().translated (0, -12).withHeight (12), Justification::centred);
    g.drawText ("RELEASE NOISE", releaseKnob.getBounds().translated (0, -12).withHeight (12), Justification::centred);

    // ---- Sound Set status
    {
        auto r = statusArea;
        g.setColour (dali::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        using S = dgtr::SoundSetManager::State;
        const auto st = proc.soundSet.getState();
        const float p = proc.soundSet.getProgress();
        auto bar = r.reduced (14, 0).removeFromBottom (6).translated (0, -8).toFloat();
        if (st == S::Downloading || st == S::Installing || st == S::Loading)
        {
            g.setColour (dali::line);
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (dali::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, p)), 3.0f);
        }
        g.setColour (st == S::Error ? Colour (0xffff7a8a) : st == S::Ready ? dali::text : dali::dim);
        g.setFont (dali::font (12.5f));
        auto msg = proc.soundSet.getMessage();
        if (st == S::Ready && proc.engine.currentGuitar() != nullptr)
            msg = String (proc.engine.currentGuitar()->name) + "  -  real recordings, Sound Set ready";
        g.drawText (msg, r.reduced (14, 0).withTrimmedBottom (14), Justification::centredLeft);
    }
}

void DaliGTREditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (92);
    {
        auto h = headerArea.reduced (28, 18);
        volumeKnob.setBounds (h.removeFromRight (78).withTrimmedTop (4).expanded (0, 10));
        h.removeFromRight (20);
        guitarBox.setBounds (h.removeFromRight (220).withSizeKeepingCentre (220, 30).translated (0, 8));
    }
    r.reduce (20, 14);
    neck.setBounds (r.removeFromTop (210));
    r.removeFromTop (14);
    hudArea = r.removeFromTop (78);
    r.removeFromTop (22);
    controlsArea = r.removeFromTop (96);
    {
        auto c = controlsArea;
        positionBox.setBounds (c.removeFromLeft (180).withSizeKeepingCentre (180, 30).translated (0, 6));
        c.removeFromLeft (24);
        fretKnob.setBounds (c.removeFromLeft (90).withTrimmedTop (12));
        c.removeFromLeft (24);
        bendBox.setBounds (c.removeFromLeft (180).withSizeKeepingCentre (180, 30).translated (0, 6));
        c.removeFromLeft (24);
        releaseKnob.setBounds (c.removeFromLeft (100).withTrimmedTop (12));
    }
    r.removeFromTop (12);
    statusArea = r.removeFromTop (46);
    {
        auto s = statusArea.reduced (10, 8);
        locateButton.setBounds (s.removeFromRight (130));
        s.removeFromRight (8);
        downloadButton.setBounds (s.removeFromRight (180));
    }
}
