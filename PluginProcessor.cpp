#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

BlendMesh5AudioProcessor::BlendMesh5AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    for (auto& p : playheadNorms)
        p.store (0.0f);
    for (auto& s : masterScopeBuffer)
        s.store (0.0f);
}

BlendMesh5AudioProcessor::~BlendMesh5AudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout BlendMesh5AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto prefix = "slot" + juce::String (i + 1) + "_";
        auto label  = "Slot " + juce::String (i + 1) + " ";

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "level", label + "Level", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "attack", label + "Attack", juce::NormalisableRange<float> (0.001f, 3.0f, 0.001f, 0.4f), 0.01f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "decay", label + "Decay", juce::NormalisableRange<float> (0.005f, 4.0f, 0.001f, 0.4f), 0.3f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "sustain", label + "Sustain", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.85f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "release", label + "Release", juce::NormalisableRange<float> (0.005f, 5.0f, 0.001f, 0.4f), 0.35f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            prefix + "polarity", label + "Polarity Invert", false));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            prefix + "fxsend", label + "FX Rack Send", true));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "pitch", label + "Pitch (st)", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

        // Waveform cycle loop controls
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            prefix + "loop", label + "Loop Cycles", true));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "loopStart", label + "Loop Start", juce::NormalisableRange<float> (0.0f, 0.98f, 0.001f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            prefix + "loopEnd", label + "Loop End", juce::NormalisableRange<float> (0.02f, 1.0f, 0.001f), 1.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "blend_timbre", "Timbre Morph", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "blend_color", "Color Tilt", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "blend_shape", "Shape Contour", juce::NormalisableRange<float> (0.25f, 2.5f, 0.01f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "blend_movement", "Movement", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "master_gain", "Master Gain", juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 0.9f));

    // FX 1: EQ
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("eq_on", "EQ On", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eq_low", "EQ Low (dB)", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eq_mid", "EQ Mid (dB)", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eq_high", "EQ High (dB)", -12.0f, 12.0f, 0.0f));

    // FX 2: Compression
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("comp_on", "Compressor On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_thresh", "Comp Threshold", -40.0f, 0.0f, -14.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_ratio", "Comp Ratio", 1.0f, 12.0f, 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_attack", "Comp Attack (ms)", 1.0f, 100.0f, 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_release", "Comp Release (ms)", 10.0f, 500.0f, 120.0f));

    // FX 3: Saturation
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("sat_on", "Saturation On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sat_drive", "Sat Drive", 1.0f, 8.0f, 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sat_mix", "Sat Mix", 0.0f, 1.0f, 0.5f));

    // FX 4: Distortion
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("dist_on", "Distortion On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("dist_drive", "Dist Drive", 1.0f, 20.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("dist_tone", "Dist Tone (Hz)", 800.0f, 16000.0f, 6500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("dist_mix", "Dist Mix", 0.0f, 1.0f, 0.5f));

    // FX 5: Phaser
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("phaser_on", "Phaser On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("phaser_rate", "Phaser Rate (Hz)", 0.05f, 6.0f, 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("phaser_depth", "Phaser Depth", 0.0f, 1.0f, 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("phaser_fb", "Phaser Feedback", -0.8f, 0.8f, 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("phaser_mix", "Phaser Mix", 0.0f, 1.0f, 0.5f));

    // FX 6: Delay
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("delay_on", "Delay On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_time", "Delay Time (ms)", 20.0f, 1500.0f, 320.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_fb", "Delay Feedback", 0.0f, 0.85f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_mix", "Delay Mix", 0.0f, 1.0f, 0.3f));

    // FX 7: Reverb
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("reverb_on", "Reverb On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverb_size", "Reverb Size", 0.0f, 1.0f, 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverb_damp", "Reverb Damping", 0.0f, 1.0f, 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverb_width", "Reverb Width", 0.0f, 1.0f, 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverb_mix", "Reverb Mix", 0.0f, 1.0f, 0.3f));

    return { params.begin(), params.end() };
}

void BlendMesh5AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 2 };

    for (auto& slot : slots)
        slot.adsr.setSampleRate (sampleRate);

    fxBusBuffer.setSize (2, samplesPerBlock);
    dryBusBuffer.setSize (2, samplesPerBlock);

    colorLowShelfL.prepare (spec);  colorLowShelfR.prepare (spec);
    colorHighShelfL.prepare (spec); colorHighShelfR.prepare (spec);
    eqLowL.prepare (spec);  eqLowR.prepare (spec);
    eqMidL.prepare (spec);  eqMidR.prepare (spec);
    eqHighL.prepare (spec); eqHighR.prepare (spec);
    distToneL.prepare (spec); distToneR.prepare (spec);

    compressor.prepare (spec);
    phaser.prepare (spec);
    delayLineL.prepare (spec);
    delayLineR.prepare (spec);
    delayLineL.reset();
    delayLineR.reset();

    reverb.prepare (spec);
    updateFilters();
}

void BlendMesh5AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BlendMesh5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

bool BlendMesh5AudioProcessor::loadSampleIntoSlot (int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    juce::AudioBuffer<float> newBuffer (static_cast<int> (reader->numChannels),
                                        static_cast<int> (reader->lengthInSamples));
    reader->read (&newBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    const juce::ScopedLock sl (sampleLock);
    slots[slotIndex].buffer = std::move (newBuffer);
    slots[slotIndex].sourceSampleRate = reader->sampleRate;
    slots[slotIndex].fileName = file.getFileNameWithoutExtension();
    slots[slotIndex].loaded = true;
    slots[slotIndex].playPosition = 0.0;
    slots[slotIndex].noteActive = false;
    playheadNorms[slotIndex].store (0.0f);
    return true;
}

juce::String BlendMesh5AudioProcessor::getSlotFileName (int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return {};
    return slots[slotIndex].fileName;
}

bool BlendMesh5AudioProcessor::isSlotLoaded (int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return false;
    return slots[slotIndex].loaded;
}

float BlendMesh5AudioProcessor::getSlotPlayheadNorm (int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return 0.0f;
    return playheadNorms[slotIndex].load();
}

void BlendMesh5AudioProcessor::getSlotWaveformPeaks (int slotIndex, std::vector<float>& mins, std::vector<float>& maxs, int numBins) const
{
    mins.assign (static_cast<size_t> (numBins), 0.0f);
    maxs.assign (static_cast<size_t> (numBins), 0.0f);

    if (slotIndex < 0 || slotIndex >= NUM_SLOTS || numBins <= 0)
        return;

    const juce::ScopedLock sl (sampleLock);
    const auto& slot = slots[slotIndex];
    if (!slot.loaded || slot.buffer.getNumSamples() <= 0)
        return;

    const int totalSamples = slot.buffer.getNumSamples();
    const float* data = slot.buffer.getReadPointer (0);

    for (int b = 0; b < numBins; ++b)
    {
        const int startIdx = (b * totalSamples) / numBins;
        const int endIdx = std::max (startIdx + 1, ((b + 1) * totalSamples) / numBins);
        float mn = 1.0f;
        float mx = -1.0f;
        const int step = std::max (1, (endIdx - startIdx) / 64);
        for (int i = startIdx; i < endIdx && i < totalSamples; i += step)
        {
            const float v = data[i];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        mins[static_cast<size_t> (b)] = mn;
        maxs[static_cast<size_t> (b)] = mx;
    }
}

void BlendMesh5AudioProcessor::triggerAudition (bool noteOn)
{
    if (noteOn)
        auditionPendingOn.store (true);
    else
        auditionPendingOff.store (true);
}

void BlendMesh5AudioProcessor::updateFilters()
{
    const float colorVal = apvts.getRawParameterValue ("blend_color")->load();
    const float lowDb = -colorVal * 6.0f;
    const float highDb = colorVal * 6.0f;

    auto cLow  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 240.0f, 0.707f, juce::Decibels::decibelsToGain (lowDb));
    auto cHigh = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 3800.0f, 0.707f, juce::Decibels::decibelsToGain (highDb));
    colorLowShelfL.coefficients = cLow;
    colorLowShelfR.coefficients = cLow;
    colorHighShelfL.coefficients = cHigh;
    colorHighShelfR.coefficients = cHigh;

    const float eqLowDb  = apvts.getRawParameterValue ("eq_low")->load();
    const float eqMidDb  = apvts.getRawParameterValue ("eq_mid")->load();
    const float eqHighDb = apvts.getRawParameterValue ("eq_high")->load();

    auto eLow  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 140.0f, 0.707f, juce::Decibels::decibelsToGain (eqLowDb));
    auto eMid  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 1100.0f, 0.9f, juce::Decibels::decibelsToGain (eqMidDb));
    auto eHigh = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (eqHighDb));
    eqLowL.coefficients = eLow;   eqLowR.coefficients = eLow;
    eqMidL.coefficients = eMid;   eqMidR.coefficients = eMid;
    eqHighL.coefficients = eHigh; eqHighR.coefficients = eHigh;

    const float distToneHz = apvts.getRawParameterValue ("dist_tone")->load();
    auto dTone = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, distToneHz);
    distToneL.coefficients = dTone;
    distToneR.coefficients = dTone;
}

