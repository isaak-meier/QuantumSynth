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
    assert(m.bonds[0].k > 0.0);                        // value set by the file
    // carbon lists both bonds, each oxygen one
    assert(m.atoms[0].bonds.size() == 2);
    assert(m.atoms[1].bonds.size() == 1);
    assert(m.atoms[2].bonds.size() == 1);

    // round-trip: writeItp then re-parse preserves the parameters.
    {
        std::ostringstream os;
        writeItp(m, os);
        std::istringstream is(os.str());
        Molecule m2 = parseItp(is);
        assert(m2.atoms.size() == m.atoms.size());
        assert(m2.bonds.size() == m.bonds.size());
        assert(m2.angles.size() == m.angles.size());
        for (size_t i = 0; i < m.atoms.size(); ++i)
            assert(std::abs(m2.atoms[i].mass - m.atoms[i].mass) < 1e-3);
        for (size_t i = 0; i < m.bonds.size(); ++i) {
            assert(std::abs(m2.bonds[i].k - m.bonds[i].k) < 1e-3);
            assert(std::abs(m2.bonds[i].eqBondLength - m.bonds[i].eqBondLength) < 1e-3);
        }
        for (size_t i = 0; i < m.angles.size(); ++i) {
            assert(std::abs(m2.angles[i].k - m.angles[i].k) < 1e-3);
            assert(std::abs(m2.angles[i].eqAngle - m.angles[i].eqAngle) < 1e-3);  // deg<->rad survives
        }
    }
    std::cout << "all assertions passed\n";
}
