// schematic.hpp -- the design as a clean labelled diagram (like a textbook figure): bodies as outlined shapes, springs and dampers as the
// usual symbols, supports as ground symbols, applied loads as arrows, and every part named by a label with a leader line.
// The result is an SVG file: it stays sharp at any size and drops straight into Word, PowerPoint, a web page or a report.
#pragma once
#include <string>
#include "vlab/model.hpp"

namespace vlab {

struct SchematicOptions {
    bool names = true;          // label the parts
    bool values = true;         // ... with their numbers: mass, spring stiffness k, damping c, load
    bool arrowheads = false;    // arrowheads on the label lines (the plain line is the textbook look)
    bool loads = true;          // draw the applied loads as arrows
    std::string title;          // a caption in the top left (empty: none)
};

bool exportSchematicSvg(const std::string& path, const Model& m, const SchematicOptions& o, std::string* error = nullptr);

}  // namespace vlab
