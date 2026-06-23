#include "PluginEditor.h"

QuantumSynthAudioProcessorEditor::QuantumSynthAudioProcessorEditor (QuantumSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (600, 400);
}

void QuantumSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2b2b));
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText (processorRef.molecule.name, getLocalBounds(), juce::Justification::centred);
}
