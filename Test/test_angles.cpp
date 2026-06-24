// Self-check for the harmonic angle force. Build & run from Test/:
//   c++ -std=c++17 test_angles.cpp -o test_angles && ./test_angles
#include "../Source/Forces.h"
#include <cassert>
#include <cmath>
#include <iostream>

// 3 atoms forming a 90-degree angle: outer(1,0,0) - vertex(0,0,0) - outer(0,1,0).
static Molecule rightAngle(double eq, double k) {
    Molecule m;
    m.atoms.resize(3);
    m.atoms[0].x = 1; m.atoms[0].y = 0;   // outer (a1)
    m.atoms[1].x = 0; m.atoms[1].y = 0;   // vertex (a2)
    m.atoms[2].x = 0; m.atoms[2].y = 1;   // outer (a3)
    Angle a; a.k = k; a.eqAngle = eq;
    a.atom1 = &m.atoms[0]; a.atom2 = &m.atoms[1]; a.atom3 = &m.atoms[2];
    m.angles.push_back(a);
    return m;
}

int main() {
    const double halfPi = M_PI / 2.0;

    // At equilibrium (theta == theta0) the angle exerts no force.
    {
        Molecule m = rightAngle(halfPi, 1.0);
        for (const auto& f : Forces(m).compute())
            assert(std::abs(f.x) < 1e-9 && std::abs(f.y) < 1e-9 && std::abs(f.z) < 1e-9);
    }

    // theta0 smaller than current 90deg -> angle wants to close.
    // c = k(theta-theta0) = 0.1; expect f1=(0,+0.1,0), f3=(+0.1,0,0).
    {
        Molecule m = rightAngle(halfPi - 0.1, 1.0);
        auto f = Forces(m).compute();
        assert(std::abs(f[0].y - 0.1) < 1e-9 && std::abs(f[0].x) < 1e-9);   // a1 moves +y toward a3
        assert(std::abs(f[2].x - 0.1) < 1e-9 && std::abs(f[2].y) < 1e-9);   // a3 moves +x toward a1
        // total force cancels
        double sx = f[0].x + f[1].x + f[2].x;
        double sy = f[0].y + f[1].y + f[2].y;
        assert(std::abs(sx) < 1e-9 && std::abs(sy) < 1e-9);
    }

    // theta0 larger -> opens: forces reverse sign.
    {
        Molecule m = rightAngle(halfPi + 0.1, 1.0);
        auto f = Forces(m).compute();
        assert(f[0].y < 0 && f[2].x < 0);   // pushed apart
    }

    std::cout << "all assertions passed\n";
}
