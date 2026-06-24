// Self-check for the .gro coordinate reader. Build & run from Test/:
//   c++ -std=c++17 test_gro.cpp -o test_gro && ./test_gro
#include "../Source/ItpParser.h"
#include "../Source/GroParser.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    Molecule m = parseItpFile("CO2.itp");
    parseGroFileInto(m, "CO2.gro");
    assert(m.atoms.size() == 3);
    // linear molecule: C at 0, O at +1, O at -1 along x; y/z zero
    assert(std::abs(m.atoms[0].x - 0.0) < 1e-9);
    assert(std::abs(m.atoms[1].x - 1.0) < 1e-9);
    assert(std::abs(m.atoms[2].x + 1.0) < 1e-9);
    for (const auto& a : m.atoms) {
        assert(std::abs(a.y) < 1e-9);
        assert(std::abs(a.z) < 1e-9);
    }
    std::cout << "all assertions passed\n";
}
