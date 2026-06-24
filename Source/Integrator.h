#pragma once

#include "Molecule.h"
#include "Forces.h"
#include <cmath>

// Largest allowed bond stretch, as a multiple of r0: |r - r0| <= MAX_STRETCH*r0.
inline constexpr double MAX_STRETCH = 1.5;

// Advance the molecule by one timestep dt using semi-implicit (symplectic)
// Euler: update velocity from the force, then position from the new velocity.
// Symplectic Euler keeps a harmonic oscillator's energy bounded (it won't blow
// up the way plain forward Euler does).
inline void step (Molecule & m, double dt) {
    const auto f = Forces (m).compute();
    for (size_t i = 0; i < m.atoms.size(); ++i) {
        Atom & a = m.atoms[i];
        if (a.mass <= 0.0) continue;             // immovable / unset mass
        const double inv = dt / a.mass;
        a.vx += f[i].x * inv;  a.vy += f[i].y * inv;  a.vz += f[i].z * inv;
        a.x  += a.vx * dt;     a.y  += a.vy * dt;     a.z  += a.vz * dt;
    }

    // Hard-limit every bond's stretch so atoms can't fly apart. Each bond pulls
    // its two atoms back along the axis, mass-weighted so the COM doesn't drift.
    // Bonds share atoms, so fixing one can break another — relax over a few
    // passes (early-out the common case where nothing is over the limit).
    // ponytail: position-only Gauss-Seidel; per-sample dt makes overshoot tiny.
    for (int iter = 0; iter < 16; ++iter) {
        bool any = false;
        for (const auto & b : m.bonds) {
            Atom & a1 = *b.atom1;
            Atom & a2 = *b.atom2;
            const double dx = a2.x - a1.x, dy = a2.y - a1.y, dz = a2.z - a1.z;
            const double r = std::sqrt (dx * dx + dy * dy + dz * dz);
            const double maxR = b.eqBondLength * (1.0 + MAX_STRETCH);
            if (r <= maxR + 1e-9 || r < 1e-12) continue;

            const double invm1 = a1.mass > 0.0 ? 1.0 / a1.mass : 0.0;
            const double invm2 = a2.mass > 0.0 ? 1.0 / a2.mass : 0.0;
            const double sum = invm1 + invm2;
            if (sum <= 0.0) continue;            // both immovable

            const double over = r - maxR;        // distance to pull back
            const double ux = dx / r, uy = dy / r, uz = dz / r;
            const double c1 = over * invm1 / sum, c2 = over * invm2 / sum;
            a1.x += ux * c1; a1.y += uy * c1; a1.z += uz * c1;   // a1 toward a2
            a2.x -= ux * c2; a2.y -= uy * c2; a2.z -= uz * c2;   // a2 toward a1
            any = true;
        }
        if (! any) break;
    }
}