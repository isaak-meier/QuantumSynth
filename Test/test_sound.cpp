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

    // Bond straight up (+y): r=2, r0=1 -> amp=1; angle to x-axis = pi/2.
    // At t=0: 1 * sin(0 + pi/2) = 1.
    {
        Molecule m = oneBond(0, 2, 0, 1);
        assert(std::abs(moleculeSample(m, 0.0, f) - 1.0) < 1e-9);
    }

    // Bond along +x: angle = 0 -> phase 0; at t=0 sin(0) = 0 regardless of amp.
    {
        Molecule m = oneBond(2, 0, 0, 1);   // amp = 1
        assert(std::abs(moleculeSample(m, 0.0, f) - 0.0) < 1e-9);
    }

    // Diagonal (1,1,0): r=sqrt2, amp=sqrt2-1, angle=pi/4; t=0 -> amp*sin(pi/4).
    {
        Molecule m = oneBond(1, 1, 0, 1);
        const double expect = (std::sqrt(2.0) - 1.0) * std::sin(M_PI / 4.0);
        assert(std::abs(moleculeSample(m, 0.0, f) - expect) < 1e-9);
    }

    // At equilibrium amp = 0 -> silence at all times.
    {
        Molecule m = oneBond(0, 1, 0, 1);
        assert(std::abs(moleculeSample(m, 0.123, f)) < 1e-9);
    }

    // Compressed bond: r=0.5, r0=1 -> amp=0.5, but compression adds pi to phase.
    // angle = pi/2, phase = pi/2 + pi; t=0 -> 0.5 * sin(3pi/2) = -0.5.
    {
        Molecule m = oneBond(0, 0.5, 0, 1);
        assert(std::abs(moleculeSample(m, 0.0, f) - (-0.5)) < 1e-9);
    }

    // Huge stretch: r=5, r0=1 -> disp=4, clamped to 2*r0 = 2.
    // angle = pi/2, t=0 -> 2.0, not 4.
    {
        Molecule m = oneBond(0, 5, 0, 1);
        assert(std::abs(moleculeSample(m, 0.0, f) - 2.0) < 1e-9);
    }

    // Phase disabled: phase term is 0, so a vertical bond (amp 1) reads
    // amp*sin(w) instead of amp*sin(w + pi/2). At t=0 that's 0, not 1.
    {
        Molecule m = oneBond(0, 2, 0, 1);
        assert(std::abs(moleculeSample(m, 0.0, f, /*usePhase=*/false)) < 1e-9);
    }

    std::cout << "all assertions passed\n";
}
