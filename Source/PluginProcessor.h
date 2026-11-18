#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

static constexpr int NUM_SLOTS = 5;

struct SampleSlot
{
    juce::AudioBuffer<float> buffer;
    double sourceSampleRate = 44100.0;
    juce::String fileName { "Empty Slot" };
    bool loaded = false;

    double playPosition = 0.0;
    bool noteActive = false;
    float velocity = 1.0f;
    int currentMidiNote = 60;
    juce::ADSR adsr;
};

class BlendMesh5AudioProcessor : public juce::AudioProcessor
{
public:
    BlendMesh5AudioProcessor();
    ~BlendMesh5AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool loadSampleIntoSlot (int slotIndex, const juce::File& file);
    juce::String getSlotFileName (int slotIndex) const;
    bool isSlotLoaded (int slotIndex) const;
    float getSlotPlayheadNorm (int slotIndex) const;
    void getSlotWaveformPeaks (int slotIndex, std::vector<float>& mins, std::vector<float>& maxs, int numBins) const;
    void triggerAudition (bool noteOn);
    void getMasterScopeSamples (std::vector<float>& dest) const;
    void getCompiledPreviewPeaks (std::vector<float>& mins, std::vector<float>& maxs, int numBins) const;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    std::array<SampleSlot, NUM_SLOTS> slots;
    std::array<std::atomic<float>, NUM_SLOTS> playheadNorms;
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> masterScopeBuffer;
    std::atomic<int> masterScopeWritePos { 0 };
    juce::AudioFormatManager formatManager;
    mutable juce::CriticalSection sampleLock;

    std::atomic<bool> auditionPendingOn { false };
    std::atomic<bool> auditionPendingOff { false };

    double currentSampleRate = 44100.0;
    double movementPhase = 0.0;

    // Color DSP
    juce::dsp::IIR::Filter<float> colorLowShelfL, colorLowShelfR;
    juce::dsp::IIR::Filter<float> colorHighShelfL, colorHighShelfR;

    // FX Rack DSP
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR;
    juce::dsp::IIR::Filter<float> eqMidL, eqMidR;
    juce::dsp::IIR::Filter<float> eqHighL, eqHighR;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::IIR::Filter<float> distToneL, distToneR;
    juce::dsp::Phaser<float> phaser;

    static constexpr int maxDelaySamples = 192000;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { maxDelaySamples };

    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> fxBusBuffer;
    juce::AudioBuffer<float> dryBusBuffer;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlendMesh5AudioProcessor)
};
