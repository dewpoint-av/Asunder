#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    const juce::StringArray kDistTypeNames { "Hard Clip", "Soft Clip", "Tube",
                                             "Wave Fold", "Bit Crush", "Sample Rate" };
    const juce::StringArray kTapeColorNames { "Tape", "Tube", "FET", "Class A" };
    const juce::StringArray kQualityNames   { "Draft 1x", "Standard 2x", "High 4x", "Ultra 8x" };

    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        using FloatP  = juce::AudioParameterFloat;
        using ChoiceP = juce::AudioParameterChoice;
        using IntP    = juce::AudioParameterInt;
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // ---- Global mode + quality ----------------------------------------
        layout.add (std::make_unique<ChoiceP> ("mode", "Mode",
            juce::StringArray { "Distortion", "Tape" }, 0));
        layout.add (std::make_unique<ChoiceP> ("quality", "Quality", kQualityNames, 2));

        // ---- Per-band distortion (5 bands) --------------------------------
        for (int b = 1; b <= asunder::kMaxBands; ++b)
        {
            const juce::String s = juce::String (b);
            layout.add (std::make_unique<FloatP> ("b" + s + "drive", "Band " + s + " Drive", 0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<FloatP> ("b" + s + "tone",  "Band " + s + " Tone",  0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<FloatP> ("b" + s + "bias",  "Band " + s + " Bias",  0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<ChoiceP> ("b" + s + "type", "Band " + s + " Type", kDistTypeNames, 2));
        }
        layout.add (std::make_unique<IntP>   ("band",  "Band",  1, 5, 1));
        layout.add (std::make_unique<FloatP> ("mix",   "Mix",   0.0f, 1.0f, 1.0f));
        layout.add (std::make_unique<FloatP> ("output","Output",0.0f, 2.0f, 1.0f));
        layout.add (std::make_unique<IntP>   ("bands", "Bands", 1, 5, 1));
        layout.add (std::make_unique<FloatP> ("xover1", "X-Over 1", juce::NormalisableRange<float> (50.0f, 20000.0f, 0.0f, 0.30f), 300.0f));
        layout.add (std::make_unique<FloatP> ("xover2", "X-Over 2", juce::NormalisableRange<float> (50.0f, 20000.0f, 0.0f, 0.30f), 1000.0f));
        layout.add (std::make_unique<FloatP> ("xover3", "X-Over 3", juce::NormalisableRange<float> (50.0f, 20000.0f, 0.0f, 0.30f), 3000.0f));
        layout.add (std::make_unique<FloatP> ("xover4", "X-Over 4", juce::NormalisableRange<float> (50.0f, 20000.0f, 0.0f, 0.30f), 8000.0f));

        // ---- Tape saturation ----------------------------------------------
        layout.add (std::make_unique<ChoiceP> ("tapeColor", "Tape Color", kTapeColorNames, 0));
        layout.add (std::make_unique<FloatP> ("tapeDrive",  "Tape Drive",  0.0f, 1.0f, 0.35f));
        layout.add (std::make_unique<FloatP> ("tapeBias",   "Tape Bias",   0.0f, 1.0f, 0.5f));
        layout.add (std::make_unique<FloatP> ("tapeTone",   "Tape Tone",   0.0f, 1.0f, 0.6f));
        layout.add (std::make_unique<FloatP> ("tapeBump",   "Tape Bump",   0.0f, 1.0f, 0.3f));
        layout.add (std::make_unique<FloatP> ("tapePunch",  "Tape Punch",  0.0f, 1.0f, 0.5f));
        layout.add (std::make_unique<FloatP> ("tapeWow",    "Tape Wow",    0.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<FloatP> ("tapeNoise",  "Tape Hiss",   0.0f, 1.0f, 0.0f));
        layout.add (std::make_unique<FloatP> ("tapeMix",    "Tape Mix",    0.0f, 1.0f, 1.0f));
        layout.add (std::make_unique<FloatP> ("tapeOutput", "Tape Output", 0.0f, 2.0f, 1.0f));

        return layout;
    }
}

AsunderAudioProcessor::AsunderAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", makeLayout())
{
}

AsunderAudioProcessor::~AsunderAudioProcessor() = default;

void AsunderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;
    numChannels    = juce::jmax (1, getTotalNumInputChannels());

    // Pre-allocate one oversampler per quality tier. FIR equiripple half-band
    // stages give linear-phase, high-quality anti-aliasing for the waveshapers.
    using OS = juce::dsp::Oversampling<float>;
    for (int q = 0; q < kNumQuality; ++q)
    {
        if (q == 0)
        {
            // 1x (Draft): a single dummy stage = transparent passthrough, 0 latency.
            oversamplers[(size_t) q] = std::make_unique<OS> ((size_t) numChannels);
            oversamplers[(size_t) q]->addDummyOversamplingStage();
        }
        else
        {
            oversamplers[(size_t) q] = std::make_unique<OS> (
                (size_t) numChannels, (size_t) q,
                OS::filterHalfBandFIREquiripple, true, false);
        }
        oversamplers[(size_t) q]->initProcessing ((size_t) samplesPerBlock);
        oversamplers[(size_t) q]->reset();
    }

    // Tape buffers sized for the maximum oversampled rate.
    tape.prepare (sampleRate * (1 << (kNumQuality - 1)), samplesPerBlock * (1 << (kNumQuality - 1)));

    waveform.fill (0.0f);
    waveWrite.store (0, std::memory_order_relaxed);
    waveDecimCounter = 0;

    applyQuality (true);
}

void AsunderAudioProcessor::releaseResources()
{
    engine.reset();
    tape.reset();
}

void AsunderAudioProcessor::applyQuality(bool force)
{
    int q = juce::roundToInt (parameters.getRawParameterValue ("quality")->load());
    q = juce::jlimit (0, kNumQuality - 1, q);
    if (! force && q == activeQuality)
        return;

    activeQuality = q;
    const int    factor = 1 << q;
    const double osRate = baseSampleRate * factor;

    engine.prepare (osRate, 0);     // alloc-free: recomputes crossover coeffs
    tape.setSampleRate (osRate);    // alloc-free: recomputes tape coeffs
    if (oversamplers[(size_t) q])
    {
        oversamplers[(size_t) q]->reset();
        setLatencySamples (juce::roundToInt (oversamplers[(size_t) q]->getLatencyInSamples()));
    }
}

bool AsunderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void AsunderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (totalIn == 0 || totalOut == 0)
        return;

    const auto param = [&](const char* id) { return parameters.getRawParameterValue(id)->load(); };

    // Switch oversampling tier if the Quality control changed (alloc-free).
    applyQuality (false);

    const int   mode       = juce::roundToInt (param ("mode"));
    const int   numSamples = buffer.getNumSamples();
    const int   chans      = juce::jmin (numChannels, totalIn);

    // Input level (pre-process) for the IN meter.
    float inPeak = 0.0f;
    for (int c = 0; c < chans; ++c)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, 0, numSamples));

    // ---- oversample → process → downsample --------------------------------
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                        (size_t) numChannels, (size_t) numSamples);
    auto& os = *oversamplers[(size_t) activeQuality];
    auto osBlock = os.processSamplesUp (block);

    float* osL = osBlock.getChannelPointer (0);
    float* osR = (osBlock.getNumChannels() > 1) ? osBlock.getChannelPointer (1) : osL;
    const int osN = (int) osBlock.getNumSamples();

    if (mode == 1)   // ---- Tape saturation --------------------------------
    {
        asunder::TapeParams tp;
        tp.color  = juce::roundToInt (param ("tapeColor"));
        tp.drive  = param ("tapeDrive");
        tp.bias   = param ("tapeBias");
        tp.tone   = param ("tapeTone");
        tp.bump   = param ("tapeBump");
        tp.punch  = param ("tapePunch");
        tp.wow    = param ("tapeWow");
        tp.noise  = param ("tapeNoise");
        tp.mix    = param ("tapeMix");
        tp.output = param ("tapeOutput");
        tape.setParams (tp);
        tape.process (osL, osR, osL, osR, osN);
    }
    else             // ---- Multiband distortion ---------------------------
    {
        asunder::Params p;
        for (int b = 0; b < asunder::kMaxBands; ++b)
        {
            const juce::String s = juce::String (b + 1);
            p.drive[(size_t) b] = param (("b" + s + "drive").toRawUTF8());
            p.tone [(size_t) b] = param (("b" + s + "tone").toRawUTF8());
            p.bias [(size_t) b] = param (("b" + s + "bias").toRawUTF8());
            p.type [(size_t) b] = juce::roundToInt (param (("b" + s + "type").toRawUTF8()));
        }
        p.mix    = param ("mix");
        p.output = param ("output");
        p.bands  = juce::roundToInt (param ("bands"));
        p.xover0 = param ("xover1");
        p.xover1 = param ("xover2");
        p.xover2 = param ("xover3");
        p.xover3 = param ("xover4");
        engine.setParams (p);
        engine.process (osL, osR, osL, osR, osN);
    }

    os.processSamplesDown (block);

    // ---- metering (base rate) ---------------------------------------------
    float outPeak = 0.0f;
    for (int c = 0; c < chans; ++c)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, 0, numSamples));

    // One-pole ballistics (fast attack / slow release), single writer.
    const auto ballistic = [] (std::atomic<float>& env, float target)
    {
        float e = env.load (std::memory_order_relaxed);
        const float coeff = (target > e) ? 0.5f : 0.08f;
        e += (target - e) * coeff;
        env.store (e, std::memory_order_relaxed);
    };
    ballistic (inLevel,  inPeak);
    ballistic (outLevel, outPeak);

    // Decimated output waveform ring for the scrolling scope.
    const int decim = juce::jmax (1, (int) (baseSampleRate * 0.05 / kWaveSize));
    int w = waveWrite.load (std::memory_order_relaxed);
    const float* outL = buffer.getReadPointer (0);
    const float* outR = (totalOut > 1) ? buffer.getReadPointer (1) : outL;
    for (int s = 0; s < numSamples; ++s)
    {
        if (++waveDecimCounter >= decim)
        {
            waveDecimCounter = 0;
            waveform[(size_t) w] = 0.5f * (outL[s] + outR[s]);
            w = (w + 1) % kWaveSize;
        }
    }
    waveWrite.store (w, std::memory_order_relaxed);
}

int AsunderAudioProcessor::copyWaveform(float* dest, int num) const
{
    num = juce::jlimit (0, kWaveSize, num);
    const int w = waveWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < num; ++i)
        dest[i] = waveform[(size_t) ((w + i) % kWaveSize)];
    return num;
}

juce::AudioProcessorEditor* AsunderAudioProcessor::createEditor()
{
    return new AsunderAudioProcessorEditor(*this);
}

void AsunderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AsunderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AsunderAudioProcessor();
}
