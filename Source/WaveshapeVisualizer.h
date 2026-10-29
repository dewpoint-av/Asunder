#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Draws the distortion transfer curve (input -> output) for the currently
// selected band, with the dry y=x line as a reference. Reads the selected
// band's `type` and `drive` live from the value tree so it always matches the
// DSP exactly.
class WaveshapeVisualizer : public juce::Component,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    explicit WaveshapeVisualizer(juce::AudioProcessorValueTreeState& state);
    ~WaveshapeVisualizer() override;

    void setBand(int bandOneBased);

    void paint(juce::Graphics& g) override;

private:
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { repaint(); }

    juce::String typeParamId() const;
    juce::String driveParamId() const;

    float drive() const;
    int   type() const;

    juce::AudioProcessorValueTreeState& state;
    int band = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveshapeVisualizer)
};
