#pragma once

#include "Molecule.h"
#include <cmath>

// Oscillator shapes, selectable per render. All period 2*pi, range [-1, 1],
// phase-aligned with sine so the bond phase term stays meaningful.
enum class Wave { Sine = 0, Triangle = 1, Sawtooth = 2, Square = 3 };

inline double oscillate (int wave, double x) {
    switch (wave) {
        case (int) Wave::Triangle: return (2.0 / M_PI) * std::asin (std::sin (x));
        case (int) Wave::Sawtooth: { double p = x / (2.0 * M_PI); return 2.0 * (p - std::floor (p)) - 1.0; }
        case (int) Wave::Square:   return std::sin (x) >= 0.0 ? 1.0 : -1.0;
        default:                   return std::sin (x);
    }
}

// One wave per bond, summed:
//   amplitude = uniform (1 per bond) — overall level is the gain control;
//   phase     = the bond's displacement from equilibrium (r - r0);
//   sample(t) = average over bonds of osc(2*pi*freq*t + phase).
// Averaging keeps the level consistent regardless of bond count, so volume no
// longer swings with how stretched the molecule is.
//
// freq defaults to 0 (no carrier) — MIDI supplies it.
inline double moleculeSample (const Molecule & m, double t, double freqHz = 0.0,
                              bool usePhase = true, int wave = 0) {
    const double w = 2.0 * M_PI * freqHz * t;
    const int nb = (int) m.bonds.size();
    if (nb == 0) return 0.0;
    double out = 0.0;
    for (const auto & b : m.bonds) {
        const double dx = b.atom2->x - b.atom1->x;
        const double dy = b.atom2->y - b.atom1->y;
        const double dz = b.atom2->z - b.atom1->z;
        const double r = std::sqrt (dx * dx + dy * dy + dz * dz);
        if (r < 1e-12) continue;
        const double phase = usePhase ? (r - b.eqBondLength) : 0.0;   // displacement = phase
        out += oscillate (wave, w + phase);
    }
    return out / nb;
}
