// Self-check for the molecule->sound mapping. Build & run from Test/:
//   c++ -std=c++17 test_sound.cpp -o test_sound && ./test_sound
#include "../Source/SoundSynth.h"
#include <cassert>
#include <cmath>
#include <iostream>

// Build a 2-atom, 1-bond molecule with atom2 at the given offset from origin.
static Molecule oneBond(double x2, double y2, double z2, double r0) {
    Molecule m;
    m.atoms.resize(2);
    m.atoms[0] = Atom{};
    m.atoms[1].x = x2; m.atoms[1].y = y2; m.atoms[1].z = z2;
    Bond b; b.eqBondLength = r0; b.k = 1.0;
    b.atom1 = &m.atoms[0]; b.atom2 = &m.atoms[1];
    m.bonds.push_back(b);
    return m;
}

int main() {
    const double f = 100.0;

    // Displacement is the phase. Stretch so disp = pi/2 (r = r0 + pi/2).
    // At t=0: osc(sine, 0 + pi/2) = sin(pi/2) = 1.  (unit amplitude per bond)
    {
        Molecule m = oneBond(0, 1.0 + M_PI / 2.0, 0, 1.0);
        assert(std::abs(moleculeSample(m, 0.0, f) - 1.0) < 1e-9);
    }

    // At equilibrium disp = 0 -> phase 0 -> sin(2*pi*f*t); at t=0 that's 0.
    // But the molecule is NOT silent at equilibrium for t!=0 (constant amplitude).
    {
        Molecule m = oneBond(0, 1, 0, 1);
        assert(std::abs(moleculeSample(m, 0.0, f)) < 1e-9);            // t=0 -> sin(0)=0
        assert(std::abs(moleculeSample(m, 0.0025, f) - std::sin(2.0 * M_PI * f * 0.0025)) < 1e-9);
    }

    // Phase disabled -> phase 0 regardless of displacement; at t=0 -> 0.
    {
        Molecule m = oneBond(0, 1.0 + M_PI / 2.0, 0, 1.0);
        assert(std::abs(moleculeSample(m, 0.0, f, /*usePhase=*/false)) < 1e-9);
    }

    // Amplitude is bond-count independent (averaged): two bonds with the same
    // displacement read the same as one (not double).
    {
        Molecule m;
        m.atoms.resize(3);
        m.atoms[1].y =  1.0 + M_PI / 2.0;   // bond 0 stretched, disp = pi/2
        m.atoms[2].y = -(1.0 + M_PI / 2.0); // bond 1 stretched, disp = pi/2
        Bond b0; b0.eqBondLength = 1.0; b0.atom1 = &m.atoms[0]; b0.atom2 = &m.atoms[1];
        Bond b1; b1.eqBondLength = 1.0; b1.atom1 = &m.atoms[0]; b1.atom2 = &m.atoms[2];
        m.bonds = { b0, b1 };
        assert(std::abs(moleculeSample(m, 0.0, f) - 1.0) < 1e-9);   // not 2.0
    }

    // Oscillator shapes at known phases.
    assert(std::abs(oscillate(0, M_PI / 2) - 1.0) < 1e-9);   // sine peak
    assert(std::abs(oscillate(1, 0.0)) < 1e-9);              // triangle zero-crossing
    assert(std::abs(oscillate(2, M_PI) - 0.0) < 1e-9);       // sawtooth midpoint
    assert(std::abs(oscillate(3, 0.5) - 1.0) < 1e-9);        // square high
    assert(std::abs(oscillate(3, -0.5) + 1.0) < 1e-9);       // square low

    std::cout << "all assertions passed\n";
}
