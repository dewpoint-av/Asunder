#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Draws the current distortion transfer curve (input -> output) over a grid,
// with the dry y=x line as a reference. Reads `type` and `drive` live from the
// value tree so it always matches the DSP exactly.
class WaveshapeVisualizer : public juce::Component,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    explicit WaveshapeVisualizer(juce::AudioProcessorValueTreeState& state);
    ~WaveshapeVisualizer() override;

    void paint(juce::Graphics& g) override;

private:
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { repaint(); }

    float drive() const;
    int   type() const;

    juce::AudioProcessorValueTreeState& state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveshapeVisualizer)
};
