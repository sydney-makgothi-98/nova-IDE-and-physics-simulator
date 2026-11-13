// theme.cpp -- light and dark interface themes (ImGui, ImPlot and the canvas palette).
#include <fstream>
#include "app.hpp"
#include "implot.h"

namespace vlab {

static ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }

static void applyColors(bool dark) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    if (dark) {
        ImGui::StyleColorsDark();
        c[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.094f, 0.118f, 1);
        c[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.082f, 0.104f, 1);
        c[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.12f, 0.15f, 0.98f);
        c[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.28f, 1);
        c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.145f, 0.18f, 1);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.19f, 0.24f, 1);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.23f, 0.29f, 1);
        c[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.075f, 0.095f, 1);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.115f, 0.15f, 1);
        c[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.075f, 0.095f, 1);
        c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.26f, 1);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.28f, 0.37f, 1);
        c[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.45f, 0.95f, 1);
        c[ImGuiCol_Header] = ImVec4(0.17f, 0.30f, 0.55f, 0.55f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.38f, 0.70f, 0.70f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.45f, 0.95f, 0.80f);
        c[ImGuiCol_Tab] = ImVec4(0.11f, 0.125f, 0.16f, 1);
        c[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.45f, 0.95f, 0.80f);
        c[ImGuiCol_TabSelected] = ImVec4(0.17f, 0.36f, 0.75f, 1);
        c[ImGuiCol_TabDimmed] = ImVec4(0.09f, 0.10f, 0.13f, 1);
        c[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.28f, 0.58f, 1);
        c[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.78f, 1.0f, 1);
        c[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.58f, 1.0f, 1);
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.70f, 1.0f, 1);
        c[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.45f, 0.95f, 0.5f);
        ImPlot::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
        c[ImGuiCol_Text] = rgb(28, 34, 46);
        c[ImGuiCol_TextDisabled] = rgb(120, 128, 146);
        c[ImGuiCol_WindowBg] = rgb(244, 246, 250);
        c[ImGuiCol_ChildBg] = rgb(250, 251, 253);
        c[ImGuiCol_PopupBg] = rgb(255, 255, 255, 0.98f);
        c[ImGuiCol_Border] = rgb(204, 210, 222);
        c[ImGuiCol_FrameBg] = rgb(232, 236, 244);
        c[ImGuiCol_FrameBgHovered] = rgb(220, 227, 240);
        c[ImGuiCol_FrameBgActive] = rgb(208, 218, 238);
        c[ImGuiCol_TitleBg] = rgb(228, 232, 240);
        c[ImGuiCol_TitleBgActive] = rgb(214, 222, 236);
        c[ImGuiCol_MenuBarBg] = rgb(236, 239, 245);
        c[ImGuiCol_Button] = rgb(222, 228, 240);
        c[ImGuiCol_ButtonHovered] = rgb(200, 214, 240);
        c[ImGuiCol_ButtonActive] = rgb(51, 115, 242);
        c[ImGuiCol_Header] = rgb(170, 195, 238, 0.65f);
        c[ImGuiCol_HeaderHovered] = rgb(140, 175, 235, 0.80f);
        c[ImGuiCol_HeaderActive] = rgb(51, 115, 242, 0.85f);
        c[ImGuiCol_Tab] = rgb(222, 227, 237);
        c[ImGuiCol_TabHovered] = rgb(120, 160, 235);
        c[ImGuiCol_TabSelected] = rgb(255, 255, 255);
        c[ImGuiCol_TabDimmed] = rgb(228, 232, 241);
        c[ImGuiCol_TabDimmedSelected] = rgb(246, 248, 252);
        c[ImGuiCol_CheckMark] = rgb(30, 100, 230);
        c[ImGuiCol_SliderGrab] = rgb(60, 125, 240);
        c[ImGuiCol_SliderGrabActive] = rgb(30, 100, 230);
        c[ImGuiCol_DockingPreview] = rgb(51, 115, 242, 0.5f);
        c[ImGuiCol_Separator] = rgb(204, 210, 222);
        c[ImGuiCol_ScrollbarBg] = rgb(236, 239, 245);
        ImPlot::StyleColorsLight();
    }
}

Pal App::palette() const {
    if (darkTheme)
        return {IM_COL32(18, 20, 26, 255), IM_COL32(28, 31, 40, 255), IM_COL32(40, 45, 58, 255), IM_COL32(90, 100, 125, 255), IM_COL32(100, 110, 130, 255),
                IM_COL32(200, 205, 220, 255), IM_COL32(120, 128, 148, 255), IM_COL32(150, 158, 178, 255), IM_COL32(205, 212, 228, 255), IM_COL32(225, 228, 238, 255),
                IM_COL32(255, 214, 120, 255), IM_COL32(14, 16, 22, 215)};
    return {IM_COL32(247, 249, 252, 255), IM_COL32(233, 237, 243, 255), IM_COL32(215, 221, 231, 255), IM_COL32(150, 160, 182, 255), IM_COL32(118, 128, 150, 255),
            IM_COL32(30, 36, 48, 255), IM_COL32(104, 114, 134, 255), IM_COL32(86, 96, 118, 255), IM_COL32(62, 70, 86, 255), IM_COL32(72, 78, 94, 255),
            IM_COL32(170, 104, 0, 255), IM_COL32(255, 255, 255, 228)};
}

void App::applyTheme() { applyColors(darkTheme); }

void App::setTheme(bool dark) {
    darkTheme = dark;
    applyTheme();
    if (!docsDir.empty()) std::ofstream(docsDir + "/theme.txt") << (dark ? "dark" : "light");
}

}  // namespace vlab
