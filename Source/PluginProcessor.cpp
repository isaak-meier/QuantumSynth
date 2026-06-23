#include "PluginProcessor.h"
#include "PluginEditor.h"

QuantumSynthAudioProcessor::QuantumSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void QuantumSynthAudioProcessor::prepareToPlay (double, int)
{
}

void QuantumSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    // ponytail: silent until the Molecule oscillators exist
    buffer.clear();
}

juce::AudioProcessorEditor* QuantumSynthAudioProcessor::createEditor()
{
    return new QuantumSynthAudioProcessorEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuantumSynthAudioProcessor();
}
