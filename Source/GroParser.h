#pragma once

#include "Molecule.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

// Fill a Molecule's atom coordinates from a GROMACS .gro file.
// Format: line 1 title, line 2 atom count, then one line per atom with the
// x/y/z (nm) in fixed columns starting at index 20, then a box line.
// Atom order must match the Molecule (i.e. the .itp it was parsed from).

inline void parseGroInto(Molecule & mol, std::istream & in) {
    std::string title, line;
    std::getline(in, title);                 // title (ignored)
    if (!std::getline(in, line))
        throw std::runtime_error("gro: missing atom count");

    int count = std::stoi(line);
    if (count != (int) mol.atoms.size())
        throw std::runtime_error("gro: atom count " + std::to_string(count) +
                                 " != molecule's " + std::to_string(mol.atoms.size()));

    for (int i = 0; i < count; ++i) {
        if (!std::getline(in, line) || line.size() < 20)
            throw std::runtime_error("gro: bad atom line " + std::to_string(i + 1));
        std::istringstream ss(line.substr(20));   // skip resnum/resname/atomname/atomnum
        Atom & a = mol.atoms[(size_t) i];
        if (!(ss >> a.x >> a.y >> a.z))           // velocities, if any, ignored
            throw std::runtime_error("gro: bad coords on atom " + std::to_string(i + 1));
    }
}

inline void parseGroFileInto(Molecule & mol, const std::string & path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    parseGroInto(mol, f);
}
