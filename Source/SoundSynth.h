#pragma once

#include "Molecule.h"
#include <cmath>

// One sine wave per bond, summed:
//   amplitude = |displacement from equilibrium|, capped at 2*r0
//   phase     = angle of the bond vector (a1->a2) to the x-axis (1,0,0),
//               plus pi when the bond is compressed (restores the stretch/
//               compress sign so out-of-sync bonds interfere)
//   sample(t) = sum over bonds of amp * sin(2*pi*freq*t + phase)
//
// freq defaults to 0 (no carrier) — MIDI will supply it later. Per-bond pitch
// (e.g. sqrt(k/reduced-mass)) is another option if MIDI isn't the only driver.
inline double moleculeSample (const Molecule & m, double t, double freqHz = 0.0,
                              bool usePhase = true) {
    const double w = 2.0 * M_PI * freqHz * t;
    double out = 0.0;
    for (const auto & b : m.bonds) {
        const double dx = b.atom2->x - b.atom1->x;
        const double dy = b.atom2->y - b.atom1->y;
        const double dz = b.atom2->z - b.atom1->z;
        const double r = std::sqrt (dx * dx + dy * dy + dz * dz);
        if (r < 1e-12) continue;
        // amplitude = |displacement from equilibrium|, capped at twice r0
        const double disp  = r - b.eqBondLength;
        const double amp   = std::min (std::abs (disp), 2.0 * b.eqBondLength);
        const double phase = usePhase
            ? std::acos (std::clamp (dx / r, -1.0, 1.0))    // angle to (1,0,0)
              + (disp < 0.0 ? M_PI : 0.0)                   // compression flip
            : 0.0;
        out += amp * std::sin (w + phase);
    }
    return out;
}
