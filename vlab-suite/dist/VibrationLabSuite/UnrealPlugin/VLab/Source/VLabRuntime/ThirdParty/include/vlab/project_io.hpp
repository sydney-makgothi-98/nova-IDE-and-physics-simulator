// project_io.hpp -- save/load a design as a small text file (.vlab), plus the built-in example gallery.
#pragma once
#include <string>
#include <vector>
#include "model.hpp"

namespace vlab {

bool saveModelFile(const std::string& path, const Model& m);
bool loadModelFile(const std::string& path, Model& out, std::string* error = nullptr);
std::string modelToText(const Model& m);
bool modelFromText(const std::string& text, Model& out, std::string* error = nullptr);

struct Example {
    std::string title;
    std::string summary;     // one or two sentences: what it shows and what to look for
    Model model;
    int category = 0;        // 0 Basic, 1 Intermediate, 2 Complex
};
const std::vector<Example>& exampleGallery();

}  // namespace vlab
