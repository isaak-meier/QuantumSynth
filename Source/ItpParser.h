#pragma once

#include "Molecule.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

// Parse a GROMACS-style .itp topology into a Molecule.
// Recognises [ molecule ], [ atoms ], [ bonds ]. Other sections (e.g.
// [ angles ]) are skipped — Molecule has no place to put them yet.
// ';' starts a comment; blank lines ignored. Throws on malformed input.

inline Molecule parseItp(std::istream & in) {
    Molecule mol;
    std::vector<std::pair<int, int>> bondAtoms;   // 0-based endpoints, resolved below
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
    return mol;
}

inline Molecule parseItpFile(const std::string & path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    return parseItp(f);
}
