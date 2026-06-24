// Self-check for the harmonic bond force. Build & run from Test/:
//   c++ -std=c++17 test_forces.cpp -o test_forces && ./test_forces
#include "../Source/ItpParser.h"
#include "../Source/GroParser.h"
#include "../Source/Forces.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    Molecule m = parseItpFile("CO2.itp");
    parseGroFileInto(m, "CO2.gro");
    m.angles.clear();                               // isolate the bond force -> test_angles
    for (auto& b : m.bonds) { b.k = 1.0; b.eqBondLength = 1.0; }  // fix params, ignore file values

    // 2) Stretch: pull the +x oxygen (atom 1) from x=1 out to x=1.5.
    m.atoms[1].x = 1.5;
    auto f = Forces(m).compute();
    // r=1.5, r0=1, k=1  ->  s = k(r-r0)/r = 1/3 ; force magnitude = s*r = 0.5
    assert(std::abs(f[1].x - (-0.5)) < 1e-9);   // oxygen pulled back toward carbon (-x)
    assert(std::abs(f[0].x - (+0.5)) < 1e-9);   // carbon pulled toward oxygen (+x)
    assert(std::abs(f[2].x) < 1e-9);            // other bond still at equilibrium

    // total force is zero (equal-and-opposite)
    double sx = 0, sy = 0, sz = 0;
    for (const auto& v : f) { sx += v.x; sy += v.y; sz += v.z; }
    assert(std::abs(sx) < 1e-9 && std::abs(sy) < 1e-9 && std::abs(sz) < 1e-9);

    std::cout << "all assertions passed\n";
}
