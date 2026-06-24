#pragma once

#include <JuceHeader.h>
#include "Molecule.h"

class QuantumSynthAudioProcessor : public juce::AudioProcessor
{
public:
    QuantumSynthAudioProcessor();
    ~QuantumSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    Molecule molecule;   // loaded from the .itp at construction
    void resetMolecule();   // restore atoms to their initial position & velocity
    void kickMolecule();    // apply a fresh random kick on demand
    juce::String loadMolecule (const juce::File& itp, const juce::File& gro);  // "" = ok, else error
    juce::String saveMolecule (const juce::File& itp) const;                   // "" = ok, else error
    void setStiffnessPercent (double percent);      // % change from each bond's original k
    void setAngleStiffnessPercent (double percent); // % change from each angle's original k
    void setMassScale (double factor);              // multiply each atom's original mass

    juce::MidiKeyboardState keyboardState;   // shared with the editor's keyboard
    std::atomic<float> outputGain { 0.2f };  // molecule signal level, driven by the knob
    std::atomic<bool> usePhase { true };     // include the per-bond phase term?

    // Scope ring buffer of recent output samples, for the editor's waveform
    // display. ponytail: plain array, racy GUI read — it's a scope, not data.
    static constexpr int scopeSize = 1024;   // power of two
    float scope[scopeSize] = {};
    std::atomic<int> scopeWrite { 0 };

private:
    void applyKick();   // random per-axis nudge of the front atom

    double sampleRate = 44100.0;
    double tSeconds = 0.0;        // oscillator time, advanced per sample
    int currentNote = -1;         // held MIDI note, -1 = none (monophonic)
    std::vector<Atom> initialAtoms;   // snapshot for resetMolecule()
    std::vector<double> baseBondK;    // original bond stiffness, for % changes
    std::vector<double> baseAngleK;   // original angle stiffness, for % changes
    std::vector<double> baseMass;     // original atom masses, for mass scaling

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthAudioProcessor)
};
