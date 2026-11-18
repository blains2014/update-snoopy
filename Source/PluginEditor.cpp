#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace Palette
{
    static const juce::Colour bgObsidian   (0xff0b0c0f);
    static const juce::Colour panelDark    (0xff13151b);
    static const juce::Colour panelBorder  (0xff232734);
    static const juce::Colour crispWhite   (0xfff5f7fa);
    static const juce::Colour mutedGrey    (0xff8e96a8);
    static const juce::Colour electricYel  (0xffffd426);
    static const juce::Colour vibrantRed   (0xffff3b4e);
}

OsotruxeLookAndFeel::OsotruxeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::crispWhite);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void OsotruxeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const float radius = juce::jmin (width, height) * 0.42f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    if (accent.isTransparent()) accent = Palette::electricYel;

    // Subtle dark knob body
    g.setColour (juce::Colour (0xff191c24));
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (Palette::panelBorder);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // Track arc
    juce::Path bgArc;
    bgArc.addCentredArc (cx, cy, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff262b38));
    g.strokePath (bgArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc
    juce::Path valArc;
    valArc.addCentredArc (cx, cy, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Crisp pointer line
    juce::Path pointer;
    const float px = cx + (radius * 0.72f) * std::sin (angle);
    const float py = cy - (radius * 0.72f) * std::cos (angle);
    g.setColour (Palette::crispWhite);
    g.drawLine (cx, cy, px, py, 1.8f);
}

void OsotruxeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    auto accent = button.findColour (juce::ToggleButton::tickColourId);
    if (accent.isTransparent()) accent = Palette::electricYel;

    g.setColour (on ? accent.withAlpha (0.16f) : juce::Colour (0xff161922));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? accent : (shouldDrawButtonAsHighlighted ? Palette::mutedGrey : Palette::panelBorder));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    g.setColour (on ? accent : Palette::mutedGrey);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void OsotruxeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (isDown ? Palette::vibrantRed.withAlpha (0.28f)
                        : (isHighlighted ? juce::Colour (0xff232836) : juce::Colour (0xff181b24)));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (isDown ? Palette::vibrantRed : Palette::panelBorder);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

// ==============================================================================
SampleWaveformView::SampleWaveformView (BlendMesh5AudioProcessor& proc, int slotIndex)
    : processor (proc), slotIdx (slotIndex)
{
}

float SampleWaveformView::getLoopParam (const juce::String& suffix) const
{
    auto id = "slot" + juce::String (slotIdx + 1) + "_" + suffix;
    return processor.apvts.getRawParameterValue (id)->load();
}

void SampleWaveformView::setLoopParam (const juce::String& suffix, float normVal)
{
    auto id = "slot" + juce::String (slotIdx + 1) + "_" + suffix;
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (juce::jlimit (0.0f, 1.0f, normVal)));
}

