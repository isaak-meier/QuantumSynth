#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class QuantumSynthAudioProcessorEditor : public juce::AudioProcessorEditor {

public:
    explicit QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor&);
    ~QuantumSynthAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    QuantumSynthAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthAudioProcessorEditor)
};
