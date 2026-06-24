#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ItpParser.h"
#include "GroParser.h"
#include "SoundSynth.h"
#include "Integrator.h"

QuantumSynthAudioProcessor::QuantumSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // ponytail: hardcoded dev path — a loaded plugin has no useful cwd.
    // Replace with a file picker / bundled resource when there's a real UI.
    try {
        const juce::String dir = "/Users/isaak/Local System/Super Code/Git/QuantumSynth/Test/";
        molecule = parseItpFile ((dir + "CO2.itp").toStdString());
        parseGroFileInto (molecule, (dir + "CO2.gro").toStdString());
        // ponytail: kick a bond off equilibrium so the sim has something to do.
        if (! molecule.atoms.empty()) {
            molecule.atoms.front().x += 0.4;
            molecule.atoms.front().y += 0.4;
        }
    } catch (const std::exception& e) {
        molecule.name = std::string ("load failed: ") + e.what();
    }
    initialAtoms = molecule.atoms;   // snapshot the launch state for reset
}

void QuantumSynthAudioProcessor::resetMolecule()
{
    // ponytail: racy write vs. the sim thread; benign for a toy.
    for (size_t i = 0; i < molecule.atoms.size() && i < initialAtoms.size(); ++i)
        molecule.atoms[i] = initialAtoms[i];   // position + velocity (bonds untouched)
}

void QuantumSynthAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    tSeconds = 0.0;
}

void QuantumSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    const double dt = 1.0 / sampleRate;
    const float gain = outputGain.load (std::memory_order_relaxed);

    // Merge on-screen keyboard presses into the incoming MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Sim runs here, on the audio thread, so the modulation evolves smoothly
    // per sample instead of in graphics-frame steps. simSpeed matches the old
    // visual pace (was 60fps * 20 substeps * 0.006).
    constexpr double simSpeed = 7.2;
    const double simDt = simSpeed / sampleRate;

    // ponytail: reads molecule positions that the editor's sim thread writes —
    // a benign race for a toy; snapshot/lock if it audibly glitches.
    auto render = [&] (int from, int to) {
        const double freq = currentNote >= 0
            ? juce::MidiMessage::getMidiNoteInHertz (currentNote) : 0.0;
        for (int n = from; n < to; ++n) {
            step (molecule, simDt);                                  // advance physics
            const float s = currentNote < 0 ? 0.0f                    // gated silent
                          : gain * (float) moleculeSample (molecule, tSeconds, freq);
            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, n, s);

            const int w = scopeWrite.load (std::memory_order_relaxed);
            scope[w & (scopeSize - 1)] = s;
            scopeWrite.store (w + 1, std::memory_order_relaxed);
            tSeconds += dt;
        }
    };

    int cursor = 0;
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        const int ts = juce::jlimit (0, numSamples, meta.samplePosition);
        render (cursor, ts);
        cursor = ts;
        if (msg.isNoteOn())
            currentNote = msg.getNoteNumber();
        else if (msg.isNoteOff() && msg.getNoteNumber() == currentNote)
            currentNote = -1;
    }
    render (cursor, numSamples);
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
