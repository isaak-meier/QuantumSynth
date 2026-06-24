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
        voices = {};   // silence any held notes on reload
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

void QuantumSynthAudioProcessor::setHeldAtom (int index, double x, double y)
{
    heldX.store (x, std::memory_order_relaxed);
    heldY.store (y, std::memory_order_relaxed);
    heldAtom.store (index, std::memory_order_relaxed);
}

void QuantumSynthAudioProcessor::clearHeldAtom()
{
    heldAtom.store (-1, std::memory_order_relaxed);
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
    // per sample instead of in graphics-frame steps. The timestep dial sets
    // simSpeed (sim units/sec).
    const double simDt = simSpeed.load (std::memory_order_relaxed) / sampleRate;

    const bool ph = usePhase.load (std::memory_order_relaxed);
    const int  wv = waveform.load (std::memory_order_relaxed);
    const float attRate = (float) (1.0 / (0.005 * sampleRate));   // ~5ms attack
    const float relRate = (float) (1.0 / (0.080 * sampleRate));   // ~80ms release

    // ponytail: reads molecule positions that the editor's sim thread writes —
    // a benign race for a toy; snapshot/lock if it audibly glitches.
    auto render = [&] (int from, int to) {
        for (int n = from; n < to; ++n) {
            step (molecule, simDt);                                  // advance physics

            // Pin a dragged atom to the mouse target; the rest follow via bonds.
            const int hi = heldAtom.load (std::memory_order_relaxed);
            if (hi >= 0 && hi < (int) molecule.atoms.size()) {
                Atom& a = molecule.atoms[(size_t) hi];
                a.x = heldX.load (std::memory_order_relaxed);
                a.y = heldY.load (std::memory_order_relaxed);
                a.vx = a.vy = a.vz = 0.0;
            }

            // Sum all active voices reading the shared molecule at their pitch.
            float mix = 0.0f;
            for (auto& v : voices) {
                if (v.note < 0 && v.level <= 0.0f) continue;
                v.level = v.held ? juce::jmin (1.0f, v.level + attRate)
                                 : juce::jmax (0.0f, v.level - relRate);
                if (! v.held && v.level <= 0.0f) { v.note = -1; continue; }   // free spent voice
                mix += v.level * (float) moleculeSample (molecule, tSeconds, v.freq, ph, wv);
            }
            const float s = gain * mix;
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
        if (msg.isNoteOn())       voiceNoteOn (msg.getNoteNumber());
        else if (msg.isNoteOff()) voiceNoteOff (msg.getNoteNumber());
    }
    render (cursor, numSamples);
}

void QuantumSynthAudioProcessor::voiceNoteOn (int note)
{
    // Reuse a voice already on this note, else a free slot, else steal the quietest.
    int slot = -1, freeSlot = -1, quietest = 0;
    for (int i = 0; i < kMaxVoices; ++i) {
        if (voices[i].note == note) { slot = i; break; }
        if (voices[i].note < 0 && voices[i].level <= 0.0f && freeSlot < 0) freeSlot = i;
        if (voices[i].level < voices[quietest].level) quietest = i;
    }
    if (slot < 0) slot = (freeSlot >= 0) ? freeSlot : quietest;
    voices[slot].note = note;
    voices[slot].freq = juce::MidiMessage::getMidiNoteInHertz (note);
    voices[slot].held = true;
}

void QuantumSynthAudioProcessor::voiceNoteOff (int note)
{
    for (auto& v : voices)
        if (v.note == note && v.held) v.held = false;   // enter release
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