void BlendMesh5AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    fxBusBuffer.setSize (2, numSamples, false, false, true);
    dryBusBuffer.setSize (2, numSamples, false, false, true);
    fxBusBuffer.clear();
    dryBusBuffer.clear();

    auto startNoteAll = [this] (int noteNum, float vel)
    {
        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            auto& slot = slots[i];
            if (slot.loaded && slot.buffer.getNumSamples() > 1)
            {
                auto p = "slot" + juce::String (i + 1) + "_";
                const bool loopOn = apvts.getRawParameterValue (p + "loop")->load() > 0.5f;
                const float lStart = apvts.getRawParameterValue (p + "loopStart")->load();
                slot.playPosition = loopOn ? static_cast<double> (lStart) * (slot.buffer.getNumSamples() - 1) : 0.0;
                slot.currentMidiNote = noteNum;
                slot.velocity = vel;
                slot.noteActive = true;
                slot.adsr.noteOn();
            }
        }
    };

    if (auditionPendingOn.exchange (false))
        startNoteAll (60, 0.9f);

    if (auditionPendingOff.exchange (false))
    {
        for (auto& slot : slots)
            slot.adsr.noteOff();
    }

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            startNoteAll (msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            for (auto& slot : slots)
                slot.adsr.noteOff();
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& slot : slots)
            {
                slot.adsr.reset();
                slot.noteActive = false;
            }
        }
    }

    const float timbre   = apvts.getRawParameterValue ("blend_timbre")->load();
    const float shape    = apvts.getRawParameterValue ("blend_shape")->load();
    const float movement = apvts.getRawParameterValue ("blend_movement")->load();

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto p = "slot" + juce::String (i + 1) + "_";
        juce::ADSR::Parameters adsrParams;
        adsrParams.attack  = apvts.getRawParameterValue (p + "attack")->load() * shape;
        adsrParams.decay   = apvts.getRawParameterValue (p + "decay")->load() * shape;
        adsrParams.sustain = apvts.getRawParameterValue (p + "sustain")->load();
        adsrParams.release = apvts.getRawParameterValue (p + "release")->load() * shape;
        slots[i].adsr.setParameters (adsrParams);
    }

    const juce::ScopedLock sl (sampleLock);

    const double lfoFreq = 0.15 + movement * 2.5;
    const double phaseInc = (juce::MathConstants<double>::twoPi * lfoFreq) / currentSampleRate;

    for (int s = 0; s < numSamples; ++s)
    {
        movementPhase += phaseInc;
        if (movementPhase >= juce::MathConstants<double>::twoPi)
            movementPhase -= juce::MathConstants<double>::twoPi;

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            auto& slot = slots[i];
            if (!slot.loaded || !slot.adsr.isActive())
                continue;

            const int len = slot.buffer.getNumSamples();
            if (len <= 2) continue;

            auto p = "slot" + juce::String (i + 1) + "_";
            const float level    = apvts.getRawParameterValue (p + "level")->load();
            const float pitchSt  = apvts.getRawParameterValue (p + "pitch")->load();
            const bool invert    = apvts.getRawParameterValue (p + "polarity")->load() > 0.5f;
            const bool toFx      = apvts.getRawParameterValue (p + "fxsend")->load() > 0.5f;
            const bool loopOn    = apvts.getRawParameterValue (p + "loop")->load() > 0.5f;
            const float lStartN  = apvts.getRawParameterValue (p + "loopStart")->load();
            const float lEndN    = std::max (lStartN + 0.005f, apvts.getRawParameterValue (p + "loopEnd")->load());

            const double loopStartPos = static_cast<double> (lStartN) * (len - 1);
            const double loopEndPos   = std::min (static_cast<double> (len - 1), static_cast<double> (lEndN) * (len - 1));
            const double loopSpan     = std::max (4.0, loopEndPos - loopStartPos);

            if (loopOn && slot.playPosition >= loopEndPos)
            {
                slot.playPosition = loopStartPos + std::fmod (slot.playPosition - loopStartPos, loopSpan);
            }
            else if (!loopOn && slot.playPosition >= len - 1)
            {
                slot.adsr.reset();
                slot.noteActive = false;
                continue;
            }

            const float slotNorm = static_cast<float> (i) / static_cast<float> (NUM_SLOTS - 1);
            const float dist = std::abs (slotNorm - timbre);
            const float timbreWeight = juce::jlimit (0.25f, 1.25f, 1.25f - dist * 1.15f);

            const double slotPhase = movementPhase + (i * juce::MathConstants<double>::twoPi / NUM_SLOTS);
            const float lfoMod = 1.0f - movement * 0.45f * (0.5f + 0.5f * static_cast<float> (std::sin (slotPhase)));

            const float env = slot.adsr.getNextSample();
            const float polarity = invert ? -1.0f : 1.0f;
            const float gain = level * slot.velocity * env * polarity * timbreWeight * lfoMod;

            const int idx0 = juce::jlimit (0, len - 2, static_cast<int> (slot.playPosition));
            const float frac = static_cast<float> (slot.playPosition - idx0);
            const float* chL = slot.buffer.getReadPointer (0);
            const float* chR = slot.buffer.getNumChannels() > 1 ? slot.buffer.getReadPointer (1) : chL;

            float rawL = chL[idx0] + frac * (chL[idx0 + 1] - chL[idx0]);
            float rawR = chR[idx0] + frac * (chR[idx0 + 1] - chR[idx0]);

            // Smooth micro-crossfade at loop boundary to prevent clicks on arbitrary cycle cuts
            if (loopOn && loopSpan > 32.0)
            {
                const double xfadeLen = std::min (64.0, loopSpan * 0.15);
                const double distToEnd = loopEndPos - slot.playPosition;
                if (distToEnd >= 0.0 && distToEnd < xfadeLen)
                {
                    const float alpha = static_cast<float> (distToEnd / xfadeLen);
                    const int wrapIdx = juce::jlimit (0, len - 2, static_cast<int> (loopStartPos + (xfadeLen - distToEnd)));
                    rawL = rawL * alpha + chL[wrapIdx] * (1.0f - alpha);
                    rawR = rawR * alpha + chR[wrapIdx] * (1.0f - alpha);
                }
            }

            const float sampleL = rawL * gain;
            const float sampleR = rawR * gain;

            if (toFx)
            {
                fxBusBuffer.addSample (0, s, sampleL);
                fxBusBuffer.addSample (1, s, sampleR);
            }
            else
            {
                dryBusBuffer.addSample (0, s, sampleL);
                dryBusBuffer.addSample (1, s, sampleR);
            }

            const double midiRatio = std::pow (2.0, (slot.currentMidiNote - 60 + pitchSt) / 12.0);
            const double srRatio = slot.sourceSampleRate / currentSampleRate;
            slot.playPosition += midiRatio * srRatio;
        }
    }

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (slots[i].loaded && slots[i].adsr.isActive() && slots[i].buffer.getNumSamples() > 1)
            playheadNorms[i].store (static_cast<float> (slots[i].playPosition / (slots[i].buffer.getNumSamples() - 1)));
        else
            playheadNorms[i].store (0.0f);
    }

    updateFilters();

    juce::dsp::AudioBlock<float> fxBlock (fxBusBuffer);
    juce::dsp::ProcessContextReplacing<float> fxContext (fxBlock);

    if (apvts.getRawParameterValue ("eq_on")->load() > 0.5f)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float l = eqLowL.processSample (fxBusBuffer.getSample (0, s));
            l = eqMidL.processSample (l);
            l = eqHighL.processSample (l);
            fxBusBuffer.setSample (0, s, l);

            float r = eqLowR.processSample (fxBusBuffer.getSample (1, s));
            r = eqMidR.processSample (r);
            r = eqHighR.processSample (r);
            fxBusBuffer.setSample (1, s, r);
        }
    }

    if (apvts.getRawParameterValue ("comp_on")->load() > 0.5f)
    {
        compressor.setThreshold (apvts.getRawParameterValue ("comp_thresh")->load());
        compressor.setRatio     (apvts.getRawParameterValue ("comp_ratio")->load());
        compressor.setAttack    (apvts.getRawParameterValue ("comp_attack")->load());
        compressor.setRelease   (apvts.getRawParameterValue ("comp_release")->load());
        compressor.process (fxContext);
    }

    if (apvts.getRawParameterValue ("sat_on")->load() > 0.5f)
    {
        const float drive = apvts.getRawParameterValue ("sat_drive")->load();
        const float mix   = apvts.getRawParameterValue ("sat_mix")->load();
        const int outCh = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < outCh; ++ch)
        {
            auto* data = fxBusBuffer.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                const float dry = data[s];
                const float wet = std::tanh (dry * drive + 0.08f * dry * dry) / std::tanh (drive);
                data[s] = dry * (1.0f - mix) + wet * mix;
            }
        }
    }

    if (apvts.getRawParameterValue ("dist_on")->load() > 0.5f)
    {
        const float drive = apvts.getRawParameterValue ("dist_drive")->load();
        const float mix   = apvts.getRawParameterValue ("dist_mix")->load();
        for (int s = 0; s < numSamples; ++s)
        {
            const float dryL = fxBusBuffer.getSample (0, s);
            const float dryR = fxBusBuffer.getSample (1, s);
            const float clipL = juce::jlimit (-0.85f, 0.85f, dryL * drive);
            const float clipR = juce::jlimit (-0.85f, 0.85f, dryR * drive);
            const float wetL = distToneL.processSample (clipL);
            const float wetR = distToneR.processSample (clipR);
            fxBusBuffer.setSample (0, s, dryL * (1.0f - mix) + wetL * mix);
            fxBusBuffer.setSample (1, s, dryR * (1.0f - mix) + wetR * mix);
        }
    }

    if (apvts.getRawParameterValue ("phaser_on")->load() > 0.5f)
    {
        phaser.setRate     (apvts.getRawParameterValue ("phaser_rate")->load());
        phaser.setDepth    (apvts.getRawParameterValue ("phaser_depth")->load());
        phaser.setFeedback (apvts.getRawParameterValue ("phaser_fb")->load());
        phaser.setMix      (apvts.getRawParameterValue ("phaser_mix")->load());
        phaser.process (fxContext);
    }

    if (apvts.getRawParameterValue ("delay_on")->load() > 0.5f)
    {
        const float delayMs = apvts.getRawParameterValue ("delay_time")->load();
        const float fb      = apvts.getRawParameterValue ("delay_fb")->load();
        const float mix     = apvts.getRawParameterValue ("delay_mix")->load();
        const float delaySamples = juce::jlimit (1.0f, static_cast<float> (maxDelaySamples - 2),
                                                 (delayMs / 1000.0f) * static_cast<float> (currentSampleRate));
        delayLineL.setDelay (delaySamples);
        delayLineR.setDelay (delaySamples * 1.03f);

        for (int s = 0; s < numSamples; ++s)
        {
            const float inL = fxBusBuffer.getSample (0, s);
            const float inR = fxBusBuffer.getSample (1, s);
            const float delayedL = delayLineL.popSample (0);
            const float delayedR = delayLineR.popSample (0);

            delayLineL.pushSample (0, inL + delayedL * fb);
            delayLineR.pushSample (0, inR + delayedR * fb);

            fxBusBuffer.setSample (0, s, inL * (1.0f - mix) + delayedL * mix);
            fxBusBuffer.setSample (1, s, inR * (1.0f - mix) + delayedR * mix);
        }
    }

    if (apvts.getRawParameterValue ("reverb_on")->load() > 0.5f)
    {
        juce::dsp::Reverb::Parameters revParams;
        revParams.roomSize = apvts.getRawParameterValue ("reverb_size")->load();
        revParams.damping  = apvts.getRawParameterValue ("reverb_damp")->load();
        revParams.width    = apvts.getRawParameterValue ("reverb_width")->load();
        revParams.wetLevel = apvts.getRawParameterValue ("reverb_mix")->load();
        revParams.dryLevel = 1.0f - revParams.wetLevel * 0.5f;
        reverb.setParameters (revParams);
        reverb.process (fxContext);
    }

    const float masterGain = apvts.getRawParameterValue ("master_gain")->load();
    const int outCh = juce::jmin (2, buffer.getNumChannels());
    for (int ch = 0; ch < outCh; ++ch)
    {
        buffer.addFrom (ch, 0, dryBusBuffer, ch, 0, numSamples);
        buffer.addFrom (ch, 0, fxBusBuffer, ch, 0, numSamples);
    }

    for (int s = 0; s < numSamples; ++s)
    {
        float l = colorLowShelfL.processSample (buffer.getSample (0, s));
        l = colorHighShelfL.processSample (l) * masterGain;
        float r = colorLowShelfR.processSample (buffer.getSample (1, s));
        r = colorHighShelfR.processSample (r) * masterGain;

        const float outL = juce::jlimit (-1.0f, 1.0f, l);
        const float outR = juce::jlimit (-1.0f, 1.0f, r);
        if (buffer.getNumChannels() > 0) buffer.setSample (0, s, outL);
        if (buffer.getNumChannels() > 1) buffer.setSample (1, s, outR);

        const int wp = masterScopeWritePos.fetch_add (1, std::memory_order_relaxed) & (scopeSize - 1);
        masterScopeBuffer[static_cast<size_t> (wp)].store (0.5f * (outL + outR), std::memory_order_relaxed);
    }
}

