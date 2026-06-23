#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ItpParser.h"

QuantumSynthAudioProcessor::QuantumSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // ponytail: hardcoded dev path — a loaded plugin has no useful cwd.
    // Replace with a file picker / bundled resource when there's a real UI.
    try {
        molecule = parseItpFile ("/Users/isaak/Local System/Super Code/Git/QuantumSynth/Test/CO2.itp");
    } catch (const std::exception& e) {
        molecule.name = std::string ("load failed: ") + e.what();
    }
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
