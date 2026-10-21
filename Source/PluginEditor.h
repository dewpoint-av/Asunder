#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "AsunderLookAndFeel.h"

class AsunderAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AsunderAudioProcessorEditor(AsunderAudioProcessor&);
    ~AsunderAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AsunderAudioProcessor& processor;

    AsunderLookAndFeel lookAndFeel;   // value member, declared before impl (destroyed last)

    class Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AsunderAudioProcessorEditor)
};
