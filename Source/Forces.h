#pragma once

#include "Molecule.h"
#include <vector>
#include <cmath>

struct Vec3 { double x = 0.0, y = 0.0, z = 0.0; };

// Computes the net force on each atom of a Molecule from its harmonic bonds.
class Forces {
public:
    explicit Forces (const Molecule & m) : mol (m) {}

    // Returns one force vector per atom, indexed like Molecule::atoms.
    std::vector<Vec3> compute() const {
        std::vector<Vec3> f (mol.atoms.size());   // zero-initialised

        for (const auto & b : mol.bonds) {
            const size_t i1 = (size_t) (b.atom1 - mol.atoms.data());
            const size_t i2 = (size_t) (b.atom2 - mol.atoms.data());
            const Atom & a1 = mol.atoms[i1];
            const Atom & a2 = mol.atoms[i2];

            // displacement a1 -> a2 and its length
            const double dx = a2.x - a1.x, dy = a2.y - a1.y, dz = a2.z - a1.z;
            const double r = std::sqrt (dx * dx + dy * dy + dz * dz);
            if (r < 1e-12) continue;              // coincident atoms: no direction

            // s = k (r - r0); force on a1 is +s*u, on a2 is -s*u  (u = d/r)
            const double s = b.k * (r - b.eqBondLength) / r;   // fold 1/r in here
            f[i1].x += s * dx; f[i1].y += s * dy; f[i1].z += s * dz;
            f[i2].x -= s * dx; f[i2].y -= s * dy; f[i2].z -= s * dz;
        }
        return f;
    }

private:
    const Molecule & mol;
};
