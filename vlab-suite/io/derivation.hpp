// derivation.hpp -- the "system sheet": a labelled diagram of the design, its specifications, and the full derivation of its
// governing equations of motion, typeset like a textbook solution. The sheet is built as one self-contained web page and rendered
// to a JPG or a PDF (no SVG files are ever written).
#pragma once
#include <string>
#include "schematic.hpp"
#include "vlab/model.hpp"

namespace vlab {

enum class SheetKind {
    SystemOnly,     // just the picture of the system (names and values as chosen in SchematicOptions)
    Full            // the diagram labelled with variables, the specifications under it, then the derivation
};

struct Sheet {
    std::string html;       // the page
    int width = 1000;       // CSS pixels the page is laid out in (the screenshot width)
};

// Builds the page. `diagram` only matters for SystemOnly (the Full sheet always labels the parts with variables).
Sheet buildSheet(const Model& m, SheetKind kind, const SchematicOptions& diagram);

enum class SheetFormat { Jpg, Png, Pdf };
inline const char* sheetExtension(SheetFormat f) { return f == SheetFormat::Pdf ? ".pdf" : f == SheetFormat::Png ? ".png" : ".jpg"; }

// Renders a page to a JPG, PNG or PDF with the Microsoft Edge (or Chrome) that comes with Windows. Returns false with a reason in *error.
bool renderSheet(const Sheet& s, const std::string& outPath, SheetFormat format, std::string* error);

// Pieces exposed for the tests: the math markup -> HTML converter used by the derivation.
std::string mathMarkup(const std::string& s);

}  // namespace vlab
