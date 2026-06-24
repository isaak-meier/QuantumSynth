#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ItpParser.h"
#include "GroParser.h"
#include "SoundSynth.h"
#include "Integrator.h"

#ifndef QS_MOLECULES_DIR
 #define QS_MOLECULES_DIR "Molecules"   // CMake injects the real path
#endif

QuantumSynthAudioProcessor::QuantumSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default molecule; the editor's dropdown can switch to others.
    const auto co2 = juce::File (QS_MOLECULES_DIR).getChildFile ("CO2");
    const auto err = loadMolecule (co2.getChildFile ("CO2.itp"), co2.getChildFile ("CO2.gro"));
    if (err.isNotEmpty())
        molecule.name = ("load failed: " + err).toStdString();
}

juce::String QuantumSynthAudioProcessor::loadMolecule (const juce::File& itp, const juce::File& gro)
{
    try {
        Molecule m = parseItpFile (itp.getFullPathName().toStdString());
        if (gro.existsAsFile())
            parseGroFileInto (m, gro.getFullPathName().toStdString());

        // Suspend audio while swapping in the new molecule (processBlock reads it).
        // ponytail: the GL thread's read is still racy, like the rest of the sim.
        suspendProcessing (true);
        molecule = std::move (m);       // vector move keeps atom addresses, so Bond ptrs stay valid
        initialAtoms = molecule.atoms;  // pristine .gro coords (pre-kick) for reset
        baseBondK.clear();              // snapshot original stiffness for % changes
        for (const auto& b : molecule.bonds) baseBondK.push_back (b.k);
        baseAngleK.clear();             // snapshot original angle stiffness for % changes
        for (const auto& a : molecule.angles) baseAngleK.push_back (a.k);
        baseMass.clear();               // snapshot original masses for mass scaling
        for (const auto& a : molecule.atoms) baseMass.push_back (a.mass);
        applyKick();   // off-equilibrium start; reset() returns to pristine coords then re-kicks
        currentNote = -1;
        suspendProcessing (false);
        return {};
    } catch (const std::exception& e) {
        return juce::String (e.what());
    }
}

void QuantumSynthAudioProcessor::kickMolecule()
{
    applyKick();
}

juce::String QuantumSynthAudioProcessor::saveMolecule (const juce::File& itp) const
{
    try {
        writeItpFile (molecule, itp.getFullPathName().toStdString());
        return {};
    } catch (const std::exception& e) {
        return juce::String (e.what());
    }
}

void QuantumSynthAudioProcessor::applyKick()
{
    // Random per-axis nudge of the front atom, each offset in [-0.3, 0.3)*r0,
    // so the sim has something to do. ponytail: racy vs. the sim thread.
    if (molecule.atoms.empty()) return;
    auto& rng = juce::Random::getSystemRandom();
    const double r0 = molecule.bonds.empty() ? 1.0 : molecule.bonds.front().eqBondLength;
    auto kick = [&] { return (rng.nextDouble() * 0.6 - 0.3) * r0; };   // [-0.3, 0.3)*r0
    Atom& a = molecule.atoms.front();
    a.x += kick();
    a.y += kick();
    a.z += kick();
}

void QuantumSynthAudioProcessor::setStiffnessPercent (double percent)
{
    // ponytail: racy write vs. the sim thread; benign for a toy.
    const double scale = 1.0 + percent / 100.0;   // -99% -> 0.01x, +100% -> 2x
    for (size_t i = 0; i < molecule.bonds.size() && i < baseBondK.size(); ++i)
        molecule.bonds[i].k = baseBondK[i] * scale;
}

void QuantumSynthAudioProcessor::setAngleStiffnessPercent (double percent)
{
    // ponytail: racy write vs. the sim thread; benign for a toy.
    const double scale = 1.0 + percent / 100.0;
    for (size_t i = 0; i < molecule.angles.size() && i < baseAngleK.size(); ++i)
        molecule.angles[i].k = baseAngleK[i] * scale;
}

void QuantumSynthAudioProcessor::setMassScale (double factor)
{
    // ponytail: racy write vs. the sim thread; benign for a toy.
    for (size_t i = 0; i < molecule.atoms.size() && i < baseMass.size(); ++i)
        molecule.atoms[i].mass = baseMass[i] * factor;
}

void QuantumSynthAudioProcessor::resetMolecule()
{
    // ponytail: racy write vs. the sim thread; benign for a toy.
    for (size_t i = 0; i < molecule.atoms.size() && i < initialAtoms.size(); ++i)
        molecule.atoms[i] = initialAtoms[i];   // position + velocity (bonds untouched)
    applyKick();   // re-apply a fresh random kick so the molecule moves again
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
                          : gain * (float) moleculeSample (molecule, tSeconds, freq,
                                                           usePhase.load (std::memory_order_relaxed));
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
