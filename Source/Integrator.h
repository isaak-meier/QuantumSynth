#pragma once

#include "Molecule.h"
#include "Forces.h"

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
}