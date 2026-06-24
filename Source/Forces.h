#pragma once

#include "Molecule.h"
#include <vector>
#include <cmath>

struct Vec3 { double x = 0.0, y = 0.0, z = 0.0; };

// Computes the net force on each atom of a Molecule from its harmonic bonds
// and harmonic angles.
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

        // Harmonic angle: U = 1/2 k (theta - theta0)^2. theta is at vertex a2,
        // between u = a1-a2 and v = a3-a2. Force is -dU/dtheta * grad(theta).
        for (const auto & ang : mol.angles) {
            const size_t i1 = (size_t) (ang.atom1 - mol.atoms.data());
            const size_t i2 = (size_t) (ang.atom2 - mol.atoms.data());   // vertex
            const size_t i3 = (size_t) (ang.atom3 - mol.atoms.data());
            const Atom & v0 = mol.atoms[i2];

            const double ux = mol.atoms[i1].x - v0.x, uy = mol.atoms[i1].y - v0.y, uz = mol.atoms[i1].z - v0.z;
            const double vx = mol.atoms[i3].x - v0.x, vy = mol.atoms[i3].y - v0.y, vz = mol.atoms[i3].z - v0.z;
            const double lu = std::sqrt (ux * ux + uy * uy + uz * uz);
            const double lv = std::sqrt (vx * vx + vy * vy + vz * vz);
            if (lu < 1e-12 || lv < 1e-12) continue;

            double cosT = (ux * vx + uy * vy + uz * vz) / (lu * lv);
            cosT = std::clamp (cosT, -1.0, 1.0);
            const double sinT = std::sqrt (1.0 - cosT * cosT);
            if (sinT < 1e-9) continue;            // straight/degenerate: force ill-defined

            const double theta = std::acos (cosT);
            const double c = ang.k * (theta - ang.eqAngle) / sinT;

            // F on outer atom 1 (varies u), and on outer atom 3 (varies v).
            const double f1x = c * (vx / (lu * lv) - cosT * ux / (lu * lu));
            const double f1y = c * (vy / (lu * lv) - cosT * uy / (lu * lu));
            const double f1z = c * (vz / (lu * lv) - cosT * uz / (lu * lu));
            const double f3x = c * (ux / (lu * lv) - cosT * vx / (lv * lv));
            const double f3y = c * (uy / (lu * lv) - cosT * vy / (lv * lv));
            const double f3z = c * (uz / (lu * lv) - cosT * vz / (lv * lv));

            f[i1].x += f1x; f[i1].y += f1y; f[i1].z += f1z;
            f[i3].x += f3x; f[i3].y += f3y; f[i3].z += f3z;
            f[i2].x -= f1x + f3x; f[i2].y -= f1y + f3y; f[i2].z -= f1z + f3z;  // vertex balances
        }
        return f;
    }

private:
    const Molecule & mol;
};