void SampleWaveformView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff090a0e));
    g.fillRoundedRectangle (b, 4.0f);

    // Subtle Pro-Q style grid lines
    g.setColour (juce::Colour (0xff1a1d26));
    for (int i = 1; i < 4; ++i)
    {
        float gx = b.getX() + (b.getWidth() * i) / 4.0f;
        g.drawVerticalLine (static_cast<int> (gx), b.getY() + 2, b.getBottom() - 2);
    }
    g.drawHorizontalLine (static_cast<int> (b.getCentreY()), b.getX() + 2, b.getRight() - 2);

    if (!processor.isSlotLoaded (slotIdx))
    {
        g.setColour (Palette::mutedGrey.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("Drop / Load Sample", b, juce::Justification::centred);
        g.setColour (Palette::panelBorder);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        return;
    }

    const int bins = juce::jmax (16, getWidth() - 4);
    std::vector<float> mins, maxs;
    processor.getSlotWaveformPeaks (slotIdx, mins, maxs, bins);

    const float cy = b.getCentreY();
    const float halfH = b.getHeight() * 0.44f;
    const float lStart = getLoopParam ("loopStart");
    const float lEnd   = juce::jmax (lStart + 0.01f, getLoopParam ("loopEnd"));
    const bool loopOn  = getLoopParam ("loop") > 0.5f;

    // Loop cycle selection highlight
    if (loopOn)
    {
        const float lx1 = b.getX() + lStart * b.getWidth();
        const float lx2 = b.getX() + lEnd * b.getWidth();
        g.setColour (Palette::electricYel.withAlpha (0.14f));
        g.fillRect (lx1, b.getY(), lx2 - lx1, b.getHeight());
    }

    // Waveform trace (white outside loop, electric yellow inside loop)
    for (int x = 0; x < bins; ++x)
    {
        const float normX = static_cast<float> (x) / static_cast<float> (bins);
        const bool inLoop = loopOn && (normX >= lStart && normX <= lEnd);
        g.setColour (inLoop ? Palette::electricYel : Palette::crispWhite.withAlpha (0.75f));

        const float yTop = cy - maxs[static_cast<size_t> (x)] * halfH;
        const float yBot = cy - mins[static_cast<size_t> (x)] * halfH;
        g.drawVerticalLine (static_cast<int> (b.getX() + 2 + x), yTop, juce::jmax (yTop + 1.0f, yBot));
    }

    // Loop start & end handles
    if (loopOn)
    {
        const float lx1 = b.getX() + lStart * b.getWidth();
        const float lx2 = b.getX() + lEnd * b.getWidth();
        g.setColour (Palette::electricYel);
        g.drawVerticalLine (static_cast<int> (lx1), b.getY(), b.getBottom());
        g.setColour (Palette::vibrantRed);
        g.drawVerticalLine (static_cast<int> (lx2), b.getY(), b.getBottom());
    }

    // Playhead
    const float ph = processor.getSlotPlayheadNorm (slotIdx);
    if (ph > 0.0f)
    {
        const float px = b.getX() + ph * b.getWidth();
        g.setColour (Palette::vibrantRed);
        g.drawVerticalLine (static_cast<int> (px), b.getY(), b.getBottom());
    }

    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
}

void SampleWaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (!processor.isSlotLoaded (slotIdx) || getWidth() <= 0)
        return;

    const float normX = juce::jlimit (0.0f, 1.0f, static_cast<float> (e.x) / static_cast<float> (getWidth()));
    const float lStart = getLoopParam ("loopStart");
    const float lEnd   = getLoopParam ("loopEnd");

    if (std::abs (normX - lStart) < 0.06f)
        dragMode = DragMode::MoveStart;
    else if (std::abs (normX - lEnd) < 0.06f)
        dragMode = DragMode::MoveEnd;
    else
    {
        dragMode = DragMode::NewRange;
        dragAnchorNorm = normX;
    }
}

void SampleWaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (!processor.isSlotLoaded (slotIdx) || getWidth() <= 0)
        return;

    const float normX = juce::jlimit (0.0f, 1.0f, static_cast<float> (e.x) / static_cast<float> (getWidth()));
    if (dragMode == DragMode::MoveStart)
    {
        setLoopParam ("loopStart", juce::jmin (normX, getLoopParam ("loopEnd") - 0.02f));
    }
    else if (dragMode == DragMode::MoveEnd)
    {
        setLoopParam ("loopEnd", juce::jmax (normX, getLoopParam ("loopStart") + 0.02f));
    }
    else if (dragMode == DragMode::NewRange)
    {
        float a = juce::jmin (dragAnchorNorm, normX);
        float b = juce::jmax (dragAnchorNorm, normX);
        if (b - a >= 0.015f)
        {
            setLoopParam ("loopStart", a);
            setLoopParam ("loopEnd", b);
        }
    }
    repaint();
}