void BlendMesh5AudioProcessor::getMasterScopeSamples (std::vector<float>& dest) const
{
    dest.resize (scopeSize);
    const int wp = masterScopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < scopeSize; ++i)
    {
        const int idx = (wp + i) & (scopeSize - 1);
        dest[static_cast<size_t> (i)] = masterScopeBuffer[static_cast<size_t> (idx)].load (std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* BlendMesh5AudioProcessor::createEditor()
{
    return new BlendMesh5AudioProcessorEditor (*this);
}

void BlendMesh5AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BlendMesh5AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlendMesh5AudioProcessor();
}

void BlendMesh5AudioProcessor::getCompiledPreviewPeaks (std::vector<float>& mins, std::vector<float>& maxs, int numBins) const
{
    mins.assign (static_cast<size_t> (numBins), 0.0f);
    maxs.assign (static_cast<size_t> (numBins), 0.0f);
    if (numBins <= 0) return;

    const float timbre     = apvts.getRawParameterValue ("blend_timbre")->load();
    const float masterGain = apvts.getRawParameterValue ("master_gain")->load();

    const juce::ScopedLock sl (sampleLock);
    bool anyLoaded = false;

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        const auto& slot = slots[i];
        if (!slot.loaded || slot.buffer.getNumSamples() <= 1)
            continue;
        anyLoaded = true;

        auto p = "slot" + juce::String (i + 1) + "_";
        const float level   = apvts.getRawParameterValue (p + "level")->load();
        const bool invert   = apvts.getRawParameterValue (p + "polarity")->load() > 0.5f;
        const bool loopOn   = apvts.getRawParameterValue (p + "loop")->load() > 0.5f;
        const float lStartN = loopOn ? apvts.getRawParameterValue (p + "loopStart")->load() : 0.0f;
        const float lEndN   = loopOn ? std::max (lStartN + 0.01f, apvts.getRawParameterValue (p + "loopEnd")->load()) : 1.0f;

        const float slotNorm = static_cast<float> (i) / static_cast<float> (NUM_SLOTS - 1);
        const float dist = std::abs (slotNorm - timbre);
        const float timbreWeight = juce::jlimit (0.25f, 1.25f, 1.25f - dist * 1.15f);
        const float gain = level * (invert ? -1.0f : 1.0f) * timbreWeight * masterGain;

        const int total = slot.buffer.getNumSamples();
        const int rStart = juce::jlimit (0, total - 1, static_cast<int> (lStartN * (total - 1)));
        const int rEnd   = juce::jlimit (rStart + 1, total, static_cast<int> (lEndN * total));
        const int span   = std::max (1, rEnd - rStart);
        const float* data = slot.buffer.getReadPointer (0);

        for (int b = 0; b < numBins; ++b)
        {
            const int idxA = rStart + (b * span) / numBins;
            const int idxB = std::min (rEnd, rStart + std::max ((b * span) / numBins + 1, ((b + 1) * span) / numBins));
            float mn = 1.0f, mx = -1.0f;
            const int step = std::max (1, (idxB - idxA) / 16);
            for (int k = idxA; k < idxB; k += step)
            {
                const float v = data[k] * gain;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            if (idxA < idxB)
            {
                mins[static_cast<size_t> (b)] += mn;
                maxs[static_cast<size_t> (b)] += mx;
            }
        }
    }

    if (!anyLoaded) return;

    float maxAbs = 1.0f;
    for (int b = 0; b < numBins; ++b)
        maxAbs = std::max (maxAbs, std::max (std::abs (mins[static_cast<size_t> (b)]), std::abs (maxs[static_cast<size_t> (b)])));

    if (maxAbs > 1.0f)
    {
        for (int b = 0; b < numBins; ++b)
        {
            mins[static_cast<size_t> (b)] /= maxAbs;
            maxs[static_cast<size_t> (b)] /= maxAbs;
        }
    }
}
