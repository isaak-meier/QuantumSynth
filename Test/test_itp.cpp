// Self-check for the .itp parser. Build & run:
//   c++ -std=c++17 -I.. test_itp.cpp -o test_itp && ./test_itp
// (or from repo root: c++ -std=c++17 -I. Test/test_itp.cpp -o /tmp/t && /tmp/t)
#include "../Source/ItpParser.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    Molecule m = parseItpFile("CO2.itp");
    assert(m.name == "CO2");
    assert(m.atoms.size() == 3);
    assert(m.bonds.size() == 2);
    assert(std::abs(m.atoms[0].mass - 12.0) < 1e-9);   // C
    assert(std::abs(m.atoms[1].mass - 16.0) < 1e-9);   // O
    assert(std::abs(m.atoms[2].mass - 16.0) < 1e-9);   // O
    // both bonds anchor on carbon (atom 0)
    assert(m.bonds[0].atom1 == &m.atoms[0] && m.bonds[0].atom2 == &m.atoms[1]);
    assert(m.bonds[1].atom1 == &m.atoms[0] && m.bonds[1].atom2 == &m.atoms[2]);
    assert(std::abs(m.bonds[0].eqBondLength - 1.0) < 1e-9);
    assert(std::abs(m.bonds[0].k - 1.0) < 1e-9);
    // carbon lists both bonds, each oxygen one
    assert(m.atoms[0].bonds.size() == 2);
    assert(m.atoms[1].bonds.size() == 1);
    assert(m.atoms[2].bonds.size() == 1);
    std::cout << "all assertions passed\n";
}
