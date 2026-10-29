#include "PluginProcessor.h"
#include "PluginEditor.h"

AsunderAudioProcessor::AsunderAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters",
        {
            // Per-band distortion (5 bands). Each band has its own drive, tone,
            // bias and distortion type, selected via the `band` parameter in the UI.
            std::make_unique<juce::AudioParameterFloat>("b1drive", "Band 1 Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b1tone",  "Band 1 Tone",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b1bias",  "Band 1 Bias",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterChoice>("b1type", "Band 1 Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),
            std::make_unique<juce::AudioParameterFloat>("b2drive", "Band 2 Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b2tone",  "Band 2 Tone",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b2bias",  "Band 2 Bias",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterChoice>("b2type", "Band 2 Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),
            std::make_unique<juce::AudioParameterFloat>("b3drive", "Band 3 Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b3tone",  "Band 3 Tone",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b3bias",  "Band 3 Bias",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterChoice>("b3type", "Band 3 Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),
            std::make_unique<juce::AudioParameterFloat>("b4drive", "Band 4 Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b4tone",  "Band 4 Tone",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b4bias",  "Band 4 Bias",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterChoice>("b4type", "Band 4 Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),
            std::make_unique<juce::AudioParameterFloat>("b5drive", "Band 5 Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b5tone",  "Band 5 Tone",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("b5bias",  "Band 5 Bias",  0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterChoice>("b5type", "Band 5 Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),

            // UI: which band the editor is currently editing (1..5).
            std::make_unique<juce::AudioParameterInt>("band", "Band", 1, 5, 1),

            // Global.
            std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("output", "Output", 0.0f, 2.0f, 1.0f),
            std::make_unique<juce::AudioParameterInt>("bands", "Bands", 1, 5, 1),
            std::make_unique<juce::AudioParameterFloat>("xover1", "X-Over 1",
                juce::NormalisableRange<float>(50.0f, 20000.0f, 0.0f, 0.30f), 300.0f),
            std::make_unique<juce::AudioParameterFloat>("xover2", "X-Over 2",
                juce::NormalisableRange<float>(50.0f, 20000.0f, 0.0f, 0.30f), 1000.0f),
            std::make_unique<juce::AudioParameterFloat>("xover3", "X-Over 3",
                juce::NormalisableRange<float>(50.0f, 20000.0f, 0.0f, 0.30f), 3000.0f),
            std::make_unique<juce::AudioParameterFloat>("xover4", "X-Over 4",
                juce::NormalisableRange<float>(50.0f, 20000.0f, 0.0f, 0.30f), 8000.0f)
        })
{
}

AsunderAudioProcessor::~AsunderAudioProcessor() = default;

void AsunderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
}

void AsunderAudioProcessor::releaseResources()
{
    engine.reset();
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
    asunder::Params p;
    for (int b = 0; b < asunder::kMaxBands; ++b)
    {
        const juce::String s = juce::String(b + 1);
        p.drive[static_cast<size_t>(b)] = param(("b" + s + "drive").toRawUTF8());
        p.tone[static_cast<size_t>(b)]  = param(("b" + s + "tone").toRawUTF8());
        p.bias[static_cast<size_t>(b)]  = param(("b" + s + "bias").toRawUTF8());
        p.type[static_cast<size_t>(b)]  = juce::roundToInt(param(("b" + s + "type").toRawUTF8()));
    }
    p.mix    = param("mix");
    p.output = param("output");
    p.bands  = juce::roundToInt(param("bands"));
    p.xover0 = param("xover1");
    p.xover1 = param("xover2");
    p.xover2 = param("xover3");
    p.xover3 = param("xover4");
    engine.setParams(p);

    const int numSamples = buffer.getNumSamples();
    const float* inL = buffer.getReadPointer(0);
    const float* inR = (totalIn > 1) ? buffer.getReadPointer(1) : inL;
    float* outL = buffer.getWritePointer(0);
    float* outR = (totalOut > 1) ? buffer.getWritePointer(1) : outL;

    engine.process(inL, inR, outL, outR, numSamples);
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