void MasterWaveformScope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff090a0e));
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (juce::Colour (0xff1a1d26));
    for (int i = 1; i < 6; ++i)
        g.drawVerticalLine (static_cast<int> (b.getX() + (b.getWidth() * i) / 6.0f), b.getY() + 2, b.getBottom() - 2);
    g.drawHorizontalLine (static_cast<int> (b.getCentreY()), b.getX() + 2, b.getRight() - 2);

    std::vector<float> samples;
    processor.getMasterScopeSamples (samples);

    float livePeak = 0.0f;
    for (float v : samples)
        livePeak = juce::jmax (livePeak, std::abs (v));

    const float cy = b.getCentreY();
    const float halfH = b.getHeight() * 0.42f;

    if (livePeak > 0.002f && !samples.empty())
    {
        juce::Path wavePath;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float x = b.getX() + (static_cast<float> (i) / static_cast<float> (samples.size() - 1)) * b.getWidth();
            const float y = cy - samples[i] * halfH;
            if (i == 0) wavePath.startNewSubPath (x, y);
            else        wavePath.lineTo (x, y);
        }
        g.setColour (livePeak > 0.92f ? Palette::vibrantRed : Palette::electricYel);
        g.strokePath (wavePath, juce::PathStrokeType (1.7f));
    }
    else
    {
        const int bins = juce::jmax (32, getWidth() - 6);
        std::vector<float> mins, maxs;
        processor.getCompiledPreviewPeaks (mins, maxs, bins);
        g.setColour (Palette::electricYel.withAlpha (0.85f));
        for (int x = 0; x < bins; ++x)
        {
            const float yTop = cy - maxs[static_cast<size_t> (x)] * halfH;
            const float yBot = cy - mins[static_cast<size_t> (x)] * halfH;
            if (std::abs (yBot - yTop) > 0.5f)
                g.drawVerticalLine (static_cast<int> (b.getX() + 3 + x), yTop, yBot);
        }
    }

    g.setColour (Palette::mutedGrey.withAlpha (0.75f));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("COMPILED OUTPUT", b.reduced (6.0f, 3.0f), juce::Justification::topLeft);

    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
}

