#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class OsotruxeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OsotruxeLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class SampleWaveformView : public juce::Component
{
public:
    SampleWaveformView (BlendMesh5AudioProcessor& proc, int slotIndex);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    BlendMesh5AudioProcessor& processor;
    int slotIdx;
    float dragAnchorNorm = 0.0f;
    enum class DragMode { None, NewRange, MoveStart, MoveEnd };
    DragMode dragMode = DragMode::None;

    void setLoopParam (const juce::String& suffix, float normVal);
    float getLoopParam (const juce::String& suffix) const;
};

class MasterWaveformScope : public juce::Component
{
public:
    explicit MasterWaveformScope (BlendMesh5AudioProcessor& proc) : processor (proc) {}
    void paint (juce::Graphics& g) override;
private:
    BlendMesh5AudioProcessor& processor;
};

class BlendMesh5AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BlendMesh5AudioProcessorEditor (BlendMesh5AudioProcessor&);
    ~BlendMesh5AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    BlendMesh5AudioProcessor& audioProcessor;
    OsotruxeLookAndFeel customLnf;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Image logoImage;

    struct SlotStrip
    {
        juce::TextButton loadBtn { "LOAD" };
        juce::Label fileLabel;
        std::unique_ptr<SampleWaveformView> waveform;

        juce::ToggleButton polarityBtn { "INV Ø" };
        juce::ToggleButton fxSendBtn   { "FX" };
        juce::ToggleButton loopBtn     { "LOOP" };

        juce::Slider levelSlider, pitchSlider;
        juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
        juce::Slider loopStartSlider, loopEndSlider;

        juce::Label levelLbl, pitchLbl, atkLbl, decLbl, susLbl, relLbl, lStartLbl, lEndLbl;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> polarityAtt, fxSendAtt, loopAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt, pitchAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAtt, decayAtt, sustainAtt, releaseAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loopStartAtt, loopEndAtt;
    };

    std::array<SlotStrip, NUM_SLOTS> strips;

    // Master Blend Section
    juce::Slider timbreSlider, colorSlider, shapeSlider, movementSlider, masterGainSlider;
    juce::Label timbreLbl, colorLbl, shapeLbl, movementLbl, masterLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timbreAtt, colorAtt, shapeAtt, movementAtt, masterGainAtt;
    juce::TextButton auditionBtn { "AUDITION" };
    MasterWaveformScope masterScope { audioProcessor };

    // FX Rack Controls
    juce::ToggleButton eqOn, compOn, satOn, distOn, phaserOn, delayOn, reverbOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, compOnAtt, satOnAtt, distOnAtt, phaserOnAtt, delayOnAtt, reverbOnAtt;

    juce::Slider eqLow, eqMid, eqHigh;
    juce::Slider compThresh, compRatio;
    juce::Slider satDrive, satMix;
    juce::Slider distDrive, distTone, distMix;
    juce::Slider phaserRate, phaserDepth, phaserMix;
    juce::Slider delayTime, delayFb, delayMix;
    juce::Slider reverbSize, reverbDamp, reverbMix;

    std::vector<std::unique_ptr<juce::Label>> fxLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> fxSliderAtts;

    void setupMiniKnob (juce::Slider& s, juce::Label& lbl, const juce::String& name,
                        const juce::String& suffix = {}, juce::Colour accent = juce::Colour (0xffffd426));
    void attachFxKnob (juce::Slider& s, const juce::String& paramId, const juce::String& name,
                       const juce::String& suffix = {}, juce::Colour accent = juce::Colour (0xffffd426));

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlendMesh5AudioProcessorEditor)
};
