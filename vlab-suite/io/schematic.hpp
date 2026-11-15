// schematic.hpp -- the design as a clean labelled diagram (like a textbook figure): bodies as outlined shapes, springs and dampers as the
// usual symbols, supports as ground symbols, applied loads as arrows, and every part named by a label with a leader line.
// The result is an SVG file: it stays sharp at any size and drops straight into Word, PowerPoint, a web page or a report.
#pragma once
#include <string>
#include <vector>
#include "vlab/model.hpp"

namespace vlab {

struct SchematicOptions {
    bool names = true;          // label the parts
    bool values = true;         // ... with their numbers: mass, spring stiffness k, damping c, load
    bool arrowheads = false;    // arrowheads on the label lines (the plain line is the textbook look)
    bool loads = true;          // draw the applied loads as arrows
    std::string title;          // a caption in the top left (empty: none)
    bool symbols = false;       // label the parts with their VARIABLES (m_1, k_1, c_1, F_1(t) ...) instead of names and numbers, and draw the axes
    bool serif = false;         // textbook lettering (italic serif) instead of the sans-serif UI font
};

// The variable each part is called in a derivation (shared by the labelled diagram and the derivation sheet).
// Numbers are 1-based; 0 means "this part has no symbol".
struct SymbolMap {
    std::vector<int> body;      // moving bodies: m_i, J_i, x_i, y_i, theta_i
    std::vector<int> link;      // springs k_j, dampers c_j, rods L_j, spring distributions, damper distributions, torsional springs, joints: counted per kind
    int moving = 0, springs = 0, dampers = 0, springDists = 0, damperDists = 0, rods = 0, torsions = 0, joints = 0;
};
SymbolMap makeSymbols(const Model& m);

// The diagram as inline SVG markup. It is only ever embedded in the page that the sheet exporter renders to JPG or PDF
// (derivation.hpp); no .svg file is written. Empty when the design is empty.
std::string schematicSvgString(const Model& m, const SchematicOptions& o);

}  // namespace vlab