// ==============================================================================
BlendMesh5AudioProcessorEditor::BlendMesh5AudioProcessorEditor (BlendMesh5AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLnf);
    logoImage = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto& st = strips[i];
        auto prefix = "slot" + juce::String (i + 1) + "_";

        addAndMakeVisible (st.loadBtn);
        st.loadBtn.onClick = [this, i]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Select Audio Sample for Slot " + juce::String (i + 1),
                juce::File{},
                "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, i] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile() && audioProcessor.loadSampleIntoSlot (i, file))
                    {
                        strips[i].fileLabel.setText (audioProcessor.getSlotFileName (i), juce::dontSendNotification);
                        strips[i].waveform->repaint();
                    }
                });
        };

        addAndMakeVisible (st.fileLabel);
        st.fileLabel.setText (audioProcessor.getSlotFileName (i), juce::dontSendNotification);
        st.fileLabel.setJustificationType (juce::Justification::centredLeft);
        st.fileLabel.setColour (juce::Label::textColourId, Palette::crispWhite);
        st.fileLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        st.waveform = std::make_unique<SampleWaveformView> (audioProcessor, i);
        addAndMakeVisible (*st.waveform);

        st.polarityBtn.setColour (juce::ToggleButton::tickColourId, Palette::vibrantRed);
        st.fxSendBtn.setColour   (juce::ToggleButton::tickColourId, Palette::electricYel);
        st.loopBtn.setColour     (juce::ToggleButton::tickColourId, Palette::electricYel);
        addAndMakeVisible (st.polarityBtn);
        addAndMakeVisible (st.fxSendBtn);
        addAndMakeVisible (st.loopBtn);

        st.polarityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "polarity", st.polarityBtn);
        st.fxSendAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "fxsend", st.fxSendBtn);
        st.loopAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "loop", st.loopBtn);

        setupMiniKnob (st.levelSlider,     st.levelLbl,  "LVL",   {},     Palette::electricYel);
        setupMiniKnob (st.pitchSlider,     st.pitchLbl,  "PITCH", " st",  Palette::crispWhite);
        setupMiniKnob (st.attackSlider,    st.atkLbl,    "ATK",   " s",   Palette::electricYel);
        setupMiniKnob (st.decaySlider,     st.decLbl,    "DEC",   " s",   Palette::electricYel);
        setupMiniKnob (st.sustainSlider,   st.susLbl,    "SUS",   {},     Palette::electricYel);
        setupMiniKnob (st.releaseSlider,   st.relLbl,    "REL",   " s",   Palette::electricYel);
        setupMiniKnob (st.loopStartSlider, st.lStartLbl, "START", {},     Palette::electricYel);
        setupMiniKnob (st.loopEndSlider,   st.lEndLbl,   "END",   {},     Palette::vibrantRed);

        st.levelAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "level", st.levelSlider);
        st.pitchAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "pitch", st.pitchSlider);
        st.attackAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "attack", st.attackSlider);
        st.decayAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "decay", st.decaySlider);
        st.sustainAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "sustain", st.sustainSlider);
        st.releaseAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "release", st.releaseSlider);
        st.loopStartAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "loopStart", st.loopStartSlider);
        st.loopEndAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "loopEnd", st.loopEndSlider);
    }

    setupMiniKnob (timbreSlider,     timbreLbl,   "TIMBRE",   {}, Palette::electricYel);
    setupMiniKnob (colorSlider,      colorLbl,    "COLOR",    {}, Palette::vibrantRed);
    setupMiniKnob (shapeSlider,      shapeLbl,    "SHAPE",    {}, Palette::electricYel);
    setupMiniKnob (movementSlider,   movementLbl, "MOVEMENT", {}, Palette::electricYel);
    setupMiniKnob (masterGainSlider, masterLbl,   "MASTER",   {}, Palette::crispWhite);

    timbreAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "blend_timbre", timbreSlider);
    colorAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "blend_color", colorSlider);
    shapeAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "blend_shape", shapeSlider);
    movementAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "blend_movement", movementSlider);
    masterGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "master_gain", masterGainSlider);

    addAndMakeVisible (masterScope);
    addAndMakeVisible (auditionBtn);
    auditionBtn.onStateChange = [this]
    {
        audioProcessor.triggerAudition (auditionBtn.isDown());
    };

    auto addFxToggle = [this] (juce::ToggleButton& btn, const juce::String& text, const juce::String& paramId,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                               juce::Colour col)
    {
        btn.setButtonText (text);
        btn.setColour (juce::ToggleButton::tickColourId, col);
        addAndMakeVisible (btn);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, paramId, btn);
    };

    addFxToggle (eqOn,     "EQ",     "eq_on",     eqOnAtt,     Palette::electricYel);
    addFxToggle (compOn,   "COMP",   "comp_on",   compOnAtt,   Palette::electricYel);
    addFxToggle (satOn,    "SAT",    "sat_on",    satOnAtt,    Palette::vibrantRed);
    addFxToggle (distOn,   "DIST",   "dist_on",   distOnAtt,   Palette::vibrantRed);
    addFxToggle (phaserOn, "PHASER", "phaser_on", phaserOnAtt, Palette::electricYel);
    addFxToggle (delayOn,  "DELAY",  "delay_on",  delayOnAtt,  Palette::electricYel);
    addFxToggle (reverbOn, "REVERB", "reverb_on", reverbOnAtt, Palette::electricYel);

    attachFxKnob (eqLow,  "eq_low",  "LOW",  " dB", Palette::electricYel);
    attachFxKnob (eqMid,  "eq_mid",  "MID",  " dB", Palette::electricYel);
    attachFxKnob (eqHigh, "eq_high", "HIGH", " dB", Palette::electricYel);

    attachFxKnob (compThresh, "comp_thresh", "THR",   " dB", Palette::electricYel);
    attachFxKnob (compRatio,  "comp_ratio",  "RATIO", ":1",  Palette::electricYel);

    attachFxKnob (satDrive, "sat_drive", "DRIVE", {}, Palette::vibrantRed);
    attachFxKnob (satMix,   "sat_mix",   "MIX",   {}, Palette::vibrantRed);

    attachFxKnob (distDrive, "dist_drive", "DRIVE", {},    Palette::vibrantRed);
    attachFxKnob (distTone,  "dist_tone",  "TONE",  "Hz", Palette::vibrantRed);
    attachFxKnob (distMix,   "dist_mix",   "MIX",   {},    Palette::vibrantRed);

    attachFxKnob (phaserRate,  "phaser_rate",  "RATE",  "Hz", Palette::electricYel);
    attachFxKnob (phaserDepth, "phaser_depth", "DEPTH", {},   Palette::electricYel);
    attachFxKnob (phaserMix,   "phaser_mix",   "MIX",   {},   Palette::electricYel);

    attachFxKnob (delayTime, "delay_time", "TIME", "ms", Palette::electricYel);
    attachFxKnob (delayFb,   "delay_fb",   "FB",   {},   Palette::electricYel);
    attachFxKnob (delayMix,  "delay_mix",  "MIX",  {},   Palette::electricYel);

    attachFxKnob (reverbSize, "reverb_size", "SIZE", {}, Palette::electricYel);
    attachFxKnob (reverbDamp, "reverb_damp", "DAMP", {}, Palette::electricYel);
    attachFxKnob (reverbMix,  "reverb_mix",  "MIX",  {}, Palette::electricYel);

    setSize (1140, 720);
    startTimerHz (30);
}

