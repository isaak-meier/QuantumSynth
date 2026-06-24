// Self-check for the time integrator. Build & run from Test/:
//   c++ -std=c++17 test_integrator.cpp -o test_integrator && ./test_integrator
#include "../Source/ItpParser.h"
#include "../Source/GroParser.h"
#include "../Source/Integrator.h"
#include <cassert>
#include <cmath>
#include <iostream>

static double comX(const Molecule& m) {
    double mx = 0, tot = 0;
    for (const auto& a : m.atoms) { mx += a.mass * a.x; tot += a.mass; }
    return mx / tot;
}

int main() {
    Molecule m = parseItpFile("CO2.itp");
    parseGroFileInto(m, "CO2.gro");

    m.atoms[1].x = 1.5;                 // stretch one bond, start at rest
    const double com0 = comX(m);

    double minR = 1e30, maxR = -1e30;
    for (int i = 0; i < 5000; ++i) {
        step(m, 0.01);
        const double r = m.atoms[1].x - m.atoms[0].x;   // C->O1 length
        minR = std::min(minR, r);
        maxR = std::max(maxR, r);
        assert(std::abs(comX(m) - com0) < 1e-6);        // momentum conserved -> COM fixed
    }

    // It must actually oscillate: pass back through ~equilibrium (1.0) and not blow up.
    assert(minR < 1.05 && maxR > 1.45 && maxR < 2.0);

    // Bond stretch is hard-limited to (1 + MAX_STRETCH)*r0 = 2.5 for r0=1.
    // Yank an oxygen far out; one tiny step should pull the bond back to 2.5.
    {
        Molecule m2 = parseItpFile("CO2.itp");
        parseGroFileInto(m2, "CO2.gro");
        m2.atoms[1].x = 10.0;                 // way past the limit
        step(m2, 1e-5);                       // dt tiny so integration barely moves it
        const double r = m2.atoms[1].x - m2.atoms[0].x;
        assert(r < 2.5 + 1e-3);               // clamped, not 9
        assert(std::abs(r - 2.5) < 1e-2);     // pulled right to the limit
    }

    std::cout << "all assertions passed  (r in [" << minR << ", " << maxR << "])\n";
}
