#pragma once

#include <string>
#include <vector>

// A molecule is a graph: atoms are nodes, bonds are springs between them.
// ponytail: atoms/bonds reference each other by index into the Molecule's
// vectors rather than by value/pointer — avoids the Atom<->Bond reference
// cycle in the sketch, and keeps everything copyable & contiguous.

struct Atom {
    double mass = 0.0;
    double x = 0.0, y = 0.0, z = 0.0;   // position
    double vx = 0.0, vy = 0.0, vz = 0.0;   // velocity
    std::vector<int> bonds;             // indices into Molecule::bonds
};

struct Bond {
    double k = 0.0;             // spring constant
    double eqBondLength = 0.0;  // equilibrium length
    Atom * atom1;               // index into Molecule::atoms
    Atom * atom2;               // index into Molecule::atoms
};

// Harmonic angle: U = 1/2 k (theta - eqAngle)^2, with theta the angle at the
// vertex (atom2) between the rays atom2->atom1 and atom2->atom3.
struct Angle {
    double k = 0.0;             // angle stiffness
    double eqAngle = 0.0;       // equilibrium angle, radians
    Atom * atom1;               // outer atom
    Atom * atom2;               // vertex (middle atom)
    Atom * atom3;               // outer atom
};

struct Molecule {
    std::string name;
    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    std::vector<Angle> angles;
};