BlendMesh5AudioProcessorEditor::~BlendMesh5AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BlendMesh5AudioProcessorEditor::timerCallback()
{
    for (int i = 0; i < NUM_SLOTS; ++i)
        if (audioProcessor.isSlotLoaded (i))
            strips[i].waveform->repaint();
    masterScope.repaint();
}

void BlendMesh5AudioProcessorEditor::setupMiniKnob (juce::Slider& s, juce::Label& lbl, const juce::String& name,
                                                    const juce::String& suffix, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 13);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    if (suffix.isNotEmpty())
        s.setTextValueSuffix (suffix);
    addAndMakeVisible (s);

    lbl.setText (name, juce::dontSendNotification);
    lbl.setJustificationType (juce::Justification::centred);
    lbl.setColour (juce::Label::textColourId, Palette::mutedGrey);
    lbl.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    addAndMakeVisible (lbl);
}

void BlendMesh5AudioProcessorEditor::attachFxKnob (juce::Slider& s, const juce::String& paramId, const juce::String& name,
                                                   const juce::String& suffix, juce::Colour accent)
{
    auto lbl = std::make_unique<juce::Label>();
    setupMiniKnob (s, *lbl, name, suffix, accent);
    fxLabels.push_back (std::move (lbl));
    fxSliderAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, paramId, s));
}

void BlendMesh5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bgObsidian);

    if (logoImage.isValid())
        g.drawImageWithin (logoImage, 14, 6, 38, 38, juce::RectanglePlacement::centred);

    g.setColour (Palette::crispWhite);
    g.setFont (juce::Font (juce::FontOptions ("Segoe UI Symbol", 19.0f, juce::Font::bold)));
    const juce::String mathTitle (juce::CharPointer_UTF8 ("\xe2\x8a\x99 \xe2\x88\xab \xe2\x8a\x99 \xe2\x8a\xa4 \xe2\x84\x9d \xe2\x88\xaa \xc3\x97 \xe2\x88\x83"));
    g.drawText (mathTitle, 58, 5, 360, 22, juce::Justification::left);

    g.setColour (Palette::electricYel);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("SnoopyChu  |  5-Layer Cycle Morph & Selective FX", 58, 26, 440, 16, juce::Justification::left);

    // 5 Sample Slot Cards
    const int slotW = (getWidth() - 48) / NUM_SLOTS;
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        const int x = 14 + i * (slotW + 5);
        g.setColour (Palette::panelDark);
        g.fillRoundedRectangle (static_cast<float> (x), 50.0f, static_cast<float> (slotW), 372.0f, 6.0f);
        g.setColour (Palette::panelBorder);
        g.drawRoundedRectangle (static_cast<float> (x), 50.0f, static_cast<float> (slotW), 372.0f, 6.0f, 1.0f);
    }

    // Master Morph Panel
    g.setColour (Palette::panelDark);
    g.fillRoundedRectangle (14.0f, 432.0f, static_cast<float> (getWidth() - 28), 96.0f, 6.0f);
    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (14.0f, 432.0f, static_cast<float> (getWidth() - 28), 96.0f, 6.0f, 1.0f);

    // FX Rack Panel
    g.setColour (Palette::panelDark);
    g.fillRoundedRectangle (14.0f, 538.0f, static_cast<float> (getWidth() - 28), 168.0f, 6.0f);
    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (14.0f, 538.0f, static_cast<float> (getWidth() - 28), 168.0f, 6.0f, 1.0f);

    g.setColour (Palette::mutedGrey);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("SELECTIVE FX RACK  (Processes layers with FX active)", 24, 544, 420, 16, juce::Justification::left);
}

