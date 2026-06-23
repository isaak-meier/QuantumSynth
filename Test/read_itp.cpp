#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

// Adapted from the reForge Python package to read itp files
// https://github.com/DanYev/reForge#
/*
Please cite:
    Yangaliev D, Ozkan SB. Coarse-grained RNA model for the Martini 3 force field.
    Biophys J. 2025 Aug 5:S0006-3495(25)00483-7. doi: 10.1016/j.bpj.2025.07.034.
*/

// ---------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------

// Represents one parsed line of a topology section.
// NOTE: the Python original returns (connectivity, parameters, comment)
// from line2bond() but doesn't show that function, so the types below
// are a best guess (atom indices as ints, force-field params as doubles).
// Adjust if your real line2bond() returns something different.
struct BondEntry {
    std::vector<int> connectivity;   // e.g. atom indices
    std::vector<double> parameters;  // e.g. force constants
    std::string comment;             // trailing comment, if any
};

using ItpData = std::map<std::string, std::vector<BondEntry>>;

// ---------------------------------------------------------------
// String helpers (Python's str.strip() equivalent)
// ---------------------------------------------------------------

static inline std::string strip(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------
// Stand-in for the Python line2bond() function.
// Replace this with the real parsing logic from your codebase --
// the actual Python implementation wasn't included in the snippet
// you shared, so this is just a placeholder that tokenizes the line
// and splits off any trailing comment.
// ---------------------------------------------------------------

BondEntry line2bond(const std::string& line, const std::string& currentTag) {
    BondEntry entry;

    std::string content = line;
    auto semi = content.find(';');
    if (semi != std::string::npos) {
        entry.comment = strip(content.substr(semi + 1));
        content = content.substr(0, semi);
    }

    std::istringstream contentStream(content);
    std::vector<std::string> fields;
    std::string token;
    while (contentStream >> token) fields.push_back(token);

    // TODO: split `fields` into connectivity vs parameters according
    // to the rules your real line2bond() uses (this typically depends
    // on currentTag -- e.g. "bonds" needs 2 atom indices, "angles" needs 3).
    for (size_t i = 0; i < fields.size(); ++i) {
        try {
            if (i < 2) { // placeholder rule
                entry.connectivity.push_back(std::stoi(fields[i]));
            } else {
                entry.parameters.push_back(std::stod(fields[i]));
            }
        } catch (const std::exception&) {
            // ignore non-numeric tokens
        }
    }
    return entry;
}

// ---------------------------------------------------------------
// read_itp
// ---------------------------------------------------------------

// Read a Gromacs ITP file and organize its contents by section.
ItpData read_itp(const std::string& filename) {
    // logger.debug(...) -> replace with your actual logging framework
    std::cerr << "[DEBUG] Reading ITP file: " << filename << std::endl;

    ItpData itp_data;
    std::string current_tag;

    std::ifstream file(filename);
    if (!file.is_open()) {
        // logger.warning(...)
        std::cerr << "[WARNING] ITP file not found: " << filename << std::endl;
        // Return empty structure with expected sections
        ItpData defaults;
        for (const std::string& section :
             {"bonds", "angles", "dihedrals", "constraints",
              "exclusions", "pairs", "virtual_sites3"}) {
            defaults[section] = {};
        }
        return defaults;
    }

    std::string line;
    while (std::getline(file, line)) {
        // getline() already strips '\n'; drop a trailing '\r' too (Windows files)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string stripped_line = strip(line);

        // Skip comments and empty lines
        if (stripped_line.empty() || stripped_line[0] == ';') {
            continue;
        }

        // Detect section headers: "[ section ]"
        // (mirrors the Python check, which tests the raw line, not the
        // stripped one -- so a header must start at column 0)
        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            current_tag = strip(line.substr(1, line.size() - 2));
            itp_data[current_tag] = {};
        } else if (!current_tag.empty()) {
            BondEntry entry = line2bond(line, current_tag);
            itp_data[current_tag].push_back(entry);
        }
    }

    // logger.debug(...)
    std::cerr << "[DEBUG] Successfully read ITP file with "
              << itp_data.size() << " sections" << std::endl;

    return itp_data;
}

int main() {
    // Call the function — this is your "object" holding all parsed data
    ItpData topology = read_itp("./CO2.itp");

    // Iterate over each section (e.g. "bonds", "angles", ...)
    for (const auto& [section, entries] : topology) {
        std::cout << "Section: " << section
                  << " (" << entries.size() << " entries)\n";

        // Iterate over each BondEntry within that section
        for (const BondEntry& entry : entries) {
            std::cout << "  connectivity: ";
            for (int idx : entry.connectivity) std::cout << idx << " ";

            std::cout << "| parameters: ";
            for (double p : entry.parameters) std::cout << p << " ";

            std::cout << "| comment: " << entry.comment << "\n";
        }
    }
//
//    // Direct access to a specific section, e.g. "bonds"
//    if (topology.count("bonds")) {
//        const std::vector<BondEntry>& bonds = topology["bonds"];
//        std::cout << "Number of bonds: " << bonds.size() << "\n";
//    }
//
//    return 0;
}



