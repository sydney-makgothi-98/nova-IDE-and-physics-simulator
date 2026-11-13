// section_ui.cpp -- draws a beam's cross-section, optionally coloured by the normal stress through it.
#include <algorithm>
#include <cmath>
#include "app.hpp"

namespace vlab {

void drawSectionSketch(const Section& s, ImVec2 p0, ImVec2 size, const SectionStress* st, const Pal& pal) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), pal.panel, 6.0f);
    dl->AddRect(p0, ImVec2(p0.x + size.x, p0.y + size.y), pal.gridAxis, 6.0f);
    double bmax = 0;
    for (double b : s.b) bmax = std::max(bmax, b);
    if (bmax <= 0 || s.H <= 0) return;
    const float pad = 12.0f;
    const double sc = std::min((size.x - 2 * pad) / bmax, (size.y - 2 * pad) / s.H);
    const ImVec2 c(p0.x + size.x / 2, p0.y + size.y / 2);
    const float top = (float)(c.y - s.H * sc / 2);
    double smax = 1e-9;
    if (st) smax = std::max(std::max(std::fabs(st->sigMax), std::fabs(st->sigMin)), 1e-9);
    for (int k = 0; k < s.n; ++k) {
        float y0 = top + (float)((s.y(k) - s.dy / 2) * sc), y1 = y0 + (float)(s.dy * sc) + 0.7f;
        float half = (float)(s.b[k] * sc / 2);
        if (half < 0.1f) continue;
        ImU32 col = IM_COL32(150, 162, 186, 255);
        if (st && k < (int)st->sigma.size()) {                                   // compression blue, tension red, zero white
            float v = (float)std::clamp(st->sigma[k] / smax, -1.0, 1.0), a = std::fabs(v);
            int r = (int)(245 + ((v > 0 ? 225 : 50) - 245) * a), g = (int)(245 + ((v > 0 ? 60 : 110) - 245) * a), b = (int)(245 + ((v > 0 ? 55 : 235) - 245) * a);
            col = IM_COL32(r, g, b, 255);
        }
        dl->AddRectFilled(ImVec2(c.x - half, y0), ImVec2(c.x + half, y1), col);
    }
    const float ny = top + (float)(s.yc * sc);                                    // neutral axis, dashed
    for (float x = p0.x + 6; x < p0.x + size.x - 6; x += 10.0f) dl->AddLine(ImVec2(x, ny), ImVec2(x + 5, ny), pal.amber, 1.5f);
    dl->AddText(ImVec2(p0.x + 8, ny - 17), pal.amber, "neutral axis");
    char b1[48], b2[48];
    std::snprintf(b1, sizeof b1, "h = %.1f mm", s.H * 1000.0);
    std::snprintf(b2, sizeof b2, "b = %.1f mm", bmax * 1000.0);
    dl->AddText(ImVec2(p0.x + size.x - ImGui::CalcTextSize(b1).x - 8, p0.y + 5), pal.dim, b1);
    dl->AddText(ImVec2(p0.x + size.x - ImGui::CalcTextSize(b2).x - 8, p0.y + size.y - 20), pal.dim, b2);
}

}  // namespace vlab