void BlendMesh5AudioProcessorEditor::resized()
{
    const int slotW = (getWidth() - 48) / NUM_SLOTS;
    auto placeKnob = [] (juce::Slider& s, juce::Label& l, int x, int y, int w = 48, int h = 50)
    {
        l.setBounds (x, y, w, 12);
        s.setBounds (x, y + 11, w, h - 11);
    };

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto& st = strips[i];
        const int x = 14 + i * (slotW + 5);

        st.loadBtn.setBounds (x + 8, 58, 52, 22);
        st.fileLabel.setBounds (x + 64, 58, slotW - 72, 22);
        if (st.waveform != nullptr)
            st.waveform->setBounds (x + 8, 86, slotW - 16, 68);

        st.loopBtn.setBounds     (x + 8,   160, 58, 20);
        st.polarityBtn.setBounds (x + 72,  160, 62, 20);
        st.fxSendBtn.setBounds   (x + 140, 160, 58, 20);

        // Compact 4x2 mini-knob grid (44-48px)
        const int c1 = x + 6;
        const int c2 = x + 56;
        const int c3 = x + 108;
        const int c4 = x + 158;

        placeKnob (st.levelSlider,   st.levelLbl,  c1, 190, 48, 54);
        placeKnob (st.pitchSlider,   st.pitchLbl,  c2, 190, 48, 54);
        placeKnob (st.loopStartSlider, st.lStartLbl, c3, 190, 48, 54);
        placeKnob (st.loopEndSlider,   st.lEndLbl,   c4, 190, 48, 54);

        placeKnob (st.attackSlider,  st.atkLbl,    c1, 254, 48, 54);
        placeKnob (st.decaySlider,   st.decLbl,    c2, 254, 48, 54);
        placeKnob (st.sustainSlider, st.susLbl,    c3, 254, 48, 54);
        placeKnob (st.releaseSlider, st.relLbl,    c4, 254, 48, 54);
    }

    // Master Morph Row (compact knobs)
    placeKnob (timbreSlider,     timbreLbl,   28,  444, 62, 68);
    placeKnob (colorSlider,      colorLbl,    104, 444, 62, 68);
    placeKnob (shapeSlider,      shapeLbl,    180, 444, 62, 68);
    placeKnob (movementSlider,   movementLbl, 256, 444, 62, 68);
    placeKnob (masterGainSlider, masterLbl,   332, 444, 62, 68);
    masterScope.setBounds (412, 444, getWidth() - 588, 72);
    auditionBtn.setBounds (getWidth() - 164, 458, 134, 38);

    if (fxLabels.size() < 19) return;
    // FX Rack (7 compact modules)
    const int fxY = 566;
    auto placeFxKnob = [] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        l.setBounds (x, y, 46, 12);
        s.setBounds (x, y + 11, 46, 48);
    };

    eqOn.setBounds (24, fxY, 64, 20);
    placeFxKnob (eqLow,  *fxLabels[0], 20,  fxY + 26);
    placeFxKnob (eqMid,  *fxLabels[1], 68,  fxY + 26);
    placeFxKnob (eqHigh, *fxLabels[2], 116, fxY + 26);

    compOn.setBounds (176, fxY, 64, 20);
    placeFxKnob (compThresh, *fxLabels[3], 172, fxY + 26);
    placeFxKnob (compRatio,  *fxLabels[4], 222, fxY + 26);

    satOn.setBounds (282, fxY, 64, 20);
    placeFxKnob (satDrive, *fxLabels[5], 278, fxY + 26);
    placeFxKnob (satMix,   *fxLabels[6], 328, fxY + 26);

    distOn.setBounds (388, fxY, 64, 20);
    placeFxKnob (distDrive, *fxLabels[7],  384, fxY + 26);
    placeFxKnob (distTone,  *fxLabels[8],  432, fxY + 26);
    placeFxKnob (distMix,   *fxLabels[9],  480, fxY + 26);

    phaserOn.setBounds (542, fxY, 72, 20);
    placeFxKnob (phaserRate,  *fxLabels[10], 538, fxY + 26);
    placeFxKnob (phaserDepth, *fxLabels[11], 586, fxY + 26);
    placeFxKnob (phaserMix,   *fxLabels[12], 634, fxY + 26);

    delayOn.setBounds (696, fxY, 68, 20);
    placeFxKnob (delayTime, *fxLabels[13], 692, fxY + 26);
    placeFxKnob (delayFb,   *fxLabels[14], 740, fxY + 26);
    placeFxKnob (delayMix,  *fxLabels[15], 788, fxY + 26);

    reverbOn.setBounds (850, fxY, 72, 20);
    placeFxKnob (reverbSize, *fxLabels[16], 846, fxY + 26);
    placeFxKnob (reverbDamp, *fxLabels[17], 894, fxY + 26);
    placeFxKnob (reverbMix,  *fxLabels[18], 942, fxY + 26);
}
