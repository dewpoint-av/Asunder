#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// Oxide-style tape metering: analog IN / OUT VU meters with swinging needles,
// a scrolling output waveform, and two slowly-rotating tape reels for flavour.
// Reads smoothed levels + a decimated waveform snapshot from the processor on a
// timer; no audio-thread work and no gradients/heavy paths (Windows-safe).
class TapeVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    explicit TapeVisualizer(AsunderAudioProcessor& p);
    ~TapeVisualizer() override;

    void setAccentColour(juce::Colour c) { accent = c; }

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    void drawVU(juce::Graphics& g, juce::Rectangle<float> r, float level, const juce::String& label);
    void drawReel(juce::Graphics& g, float cx, float cy, float radius, float phase);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> r);

    AsunderAudioProcessor& processor;
    juce::Colour accent { 0xffff9e5e };

    float reelPhase = 0.0f;
    std::array<float, 512> wave {};
    int waveLen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeVisualizer)
};
