#include "PluginProcessor.h"
#include "PluginEditor.h"

AsunderAudioProcessor::AsunderAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters",
        {
            std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("tone", "Tone", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("bias", "Bias", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("output", "Output", 0.0f, 2.0f, 1.0f),
            std::make_unique<juce::AudioParameterChoice>("type", "Type",
                juce::StringArray { "Hard Clip", "Soft Clip", "Tube", "Wave Fold", "Bit Crush", "Sample Rate" }, 2),
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
    p.drive  = param("drive");
    p.tone   = param("tone");
    p.bias   = param("bias");
    p.mix    = param("mix");
    p.output = param("output");
    p.type   = juce::roundToInt(param("type"));
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
