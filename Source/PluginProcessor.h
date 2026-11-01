#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>

#include "dsp/DistortionEngine.h"
#include "dsp/TapeEngine.h"

class AsunderAudioProcessor : public juce::AudioProcessor
{
public:
    AsunderAudioProcessor();
    ~AsunderAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override               { return true; }

    const juce::String getName() const override   { return JucePlugin_Name; }
    bool acceptsMidi() const override             { return false; }
    bool producesMidi() const override            { return false; }
    bool isMidiEffect() const override            { return false; }
    double getTailLengthSeconds() const override  { return 0.0; }

    int getNumPrograms() override                 { return 1; }
    int getCurrentProgram() override              { return 0; }
    void setCurrentProgram(int) override          {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    juce::AudioProcessorValueTreeState& getState() { return parameters; }

    asunder::DistortionEngine& getEngine() { return engine; }

    // --- metering for the tape (Oxide-style) visualizer --------------------
    // Levels are smoothed linear amplitudes (0..~1.3). Thread-safe single reader.
    float getInLevel()  const { return inLevel.load (std::memory_order_relaxed); }
    float getOutLevel() const { return outLevel.load (std::memory_order_relaxed); }

    // Copies up to `num` of the most recent output samples (oldest→newest) into
    // dest. Returns the number written. Lock-free snapshot (cosmetic tearing ok).
    int   copyWaveform(float* dest, int num) const;
    int   getWaveformCapacity() const { return kWaveSize; }

private:
    void applyQuality(bool force);

    asunder::DistortionEngine engine;
    asunder::TapeEngine       tape;

    // One oversampler per quality tier: 1x, 2x, 4x, 8x (indices 0..3).
    static constexpr int kNumQuality = 4;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumQuality> oversamplers;
    int  activeQuality = 2;         // default High (4x)
    int  numChannels   = 2;
    double baseSampleRate = 48000.0;

    // Metering.
    std::atomic<float> inLevel { 0.0f };
    std::atomic<float> outLevel { 0.0f };

    static constexpr int kWaveSize = 1024;
    std::array<float, kWaveSize> waveform {};
    std::atomic<int>   waveWrite { 0 };
    int                waveDecimCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AsunderAudioProcessor)
};
