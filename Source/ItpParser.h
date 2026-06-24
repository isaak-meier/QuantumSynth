#pragma once

#include "Molecule.h"
#include <array>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// Parse a GROMACS-style .itp topology into a Molecule.
// Recognises [ molecule ], [ atoms ], [ bonds ], [ angles ]. Angle vertex is
// the middle atom (a1-a2-a3, vertex a2), per GROMACS convention; the angle's
// equilibrium value is read in degrees and stored as radians.
// ';' starts a comment; blank lines ignored. Throws on malformed input.

inline Molecule parseItp(std::istream & in) {
    Molecule mol;
    std::vector<std::pair<int, int>> bondAtoms;        // 0-based endpoints, resolved below
    std::vector<std::array<int, 3>> angleAtoms;        // 0-based triplets, resolved below
    std::string section;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (auto c = line.find(';'); c != std::string::npos)
            line.erase(c);                            // strip comment

        std::istringstream ss(line);
        std::string first;
        if (!(ss >> first)) continue;                 // blank

        if (first == "[") {                           // [ section ]
            ss >> section;
            continue;
        }

        std::istringstream ls(line);                  // fresh stream over whole line
        if (section == "molecule") {
            ls >> mol.name;                           // exclusions ignored
        } else if (section == "atoms") {
            // id type resnr res atom cgnr charge mass
            std::string tok;
            for (int i = 0; i < 6 && ls >> tok; ++i) {}
            double charge;
            Atom a;
            if (!(ls >> charge >> a.mass))
                throw std::runtime_error("bad atom line " + std::to_string(lineNo));
            mol.atoms.push_back(a);
        } else if (section == "bonds") {
            // atom1 atom2 functype r0 k   (atoms are 1-based)
            int a1, a2, functype;
            Bond b;
            if (!(ls >> a1 >> a2 >> functype >> b.eqBondLength >> b.k))
                throw std::runtime_error("bad bond line " + std::to_string(lineNo));
            bondAtoms.emplace_back(a1 - 1, a2 - 1);
            mol.bonds.push_back(b);
        } else if (section == "angles") {
            // atom1 atom2 atom3 functype th0(degrees) k   (atoms 1-based, a2 = vertex)
            int a1, a2, a3, functype;
            double th0deg;
            Angle ang;
            if (!(ls >> a1 >> a2 >> a3 >> functype >> th0deg >> ang.k))
                throw std::runtime_error("bad angle line " + std::to_string(lineNo));
            constexpr double deg2rad = 3.14159265358979323846 / 180.0;
            ang.eqAngle = th0deg * deg2rad;     // file is degrees; store radians
            angleAtoms.push_back({a1 - 1, a2 - 1, a3 - 1});
            mol.angles.push_back(ang);
        }
    }

    // Wire bonds to atoms now that mol.atoms won't reallocate (pointers stable),
    // and record each bond's index on its endpoints.
    for (std::size_t i = 0; i < mol.bonds.size(); ++i) {
        auto [i1, i2] = bondAtoms[i];
        if (i1 < 0 || i1 >= (int) mol.atoms.size() ||
            i2 < 0 || i2 >= (int) mol.atoms.size())
            throw std::runtime_error("bond references missing atom");
        mol.bonds[i].atom1 = &mol.atoms[(std::size_t) i1];
        mol.bonds[i].atom2 = &mol.atoms[(std::size_t) i2];
        mol.atoms[(std::size_t) i1].bonds.push_back((int) i);
        mol.atoms[(std::size_t) i2].bonds.push_back((int) i);
    }

    // Wire angles to their three atoms.
    const int nAtoms = (int) mol.atoms.size();
    for (std::size_t i = 0; i < mol.angles.size(); ++i) {
        const auto [i1, i2, i3] = angleAtoms[i];
        if (i1 < 0 || i1 >= nAtoms || i2 < 0 || i2 >= nAtoms || i3 < 0 || i3 >= nAtoms)
            throw std::runtime_error("angle references missing atom");
        mol.angles[i].atom1 = &mol.atoms[(std::size_t) i1];
        mol.angles[i].atom2 = &mol.atoms[(std::size_t) i2];
        mol.angles[i].atom3 = &mol.atoms[(std::size_t) i3];
    }
    return mol;
}

inline Molecule parseItpFile(const std::string & path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    return parseItp(f);
}

// Serialise a Molecule back to .itp text. Writes the parameters parseItp reads
// (mass, bond r0/k, angle th0-in-degrees/k). Atom type/name is a placeholder
// "X" — Molecule doesn't keep the element symbol. Round-trips through parseItp.
inline void writeItp(const Molecule & m, std::ostream & out) {
    out << std::fixed << std::setprecision(4);
    out << "[ molecule ]\n; Name         Exclusions\n";
    out << (m.name.empty() ? std::string("MOL") : m.name) << "            1\n\n";

    out << "[ atoms ]\n;; id type resnr res atom cgnr charge mass\n";
    for (std::size_t i = 0; i < m.atoms.size(); ++i)
        out << "   " << (i + 1) << "   X   1   A   X   1   0.0000 " << m.atoms[i].mass << "\n";

    out << "\n[ bonds ]\n;; Atom1    Atom2    Functype    r0          k\n";
    for (const auto & b : m.bonds) {
        const std::size_t i1 = (std::size_t) (b.atom1 - m.atoms.data());
        const std::size_t i2 = (std::size_t) (b.atom2 - m.atoms.data());
        out << "   " << (i1 + 1) << "        " << (i2 + 1)
            << "        1           " << b.eqBondLength << "      " << b.k << "\n";
    }

    out << "\n[ angles ]\n;; Atom1    Atom2    Atom3      Functype    th0         k\n";
    constexpr double rad2deg = 180.0 / 3.14159265358979323846;
    for (const auto & a : m.angles) {
        const std::size_t i1 = (std::size_t) (a.atom1 - m.atoms.data());
        const std::size_t i2 = (std::size_t) (a.atom2 - m.atoms.data());
        const std::size_t i3 = (std::size_t) (a.atom3 - m.atoms.data());
        out << "   " << (i1 + 1) << "        " << (i2 + 1) << "        " << (i3 + 1)
            << "          1           " << (a.eqAngle * rad2deg) << "      " << a.k << "\n";
    }
}

inline void writeItpFile(const Molecule & m, const std::string & path) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    writeItp(m, f);
}
